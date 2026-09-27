/* trayicon-probe: a notification-area icon changes from a solid red square
 * to an almost transparent one (a white dot); what the tray shows then must
 * not still be red (patches/sg/0442). Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static HICON make_icon( BOOL red )
{
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), 16, -16, 1, 32, BI_RGB }};
    DWORD *px;
    HDC dc = CreateCompatibleDC( NULL );
    HBITMAP color = CreateDIBSection( dc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0 ), mask = CreateBitmap( 16, 16, 1, 1, NULL );
    ICONINFO ii = { TRUE, 0, 0, mask, color };
    HICON icon;
    int i;
    for (i = 0; i < 256; i++)
    {
        int x = i % 16, y = i / 16;
        if (red) px[i] = 0xFFFF0000;
        else px[i] = (x >= 6 && x < 10 && y >= 6 && y < 10) ? 0xFFFFFFFF : 0;
    }
    icon = CreateIconIndirect( &ii );
    DeleteObject( color ); DeleteObject( mask ); DeleteDC( dc );
    return icon;
}

static HWND g_found;
static BOOL CALLBACK find_icon( HWND hwnd, LPARAM lp )
{
    WCHAR cls[64];
    GetClassNameW( hwnd, cls, 64 );
    if (!lstrcmpW( cls, L"__wine_tray_icon" ) && IsWindowVisible( hwnd )) { g_found = hwnd; return FALSE; }
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

int main( void )
{
    NOTIFYICONDATAW nid = { sizeof(nid) };
    HWND owner = CreateWindowW( L"STATIC", L"probe", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL );
    HWND tray = FindWindowW( L"Shell_TrayWnd", NULL );
    RECT r;
    HDC dc;
    int x, y, red = 0, total = 0;

    nid.hWnd = owner; nid.uID = 1; nid.uFlags = NIF_ICON | NIF_TIP;
    nid.hIcon = make_icon( TRUE );
    lstrcpyW( nid.szTip, L"probe" );
    printf( "add=%d\n", Shell_NotifyIconW( NIM_ADD, &nid ) );
    pump( 1500 );
    nid.hIcon = make_icon( FALSE );
    printf( "modify=%d\n", Shell_NotifyIconW( NIM_MODIFY, &nid ) );
    pump( 1500 );
    if (tray) EnumChildWindows( tray, find_icon, 0 );
    if (!g_found) EnumWindows( find_icon, 0 );
    if (!g_found) { printf( "icon=0\n" ); return 1; }
    GetWindowRect( g_found, &r );
    dc = GetDC( NULL );
    for (y = r.top; y < r.bottom; y++)
        for (x = r.left; x < r.right; x++)
        {
            COLORREF c = GetPixel( dc, x, y );
            total++;
            if (GetRValue( c ) > 180 && GetGValue( c ) < 80 && GetBValue( c ) < 80) red++;
        }
    ReleaseDC( NULL, dc );
    printf( "icon=1 red=%d of %d\n", red, total );
    Shell_NotifyIconW( NIM_DELETE, &nid );
    return 0;
}
