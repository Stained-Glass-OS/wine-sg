/* DwmExtendFrameIntoClientArea "sheet of glass" (patches/sg/0435), for
 * test/glass-gate.sh: a popup drawn with a 32-bit DIB whose left half is
 * transparent and right half opaque blue.
 *   glass-probe DIR  -- prints "GLASS <xid>" after negative margins, waits
 *                       for DIR\next, prints "OPAQUE <xid>" after zero margins,
 *                       waits for DIR\done
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>

#define W 200
#define H 100

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_PAINT)
    {
        static DWORD bits[W * H];
        BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB }};
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint( hwnd, &ps ), mem = CreateCompatibleDC( hdc );
        void *dib;
        HBITMAP bmp = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, &dib, NULL, 0 ), old;
        int x, y;
        for (y = 0; y < H; y++) for (x = 0; x < W; x++) bits[y * W + x] = x < W / 2 ? 0 : 0xff0000ff;
        memcpy( dib, bits, sizeof(bits) );
        old = SelectObject( mem, bmp );
        BitBlt( hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY );
        SelectObject( mem, old );
        DeleteObject( bmp );
        DeleteDC( mem );
        EndPaint( hwnd, &ps );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }
        Sleep( 10 );
    }
}

static void wait_for( const char *dir, const char *name )
{
    char path[MAX_PATH];
    int i;
    snprintf( path, sizeof(path), "%s\\%s", dir, name );
    for (i = 0; i < 600 && GetFileAttributesA( path ) == INVALID_FILE_ATTRIBUTES; i++) pump( 100 );
}

int main( int argc, char **argv )
{
    WNDCLASSW wc = { 0, wndproc, 0, 0, GetModuleHandleW( NULL ), 0, 0, 0, 0, L"glassprobe" };
    MARGINS glass = { -1, -1, -1, -1 }, none = { 0, 0, 0, 0 };
    HWND hwnd;
    HRESULT hr;

    if (argc < 2) return 2;
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"glassprobe", L"glass", WS_POPUP,
                            100, 100, W, H, NULL, NULL, NULL, NULL );
    ShowWindow( hwnd, SW_SHOWNA );
    UpdateWindow( hwnd );
    pump( 300 );

    hr = DwmExtendFrameIntoClientArea( NULL, NULL );
    printf( "NULLMARGINS %08lx\n", hr );
    hr = DwmExtendFrameIntoClientArea( hwnd, &glass );
    pump( 1000 );
    printf( "GLASS %08lx %lx\n", hr, (ULONG)(ULONG_PTR)GetPropA( hwnd, "__wine_x11_whole_window" ) );
    fflush( stdout );
    wait_for( argv[1], "next" );

    hr = DwmExtendFrameIntoClientArea( hwnd, &none );
    pump( 1000 );
    printf( "OPAQUE %08lx %lx\n", hr, (ULONG)(ULONG_PTR)GetPropA( hwnd, "__wine_x11_whole_window" ) );
    fflush( stdout );
    wait_for( argv[1], "done" );
    DestroyWindow( hwnd );
    return 0;
}
