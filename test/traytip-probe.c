/* traytip-probe: for traytip-gate.sh.
 *   traytip-probe add SECONDS -- a notification-area icon with a tooltip,
 *     kept SECONDS; prints "rect=L T R B" of its icon in the taskbar
 *   traytip-probe tips -- prints "tips=N": the tooltip windows shown now
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static HWND g_found;
static int g_tips;

static BOOL CALLBACK find_icon( HWND hwnd, LPARAM lp )
{
    WCHAR cls[64];
    GetClassNameW( hwnd, cls, 64 );
    if (!lstrcmpW( cls, L"__wine_tray_icon" ) && IsWindowVisible( hwnd )) { g_found = hwnd; return FALSE; }
    EnumChildWindows( hwnd, find_icon, lp );
    return !g_found;
}

static BOOL CALLBACK count_tips( HWND hwnd, LPARAM lp )
{
    WCHAR cls[64];
    GetClassNameW( hwnd, cls, 64 );
    if (!lstrcmpW( cls, L"tooltips_class32" ) && IsWindowVisible( hwnd )) g_tips++;
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

int main( int argc, char **argv )
{
    NOTIFYICONDATAW nid = { sizeof(nid) };
    RECT r;
    HWND owner;

    if (argc > 1 && !strcmp( argv[1], "tips" ))
    {
        EnumWindows( count_tips, 0 );
        printf( "tips=%d\n", g_tips );
        return 0;
    }
    if (argc < 3 || strcmp( argv[1], "add" )) return 2;
    owner = CreateWindowW( L"STATIC", L"traytip", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL );
    nid.hWnd = owner;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP;
    nid.hIcon = LoadIconW( NULL, (LPCWSTR)IDI_APPLICATION );
    lstrcpyW( nid.szTip, L"Tray tip probe" );
    if (!Shell_NotifyIconW( NIM_ADD, &nid )) { printf( "add=0\n" ); return 1; }
    pump( 1500 );
    EnumWindows( find_icon, 0 );
    if (!g_found) { printf( "rect=none\n" ); fflush( stdout ); }
    else
    {
        GetWindowRect( g_found, &r );
        printf( "rect=%ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom );
        fflush( stdout );
    }
    pump( atoi( argv[2] ) * 1000 );
    Shell_NotifyIconW( NIM_DELETE, &nid );
    return 0;
}
