/* printwindow-probe: PrintWindow of another process's window (patches/sg/0076),
 * for test/printwindow-gate.sh.
 *
 *   printwindow-probe window TITLE [cloak]   a window painted solid green (its
 *                                            class brush too); "cloak" cloaks it
 *                                            once it has painted
 *   printwindow-probe print TITLE FLAGS      PrintWindow it into a DIB from this
 *                                            process: the colour at its middle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint( hwnd, &ps );
        HBRUSH brush = CreateSolidBrush( RGB(0, 200, 0) );
        FillRect( dc, &ps.rcPaint, brush );
        DeleteObject( brush );
        EndPaint( hwnd, &ps );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int main( int argc, char **argv )
{
    WCHAR title[64];

    if (argc < 3) return 2;
    MultiByteToWideChar( CP_ACP, 0, argv[2], -1, title, 64 );
    if (!strcmp( argv[1], "window" ))
    {
        WNDCLASSW wc = { 0 };
        MSG msg;
        HWND hwnd;
        DWORD start = GetTickCount();
        BOOL cloaked = FALSE;

        wc.lpfnWndProc = proc;
        wc.hbrBackground = CreateSolidBrush( RGB(0, 200, 0) );
        wc.lpszClassName = L"SgPrintProbe";
        RegisterClassW( &wc );
        hwnd = CreateWindowW( wc.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 300, 200,
                              NULL, NULL, NULL, NULL );
        for (;;)
        {
            while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
            if (argc > 3 && !cloaked && GetTickCount() - start > 1500)
            {
                BOOL on = TRUE;
                DwmSetWindowAttribute( hwnd, DWMWA_CLOAK, &on, sizeof(on) );
                cloaked = TRUE;
            }
            if (GetTickCount() - start > 60000) return 0;
            Sleep( 20 );
        }
    }
    if (!strcmp( argv[1], "print" ) && argc == 4)
    {
        HWND hwnd = FindWindowW( L"SgPrintProbe", title );
        HDC screen = GetDC( 0 ), dc = CreateCompatibleDC( screen );
        HBITMAP bmp = CreateCompatibleBitmap( screen, 300, 200 );
        COLORREF c;
        BOOL ret;

        if (!hwnd) { printf( "no window\n" ); return 1; }
        SelectObject( dc, bmp );
        PatBlt( dc, 0, 0, 300, 200, BLACKNESS );
        ret = PrintWindow( hwnd, dc, atoi( argv[3] ) );
        c = GetPixel( dc, 150, 110 );
        printf( "ret=%d pixel=%d,%d,%d\n", ret, GetRValue(c), GetGValue(c), GetBValue(c) );
        return 0;
    }
    return 2;
}
