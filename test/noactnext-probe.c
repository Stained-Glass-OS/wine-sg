/* Which window becomes active when the active one is hidden or destroyed
 * (patches/sg/0862): never a window with WS_EX_NOACTIVATE, such as the
 * taskbar. The system does not bring such a window forward when the window
 * that was active goes away; Wine took the next one in Z order, and the
 * taskbar (topmost, right below a closing dropdown) took the keyboard.
 * Prints "name value" lines for test/noactnext-gate.sh. */
#include <windows.h>
#include <stdio.h>

static const char *who( HWND hwnd, HWND main, HWND bar )
{
    if (!hwnd) return "none";
    if (hwnd == main) return "main";
    if (hwnd == bar) return "bar";
    return "other";
}

static void pump( void )
{
    MSG msg;
    while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
}

int main( void )
{
    HWND main, bar, popup;

    main = CreateWindowExW( 0, L"static", L"main", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                            100, 100, 300, 200, 0, 0, 0, NULL );
    SetForegroundWindow( main );
    SetActiveWindow( main );
    pump();
    /* a topmost bar that never takes activation, like the taskbar */
    bar = CreateWindowExW( WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"static", L"bar",
                           WS_POPUP, 0, 560, 800, 40, 0, 0, 0, NULL );
    ShowWindow( bar, SW_SHOWNA );
    pump();
    printf( "start %s\n", who( GetActiveWindow(), main, bar ) );

    /* a topmost dropdown over both, made active (the focus goes into it) */
    popup = CreateWindowExW( WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"static", L"popup",
                             WS_POPUP, 120, 130, 200, 100, 0, 0, 0, NULL );
    ShowWindow( popup, SW_SHOWNOACTIVATE );
    SetFocus( popup );
    pump();
    printf( "popup-active %d\n", GetActiveWindow() == popup );

    ShowWindow( popup, SW_HIDE );
    pump();
    printf( "after-hide %s\n", who( GetActiveWindow(), main, bar ) );
    printf( "focus-after-hide %s\n", who( GetFocus(), main, bar ) );

    /* the same when it is destroyed */
    ShowWindow( popup, SW_SHOWNOACTIVATE );
    SetFocus( popup );
    pump();
    DestroyWindow( popup );
    pump();
    printf( "after-destroy %s\n", who( GetActiveWindow(), main, bar ) );

    /* a click-free activation request of the bar itself still works */
    SetActiveWindow( bar );
    pump();
    printf( "explicit %s\n", who( GetActiveWindow(), main, bar ) );

    DestroyWindow( bar );
    DestroyWindow( main );
    return 0;
}
