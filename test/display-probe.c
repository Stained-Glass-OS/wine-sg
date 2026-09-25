/* display-probe: windows for test/display-gate.sh.
 *
 *   display-probe hold SECONDS         a window, kept for SECONDS
 *   display-probe spawn                a window, then a second process
 *                                      (display-probe second) with its own,
 *                                      which clips and releases the cursor;
 *                                      waits for it and prints its exit code
 *   display-probe green X Y W H [BMP]  a solid green window (painted once);
 *                                      then the desktop repaints itself
 *                                      (RedrawWindow of the desktop, or BMP
 *                                      as the new wallpaper); it stays 10 s
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BOOL painted;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint( hwnd, &ps );
        HBRUSH brush = CreateSolidBrush( RGB( 0, 255, 0 ) );
        RECT rc;
        /* green once: a later repaint would hide what was drawn over it */
        GetClientRect( hwnd, &rc );
        if (!painted) FillRect( hdc, &rc, brush );
        DeleteObject( brush );
        EndPaint( hwnd, &ps );
        painted = TRUE;
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcA( hwnd, msg, wp, lp );
}

static HWND make_window( const char *title, DWORD style, int x, int y, int w, int h )
{
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA( NULL );
    wc.lpszClassName = "SgDisplayProbe";
    wc.hCursor = LoadCursorA( NULL, (LPCSTR)IDC_ARROW );
    RegisterClassA( &wc );
    return CreateWindowA( "SgDisplayProbe", title, style | WS_VISIBLE, x, y, w, h, NULL, NULL, NULL, NULL );
}

static void pump( DWORD ms )
{
    DWORD start = GetTickCount();
    MSG msg;

    while (GetTickCount() - start < ms)
    {
        while (PeekMessageA( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            TranslateMessage( &msg );
            DispatchMessageA( &msg );
        }
        Sleep( 10 );
    }
}

int main( int argc, char **argv )
{
    HWND hwnd;

    if (argc == 3 && !strcmp( argv[1], "hold" ))
    {
        make_window( "hold", WS_OVERLAPPEDWINDOW, 10, 10, 200, 150 );
        pump( atoi( argv[2] ) * 1000 );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "spawn" ))
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[] = "display-probe.exe second";
        DWORD code = 99;

        hwnd = make_window( "first", WS_OVERLAPPEDWINDOW, 50, 50, 300, 200 );
        printf( "first=%s\n", hwnd ? "window" : "none" );
        fflush( stdout );
        if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "second=not started %lu\n", GetLastError() );
            return 1;
        }
        {
            DWORD start = GetTickCount();
            while (WaitForSingleObject( pi.hProcess, 0 ) == WAIT_TIMEOUT && GetTickCount() - start < 30000) pump( 100 );
        }
        GetExitCodeProcess( pi.hProcess, &code );
        printf( "second exit=%lu\n", code );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "second" ))
    {
        RECT clip = { 100, 100, 400, 300 };

        hwnd = make_window( "second", WS_OVERLAPPEDWINDOW, 150, 80, 300, 200 );
        pump( 1500 );
        SetForegroundWindow( hwnd );
        pump( 500 );
        printf( "clip=%d\n", ClipCursor( &clip ) );
        pump( 500 );
        printf( "unclip=%d\n", ClipCursor( NULL ) );
        pump( 1500 );
        printf( "second=alive window=%d\n", IsWindow( hwnd ) );
        fflush( stdout );
        return 7;
    }
    if (argc >= 6 && !strcmp( argv[1], "green" ))
    {
        hwnd = make_window( "green", WS_POPUP, atoi( argv[2] ), atoi( argv[3] ), atoi( argv[4] ), atoi( argv[5] ) );
        while (!painted) pump( 50 );
        pump( 1000 );
        printf( "painted\n" );
        fflush( stdout );
        /* the desktop repaints itself -- a new wallpaper (a BMP made here) */
        if (argc > 6)
            printf( "wallpaper=%d\n", SystemParametersInfoA( SPI_SETDESKWALLPAPER, 0, argv[6], SPIF_SENDCHANGE ) );
        else
            RedrawWindow( GetDesktopWindow(), NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW );
        printf( "desktop redrawn\n" );
        fflush( stdout );
        pump( 10000 );
        return 0;
    }
    printf( "usage: see the source\n" );
    return 2;
}
