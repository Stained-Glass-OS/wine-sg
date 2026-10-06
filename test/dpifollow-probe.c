/* dpifollow-gate.sh's probe (1120-1123): what a program sees and shows
 * after the display scale changes while it runs.
 *   dpifollow-probe.exe win pmv2|pmv1|system|unaware X Y W H LOG [max|maxtop]
 *       a top-level window with a menu bar (File, Edit), W x H at 100% (an
 *       aware one sizes itself for the DPI it starts at), maximized with
 *       "max", and topmost too with "maxtop" (above the taskbar: it hears
 *       of a settings change before the taskbar does). Twice a second it writes to LOG its state:
 *         state sys=GetDpiForSystem caps=LOGPIXELSY win=GetDpiForWindow
 *               icon=SM_CXICON menu=SM_CYMENU item=<menu bar item height>
 *       On WM_DPICHANGED it moves to the suggested rectangle, as asked;
 *       WM_DISPLAYCHANGE: "displaychange WxH".
 *       "popup" (below) opens its File menu as a popup menu: it writes
 *         popup WxH        (the menu's window, in its own pixels)
 *   dpifollow-probe.exe popup TITLE
 *       the window whose title is TITLE opens its popup menu
 *   dpifollow-probe.exe app CLASS CHILDCLASS
 *       the first window of CLASS: "app size=WxH pmv2=0|1 child=WxH" (its
 *       first child of CHILDCLASS), in the screen's pixels
 *   dpifollow-probe.exe fe
 *       File Explorer: "fe size=WxH pmv2=0|1 tree=<item height> list=<small
 *       icon spacing, the folder's list>", in the screen's pixels
 *   dpifollow-probe.exe rect TITLE
 *       per-monitor aware: "rect=L,T,R,B work=L,T,R,B bar=L,T,R,B max=0|1"
 *       for the window of that title, its monitor's work area, the taskbar
 *   dpifollow-probe.exe set DPI
 *       what Settings > Display > Scale does: LogPixels and WM_SETTINGCHANGE
 *       "WindowMetrics" to every window.
 *   dpifollow-probe.exe tell TITLE DPI
 *       the same to the window of that title only: it hears of the new scale
 *       before the taskbar does (as one above it in the broadcast's order)
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commctrl.h>
#include <shellapi.h>

#define WM_PROBE_POPUP (WM_APP + 1)
#define POPUP_TIMER 7

static FILE *log_file;
static HMENU file_menu;

static void say( const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    vfprintf( log_file, fmt, args );
    va_end( args );
    fputc( '\n', log_file );
    fflush( log_file );
}

static LRESULT CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_DPICHANGED:
    {
        RECT *r = (RECT *)lp;
        say( "dpichanged %u", LOWORD(wp) );
        SetWindowPos( hwnd, 0, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE );
        return 0;
    }
    case WM_DISPLAYCHANGE:
        say( "displaychange %ux%u", LOWORD(lp), HIWORD(lp) );
        return 0;
    case WM_PROBE_POPUP:
    {
        POINT pt = {20, 20};
        ClientToScreen( hwnd, &pt );
        SetTimer( hwnd, POPUP_TIMER, 700, NULL );
        SetForegroundWindow( hwnd );
        TrackPopupMenu( file_menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL );
        return 0;
    }
    case WM_TIMER:
        if (wp == POPUP_TIMER)
        {
            HWND menu = FindWindowW( L"#32768", NULL );
            RECT r = {0};
            KillTimer( hwnd, POPUP_TIMER );
            if (menu) GetWindowRect( menu, &r );
            say( "popup %ldx%ld", r.right - r.left, r.bottom - r.top );
            EndMenu();
        }
        else
        {
            HDC hdc = GetDC( 0 );
            RECT item = {0};
            GetMenuItemRect( hwnd, GetMenu( hwnd ), 0, &item );
            say( "state sys=%u caps=%d win=%u icon=%d menu=%d item=%ld", GetDpiForSystem(), GetDeviceCaps( hdc, LOGPIXELSY ),
                 GetDpiForWindow( hwnd ), GetSystemMetrics( SM_CXICON ), GetSystemMetrics( SM_CYMENU ),
                 item.bottom - item.top );
            ReleaseDC( 0, hdc );
        }
        return 0;
    case WM_ERASEBKGND:
    {
        RECT rc;
        GetClientRect( hwnd, &rc );
        FillRect( (HDC)wp, &rc, GetStockObject( WHITE_BRUSH ) );
        return 1;
    }
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static int run_window( const char *mode, int x, int y, int cw, int ch, const char *path, BOOL max, BOOL top )
{
    WNDCLASSW wc = {0};
    RECT rc = {0, 0, 0, 0};
    DPI_AWARENESS_CONTEXT ctx = DPI_AWARENESS_CONTEXT_UNAWARE;
    HMENU bar, edit;
    WCHAR title[64];
    HWND hwnd;
    MSG msg;
    UINT dpi;

    if (!(log_file = fopen( path, "wb" ))) return 1;
    if (!strcmp( mode, "pmv2" )) ctx = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
    else if (!strcmp( mode, "pmv1" )) ctx = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE;
    else if (!strcmp( mode, "system" )) ctx = DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
    if (!SetProcessDpiAwarenessContext( ctx )) say( "awareness not set" );

    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.lpszClassName = L"SGDpiFollow";
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    RegisterClassW( &wc );

    bar = CreateMenu();
    file_menu = CreatePopupMenu();
    edit = CreatePopupMenu();
    AppendMenuW( file_menu, MF_STRING, 100, L"&New window" );
    AppendMenuW( file_menu, MF_STRING, 101, L"&Open..." );
    AppendMenuW( file_menu, MF_STRING, 102, L"Save &as..." );
    AppendMenuW( file_menu, MF_SEPARATOR, 0, NULL );
    AppendMenuW( file_menu, MF_STRING, 103, L"E&xit" );
    AppendMenuW( edit, MF_STRING, 200, L"&Copy" );
    AppendMenuW( bar, MF_POPUP, (UINT_PTR)file_menu, L"&File" );
    AppendMenuW( bar, MF_POPUP, (UINT_PTR)edit, L"&Edit" );

    dpi = GetDpiForSystem();
    rc.right = MulDiv( cw, dpi, 96 );
    rc.bottom = MulDiv( ch, dpi, 96 );
    AdjustWindowRectExForDpi( &rc, WS_OVERLAPPEDWINDOW, TRUE, 0, dpi );
    swprintf( title, 64, L"dpifollow %hs%hs", mode, top ? " maxtop" : max ? " max" : "" );
    hwnd = CreateWindowExW( top ? WS_EX_TOPMOST : 0, L"SGDpiFollow", title, WS_OVERLAPPEDWINDOW, x, y,
                            rc.right - rc.left, rc.bottom - rc.top, 0, bar, wc.hInstance, NULL );
    ShowWindow( hwnd, max ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE );
    UpdateWindow( hwnd );
    SetTimer( hwnd, 1, 500, NULL );
    say( "up %s", mode );
    while (GetMessageW( &msg, 0, 0, 0 ) > 0)
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    return 0;
}

static HWND find_title( const char *title )
{
    WCHAR t[128];
    MultiByteToWideChar( CP_ACP, 0, title, -1, t, 128 );
    return FindWindowW( L"SGDpiFollow", t );
}

int main( int argc, char **argv )
{
    if (argc >= 8 && !strcmp( argv[1], "win" ))
        return run_window( argv[2], atoi( argv[3] ), atoi( argv[4] ), atoi( argv[5] ), atoi( argv[6] ), argv[7],
                           argc >= 9 && !strncmp( argv[8], "max", 3 ), argc >= 9 && !strcmp( argv[8], "maxtop" ) );
    if (argc >= 3 && !strcmp( argv[1], "popup" ))
    {
        HWND hwnd = find_title( argv[2] );
        if (!hwnd) { printf( "none\n" ); return 1; }
        PostMessageW( hwnd, WM_PROBE_POPUP, 0, 0 );
        return 0;
    }
    if (argc >= 4 && !strcmp( argv[1], "app" ))
    {
        WCHAR cls[128], child_cls[128];
        HWND w, child;
        RECT rc = {0}, cr = {0};
        SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, cls, 128 );
        MultiByteToWideChar( CP_ACP, 0, argv[3], -1, child_cls, 128 );
        if (!(w = FindWindowW( cls, NULL ))) { printf( "none\n" ); return 1; }
        GetWindowRect( w, &rc );
        if ((child = FindWindowExW( w, NULL, child_cls, NULL ))) GetWindowRect( child, &cr );
        printf( "app size=%ldx%ld pmv2=%d child=%ldx%ld\n", rc.right - rc.left, rc.bottom - rc.top,
                AreDpiAwarenessContextsEqual( GetWindowDpiAwarenessContext( w ), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ),
                cr.right - cr.left, cr.bottom - cr.top );
        return 0;
    }
    if (argc >= 2 && !strcmp( argv[1], "fe" ))
    {
        HWND fe, tree, list;
        RECT rc = {0};
        DWORD sp = 0;
        SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
        if (!(fe = FindWindowW( L"ExplorerWClass", NULL ))) { printf( "none\n" ); return 1; }
        GetWindowRect( fe, &rc );
        tree = FindWindowExW( fe, NULL, L"SysTreeView32", NULL );
        list = FindWindowExW( fe, NULL, L"ExplorerBrowserControl", NULL );
        if (list) list = FindWindowExW( list, NULL, L"SHELLDLL_DefView", NULL );
        if (list) list = FindWindowExW( list, NULL, L"SysListView32", NULL );
        if (list) sp = SendMessageW( list, LVM_GETITEMSPACING, TRUE, 0 );
        printf( "fe size=%ldx%ld pmv2=%d tree=%ld list=%d,%d\n", rc.right - rc.left, rc.bottom - rc.top,
                AreDpiAwarenessContextsEqual( GetWindowDpiAwarenessContext( fe ), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ),
                tree ? (long)SendMessageW( tree, TVM_GETITEMHEIGHT, 0, 0 ) : -1L, LOWORD(sp), HIWORD(sp) );
        return 0;
    }
    if (argc >= 3 && !strcmp( argv[1], "rect" ))
    {
        MONITORINFO mi = {sizeof(mi)};
        HWND hwnd, bar;
        RECT rc = {0}, br = {0};
        SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
        if (!(hwnd = find_title( argv[2] )) || !GetWindowRect( hwnd, &rc )) { printf( "none\n" ); return 1; }
        GetMonitorInfoW( MonitorFromWindow( hwnd, MONITOR_DEFAULTTONEAREST ), &mi );
        if ((bar = FindWindowW( L"Shell_TrayWnd", NULL ))) GetWindowRect( bar, &br );
        printf( "rect=%ld,%ld,%ld,%ld work=%ld,%ld,%ld,%ld bar=%ld,%ld,%ld,%ld max=%d\n", rc.left, rc.top, rc.right, rc.bottom,
                mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom, br.left, br.top, br.right, br.bottom,
                IsZoomed( hwnd ) ? 1 : 0 );
        return 0;
    }
    if ((argc >= 3 && !strcmp( argv[1], "set" )) || (argc >= 4 && !strcmp( argv[1], "tell" )))
    {
        BOOL tell = !strcmp( argv[1], "tell" );
        DWORD dpi = atoi( argv[tell ? 3 : 2] );
        HWND to = HWND_BROADCAST;
        DWORD_PTR r;
        HKEY key;
        if (tell && !(to = find_title( argv[2] ))) { printf( "none\n" ); return 1; }
        if (RegCreateKeyExW( HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL ))
            return 1;
        RegSetValueExW( key, L"LogPixels", 0, REG_DWORD, (BYTE *)&dpi, sizeof(dpi) );
        RegCloseKey( key );
        SendMessageTimeoutW( to, WM_SETTINGCHANGE, 0, (LPARAM)L"WindowMetrics", SMTO_ABORTIFHUNG, 2000, &r );
        return 0;
    }
    if (argc >= 2 && !strcmp( argv[1], "busy" ))
    {
        /* a window whose thread does not answer for a while (a program at work) */
        MSG msg;
        CreateWindowExW( WS_EX_TOOLWINDOW, L"STATIC", L"dpifollow busy", WS_POPUP | WS_VISIBLE, 0, 0, 50, 50,
                         NULL, NULL, NULL, NULL );
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 120000 );
        return 0;
    }
    if (argc >= 4 && !strcmp( argv[1], "appbar" ))
    {
        /* dock a left appbar WIDTH pixels wide as a touch keyboard does: how
         * long the dock took ("appbar ms=N"); kept for SECONDS, then removed */
        APPBARDATA abd = {sizeof(abd)};
        LARGE_INTEGER f, a, b;
        DWORD end;
        MSG msg;
        SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
        abd.hWnd = CreateWindowExW( WS_EX_TOOLWINDOW, L"STATIC", L"dpifollow appbar", WS_POPUP | WS_VISIBLE, 0, 0,
                                    atoi( argv[2] ), 200, NULL, NULL, NULL, NULL );
        abd.uEdge = ABE_LEFT;
        SHAppBarMessage( ABM_NEW, &abd );
        SetRect( &abd.rc, 0, 0, atoi( argv[2] ), GetSystemMetrics( SM_CYSCREEN ) );
        QueryPerformanceFrequency( &f );
        QueryPerformanceCounter( &a );
        SHAppBarMessage( ABM_QUERYPOS, &abd );
        SHAppBarMessage( ABM_SETPOS, &abd );
        QueryPerformanceCounter( &b );
        printf( "appbar ms=%ld\n", (long)((b.QuadPart - a.QuadPart) * 1000 / f.QuadPart) );
        fflush( stdout );
        end = GetTickCount() + atoi( argv[3] ) * 1000;
        while ((LONG)(end - GetTickCount()) > 0)
        {
            MsgWaitForMultipleObjects( 0, NULL, FALSE, 100, QS_ALLINPUT );
            while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        }
        SHAppBarMessage( ABM_REMOVE, &abd );
        return 0;
    }
    return 2;
}
