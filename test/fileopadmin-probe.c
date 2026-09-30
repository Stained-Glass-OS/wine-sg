/* A folder the user may not write to (patches/sg/0589). "denied": a copy
 * there asks for an administrator ("Destination Folder Access Denied"); the
 * probe answers Cancel. "noerrorui": with FOF_NOERRORUI nothing asks and the
 * copy fails, not 0. "paste": files put on the clipboard without a drop effect
 * are pasted (copied) into C:\pastedst by the folder's Paste. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

static BOOL found;

static DWORD WINAPI answer( void *arg )
{
    HWND hwnd = NULL;
    int i;
    for (i = 0; i < 100 && !(hwnd = FindWindowW( L"SGFileAccessDenied", NULL )); i++) Sleep( 100 );
    if (!hwnd) return 0;
    found = TRUE;
    Sleep( 300 );
    PostMessageW( hwnd, WM_COMMAND, IDCANCEL, 0 );
    return 0;
}

int main( int argc, char **argv )
{
    SHFILEOPSTRUCTW op = { 0 };
    int ret;

    CloseHandle( CreateFileW( L"C:\\fileop-src.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
    op.wFunc = FO_COPY;
    op.pFrom = L"C:\\fileop-src.txt\0";
    op.pTo = L"C:\\readonly\0";
    op.fFlags = FOF_NOCONFIRMATION;
    if (argc > 1 && !strcmp( argv[1], "denied" ))
    {
        CloseHandle( CreateThread( NULL, 0, answer, NULL, 0, NULL ) );
        ret = SHFileOperationW( &op );
        printf( "denied: asked=%d ret=%d aborted=%d\n", found, ret, op.fAnyOperationsAborted );
        return !(found && ret == ERROR_CANCELLED && op.fAnyOperationsAborted);
    }
    if (argc > 1 && !strcmp( argv[1], "noerrorui" ))
    {
        op.fFlags |= FOF_NOERRORUI | FOF_SILENT;
        ret = SHFileOperationW( &op );
        printf( "noerrorui: ret=%d copied=%d\n", ret,
                GetFileAttributesW( L"C:\\readonly\\fileop-src.txt" ) != INVALID_FILE_ATTRIBUTES );
        return !ret;
    }
    if (argc > 1 && !strcmp( argv[1], "paste" ))
    {
        WCHAR src[] = L"C:\\fileop-src.txt\0";
        HGLOBAL mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + sizeof(src) );
        DROPFILES *df = GlobalLock( mem );
        IShellFolder *desk, *folder;
        IContextMenu *cm;
        LPITEMIDLIST pidl;
        CMINVOKECOMMANDINFO ici = { sizeof(ici) };
        HMENU menu = CreatePopupMenu();

        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        memcpy( df + 1, src, sizeof(src) );
        GlobalUnlock( mem );
        OleInitialize( NULL );
        OpenClipboard( NULL ); EmptyClipboard(); SetClipboardData( CF_HDROP, mem ); CloseClipboard();
        CreateDirectoryW( L"C:\\pastedst", NULL );
        SHGetDesktopFolder( &desk );
        IShellFolder_ParseDisplayName( desk, NULL, NULL, (WCHAR *)L"C:\\pastedst", NULL, &pidl, NULL );
        IShellFolder_BindToObject( desk, pidl, NULL, &IID_IShellFolder, (void **)&folder );
        IShellFolder_CreateViewObject( folder, NULL, &IID_IContextMenu, (void **)&cm );
        IContextMenu_QueryContextMenu( cm, menu, 0, 1, 0x7fff, CMF_NORMAL );
        ici.lpVerb = "paste";
        ici.nShow = SW_SHOWNORMAL;
        IContextMenu_InvokeCommand( cm, &ici );
        ret = GetFileAttributesW( L"C:\\pastedst\\fileop-src.txt" ) != INVALID_FILE_ATTRIBUTES;
        printf( "paste: copied=%d\n", ret );
        return !ret;
    }
    return 2;
}
