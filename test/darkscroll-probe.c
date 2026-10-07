/* darkscroll-probe (test/darkscroll-gate.sh, wine-sg 1474): two list boxes
 * with vertical scroll bars, one given SetWindowTheme( L"DarkMode_Explorer" )
 * as Windows' programs do for dark scroll bars; prints the theme state and
 * the lightness (0-255) of each one's scroll bar track and thumb:
 *   theme active=0|1
 *   plain track=L thumb=L
 *   dark track=L thumb=L */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>

static int lightness( COLORREF c )
{
    return (GetRValue( c ) * 30 + GetGValue( c ) * 59 + GetBValue( c ) * 11) / 100;
}

static void measure( const char *what, HWND list )
{
    RECT wr, cr;
    HDC dc;
    int x, sb = GetSystemMetrics( SM_CXVSCROLL ), arrow = GetSystemMetrics( SM_CYVSCROLL );
    COLORREF track, thumb;

    GetWindowRect( list, &wr );
    GetClientRect( list, &cr );
    RedrawWindow( list, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE | RDW_UPDATENOW );
    dc = GetWindowDC( list );
    /* the scroll bar is the window's right edge, inside its border */
    x = (wr.right - wr.left) - GetSystemMetrics( SM_CXEDGE ) - sb / 2 - 1;
    thumb = GetPixel( dc, x, GetSystemMetrics( SM_CYEDGE ) + arrow + 6 );
    track = GetPixel( dc, x, (wr.bottom - wr.top) - GetSystemMetrics( SM_CYEDGE ) - arrow - 6 );
    ReleaseDC( list, dc );
    printf( "%s track=%d thumb=%d\n", what, lightness( track ), lightness( thumb ) );
}

int main( void )
{
    HWND win, plain, dark;
    MSG msg;
    int i;

    win = CreateWindowExW( 0, L"STATIC", L"darkscroll", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, 520, 400,
                           NULL, NULL, NULL, NULL );
    plain = CreateWindowExW( WS_EX_CLIENTEDGE, L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                             10, 10, 220, 300, win, NULL, NULL, NULL );
    dark = CreateWindowExW( WS_EX_CLIENTEDGE, L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                            260, 10, 220, 300, win, NULL, NULL, NULL );
    for (i = 0; i < 200; i++)
    {
        SendMessageW( plain, LB_ADDSTRING, 0, (LPARAM)L"an item" );
        SendMessageW( dark, LB_ADDSTRING, 0, (LPARAM)L"an item" );
    }
    SetWindowTheme( dark, L"DarkMode_Explorer", NULL );
    UpdateWindow( win );
    for (i = 0; i < 20 && PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ); i++) DispatchMessageW( &msg );
    Sleep( 300 );
    printf( "theme active=%d\n", IsThemeActive() );
    measure( "plain", plain );
    measure( "dark", dark );
    return 0;
}
