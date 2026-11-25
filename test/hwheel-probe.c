/* hwheel-probe.exe LOG: a captioned window at (100,200) 500x300 that logs the wheel messages it
 * gets (WM_MOUSEWHEEL, WM_MOUSEHWHEEL, and 0x28C -- WM_MOUSEHWHEEL turned into a "non-client"
 * message, which does not exist) with their lParam, as lines "MSG <hex> <x> <y> <delta>".
 * First line: "RECT <window l t r b> <client in screen coordinates l t r b>". (wine-sg 9510) */
#include <windows.h>
#include <stdio.h>

static FILE *out;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL || msg == 0x28c)
    {
        fprintf( out, "MSG %04x %d %d %d\n", msg, (short)LOWORD(lp), (short)HIWORD(lp), (short)HIWORD(wp) );
        fflush( out );
        return 0;
    }
    if (msg == WM_DESTROY) PostQuitMessage( 0 );
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int wmain( int argc, WCHAR **argv )
{
    WNDCLASSW wc = { 0 };
    RECT wr, cr;
    POINT pt = { 0, 0 };
    HWND hwnd;
    MSG msg;

    if (argc < 2 || !(out = _wfopen( argv[1], L"w" ))) return 1;
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"hwheel";
    wc.hCursor = LoadCursorW( 0, (LPCWSTR)IDC_ARROW );
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    RegisterClassW( &wc );
    hwnd = CreateWindowW( L"hwheel", L"hwheel", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 200, 500, 300, 0, 0, 0, 0 );
    SetForegroundWindow( hwnd );
    UpdateWindow( hwnd );
    GetWindowRect( hwnd, &wr );
    GetClientRect( hwnd, &cr );
    ClientToScreen( hwnd, &pt );
    fprintf( out, "RECT %ld %ld %ld %ld %ld %ld %ld %ld\n", wr.left, wr.top, wr.right, wr.bottom,
             pt.x, pt.y, pt.x + cr.right, pt.y + cr.bottom );
    fflush( out );
    SetTimer( hwnd, 1, 60000, NULL );
    while (GetMessageW( &msg, 0, 0, 0 ))
    {
        if (msg.message == WM_TIMER) break;
        DispatchMessageW( &msg );
    }
    return 0;
}
