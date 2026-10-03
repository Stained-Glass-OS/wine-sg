/* threadclass-probe: a program's second thread registers a window class and
 * makes a window with it, as .NET's SystemEvents does (patches/sg/0775).
 * Prints "main OK|FAIL" and "thread OK|FAIL" with the atom and error.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
static LRESULT CALLBACK proc( HWND h, UINT m, WPARAM w, LPARAM l ) { return DefWindowProcW( h, m, w, l ); }
static void attempt( const char *who, const WCHAR *cls )
{
    WNDCLASSW wc = { 0 };
    ATOM atom;
    HWND hwnd;
    wc.lpfnWndProc = proc;
    wc.lpszClassName = cls;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    atom = RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, cls, cls, WS_POPUP, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL );
    printf( "%s %s atom=%04x err=%lu\n", who, atom >= 0xc000 && hwnd ? "OK" : "FAIL", atom, GetLastError() );
    if (hwnd) DestroyWindow( hwnd );
}
static DWORD WINAPI thread( void *arg )
{
    attempt( "thread", L".NET-BroadcastEventWindow.probe.0" );
    return 0;
}
int main( void )
{
    HANDLE t;
    attempt( "main", L"ThreadClassProbeMain" );
    t = CreateThread( NULL, 0, thread, NULL, 0, NULL );
    WaitForSingleObject( t, INFINITE );
    fflush( stdout );
    return 0;
}
