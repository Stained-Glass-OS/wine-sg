/* dpilive-gate.sh's probe (0890): a window of each kind of DPI awareness
 * while the display scale changes, and the hand that changes it.
 *   dpilive-probe.exe win pmv2|pmv1|unaware|system X Y RRGGBB LOG
 *       a top-level window at X,Y (in its own DPI's pixels) whose client
 *       area, 400x300 at 100%, is the colour RRGGBB; with a child window.
 *       It writes what it hears to LOG: WM_DPICHANGED (and moves to the
 *       suggested rectangle, as Windows asks), its child's
 *       WM_DPICHANGED_BEFOREPARENT and _AFTERPARENT, and twice a second
 *       its state (GetDpiForWindow, GetDpiForSystem, the monitor's DPI,
 *       the screen's LOGPIXELSX, its client size).
 *   dpilive-probe.exe layered X Y
 *       a DPI-unaware layered window with per-pixel alpha (UpdateLayeredWindow),
 *       200x100, its left half 0xc02020, its right half 0x2020c0
 *   dpilive-probe.exe rect CLASS [bottom]
 *       the size on the screen of the first window of CLASS: WxH (and
 *       that window put below the others)
 *   dpilive-probe.exe set DPI
 *       what Settings > Display > Scale does (sg-shell scale_write): LogPixels
 *       and WM_SETTINGCHANGE "WindowMetrics" to every window.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WM_DPICHANGED_BEFOREPARENT
#define WM_DPICHANGED_BEFOREPARENT 0x02e2
#define WM_DPICHANGED_AFTERPARENT 0x02e3
#endif

typedef HRESULT (WINAPI *GetDpiForMonitor_t)( HMONITOR, int, UINT *, UINT * );
static FILE *log_file;
static COLORREF colour;
static HBRUSH brush;
static HWND child;

static void say( const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    vfprintf( log_file, fmt, args );
    va_end( args );
    fputc( '\n', log_file );
    fflush( log_file );
}

static UINT monitor_dpi( HWND hwnd )
{
    static GetDpiForMonitor_t get;
    UINT x = 0, y = 0;
    if (!get) get = (GetDpiForMonitor_t)GetProcAddress( LoadLibraryA( "shcore.dll" ), "GetDpiForMonitor" );
    if (get) get( MonitorFromWindow( hwnd, MONITOR_DEFAULTTONEAREST ), 0, &x, &y );
    return x;
}

static LRESULT CALLBACK child_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_DPICHANGED_BEFOREPARENT) say( "child before" );
    if (msg == WM_DPICHANGED_AFTERPARENT) say( "child after" );
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static LRESULT CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_DPICHANGED:
    {
        RECT *r = (RECT *)lp;
        say( "dpichanged %u %ld %ld %ld %ld", LOWORD(wp), r->left, r->top, r->right - r->left, r->bottom - r->top );
        SetWindowPos( hwnd, 0, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE );
        return 0;
    }
    case WM_TIMER:
    {
        RECT rc;
        HDC hdc = GetDC( 0 );
        GetClientRect( hwnd, &rc );
        say( "state win=%u sys=%u mon=%u caps=%d client=%ldx%ld v2=%d", GetDpiForWindow( hwnd ), GetDpiForSystem(),
             monitor_dpi( hwnd ), GetDeviceCaps( hdc, LOGPIXELSX ), rc.right, rc.bottom,
             AreDpiAwarenessContextsEqual( GetWindowDpiAwarenessContext( hwnd ), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ) );
        ReleaseDC( 0, hdc );
        return 0;
    }
    case WM_ERASEBKGND:
    {
        RECT rc;
        GetClientRect( hwnd, &rc );
        FillRect( (HDC)wp, &rc, brush );
        return 1;
    }
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static int run_window( const char *mode, int x, int y, const char *rgb, const char *path )
{
    WNDCLASSW wc = {0};
    RECT rc = {0, 0, 400, 300};
    DPI_AWARENESS_CONTEXT ctx = DPI_AWARENESS_CONTEXT_UNAWARE;
    unsigned int c = strtoul( rgb, NULL, 16 );
    WCHAR title[64];
    HWND hwnd;
    MSG msg;
    UINT dpi;

    if (!(log_file = fopen( path, "wb" ))) return 1;
    if (!strcmp( mode, "pmv2" )) ctx = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
    else if (!strcmp( mode, "pmv1" )) ctx = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE;
    else if (!strcmp( mode, "system" )) ctx = DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
    if (!SetProcessDpiAwarenessContext( ctx )) say( "awareness not set" );
    colour = RGB( (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff );
    brush = CreateSolidBrush( colour );

    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.lpszClassName = L"SGDpiProbe";
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    RegisterClassW( &wc );
    wc.lpfnWndProc = child_proc;
    wc.lpszClassName = L"SGDpiProbeChild";
    RegisterClassW( &wc );

    /* 400x300 at 100%: an aware window sizes itself for the DPI it starts at
     * (an unaware one is told 96) */
    dpi = GetDpiForSystem();
    rc.right = MulDiv( 400, dpi, 96 );
    rc.bottom = MulDiv( 300, dpi, 96 );
    AdjustWindowRectExForDpi( &rc, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi );
    swprintf( title, 64, L"dpi probe %hs", mode );
    hwnd = CreateWindowExW( 0, L"SGDpiProbe", title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y,
                            rc.right - rc.left, rc.bottom - rc.top, 0, 0, wc.hInstance, NULL );
    child = CreateWindowExW( 0, L"SGDpiProbeChild", NULL, WS_CHILD, 0, 0, 1, 1, hwnd, 0, wc.hInstance, NULL );
    ShowWindow( hwnd, SW_SHOWNOACTIVATE );
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

static int run_layered( int x, int y )
{
    WNDCLASSW wc = {0};
    BITMAPINFO bi = {{sizeof(bi.bmiHeader), 200, -100, 1, 32, BI_RGB}};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    POINT pos = {x, y}, zero = {0, 0};
    SIZE size = {200, 100};
    HDC screen = GetDC( 0 ), mem = CreateCompatibleDC( screen );
    UINT *bits;
    HBITMAP bmp = CreateDIBSection( mem, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );
    HWND hwnd;
    MSG msg;
    int i;

    for (i = 0; i < 200 * 100; i++) bits[i] = (i % 200) < 100 ? 0xffc02020 : 0xff2020c0;
    SelectObject( mem, bmp );
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.lpszClassName = L"SGDpiLayered";
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"SGDpiLayered", L"layered", WS_POPUP,
                            x, y, 200, 100, 0, 0, wc.hInstance, NULL );
    UpdateLayeredWindow( hwnd, screen, &pos, &size, mem, &zero, 0, &blend, ULW_ALPHA );
    ShowWindow( hwnd, SW_SHOWNOACTIVATE );
    while (GetMessageW( &msg, 0, 0, 0 ) > 0) DispatchMessageW( &msg );
    return 0;
}

int main( int argc, char **argv )
{
    if (argc >= 4 && !strcmp( argv[1], "layered" )) return run_layered( atoi( argv[2] ), atoi( argv[3] ) );
    if (argc >= 7 && !strcmp( argv[1], "win" ))
        return run_window( argv[2], atoi( argv[3] ), atoi( argv[4] ), argv[5], argv[6] );
    if (argc >= 3 && !strcmp( argv[1], "rect" ))
    {
        WCHAR cls[128];
        HWND hwnd;
        RECT rc;
        SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, cls, 128 );
        if (!(hwnd = FindWindowW( cls, NULL )) || !GetWindowRect( hwnd, &rc )) { printf( "none\n" ); return 1; }
        printf( "%ldx%ld\n", rc.right - rc.left, rc.bottom - rc.top );
        if (argc >= 4) SetWindowPos( hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
        return 0;
    }
    if (argc >= 3 && !strcmp( argv[1], "set" ))
    {
        DWORD dpi = atoi( argv[2] );
        DWORD_PTR r;
        HKEY key;
        if (RegCreateKeyExW( HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL ))
            return 1;
        RegSetValueExW( key, L"LogPixels", 0, REG_DWORD, (BYTE *)&dpi, sizeof(dpi) );
        RegCloseKey( key );
        SendMessageTimeoutW( HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"WindowMetrics", SMTO_ABORTIFHUNG, 2000, &r );
        return 0;
    }
    return 2;
}
