/* taskviewmove-probe: the taskbar's Task View button is painted where it moves to
 * (patches/sg/6529). A notification-area icon is added, which moves the button and the desktop
 * pager left, and removed, which moves them back. The button must be painted whole where it
 * now is: a hook in the taskbar's thread (taskviewmove-hook.dll) records the update rectangle
 * of its next WM_PAINT. What the screen then shows is also compared with a full repaint.
 * Prints name=value lines; see test/taskviewmove-gate.sh.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

static HWND tray, pager, taskview;

static BOOL CALLBACK find_pager( HWND hwnd, LPARAM lp )
{
    char cls[64];
    GetClassNameA( hwnd, cls, sizeof(cls) );
    if (!strcmp( cls, "SgVirtualDesktopPager" ) && IsWindowVisible( hwnd )) pager = hwnd;
    return TRUE;
}

/* the Task View button: the button just left of the pager */
static BOOL CALLBACK find_taskview( HWND hwnd, LPARAM lp )
{
    RECT r, p;
    char cls[64];
    GetClassNameA( hwnd, cls, sizeof(cls) );
    if (strcmp( cls, "Button" ) || !IsWindowVisible( hwnd )) return TRUE;
    GetWindowRect( hwnd, &r );
    GetWindowRect( pager, &p );
    if (r.right == p.left) taskview = hwnd;
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

static COLORREF *grab( const RECT *r )
{
    COLORREF *px = malloc( (r->right - r->left) * (r->bottom - r->top) * sizeof(*px) );
    HDC dc = GetDC( NULL );
    int x, y, n = 0;
    for (y = r->top; y < r->bottom; y++)
        for (x = r->left; x < r->right; x++) px[n++] = GetPixel( dc, x, y );
    ReleaseDC( NULL, dc );
    return px;
}

static HICON make_icon( void )
{
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), 16, -16, 1, 32, BI_RGB }};
    DWORD *px;
    HDC dc = CreateCompatibleDC( NULL );
    HBITMAP color = CreateDIBSection( dc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0 ), mask = CreateBitmap( 16, 16, 1, 1, NULL );
    ICONINFO ii = { TRUE, 0, 0, mask, color };
    HICON icon;
    int i;
    for (i = 0; i < 256; i++) px[i] = 0xFF00A000;
    icon = CreateIconIndirect( &ii );
    DeleteObject( color ); DeleteObject( mask ); DeleteDC( dc );
    return icon;
}

struct shared
{
    HWND watch;
    LONG paints;
    RECT last;
    LONG width, height;
};

int main( void )
{
    HANDLE map = CreateFileMappingW( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct shared), L"Local\\sg_taskviewmove" );
    struct shared *shared = MapViewOfFile( map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*shared) );
    HMODULE hook_dll;
    HHOOK hook = NULL;
    LONG paints;
    NOTIFYICONDATAW nid = { sizeof(nid) };
    HWND owner = CreateWindowW( L"STATIC", L"probe", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL );
    RECT r0, r1, r2, area;
    COLORREF *before, *after;
    int i, n, diff = 0;

    setvbuf( stdout, NULL, _IONBF, 0 );
    tray = FindWindowW( L"Shell_TrayWnd", NULL );
    if (tray) EnumChildWindows( tray, find_pager, 0 );
    if (pager) EnumChildWindows( tray, find_taskview, 0 );
    printf( "found=%d tray=%p pager=%p taskview=%p\n", tray && pager && taskview, tray, pager, taskview );
    if (!taskview) return 1;
    GetWindowRect( taskview, &r0 );
    shared->watch = taskview;
    if ((hook_dll = LoadLibraryW( L"taskviewmove-hook.dll" )))
        hook = SetWindowsHookExW( WH_GETMESSAGE, (HOOKPROC)GetProcAddress( hook_dll, "hook_proc" ), hook_dll,
                                  GetWindowThreadProcessId( taskview, NULL ) );
    printf( "hooked=%d\n", hook != NULL );
    PostMessageW( taskview, WM_NULL, 0, 0 );   /* (loads the hook there) */
    pump( 500 );

    nid.hWnd = owner; nid.uID = 1; nid.uFlags = NIF_ICON | NIF_TIP;
    nid.hIcon = make_icon();
    lstrcpyW( nid.szTip, L"probe" );
    Shell_NotifyIconW( NIM_ADD, &nid );
    pump( 1500 );
    GetWindowRect( taskview, &r1 );
    paints = shared->paints;
    shared->last.left = shared->last.top = shared->last.right = shared->last.bottom = 0;
    Shell_NotifyIconW( NIM_DELETE, &nid );
    pump( 1500 );
    GetWindowRect( taskview, &r2 );
    printf( "painted_whole=%d paints=%ld update=%ld,%ld-%ld,%ld client=%ldx%ld\n",
            shared->paints > paints && !shared->last.left && !shared->last.top &&
            shared->last.right == shared->width && shared->last.bottom == shared->height,
            shared->paints - paints, shared->last.left, shared->last.top, shared->last.right, shared->last.bottom,
            shared->width, shared->height );
    printf( "moved=%d start=%ld with_icon=%ld after=%ld\n", r1.left < r0.left && r2.left == r0.left, r0.left, r1.left, r2.left );

    /* the button and the place it left */
    area = r2;
    area.left = min( r1.left, r2.left );
    n = (area.right - area.left) * (area.bottom - area.top);
    before = grab( &area );
    RedrawWindow( tray, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME );
    pump( 1500 );
    after = grab( &area );
    for (i = 0; i < n; i++) if (before[i] != after[i]) diff++;
    printf( "repainted=%d diff=%d of %d\n", diff == 0, diff, n );
    if (hook) UnhookWindowsHookEx( hook );
    printf( "done=1\n" );
    return 0;
}
