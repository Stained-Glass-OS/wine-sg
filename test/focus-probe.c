/* Keyboard focus after activation from another process (patches/sg/0527).
 *   focus-probe window NAME X Y LOGFILE   a window that appends each WM_CHAR to LOGFILE
 *   focus-probe activate NAME             SetForegroundWindow on it from this
 *                                         process, as the taskbar does */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *logpath;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_CHAR)
    {
        FILE *f = fopen( logpath, "a" );
        if (f) { fputc( (int)wp, f ); fclose( f ); }
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage( 0 ); return 0; }
    return DefWindowProcA( hwnd, msg, wp, lp );
}

int main( int argc, char **argv )
{
    if (argc == 6 && !strcmp( argv[1], "window" ))
    {
        WNDCLASSA wc = { 0 };
        MSG msg;
        HWND hwnd;
        logpath = argv[5];
        wc.lpfnWndProc = proc;
        wc.lpszClassName = "SgFocusProbe";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassA( &wc );
        hwnd = CreateWindowA( "SgFocusProbe", argv[2], WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              atoi( argv[3] ), atoi( argv[4] ), 320, 220, 0, 0, 0, 0 );
        SetFocus( hwnd );
        while (GetMessageA( &msg, 0, 0, 0 )) { TranslateMessage( &msg ); DispatchMessageA( &msg ); }
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "activate" ))
    {
        HWND hwnd = FindWindowA( "SgFocusProbe", argv[2] );
        if (!hwnd) { printf( "nowindow\n" ); return 1; }
        printf( "activated=%d\n", SetForegroundWindow( hwnd ) );
        return 0;
    }
    return 2;
}
