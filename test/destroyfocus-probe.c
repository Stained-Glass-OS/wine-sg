/* destroyfocus-probe: where the keyboard focus goes when a child window that
 * holds the focus inside it (on one of its own children) is destroyed. */
#include <windows.h>
#include <stdio.h>

static HWND main_wnd, bg, dlg, button;

static const char *name( HWND h )
{
    if (!h) return "none";
    if (h == main_wnd) return "main";
    if (h == bg) return "bg";
    if (h == dlg) return "dlg";
    if (h == button) return "button";
    return "other";
}

/* AnyDesk's modal panel: hiding it hides its backdrop, destroying it
 * destroys the backdrop too (so the backdrop is already hidden then) */
static LRESULT CALLBACK dlg_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_SHOWWINDOW && !wp && IsWindow( bg )) ShowWindow( bg, SW_HIDE );
    if (msg == WM_DESTROY && IsWindow( bg )) DestroyWindow( bg );
    return DefWindowProcA( hwnd, msg, wp, lp );
}

int main(void)
{
    WNDCLASSA wc = {0};
    MSG msg;

    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA( NULL );
    wc.lpszClassName = "dfmain";
    RegisterClassA( &wc );
    wc.lpfnWndProc = dlg_proc;
    wc.lpszClassName = "dfdlg";
    RegisterClassA( &wc );

    main_wnd = CreateWindowExA( 0, "dfmain", "main", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                0, 0, 400, 300, NULL, NULL, NULL, NULL );
    SetForegroundWindow( main_wnd );
    SetActiveWindow( main_wnd );
    while (PeekMessageA( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageA( &msg );

    /* 1: a panel with a focused button in it is destroyed */
    bg = CreateWindowExA( 0, "dfmain", "panel", WS_CHILD | WS_VISIBLE, 0, 0, 200, 100,
                          main_wnd, NULL, NULL, NULL );
    button = CreateWindowExA( 0, "button", "Cancel", WS_CHILD | WS_VISIBLE, 10, 10, 80, 20,
                              bg, NULL, NULL, NULL );
    SetFocus( button );
    printf( "panel-start %s\n", name( GetFocus() ) );
    DestroyWindow( bg );
    printf( "panel-destroyed %s\n", name( GetFocus() ) );

    /* 2: AnyDesk's "Connecting" panel: backdrop -> dialog -> focused button;
     * the dialog is destroyed and takes its backdrop with it */
    bg = CreateWindowExA( 0, "dfmain", "bg", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 400, 300,
                          main_wnd, NULL, NULL, NULL );
    dlg = CreateWindowExA( WS_EX_COMPOSITED, "dfdlg", "Connecting", WS_CHILD | WS_CLIPSIBLINGS,
                           50, 50, 300, 150, bg, NULL, NULL, NULL );
    button = CreateWindowExA( 0, "button", "Cancel", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                              10, 10, 80, 20, dlg, NULL, NULL, NULL );
    SetFocus( button );
    ShowWindow( dlg, SW_SHOW );
    ShowWindow( bg, SW_SHOW );
    printf( "dialog-start %s\n", name( GetFocus() ) );
    DestroyWindow( dlg );
    printf( "dialog-destroyed %s\n", name( GetFocus() ) );
    printf( "active %s\n", name( GetActiveWindow() ) );

    /* 3: the focused child itself destroyed (Wine's own message test) */
    button = CreateWindowExA( 0, "button", "OK", WS_CHILD | WS_VISIBLE, 10, 10, 80, 20,
                              main_wnd, NULL, NULL, NULL );
    SetFocus( button );
    DestroyWindow( button );
    printf( "child-destroyed %s\n", name( GetFocus() ) );

    DestroyWindow( main_wnd );
    return 0;
}
