/* A green window on the desktop that stays: desktopbg-gate.sh unmaps it
 * behind Wine's back and looks at what X shows where it was. */
#include <windows.h>

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_ERASEBKGND)
    {
        RECT rc;
        HBRUSH b = CreateSolidBrush( RGB(0, 200, 0) );
        GetClientRect( hwnd, &rc );
        FillRect( (HDC)wp, &rc, b );
        DeleteObject( b );
        return 1;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int WINAPI wWinMain( HINSTANCE inst, HINSTANCE prev, WCHAR *cmd, int show )
{
    WNDCLASSW wc = { 0, proc, 0, 0, inst, 0, LoadCursorW( 0, (WCHAR *)IDC_ARROW ), 0, 0, L"SgBgProbe" };
    MSG msg;
    HWND hwnd;
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( WS_EX_TOOLWINDOW, L"SgBgProbe", L"SGBGPROBE", WS_POPUP | WS_VISIBLE,
                            300, 200, 200, 150, 0, 0, inst, 0 );
    UpdateWindow( hwnd );
    while (GetMessageW( &msg, 0, 0, 0 )) DispatchMessageW( &msg );
    return 0;
}
