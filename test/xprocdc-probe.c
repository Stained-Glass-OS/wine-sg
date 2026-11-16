/* For test/xprocdc-gate.sh: another process's child window in a top-level,
 * drawn with GDI by its own process only when it chooses (as Chromium's GPU
 * process draws a WebView2 control's DirectComposition target), and the
 * top-level's own process drawing over everything (as WPF presents).
 *   host FILE        a top-level "XProcHost" (white) with a WS_EX_TRANSPARENT
 *                    child (as WPF's HwndHost); writes the child's handle to
 *                    FILE, then waits for the events below
 *   render SLOT FILE a child window in SLOT (another process's), red; draws
 *                    again on its event; writes its centre (screen) to FILE
 *   signal NAME      sets the event NAME: cover (the host fills its whole
 *                    client blue through its DC, as WPF's present), glass
 *                    (the host becomes a sheet of glass: its X window is made
 *                    again with alpha, as WPF's Mica does), redraw (the
 *                    render process draws red again), quit */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HANDLE ev_cover, ev_glass, ev_redraw, ev_quit;

static LRESULT CALLBACK quiet_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_PAINT)
    {
        /* validated, nothing drawn: what it shows is drawn when it chooses
         * (no BeginPaint either: Chromium's GPU-process window takes no DC
         * but the one it draws with) */
        ValidateRect( hwnd, NULL );
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void fill( HWND hwnd, COLORREF color )
{
    HDC dc = GetDC( hwnd );
    RECT rc;
    HBRUSH brush = CreateSolidBrush( color );
    GetClientRect( hwnd, &rc );
    FillRect( dc, &rc, brush );
    DeleteObject( brush );
    GdiFlush();
    ReleaseDC( hwnd, dc );
}

static HANDLE event( const char *name )
{
    char full[64];
    snprintf( full, sizeof(full), "Local\\xprocdc_%s", name );
    /* quit reaches both processes (manual reset) */
    return CreateEventA( NULL, !strcmp( name, "quit" ), FALSE, full );
}

static void pump( DWORD ms )
{
    MSG msg;
    DWORD end = GetTickCount() + ms;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 10 );
    }
}

int main( int argc, char **argv )
{
    WNDCLASSW wc = { 0 };
    FILE *f;

    if (argc < 2) return 2;
    ev_cover = event( "cover" ); ev_glass = event( "glass" ); ev_redraw = event( "redraw" ); ev_quit = event( "quit" );
    if (!strcmp( argv[1], "signal" ) && argc == 3)
    {
        HANDLE h = event( argv[2] );
        return !SetEvent( h );
    }
    wc.lpfnWndProc = quiet_proc;
    wc.hCursor = LoadCursorW( 0, (LPCWSTR)IDC_ARROW );
    if (!strcmp( argv[1], "host" ) && argc == 3)
    {
        HWND top, slot;
        HANDLE evs[3];
        wc.lpfnWndProc = DefWindowProcW;
        wc.hbrBackground = GetStockObject( WHITE_BRUSH );
        wc.lpszClassName = L"XProcHost";
        RegisterClassW( &wc );
        wc.lpfnWndProc = quiet_proc;
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"XProcSlot";
        RegisterClassW( &wc );
        top = CreateWindowExW( 0, L"XProcHost", L"XProc Host", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
                               100, 100, 460, 340, 0, 0, 0, 0 );
        slot = CreateWindowExW( WS_EX_TRANSPARENT, L"XProcSlot", NULL, WS_CHILD | WS_VISIBLE, 40, 40, 320, 220,
                                top, 0, 0, 0 );
        pump( 500 );
        if (!(f = fopen( argv[2], "w" ))) return 1;
        fprintf( f, "%lu\n", (unsigned long)(ULONG_PTR)slot );
        fclose( f );
        evs[0] = ev_cover; evs[1] = ev_glass; evs[2] = ev_quit;
        for (;;)
        {
            DWORD r = MsgWaitForMultipleObjects( 3, evs, FALSE, INFINITE, QS_ALLINPUT );
            if (r == WAIT_OBJECT_0) fill( top, RGB( 0, 0, 255 ) );
            else if (r == WAIT_OBJECT_0 + 1)
            {
                MARGINS m = { -1, -1, -1, -1 };
                DwmExtendFrameIntoClientArea( top, &m );
                SetWindowPos( top, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED );
            }
            else if (r == WAIT_OBJECT_0 + 2) break;
            pump( 1 );
        }
        return 0;
    }
    if (!strcmp( argv[1], "render" ) && argc == 4)
    {
        HWND slot = (HWND)(ULONG_PTR)strtoul( argv[2], NULL, 0 ), child;
        HANDLE evs[2];
        RECT rc;
        wc.lpszClassName = L"XProcRender";
        RegisterClassW( &wc );
        child = CreateWindowExW( 0, L"XProcRender", NULL, WS_CHILD | WS_VISIBLE, 30, 30, 220, 140, slot, 0, 0, 0 );
        if (!child) return 1;
        pump( 300 );
        fill( child, RGB( 255, 0, 0 ) );
        GetWindowRect( child, &rc );
        if (!(f = fopen( argv[3], "w" ))) return 1;
        fprintf( f, "%ld %ld\n", (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 );
        fclose( f );
        evs[0] = ev_redraw; evs[1] = ev_quit;
        for (;;)
        {
            DWORD r = MsgWaitForMultipleObjects( 2, evs, FALSE, INFINITE, QS_ALLINPUT );
            if (r == WAIT_OBJECT_0) fill( child, RGB( 255, 0, 0 ) );
            else if (r == WAIT_OBJECT_0 + 1) break;
            pump( 1 );
        }
        return 0;
    }
    return 2;
}
