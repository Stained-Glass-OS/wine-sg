/* A window whose program has not painted it yet (patches/sg/0587): a window
 * at 100,100 (400x300) whose program paints only a 20x20 blue square in the
 * corner of its client area for 1.5 s (WM_ERASEBKGND does nothing), then all
 * of it red. */
#include <windows.h>

static BOOL paint_now;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    PAINTSTRUCT ps;
    switch (msg)
    {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
        BeginPaint( hwnd, &ps );
        if (paint_now) FillRect( ps.hdc, &ps.rcPaint, CreateSolidBrush( RGB( 255, 0, 0 ) ) );
        else
        {
            RECT corner = { 0, 0, 20, 20 };   /* painted only in part so far */
            FillRect( ps.hdc, &corner, CreateSolidBrush( RGB( 0, 0, 255 ) ) );
        }
        EndPaint( hwnd, &ps );
        return 0;
    case WM_TIMER:
        KillTimer( hwnd, 1 );
        paint_now = TRUE;
        InvalidateRect( hwnd, NULL, FALSE );
        return 0;
    case WM_DESTROY: PostQuitMessage( 0 ); return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int WINAPI WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASSW wc = { 0 };
    MSG msg;
    HWND hwnd;

    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"FirstPaint";
    RegisterClassW( &wc );
    hwnd = CreateWindowW( L"FirstPaint", L"first paint", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          100, 100, 400, 300, 0, 0, inst, 0 );
    SetTimer( hwnd, 1, 1500, NULL );
    SetTimer( hwnd, 2, 4000, NULL );
    while (GetMessageW( &msg, 0, 0, 0 ))
    {
        if (msg.message == WM_TIMER && msg.wParam == 2) break;
        DispatchMessageW( &msg );
    }
    return 0;
}
