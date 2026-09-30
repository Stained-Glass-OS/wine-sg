/* notifyiconrect-probe: Shell_NotifyIconGetRect (patches/sg/0522), where a
 * notification-area icon is on the screen -- Microsoft OneDrive places its
 * flyout by it, and the stub's E_NOTIMPL left a click on its icon doing
 * nothing. Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static const GUID icon_guid = { 0x5a1d7e3c, 0x2b4f, 0x4c6d, { 0x9e, 0x81, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 } };

static RECT tray_icons[16];
static int n_tray_icons;
static BOOL CALLBACK find_icons( HWND hwnd, LPARAM lp )
{
    WCHAR cls[64];
    GetClassNameW( hwnd, cls, 64 );
    if (!lstrcmpW( cls, L"__wine_tray_icon" ) && IsWindowVisible( hwnd ) && n_tray_icons < 16)
        GetWindowRect( hwnd, &tray_icons[n_tray_icons++] );
    return TRUE;
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 20 );
    }
}

static BOOL is_tray_icon_rect( const RECT *r )
{
    int i;
    for (i = 0; i < n_tray_icons; i++) if (EqualRect( r, &tray_icons[i] )) return TRUE;
    return FALSE;
}

int main( void )
{
    WNDCLASSW wc = { 0, DefWindowProcW, 0, 0, GetModuleHandleW( NULL ), 0, 0, 0, 0, L"sg_nir_probe" };
    NOTIFYICONDATAW nid = { sizeof(nid) }, nid2 = { sizeof(nid2) };
    NOTIFYICONIDENTIFIER ident = { sizeof(ident) };
    RECT r1 = { 0 }, r2 = { 0 }, tray = { 0 };
    HRESULT hr;
    HWND hwnd;

    RegisterClassW( &wc );
    hwnd = CreateWindowW( L"sg_nir_probe", L"probe", 0, 0, 0, 10, 10, 0, 0, 0, 0 );
    nid.hWnd = hwnd;
    nid.uID = 7;
    nid.uFlags = NIF_ICON | NIF_TIP;
    nid.hIcon = LoadIconW( NULL, (LPCWSTR)IDI_APPLICATION );
    lstrcpyW( nid.szTip, L"probe one" );
    nid2 = nid;
    nid2.uID = 8;
    nid2.uFlags |= NIF_GUID;
    nid2.guidItem = icon_guid;
    printf( "add=%d %d\n", Shell_NotifyIconW( NIM_ADD, &nid ), Shell_NotifyIconW( NIM_ADD, &nid2 ) );
    pump( 1500 );
    /* the icon windows: top level, or the taskbar's children */
    EnumWindows( find_icons, 0 );
    EnumChildWindows( FindWindowW( L"Shell_TrayWnd", NULL ), find_icons, 0 );
    GetWindowRect( FindWindowW( L"Shell_TrayWnd", NULL ), &tray );

    ident.hWnd = hwnd;
    ident.uID = 7;
    hr = Shell_NotifyIconGetRect( &ident, &r1 );
    printf( "byid=%08lx tray=%d inbar=%d size=%ldx%ld\n", hr, is_tray_icon_rect( &r1 ),
            r1.top >= tray.top && r1.bottom <= tray.bottom && r1.left >= tray.left && r1.right <= tray.right,
            r1.right - r1.left, r1.bottom - r1.top );

    memset( &ident, 0, sizeof(ident) );
    ident.cbSize = sizeof(ident);
    ident.guidItem = icon_guid;
    hr = Shell_NotifyIconGetRect( &ident, &r2 );
    printf( "byguid=%08lx tray=%d distinct=%d\n", hr, is_tray_icon_rect( &r2 ), !EqualRect( &r1, &r2 ) );

    ident.hWnd = hwnd;
    ident.uID = 99;
    memset( &ident.guidItem, 0, sizeof(GUID) );
    printf( "unknown=%08lx\n", Shell_NotifyIconGetRect( &ident, &r2 ) );
    ident.cbSize = 4;
    printf( "badsize=%08lx\n", Shell_NotifyIconGetRect( &ident, &r2 ) );

    Shell_NotifyIconW( NIM_DELETE, &nid );
    pump( 500 );
    ident.cbSize = sizeof(ident);
    ident.uID = 7;
    printf( "deleted=%08lx\n", Shell_NotifyIconGetRect( &ident, &r2 ) );
    Shell_NotifyIconW( NIM_DELETE, &nid2 );
    DestroyWindow( hwnd );
    return 0;
}
