/* The status bar follows the view (patches/sg/0599): "setup" makes C:\one
 * with one file and C:\second.txt; "clip" puts C:\second.txt on the clipboard
 * (copied), for File Explorer's Ctrl+V. */
#include <windows.h>
#include <shlobj.h>

int main( int argc, char **argv )
{
    if (argc > 1 && !strcmp( argv[1], "setup" ))
    {
        CreateDirectoryW( L"C:\\one", NULL );
        CloseHandle( CreateFileW( L"C:\\one\\first.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
        CloseHandle( CreateFileW( L"C:\\second.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
        return 0;
    }
    if (argc > 1 && !strcmp( argv[1], "clip" ))
    {
        static const WCHAR path[] = L"C:\\second.txt\0";
        HGLOBAL mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + sizeof(path) );
        HGLOBAL effect = GlobalAlloc( GMEM_MOVEABLE, sizeof(DWORD) );
        DROPFILES *df = GlobalLock( mem );
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        memcpy( df + 1, path, sizeof(path) );
        GlobalUnlock( mem );
        *(DWORD *)GlobalLock( effect ) = DROPEFFECT_COPY;
        GlobalUnlock( effect );
        OpenClipboard( NULL );
        EmptyClipboard();
        SetClipboardData( CF_HDROP, mem );
        SetClipboardData( RegisterClipboardFormatW( L"Preferred DropEffect" ), effect );
        CloseClipboard();
        return 0;
    }
    return 2;
}
