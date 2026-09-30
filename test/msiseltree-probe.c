/* An installer's feature tree (patches/sg/0598). Runs msiexec on a package
 * whose dialog has a SelectionTree and texts subscribed to
 * SelectionDescription and SelectionSize; reads them and the tree's first
 * item's state image. The tree's item handles were cut to 32 bits: no state
 * icons, no selection, the placeholders left in the texts. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static HWND dlg;
static WCHAR desc[256], size[256];
static int state;

static BOOL CALLBACK child( HWND h, LPARAM l )
{
    WCHAR cls[64], text[256];
    GetClassNameW( h, cls, ARRAYSIZE(cls) );
    if (!wcscmp( cls, L"SysTreeView32" ))
    {
        HTREEITEM first = (HTREEITEM)SendMessageW( h, TVM_GETNEXTITEM, TVGN_ROOT, 0 );
        if (first) state = (SendMessageW( h, TVM_GETITEMSTATE, (WPARAM)first, TVIS_STATEIMAGEMASK ) >> 12) & 0xf;
        return TRUE;
    }
    GetWindowTextW( h, text, ARRAYSIZE(text) );
    if (wcsstr( text, L"main files" ) || wcsstr( text, L"DescPlaceholder" )) lstrcpyW( desc, text );
    if (wcsstr( text, L"requires" ) || wcsstr( text, L"SizePlaceholder" )) lstrcpyW( size, text );
    return TRUE;
}

int main( int argc, char **argv )
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[512];
    int i, ok;

    snprintf( cmd, sizeof(cmd), "msiexec.exe /i \"%s\"", argv[1] );
    if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return 2;
    for (i = 0; i < 300 && !(dlg = FindWindowW( NULL, L"Tree" )); i++) Sleep( 100 );
    if (!dlg) { printf( "no dialog\n" ); return 1; }
    Sleep( 1500 );
    EnumChildWindows( dlg, child, 0 );
    printf( "state=%d desc=%ls size=%ls\n", state, desc, size );
    ok = state > 0 && !wcscmp( desc, L"The main files of the test." ) && wcsstr( size, L"requires" );
    TerminateProcess( pi.hProcess, 0 );
    return !ok;
}
