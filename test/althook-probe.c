/* althook-probe: "hook" installs a low-level keyboard hook that takes 40 ms
 * per key (a busy shell's); "window" shows a window and prints what was typed
 * into it and CLOSED when WM_CLOSE comes (Alt+F4), else TIMEOUT after 8 s.
 * test/althook-gate.sh
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static LRESULT CALLBACK slow_hook( int code, WPARAM wp, LPARAM lp )
{
    if (code == HC_ACTION) Sleep( 40 );
    return CallNextHookEx( NULL, code, wp, lp );
}

static char typed[64];

static LRESULT CALLBACK proc( HWND h, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_CHAR && wp >= ' ' && wp < 127 && strlen( typed ) < sizeof(typed) - 1) typed[strlen( typed )] = (char)wp;
    if (msg == WM_CLOSE) { printf( "TYPED %s\nCLOSED\n", typed ); fflush( stdout ); PostQuitMessage( 0 ); return 0; }
    return DefWindowProcW( h, msg, wp, lp );
}

int main( int argc, char **argv )
{
    MSG m;
    DWORD start = GetTickCount();
    if (argc > 1 && !strcmp( argv[1], "hook" ))
    {
        SetWindowsHookExW( WH_KEYBOARD_LL, slow_hook, GetModuleHandleW( NULL ), 0 );
        while (GetTickCount() - start < 20000)
        {
            while (PeekMessageW( &m, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &m );
            MsgWaitForMultipleObjects( 0, NULL, FALSE, 20, QS_ALLINPUT );
        }
        return 0;
    }
    {
        WNDCLASSW wc = { 0 };
        HWND h;
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW( NULL );
        wc.hbrBackground = GetStockObject( WHITE_BRUSH );
        wc.lpszClassName = L"AltHook";
        RegisterClassW( &wc );
        h = CreateWindowW( L"AltHook", L"althook", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 400, 300, 0, 0, wc.hInstance, 0 );
        SetForegroundWindow( h );
        while (GetTickCount() - start < 8000)
        {
            while (PeekMessageW( &m, 0, 0, 0, PM_REMOVE ))
            {
                if (m.message == WM_QUIT) return 0;
                TranslateMessage( &m );
                DispatchMessageW( &m );
            }
            MsgWaitForMultipleObjects( 0, NULL, FALSE, 20, QS_ALLINPUT );
        }
        printf( "TIMEOUT\n" );
    }
    return 0;
}
