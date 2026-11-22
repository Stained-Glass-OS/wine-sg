/* pathgradblend-probe: gdiplus path gradient brushes follow their blend and preset blend (Stained Glass OS
 * wine-sg 8297; test/pathgradblend-gate.sh); both were ignored ("path gradient blend not implemented",
 * Explorer++).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef struct { float X, Y; } PointF;
typedef int (WINAPI *startup_fn)( ULONG_PTR *, const void *, void * );
static int (WINAPI *pCreateBitmap)( INT, INT, INT, INT, BYTE *, void ** );
static int (WINAPI *pGetContext)( void *, void ** );
static int (WINAPI *pCreatePathGradient)( const PointF *, INT, INT, void ** );
static int (WINAPI *pSetCenterColor)( void *, DWORD );
static int (WINAPI *pSetSurround)( void *, const DWORD *, INT * );
static int (WINAPI *pSetCenterPoint)( void *, const PointF * );
static int (WINAPI *pSetBlend)( void *, const float *, const float *, INT );
static int (WINAPI *pSetPreset)( void *, const DWORD *, const float *, INT );
static int (WINAPI *pFillRect)( void *, void *, float, float, float, float );
static int (WINAPI *pGetPixel)( void *, INT, INT, DWORD * );
static int (WINAPI *pDeleteBrush)( void * );
static int (WINAPI *pDeleteGraphics)( void * );
static int (WINAPI *pGraphicsClear)( void *, DWORD );

static int fails;

static DWORD paint( int mode, int x, int y )
{
    static const PointF square[4] = { { 0, 0 }, { 100, 0 }, { 100, 100 }, { 0, 100 } };
    static const PointF centre = { 50, 50 };
    void *bitmap, *graphics, *brush;
    DWORD black = 0xff000000, pixel = 0;
    INT count = 1;

    pCreateBitmap( 100, 100, 0, 0x26200a /* PixelFormat32bppARGB */, NULL, &bitmap );
    pGetContext( bitmap, &graphics );
    pGraphicsClear( graphics, 0xff808080 );
    pCreatePathGradient( square, 4, 0 /* WrapModeTile */, &brush );
    pSetCenterPoint( brush, &centre );
    pSetCenterColor( brush, 0xffffffff );
    pSetSurround( brush, &black, &count );
    if (mode == 1)
    {
        static const float factors[3] = { 0.0f, 0.0f, 1.0f }, positions[3] = { 0.0f, 0.5f, 1.0f };
        pSetBlend( brush, factors, positions, 3 );
    }
    else if (mode == 2)
    {
        static const DWORD colours[3] = { 0xffff0000, 0xff00ff00, 0xff0000ff };
        static const float positions[3] = { 0.0f, 0.5f, 1.0f };
        pSetPreset( brush, colours, positions, 3 );
    }
    pFillRect( graphics, brush, 0, 0, 100, 100 );
    pGetPixel( bitmap, x, y, &pixel );
    pDeleteBrush( brush );
    pDeleteGraphics( graphics );
    return pixel;
}

static void check( int ok, const char *what, DWORD v ) { printf( "%s  %s (%08lx)\n", ok ? "PASS" : "FAIL", what, v ); if (!ok) fails++; }

int main( void )
{
    HMODULE gdip = LoadLibraryA( "gdiplus.dll" );
    struct { UINT32 version; void *cb; BOOL a, b; } input = { 1 };
    ULONG_PTR token;
    DWORD p;

#define GET(v, n) v = (void *)GetProcAddress( gdip, n )
    GET(pCreateBitmap, "GdipCreateBitmapFromScan0"); GET(pGetContext, "GdipGetImageGraphicsContext");
    GET(pCreatePathGradient, "GdipCreatePathGradient"); GET(pSetCenterColor, "GdipSetPathGradientCenterColor");
    GET(pSetSurround, "GdipSetPathGradientSurroundColorsWithCount"); GET(pSetCenterPoint, "GdipSetPathGradientCenterPoint");
    GET(pSetBlend, "GdipSetPathGradientBlend"); GET(pSetPreset, "GdipSetPathGradientPresetBlend");
    GET(pFillRect, "GdipFillRectangle"); GET(pGetPixel, "GdipBitmapGetPixel");
    GET(pDeleteBrush, "GdipDeleteBrush"); GET(pDeleteGraphics, "GdipDeleteGraphics"); GET(pGraphicsClear, "GdipGraphicsClear");
    ((startup_fn)GetProcAddress( gdip, "GdiplusStartup" ))( &token, &input, NULL );

    p = paint( 0, 50, 12 );
    check( (p & 0xff) > 30 && (p & 0xff) < 100, "no blend: a quarter of the way in is grey", p );
    p = paint( 1, 50, 12 );
    check( (p & 0xffffff) < 0x0a0a0a, "blend 0,0,1 at 0,.5,1: the outer half stays the surround colour", p );
    p = paint( 1, 50, 45 );
    check( (p & 0xff) > 180, "  and near the centre it is the centre colour", p );
    p = paint( 2, 50, 25 );
    check( ((p >> 8) & 0xff) > 200 && ((p >> 16) & 0xff) < 60 && (p & 0xff) < 60, "preset red,green,blue: half way in is green", p );
    p = paint( 2, 50, 3 );
    check( ((p >> 16) & 0xff) > 180, "  and at the edge red", p );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
