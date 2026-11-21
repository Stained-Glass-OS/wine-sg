/* parentbgex-probe: uxtheme DrawThemeParentBackgroundEx, which was not exported (Stained Glass OS wine-sg
 * 8269; test/parentbgex-gate.sh). A parent paints red on WM_ERASEBKGND (or not, per mode); the child's area
 * is drawn into a memory DC; the flags pick the result and the fill.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

#define DTPB_WINDOWDC 1
#define DTPB_USECTLCOLORSTATIC 2
#define DTPB_USEERASEBKGND 4

static int mode; /* 0: erases red; 1: does nothing (defaults); 2: also answers WM_CTLCOLORSTATIC with blue */
static HBRUSH red, blue;

static LRESULT CALLBACK parent_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_ERASEBKGND)
    {
        RECT r = { 0, 0, 400, 300 };
        if (mode == 1 || mode == 2) return 0;
        FillRect( (HDC)wp, &r, red );
        return 1;
    }
    if (msg == WM_CTLCOLORSTATIC && mode == 2) return (LRESULT)blue;
    return DefWindowProcA( hwnd, msg, wp, lp );
}

static int failures;
static void check( int ok, const char *what, long v )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, v );
    if (!ok) failures++;
}

int main( void )
{
    HRESULT (WINAPI *pEx)(HWND, HDC, DWORD, const RECT *) = (void *)GetProcAddress( LoadLibraryA( "uxtheme.dll" ), "DrawThemeParentBackgroundEx" );
    WNDCLASSA cls = { 0, parent_proc, 0, 0, NULL, NULL, NULL, NULL, NULL, "pbgparent" };
    HWND parent, child;
    HDC dc;
    HBITMAP bmp;
    HRESULT hr;
    COLORREF px;

    if (!pEx) { printf( "FAIL  uxtheme exports DrawThemeParentBackgroundEx\nRESULT: FAIL\n" ); return 1; }
    red = CreateSolidBrush( RGB(255, 0, 0) );
    blue = CreateSolidBrush( RGB(0, 0, 255) );
    RegisterClassA( &cls );
    parent = CreateWindowExA( 0, "pbgparent", "p", WS_POPUP, 0, 0, 400, 300, NULL, NULL, NULL, NULL );
    child = CreateWindowExA( 0, "STATIC", "", WS_CHILD | WS_VISIBLE, 50, 40, 100, 80, parent, NULL, NULL, NULL );
    dc = CreateCompatibleDC( NULL );
    bmp = CreateBitmap( 100, 80, 1, 32, NULL );
    SelectObject( dc, bmp );

    mode = 0;
    hr = pEx( child, dc, DTPB_USEERASEBKGND, NULL );
    px = GetPixel( dc, 10, 10 );
    check( hr == S_OK && px == RGB(255, 0, 0), "USEERASEBKGND, the parent erased: S_OK, its red behind", hr );

    mode = 1;
    PatBlt( dc, 0, 0, 100, 80, WHITENESS );
    hr = pEx( child, dc, 0, NULL );
    check( hr == S_FALSE, "no flags, the parent handled nothing: S_FALSE", hr );

    hr = pEx( child, dc, DTPB_USECTLCOLORSTATIC, NULL );
    px = GetPixel( dc, 10, 10 );
    check( hr == S_OK && px == GetSysColor( COLOR_BTNFACE ), "USECTLCOLORSTATIC with no brush: COLOR_BTNFACE", px );

    mode = 2;
    hr = pEx( child, dc, DTPB_USECTLCOLORSTATIC | DTPB_USEERASEBKGND, NULL );
    px = GetPixel( dc, 10, 10 );
    check( hr == S_OK && px == RGB(0, 0, 255), "the parent's WM_CTLCOLORSTATIC brush", px );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
