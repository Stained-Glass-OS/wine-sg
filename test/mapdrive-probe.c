/* Map Network Drive and Disconnect Network Drives (patches/sg/0594). The
 * dialogs were stubs that failed at once (WN_NO_NETWORK). A thread opens
 * WNetConnectionDialog; the probe finds it, checks it offers drive letters,
 * gives it a folder that is not a network folder (a warning must come), then
 * cancels it (WN_CANCEL). Then the same for WNetDisconnectDialog. */
#include <windows.h>
#include <winnetwk.h>
#include <stdio.h>

static DWORD WINAPI map_thread( void *arg ) { return WNetConnectionDialog( NULL, RESOURCETYPE_DISK ); }
static DWORD WINAPI unmap_thread( void *arg ) { return WNetDisconnectDialog( NULL, RESOURCETYPE_DISK ); }

static HWND wait_window( const WCHAR *cls, const WCHAR *title, HANDLE thread )
{
    HWND hwnd = NULL;
    int i;
    for (i = 0; i < 100 && !(hwnd = FindWindowW( cls, title )); i++)
        if (WaitForSingleObject( thread, 100 ) == WAIT_OBJECT_0) break;
    return hwnd;
}

int main( void )
{
    HANDLE t = CreateThread( NULL, 0, map_thread, NULL, 0, NULL );
    HWND dlg, combo, edit, box;
    DWORD code = 0;
    int letters = 0, warned = 0, ok = 1;

    if (!(dlg = wait_window( L"SGMapNetworkDrive", L"Map Network Drive", t )))
    {
        GetExitCodeThread( t, &code );
        printf( "map: no dialog (returned %lu)\n", code );
        return 1;
    }
    Sleep( 300 );
    if ((combo = FindWindowExW( dlg, NULL, L"ComboBox", NULL ))) letters = SendMessageW( combo, CB_GETCOUNT, 0, 0 );
    if ((edit = FindWindowExW( dlg, NULL, L"Edit", NULL ))) SendMessageW( edit, WM_SETTEXT, 0, (LPARAM)L"not-a-share" );
    PostMessageW( dlg, WM_COMMAND, IDOK, 0 );
    if ((box = wait_window( L"#32770", L"Map Network Drive", t )))
    {
        warned = 1;
        PostMessageW( box, WM_COMMAND, IDOK, 0 );
        Sleep( 300 );
    }
    PostMessageW( dlg, WM_COMMAND, IDCANCEL, 0 );
    WaitForSingleObject( t, 5000 );
    GetExitCodeThread( t, &code );
    printf( "map: letters=%d warned=%d returned=%lu\n", letters, warned, code );
    ok = letters > 0 && warned && code == WN_CANCEL;

    t = CreateThread( NULL, 0, unmap_thread, NULL, 0, NULL );
    if (!(dlg = wait_window( L"SGMapNetworkDrive", L"Disconnect Network Drives", t )))
    {
        GetExitCodeThread( t, &code );
        printf( "disconnect: no dialog (returned %lu)\n", code );
        return 1;
    }
    Sleep( 300 );
    PostMessageW( dlg, WM_COMMAND, IDCANCEL, 0 );
    WaitForSingleObject( t, 5000 );
    GetExitCodeThread( t, &code );
    printf( "disconnect: returned=%lu\n", code );
    return !(ok && code == WN_CANCEL);
}
