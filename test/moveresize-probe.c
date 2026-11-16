/* Wine's side of test/moveresize-gate.sh (wine-sg 7790):
 *   moveresize-probe frame TITLE   rect=L,T,R,B of the frame (SgLinuxWindow TITLE), visible=0|1
 *   moveresize-probe loop TITLE    movesize=0|1 capture=0|1: its thread in a move/size loop,
 *                                  a capture held (GetGUIThreadInfo)
 *   moveresize-probe start         start=0|1: the Start menu is shown (SgStartPanel, or a popup menu)
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    HWND hwnd;
    RECT rc;

    if (argc >= 3 && !wcscmp( argv[1], L"frame" ))
    {
        if (!(hwnd = FindWindowW( L"SgLinuxWindow", argv[2] ))) { printf( "rect=none\n" ); return 1; }
        GetWindowRect( hwnd, &rc );
        printf( "rect=%ld,%ld,%ld,%ld visible=%d\n", rc.left, rc.top, rc.right, rc.bottom, IsWindowVisible( hwnd ) );
    }
    else if (argc >= 3 && !wcscmp( argv[1], L"loop" ))
    {
        GUITHREADINFO info = { sizeof(info) };
        DWORD tid;
        hwnd = FindWindowW( L"SgLinuxWindow", argv[2] );
        if (!hwnd) hwnd = FindWindowW( L"Shell_TrayWnd", NULL );
        tid = GetWindowThreadProcessId( hwnd, NULL );
        GetGUIThreadInfo( tid, &info );
        printf( "movesize=%d capture=%d\n", !!(info.flags & GUI_INMOVESIZE), info.hwndCapture != 0 );
    }
    else if (argc >= 2 && !wcscmp( argv[1], L"start" ))
    {
        /* the shell's Start panel, or the plain desktop's Start menu (a popup menu) */
        hwnd = FindWindowW( L"SgStartPanel", NULL );
        if (!hwnd || !IsWindowVisible( hwnd )) hwnd = FindWindowW( L"#32768", NULL );
        printf( "start=%d\n", hwnd && IsWindowVisible( hwnd ) );
    }
    return 0;
}
