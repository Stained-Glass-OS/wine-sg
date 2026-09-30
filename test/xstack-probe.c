/* xstack-probe: Wine's Z order is the X stacking in a virtual desktop
 * (patches/sg/0531). A window below others in Z order started at the top of
 * the desktop's X children and stayed there: Word's splash, shown after its
 * topmost error box, covered it. The gate reads the screen from X during
 * each phase; this program sets them up.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static LRESULT CALLBACK wp( HWND h, UINT m, WPARAM w, LPARAM l )
{
    if (m == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint( h, &ps );
        HBRUSH b = CreateSolidBrush( (COLORREF)GetWindowLongPtrW( h, GWLP_USERDATA ) );
        FillRect( dc, &ps.rcPaint, b );
        DeleteObject( b );
        EndPaint( h, &ps );
        return 0;
    }
    return DefWindowProcW( h, m, w, l );
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 20 );
    }
}

/* the gate reads the screen during the 5 s after each phase line */
static void phase( int n )
{
    printf( "phase=%d\n", n );
    fflush( stdout );
    pump( 5000 );
}

static HWND make( COLORREF color, int x, int y, int w, int h, DWORD ex )
{
    HWND hwnd = CreateWindowExW( ex, L"xstack", L"", WS_POPUP, x, y, w, h, 0, 0, 0, 0 );
    SetWindowLongPtrW( hwnd, GWLP_USERDATA, color );
    ShowWindow( hwnd, SW_SHOW );
    UpdateWindow( hwnd );
    return hwnd;
}

int wmain( int argc, WCHAR **argv )
{
    WNDCLASSW wc = { 0, wp, 0, 0, GetModuleHandleW( 0 ), 0, 0, 0, 0, L"xstack" };
    HWND top, splash, a, b;

    RegisterClassW( &wc );

    /* 1: a topmost window, then a normal one shown later over the same place */
    top = make( RGB(0, 0, 255), 100, 100, 200, 150, WS_EX_TOPMOST );
    pump( 300 );
    splash = make( RGB(255, 0, 0), 50, 50, 400, 300, WS_EX_TOOLWINDOW );
    pump( 800 );
    printf( "order1=%d\n", GetWindow( top, GW_HWNDNEXT ) == splash || !(GetWindowLongW( splash, GWL_EXSTYLE ) & WS_EX_TOPMOST) );
    phase( 1 );
    DestroyWindow( top );
    DestroyWindow( splash );

    /* 2: A then B (on top); A raised to the top */
    a = make( RGB(0, 255, 0), 500, 100, 300, 200, 0 );
    b = make( RGB(255, 255, 0), 550, 150, 300, 200, 0 );
    pump( 300 );
    SetWindowPos( a, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    pump( 800 );
    phase( 2 );

    /* 3: A put right after (below) B */
    SetWindowPos( a, b, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
    pump( 800 );
    printf( "order3=%d\n", GetWindow( b, GW_HWNDNEXT ) == a );
    phase( 3 );
    return 0;
}
