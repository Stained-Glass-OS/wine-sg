/* Touches as a program sees them (test/touch-gate.sh, wine-sg 1150).
 *
 *   touch-probe.exe aware     a window that takes WM_POINTER* itself (as
 *                             Chromium, Qt do; aware-small: a small one)
 *                             Chromium, Qt do): prints each pointer message
 *                             with GetPointerType, GetPointerTouchInfo and the
 *                             frame (GetPointerFrameTouchInfo), the mouse
 *                             messages it still gets, and the shell's
 *                             hotkeys Win+Tab and Win+A (an edge swipe)
 *   touch-probe.exe unaware   a window leaving pointer messages to
 *                             DefWindowProc (most programs): a button, an
 *                             edit control with text, a title bar; prints
 *                             clicks, the edit's selection, the window's
 *                             position, wheel messages and WM_GESTURE
 *   touch-probe.exe touchwin  a window registered with RegisterTouchWindow:
 *                             prints WM_TOUCH (GetTouchInputInfo) and any
 *                             WM_GESTURE
 *
 * Lines: "metrics digitizer=D maxtouches=N devices=C touchdev=T",
 * "rect NAME L T R B" (screen), then events, then "ready". Runs until killed.
 */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>

static FILE *out;
static HWND main_hwnd, button, edit;
static const char *mode;

static void say( const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    vfprintf( out, fmt, args );
    va_end( args );
    fputc( '\n', out );
    fflush( out );
}

static const char *pointer_name( UINT msg )
{
    switch (msg)
    {
    case WM_POINTERENTER: return "enter";
    case WM_POINTERLEAVE: return "leave";
    case WM_POINTERDOWN: return "down";
    case WM_POINTERUP: return "up";
    case WM_POINTERUPDATE: return "update";
    }
    return "?";
}

static void print_rect( const char *name, HWND hwnd, BOOL client )
{
    RECT rc;
    if (client)
    {
        GetClientRect( hwnd, &rc );
        MapWindowPoints( hwnd, NULL, (POINT *)&rc, 2 );
    }
    else GetWindowRect( hwnd, &rc );
    say( "rect %s %ld %ld %ld %ld", name, rc.left, rc.top, rc.right, rc.bottom );
}

static void log_mouse( const char *what )
{
    say( "mouse %s extra=%#lx", what, (unsigned long)GetMessageExtraInfo() );
}

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    static int moves_logged;

    switch (msg)
    {
    case WM_POINTERENTER:
    case WM_POINTERLEAVE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERUPDATE:
    {
        UINT32 id = GET_POINTERID_WPARAM( wp ), count = 10;
        POINTER_INPUT_TYPE type = 0;
        POINTER_TOUCH_INFO touch, frame[10];
        BOOL ok, frame_ok;

        GetPointerType( id, &type );
        memset( &touch, 0, sizeof(touch) );
        ok = GetPointerTouchInfo( id, &touch );
        frame_ok = GetPointerFrameTouchInfo( id, &count, frame );
        say( "pointer %s id=%u type=%d touchok=%d frame=%u frameok=%d flags=%#x primary=%d x=%ld y=%ld hwnd=%s stamp=%d",
                 pointer_name( msg ), id, (int)type, ok, frame_ok ? count : 0, frame_ok,
                 (unsigned)touch.pointerInfo.pointerFlags, IS_POINTER_PRIMARY_WPARAM( wp ) ? 1 : 0,
                 touch.pointerInfo.ptPixelLocation.x, touch.pointerInfo.ptPixelLocation.y,
                 hwnd == main_hwnd ? "main" : "other",
                 GetPropW( GetDesktopWindow(), L"__wine_sg_touch_time" ) != NULL );
        if (!strncmp( mode, "aware", 5 )) return 0;  /* taken: no mouse of it */
        break;
    }
    case WM_GESTURE:
    {
        GESTUREINFO gi = { sizeof(gi) };
        BOOL ok = GetGestureInfo( (HGESTUREINFO)lp, &gi );
        say( "gesture id=%u ok=%d flags=%#lx args=%lu", (unsigned)wp, ok, gi.dwFlags, (unsigned long)gi.ullArguments );
        break;  /* DefWindowProc: the wheel */
    }
    case WM_TOUCH:
    {
        TOUCHINPUT ti[10];
        UINT n = min( LOWORD( wp ), 10 ), i, down = 0, up = 0;
        if (GetTouchInputInfo( (HTOUCHINPUT)lp, n, ti, sizeof(TOUCHINPUT) ))
        {
            for (i = 0; i < n; i++)
            {
                if (ti[i].dwFlags & TOUCHEVENTF_DOWN) down++;
                if (ti[i].dwFlags & TOUCHEVENTF_UP) up++;
            }
            say( "touch count=%u down=%u up=%u x=%ld", n, down, up, ti[0].x / 100 );
        }
        else say( "touch count=%u getinfo-failed=%lu", n, GetLastError() );
        CloseTouchInputHandle( (HTOUCHINPUT)lp );
        return 0;
    }
    case WM_LBUTTONDOWN: log_mouse( "ldown" ); moves_logged = 0; break;
    case WM_LBUTTONUP: log_mouse( "lup" ); break;
    case WM_MOUSEMOVE:
        if (moves_logged++ < 2) log_mouse( "move" );
        break;
    case WM_MOUSEWHEEL:
        say( "%swheel delta=%d", (GET_KEYSTATE_WPARAM( wp ) & MK_CONTROL) ? "ctrl-" : "", GET_WHEEL_DELTA_WPARAM( wp ) );
        return 0;
    case WM_MOUSEHWHEEL:
        say( "hwheel delta=%d", GET_WHEEL_DELTA_WPARAM( wp ) );
        return 0;
    case WM_COMMAND:
        if ((HWND)lp == button && HIWORD( wp ) == BN_CLICKED) say( "clicked button" );
        return 0;
    case WM_ACTIVATE:
        if (LOWORD( wp ) != WA_INACTIVE) say( "activated" );
        break;
    case WM_HOTKEY:
        say( "hotkey %s", wp == 1 ? "taskview" : wp == 2 ? "notify" : "?" );
        return 0;
    case WM_TIMER:
    {
        static DWORD last_sel = ~0u;
        static RECT last_rc;
        DWORD sel;
        RECT rc;

        if (edit && (sel = (DWORD)SendMessageW( edit, EM_GETSEL, 0, 0 )) != last_sel)
        {
            last_sel = sel;
            say( "sel %u %u", LOWORD( sel ), HIWORD( sel ) );
        }
        GetWindowRect( hwnd, &rc );
        if (memcmp( &rc, &last_rc, sizeof(rc) ))
        {
            last_rc = rc;
            say( "pos %ld %ld", rc.left, rc.top );
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int main( int argc, char **argv )
{
    WNDCLASSW wc = {0};
    POINTER_DEVICE_INFO devs[8];
    UINT32 n = 8, i, touchdev = 0;
    int w = GetSystemMetrics( SM_CXSCREEN ), h = GetSystemMetrics( SM_CYSCREEN );
    MSG msg;

    out = stdout;
    mode = argc > 1 ? argv[1] : "aware";
    if (!GetPointerDevices( &n, devs )) n = 0;
    for (i = 0; i < n; i++) if (devs[i].pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH) touchdev++;
    say( "metrics digitizer=%#x maxtouches=%d devices=%u touchdev=%u", GetSystemMetrics( SM_DIGITIZER ),
         GetSystemMetrics( SM_MAXIMUMTOUCHES ), n, touchdev );

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.lpszClassName = L"touch-probe";
    RegisterClassW( &wc );

    if (!strcmp( mode, "unaware" ))
    {
        main_hwnd = CreateWindowExW( 0, wc.lpszClassName, L"touch-probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                     w / 10, h / 8, w * 6 / 10, h * 6 / 10, NULL, NULL, wc.hInstance, NULL );
        button = CreateWindowW( L"BUTTON", L"Press", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 10,
                                w * 25 / 100, h * 25 / 100, main_hwnd, (HMENU)1, wc.hInstance, NULL );
        edit = CreateWindowW( L"EDIT", L"The quick brown fox jumps over the lazy dog",
                              WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, w * 25 / 100 + 30, 10, w * 3 / 10,
                              30, main_hwnd, (HMENU)2, wc.hInstance, NULL );
        SetTimer( main_hwnd, 1, 100, NULL );
        print_rect( "button", button, FALSE );
        print_rect( "edit", edit, FALSE );
        print_rect( "window", main_hwnd, FALSE );
        print_rect( "client", main_hwnd, TRUE );
    }
    else
    {
        /* from edge to edge: a touch at an edge is on a window too (the
         * compositor gives a touch to the surface under it); not the whole
         * screen, which Wine would clip the cursor to -- its pointer grab
         * makes touches the X server's mouse */
        if (!strcmp( mode, "aware-small" ))  /* beside the "unaware" window, not over it */
            main_hwnd = CreateWindowExW( 0, wc.lpszClassName, L"touch-probe", WS_POPUP | WS_VISIBLE,
                                         w * 72 / 100, h / 8, w / 4, h / 3, NULL, NULL, wc.hInstance, NULL );
        else
            main_hwnd = CreateWindowExW( WS_EX_TOPMOST, wc.lpszClassName, L"touch-probe", WS_POPUP | WS_VISIBLE,
                                         0, h / 8, w, h * 3 / 4, NULL, NULL, wc.hInstance, NULL );
        print_rect( "window", main_hwnd, FALSE );
        if (!strcmp( mode, "touchwin" ))
        {
            BOOL registered = RegisterTouchWindow( main_hwnd, 0 );
            say( "registered=%d istouch=%d", registered, IsTouchWindow( main_hwnd, NULL ) );
        }
        say( "hotkeys %d %d", RegisterHotKey( main_hwnd, 1, MOD_WIN, VK_TAB ),
             RegisterHotKey( main_hwnd, 2, MOD_WIN, 'A' ) );
    }
    SetForegroundWindow( main_hwnd );
    say( "ready" );

    while (GetMessageW( &msg, NULL, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    return 0;
}
