/* The taskbar above the windows (patches/sg/0588). "window": a red window
 * over the bottom of the screen, where the taskbar is; "full": a green window
 * filling the screen, in the foreground. Each stays up for 6 s. */
#include <windows.h>

int WINAPI WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASSW wc = { 0 };
    BOOL full = strstr( cmd, "full" ) != NULL;
    DWORD start = GetTickCount();
    MSG msg;
    HWND hwnd;

    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = inst;
    wc.hbrBackground = CreateSolidBrush( full ? RGB( 0, 255, 0 ) : RGB( 255, 0, 0 ) );
    wc.lpszClassName = L"TopBar";
    RegisterClassW( &wc );
    if (full)
        hwnd = CreateWindowW( L"TopBar", L"full", WS_POPUP | WS_VISIBLE, 0, 0,
                              GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ), 0, 0, inst, 0 );
    else
        hwnd = CreateWindowW( L"TopBar", L"window", WS_POPUP | WS_VISIBLE, 100, 400,
                              600, GetSystemMetrics( SM_CYSCREEN ) - 400, 0, 0, inst, 0 );
    SetForegroundWindow( hwnd );
    while (GetTickCount() - start < 6000)
    {
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 20 );
    }
    return 0;
}
