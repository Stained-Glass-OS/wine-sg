/* taskkeys-probe (taskkeys-gate.sh, patches/sg/9068): stand-ins for sg-shell's calendar
 * (SgCalendarHost, told SgCalendarToggle) and Start (SgStartPanel, told WM_USER+10 to get
 * ready for typing), which write what they were told to OUT, one line each, for 60 s.
 *
 *   taskkeys-probe.exe OUT
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static const char *out;
static UINT toggle;

static void note( const char *what )
{
    FILE *f = fopen( out, "a" );
    if (f) { fprintf( f, "%s\n", what ); fclose( f ); }
}

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    char buf[64];
    if (msg == toggle) { snprintf( buf, sizeof(buf), "calendar toggle bar=%d", lp != 0 ); note( buf ); return 0; }
    if (msg == WM_USER + 10) { snprintf( buf, sizeof(buf), "start ready-for-typing %d", (int)wp ); note( buf ); return 0; }
    return DefWindowProcA( hwnd, msg, wp, lp );
}

int main( int argc, char **argv )
{
    WNDCLASSA wc = { 0 };
    DWORD start = GetTickCount();
    MSG msg;

    if (argc < 2) return 2;
    out = argv[1];
    toggle = RegisterWindowMessageA( "SgCalendarToggle" );
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA( NULL );
    wc.lpszClassName = "SgCalendarHost";
    RegisterClassA( &wc );
    wc.lpszClassName = "SgStartPanel";
    RegisterClassA( &wc );
    CreateWindowExA( WS_EX_TOOLWINDOW, "SgCalendarHost", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL );
    CreateWindowExA( WS_EX_TOOLWINDOW, "SgStartPanel", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL );
    note( "ready" );
    while (GetTickCount() - start < 60000)
    {
        while (PeekMessageA( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageA( &msg );
        Sleep( 20 );
    }
    return 0;
}
