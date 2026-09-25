/* vdesk-probe: drive and inspect virtual desktops (patches/sg/0068), for
 * test/vdesk-gate.sh.
 *
 *   vdesk-probe window TITLE X Y   a window of this program's, until killed
 *   vdesk-probe query              desktops=N current=I
 *   vdesk-probe new | switch I | close I | taskview
 *   vdesk-probe move TITLE I       move the window with that title to desktop I
 *   vdesk-probe state TITLE        visible= cloaked= desktop= (and the API's view)
 *   vdesk-probe hotkey KEYS        type Win+Ctrl(+Shift)+KEY with SendInput:
 *                                  new, left, right, close, moveleft, moveright, taskview
 *   vdesk-probe key VK             press and release one key (decimal VK)
 *   vdesk-probe api TITLE          IVirtualDesktopManager's view of the window, and
 *                                  MoveWindowToDesktop on it (another process's: refused)
 *   vdesk-probe taskbar            window buttons shown on the taskbar
 *   vdesk-probe exists CLASS       whether a window of that class exists
 *   vdesk-probe alttab             Alt down, Tab: is the switcher up? Alt up: which
 *                                  window is in front now?
 *   vdesk-probe foreground         the foreground window's title
 *   vdesk-probe rect TITLE         rect=l,t,r,b zoomed= iconic=
 *   vdesk-probe workarea           work=l,t,r,b
 *   vdesk-probe find CLASS [TITLE] whether such a window exists (and is visible)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <dwmapi.h>
#include <objbase.h>
#include <stdio.h>

/* IVirtualDesktopManager, declared from its published IIDs */
typedef struct { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 HRESULT (WINAPI *IsWindowOnCurrentVirtualDesktop)(void *, HWND, BOOL *);
                 HRESULT (WINAPI *GetWindowDesktopId)(void *, HWND, GUID *);
                 HRESULT (WINAPI *MoveWindowToDesktop)(void *, HWND, REFGUID); } VdmVtbl;
static const GUID CLSID_Vdm = {0xaa509086,0x5ca9,0x4c25,{0x8f,0x95,0x58,0x9d,0x3c,0x07,0xb4,0x8a}};
static const GUID IID_Vdm = {0xa5cd92ff,0x29be,0x454c,{0x8d,0x04,0xd8,0x28,0x79,0xfb,0x3f,0x1b}};

static int visible_buttons;
static BOOL CALLBACK count_proc( HWND hwnd, LPARAM lp )
{
    LONG_PTR id = GetWindowLongPtrW( hwnd, GWLP_ID );
    if (IsWindowVisible( hwnd ) && id && id != 0x5654) visible_buttons++;  /* not Start, not Task View */
    return TRUE;
}

static LRESULT send_command( WPARAM wp, LPARAM lp )
{
    HWND tray = FindWindowW( L"Shell_TrayWnd", NULL );
    UINT msg = RegisterWindowMessageW( L"SgVirtualDesktopCommand" );
    DWORD_PTR res = 0;
    if (!tray) { printf( "no taskbar\n" ); exit( 1 ); }
    SendMessageTimeoutW( tray, msg, wp, lp, SMTO_ABORTIFHUNG, 5000, &res );
    return res;
}

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        WCHAR title[64];
        RECT rc;
        HDC dc = BeginPaint( hwnd, &ps );
        GetClientRect( hwnd, &rc );
        GetWindowTextW( hwnd, title, 64 );
        DrawTextW( dc, title, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
        EndPaint( hwnd, &ps );
        return 0;
    }
    if (msg == WM_DESTROY) PostQuitMessage( 0 );
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void key( WORD vk, BOOL up )
{
    INPUT in = { INPUT_KEYBOARD };
    in.ki.wVk = vk;
    in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_LWIN) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput( 1, &in, sizeof(in) );
}

int main( int argc, char **argv )
{
    WCHAR wtitle[64];

    if (argc >= 5 && !strcmp( argv[1], "window" ))
    {
        WNDCLASSW wc = { 0 };
        MSG msg;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, wtitle, 64 );
        wc.lpfnWndProc = proc;
        wc.hbrBackground = GetSysColorBrush( COLOR_WINDOW );
        wc.hCursor = LoadCursorW( NULL, (const WCHAR *)IDC_ARROW );
        wc.hIcon = LoadIconW( NULL, (const WCHAR *)IDI_APPLICATION );
        wc.lpszClassName = L"SgVdeskProbe";
        RegisterClassW( &wc );
        CreateWindowW( wc.lpszClassName, wtitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                       atoi( argv[3] ), atoi( argv[4] ), 360, 260, NULL, NULL, NULL, NULL );
        while (GetMessageW( &msg, NULL, 0, 0 )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "query" ))
    {
        LRESULT r = send_command( 0, 0 );
        printf( "desktops=%d current=%d\n", (int)(r & 0xff), (int)((r >> 8) & 0xff) );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "new" )) { send_command( 2, 1 ); return 0; }
    if (argc == 2 && !strcmp( argv[1], "taskview" )) { send_command( 4, 0 ); return 0; }
    if (argc == 3 && !strcmp( argv[1], "switch" )) { send_command( 1, atoi( argv[2] )); return 0; }
    if (argc == 3 && !strcmp( argv[1], "close" )) { send_command( 3, atoi( argv[2] )); return 0; }
    if (argc == 4 && !strcmp( argv[1], "move" ))
    {
        HWND hwnd;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, wtitle, 64 );
        if (!(hwnd = FindWindowW( L"SgVdeskProbe", wtitle ))) { printf( "no window\n" ); return 1; }
        send_command( 0x100 + atoi( argv[3] ), (LPARAM)hwnd );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "state" ))
    {
        HWND hwnd;
        DWORD cloaked = 0;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, wtitle, 64 );
        if (!(hwnd = FindWindowW( L"SgVdeskProbe", wtitle ))) { printf( "no window\n" ); return 1; }
        DwmGetWindowAttribute( hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked) );
        printf( "visible=%d cloaked=%#lx desktop=%d\n", IsWindowVisible( hwnd ), cloaked,
                (int)(INT_PTR)GetPropW( hwnd, L"__sg_vdesk" ) - 1 );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "api" ))
    {
        HWND hwnd;
        void *vdm = NULL;
        BOOL on = -1;
        GUID id = {0};
        WCHAR str[40] = L"";
        HRESULT hr, hr2, hr3;

        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, wtitle, 64 );
        if (!(hwnd = FindWindowW( L"SgVdeskProbe", wtitle ))) { printf( "no window\n" ); return 1; }
        CoInitialize( NULL );
        hr = CoCreateInstance( &CLSID_Vdm, NULL, CLSCTX_INPROC_SERVER, &IID_Vdm, &vdm );
        if (FAILED(hr)) { printf( "create=%#lx\n", hr ); return 1; }
        hr = (*(VdmVtbl **)vdm)->IsWindowOnCurrentVirtualDesktop( vdm, hwnd, &on );
        hr2 = (*(VdmVtbl **)vdm)->GetWindowDesktopId( vdm, hwnd, &id );
        StringFromGUID2( &id, str, 40 );
        hr3 = (*(VdmVtbl **)vdm)->MoveWindowToDesktop( vdm, hwnd, &id );
        printf( "on_current=%d hr=%#lx id=%ls hr=%#lx move_other=%#lx\n", on, hr, str, hr2, hr3 );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "taskbar" ))
    {
        EnumChildWindows( FindWindowW( L"Shell_TrayWnd", NULL ), count_proc, 0 );
        printf( "buttons=%d\n", visible_buttons );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "exists" ))
    {
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, wtitle, 64 );
        printf( "exists=%d\n", FindWindowW( wtitle, NULL ) != NULL );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "rect" ))
    {
        HWND hwnd;
        RECT rc;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, wtitle, 64 );
        if (!(hwnd = FindWindowW( L"SgVdeskProbe", wtitle ))) { printf( "no window\n" ); return 1; }
        GetWindowRect( hwnd, &rc );
        printf( "rect=%ld,%ld,%ld,%ld zoomed=%d iconic=%d\n", rc.left, rc.top, rc.right, rc.bottom,
                IsZoomed( hwnd ), IsIconic( hwnd ) );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "workarea" ))
    {
        RECT rc;
        SystemParametersInfoW( SPI_GETWORKAREA, 0, &rc, 0 );
        printf( "work=%ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom );
        return 0;
    }
    if ((argc == 3 || argc == 4) && !strcmp( argv[1], "find" ))
    {
        WCHAR cls[64], title[64];
        HWND hwnd;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, cls, 64 );
        if (argc == 4) MultiByteToWideChar( CP_ACP, 0, argv[3], -1, title, 64 );
        hwnd = FindWindowW( cls, argc == 4 ? title : NULL );
        printf( "found=%d\n", hwnd && IsWindowVisible( hwnd ) );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "foreground" ))
    {
        WCHAR t[64] = L"";
        GetWindowTextW( GetForegroundWindow(), t, 64 );
        printf( "foreground=%ls\n", t );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "alttab" ))
    {
        WCHAR t[64] = L"";
        key( VK_MENU, FALSE );
        key( VK_TAB, FALSE ); key( VK_TAB, TRUE );
        Sleep( 700 );
        printf( "switcher=%d ", FindWindowW( L"SgTaskSwitcher", NULL ) != NULL );
        key( VK_MENU, TRUE );
        Sleep( 700 );
        GetWindowTextW( GetForegroundWindow(), t, 64 );
        printf( "after=%d foreground=%ls\n", FindWindowW( L"SgTaskSwitcher", NULL ) != NULL, t );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "key" ))
    {
        key( atoi( argv[2] ), FALSE ); key( atoi( argv[2] ), TRUE );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "hotkey" ))
    {
        const char *k = argv[2];
        WORD vk = !strcmp( k, "new" ) ? 'D' : !strcmp( k, "close" ) ? VK_F4 : !strcmp( k, "taskview" ) ? VK_TAB :
                  strstr( k, "left" ) ? VK_LEFT : VK_RIGHT;
        BOOL ctrl = strcmp( k, "taskview" ) != 0, shift = !strncmp( k, "move", 4 );
        key( VK_LWIN, FALSE );
        if (ctrl) key( VK_CONTROL, FALSE );
        if (shift) key( VK_SHIFT, FALSE );
        key( vk, FALSE ); key( vk, TRUE );
        if (shift) key( VK_SHIFT, TRUE );
        if (ctrl) key( VK_CONTROL, TRUE );
        key( VK_LWIN, TRUE );
        return 0;
    }
    printf( "usage: see the source\n" );
    return 2;
}
