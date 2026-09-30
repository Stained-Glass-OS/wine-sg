/* The taskbar above the windows (patches/sg/0588). "window": a red window
 * over the bottom of the screen, where the taskbar is; "full": a green window
 * filling the screen, in the foreground. Each stays up for 6 s. "flip"
 * (0604): the red window and a blue one elsewhere take the foreground in
 * turn, 40 times, as switching between them on the taskbar does. */
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
    if (strstr( cmd, "flip" ))
    {
        WNDCLASSW bc = wc;
        HWND other;
        int i;
        bc.hbrBackground = CreateSolidBrush( RGB( 0, 0, 255 ) );
        bc.lpszClassName = L"TopBarOther";
        RegisterClassW( &bc );
        other = CreateWindowW( L"TopBarOther", L"other", WS_POPUP | WS_VISIBLE, 700, 50, 200, 200, 0, 0, inst, 0 );
        for (i = 0; i < 40; i++)
        {
            SetForegroundWindow( i % 2 ? other : hwnd );
            start = GetTickCount();
            while (GetTickCount() - start < 60)
            {
                while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
                Sleep( 5 );
            }
        }
        return 0;
    }
    while (GetTickCount() - start < 6000)
    {
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 20 );
    }
    return 0;
}
