/* engpaths-probe: gdi32's engine path, line, fill, blend and gradient services for user-mode printer
 * drivers (Stained Glass OS wine-sg 6062), for test/engpaths-gate.sh. Draws on 32-bit surfaces of its
 * own, as a driver does on its bitmap, and prints KEY=VALUE lines.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>

/* the DDI's structures and constants (winddi.h), for a driver's view */
typedef LONG FIX;
typedef ULONG MIX;
typedef struct { FIX x, y; } POINTFIX;
typedef struct { FIX xLeft, yTop, xRight, yBottom; } RECTFX;
typedef struct { FLONG flags; ULONG count; POINTFIX *pptfx; } PATHDATA;
typedef struct { FLONG fl; ULONG cCurves; } PATHOBJ;
typedef struct { ULONG iUniq; RECTL rclBounds; BYTE iDComplexity, iFComplexity, iMode, fjOptions; } CLIPOBJ;
typedef struct { ULONG iSolidColor; void *pvRbrush; FLONG flColorType; } BRUSHOBJ;
typedef struct { void *dhsurf, *hsurf, *dhpdev, *hdev; SIZEL sizlBitmap; ULONG cjBits; void *pvBits, *pvScan0;
                 LONG lDelta; ULONG iUniq, iBitmapFormat; USHORT iType, fjBitmap; } SURFOBJ;
typedef union { FLOAT e; LONG l; } FLOAT_LONG;
typedef struct { FLONG fl; ULONG iJoin, iEndCap; FLOAT_LONG elWidth; FLOAT eMiterLimit; ULONG cstyle;
                 FLOAT_LONG *pstyle; FLOAT_LONG elStyleState; } LINEATTRS;
typedef struct { BLENDFUNCTION BlendFunction; } BLENDOBJ;
typedef struct { ULONG ulReserved; } XFORMOBJ;
typedef struct { ULONG iUniq; FLONG flXlate; USHORT iSrcType, iDstType; ULONG cEntries; ULONG *pulXlate; } XLATEOBJ;
#define BMF_32BPP 6
#define DC_RECT 1
#define FP_ALTERNATEMODE 1
#define FP_WINDINGMODE 2
#define LA_GEOMETRIC 1
#include <stdio.h>

#define W 32
#define H 32
static DWORD bits[H][W], bits2[H][W];
static SURFOBJ so, so2;

#define F(ret, name, args) static ret (WINAPI *p##name) args
F(CLIPOBJ *, EngCreateClip, (void));
F(void, EngDeleteClip, (CLIPOBJ *));
F(PATHOBJ *, CLIPOBJ_ppoGetPath, (CLIPOBJ *));
F(void, EngDeletePath, (PATHOBJ *));
F(void, PATHOBJ_vEnumStart, (PATHOBJ *));
F(BOOL, PATHOBJ_bEnum, (PATHOBJ *, PATHDATA *));
F(void, PATHOBJ_vGetBounds, (PATHOBJ *, RECTFX *));
F(BOOL, EngFillPath, (SURFOBJ *, PATHOBJ *, CLIPOBJ *, BRUSHOBJ *, POINTL *, MIX, FLONG));
F(BOOL, EngStrokePath, (SURFOBJ *, PATHOBJ *, CLIPOBJ *, XFORMOBJ *, BRUSHOBJ *, POINTL *, LINEATTRS *, MIX));
F(BOOL, EngStrokeAndFillPath, (SURFOBJ *, PATHOBJ *, CLIPOBJ *, XFORMOBJ *, BRUSHOBJ *, LINEATTRS *, BRUSHOBJ *, POINTL *, MIX, FLONG));
F(BOOL, EngLineTo, (SURFOBJ *, CLIPOBJ *, BRUSHOBJ *, LONG, LONG, LONG, LONG, RECTL *, MIX));
F(BOOL, EngPaint, (SURFOBJ *, CLIPOBJ *, BRUSHOBJ *, POINTL *, MIX));
F(BOOL, EngAlphaBlend, (SURFOBJ *, SURFOBJ *, CLIPOBJ *, XLATEOBJ *, RECTL *, RECTL *, BLENDOBJ *));
F(BOOL, EngGradientFill, (SURFOBJ *, CLIPOBJ *, XLATEOBJ *, TRIVERTEX *, ULONG, void *, ULONG, RECTL *, POINTL *, ULONG));
F(void *, BRUSHOBJ_pvAllocRbrush, (BRUSHOBJ *, ULONG));
F(void *, BRUSHOBJ_pvGetRbrush, (BRUSHOBJ *));
F(HANDLE, BRUSHOBJ_hGetColorTransform, (BRUSHOBJ *));

static void clear( DWORD v )
{
    int x, y;
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) bits[y][x] = v;
}

static int count( DWORD v )
{
    int x, y, n = 0;
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) n += bits[y][x] == v;
    return n;
}

int main( void )
{
    HMODULE gdi = LoadLibraryA( "gdi32.dll" );
    CLIPOBJ *clip, *clip2;
    PATHOBJ *path;
    PATHDATA pd;
    RECTFX fx;
    BRUSHOBJ red = { 0x00ff0000, NULL, 0 }, blue = { 0x000000ff, NULL, 0 }, pattern = { 0xffffffff, NULL, 0 };
    LINEATTRS cosmetic = { 0 }, wide = { LA_GEOMETRIC };
    BOOL more, ret;
    int i, n;

#define L(name) if (!(p##name = (void *)GetProcAddress( gdi, #name ))) { printf( "missing=%s\n", #name ); return 1; }
    L(EngCreateClip) L(EngDeleteClip) L(CLIPOBJ_ppoGetPath) L(EngDeletePath) L(PATHOBJ_vEnumStart) L(PATHOBJ_bEnum)
    L(PATHOBJ_vGetBounds) L(EngFillPath) L(EngStrokePath) L(EngStrokeAndFillPath) L(EngLineTo) L(EngPaint)
    L(EngAlphaBlend) L(EngGradientFill) L(BRUSHOBJ_pvAllocRbrush) L(BRUSHOBJ_pvGetRbrush) L(BRUSHOBJ_hGetColorTransform)

    so.sizlBitmap.cx = W; so.sizlBitmap.cy = H; so.pvScan0 = so.pvBits = bits; so.lDelta = W * 4;
    so.cjBits = sizeof(bits); so.iBitmapFormat = BMF_32BPP;
    so2 = so; so2.pvScan0 = so2.pvBits = bits2;

    /* a clip's outline */
    clip = pEngCreateClip();
    printf( "TrivialPath=%d\n", pCLIPOBJ_ppoGetPath( clip ) == NULL );
    clip->iDComplexity = DC_RECT;
    clip->rclBounds.left = 4; clip->rclBounds.top = 6; clip->rclBounds.right = 12; clip->rclBounds.bottom = 10;
    path = pCLIPOBJ_ppoGetPath( clip );
    printf( "Path=%d %lu\n", path != NULL, path ? path->cCurves : 0 );
    if (!path) return 1;
    pPATHOBJ_vEnumStart( path );
    memset( &pd, 0, sizeof(pd) );
    more = pPATHOBJ_bEnum( path, &pd );
    printf( "Enum=%d %lu %#lx", more, pd.count, pd.flags );
    for (i = 0; i < (int)pd.count; i++) printf( " %ld,%ld", pd.pptfx[i].x / 16, pd.pptfx[i].y / 16 );
    printf( "\n" );
    more = pPATHOBJ_bEnum( path, &pd );
    printf( "EnumEnd=%d %lu\n", more, pd.count );
    pPATHOBJ_vGetBounds( path, &fx );
    printf( "Bounds=%ld %ld %ld %ld\n", fx.xLeft / 16, fx.yTop / 16, fx.xRight / 16, fx.yBottom / 16 );

    /* filling it: the 8x4 pixels inside, nothing outside; clipped by another clip */
    clear( 0 );
    ret = pEngFillPath( &so, path, NULL, &red, NULL, R2_COPYPEN, FP_ALTERNATEMODE );
    printf( "Fill=%d %d %d %d %d\n", ret, count( 0xff0000 ), bits[6][4] == 0xff0000, bits[9][11] == 0xff0000,
            bits[10][4] == 0 && bits[6][12] == 0 );
    clip2 = pEngCreateClip();
    clip2->iDComplexity = DC_RECT;
    clip2->rclBounds.left = 0; clip2->rclBounds.top = 0; clip2->rclBounds.right = 8; clip2->rclBounds.bottom = 8;
    clear( 0 );
    ret = pEngFillPath( &so, path, clip2, &red, NULL, R2_COPYPEN, FP_WINDINGMODE );
    printf( "FillClipped=%d %d\n", ret, count( 0xff0000 ) );
    printf( "FillPattern=%d\n", pEngFillPath( &so, path, NULL, &pattern, NULL, R2_COPYPEN, 0 ) );
    /* a mix: XOR twice gives back what was there */
    clear( 0x00123456 );
    pEngFillPath( &so, path, NULL, &red, NULL, R2_XORPEN, FP_ALTERNATEMODE );
    n = count( 0x00123456 ^ 0xff0000 );
    pEngFillPath( &so, path, NULL, &red, NULL, R2_XORPEN, FP_ALTERNATEMODE );
    printf( "FillXor=%d %d\n", n, count( 0x00123456 ) );

    /* stroking it: the outline's pixels, cosmetic lines only */
    clear( 0 );
    ret = pEngStrokePath( &so, path, NULL, NULL, &blue, NULL, &cosmetic, R2_COPYPEN );
    printf( "Stroke=%d %d %d %d\n", ret, count( 0xff ), bits[6][4] == 0xff && bits[6][11] == 0xff && bits[10][12] == 0xff,
            bits[8][8] == 0 );
    printf( "StrokeWide=%d\n", pEngStrokePath( &so, path, NULL, NULL, &blue, NULL, &wide, R2_COPYPEN ) );
    clear( 0 );
    ret = pEngStrokeAndFillPath( &so, path, NULL, NULL, &blue, &cosmetic, &red, NULL, R2_COPYPEN, FP_ALTERNATEMODE );
    printf( "StrokeAndFill=%d %d %d\n", ret, bits[6][4] == 0xff, bits[8][8] == 0xff0000 );
    pEngDeletePath( path );

    /* lines: Bresenham, the last pixel left out, clipped */
    clear( 0 );
    ret = pEngLineTo( &so, NULL, &red, 0, 0, 10, 5, NULL, R2_COPYPEN );
    printf( "Line=%d %d %d %d\n", ret, count( 0xff0000 ), bits[0][0] == 0xff0000, bits[5][10] == 0 );
    clear( 0 );
    ret = pEngLineTo( &so, clip2, &red, 0, 2, 31, 2, NULL, R2_COPYPEN );
    printf( "LineClipped=%d %d\n", ret, count( 0xff0000 ) );

    /* painting a clip's area */
    clear( 0 );
    ret = pEngPaint( &so, clip, &blue, NULL, R2_COPYPEN );
    printf( "Paint=%d %d\n", ret, count( 0xff ) );
    clear( 0xffffff );
    ret = pEngPaint( &so, clip, &blue, NULL, R2_NOT );
    printf( "PaintNot=%d %d\n", ret, count( 0 ) );

    /* blending: constant alpha half over white, and a premultiplied source */
    for (i = 0; i < W * H; i++) ((DWORD *)bits2)[i] = 0x000000ff;
    clear( 0x00ffffff );
    {
        RECTL d = { 0, 0, 16, 16 }, s = { 0, 0, 16, 16 };
        BLENDOBJ b = { { AC_SRC_OVER, 0, 128, 0 } };
        ret = pEngAlphaBlend( &so, &so2, NULL, NULL, &d, &s, &b );
        printf( "AlphaConstant=%d %#lx %#lx\n", ret, bits[0][0], bits[20][20] );
        for (i = 0; i < W * H; i++) ((DWORD *)bits2)[i] = 0x80000080;   /* half-covered blue, premultiplied */
        clear( 0x00ffffff );
        b.BlendFunction.SourceConstantAlpha = 255;
        b.BlendFunction.AlphaFormat = AC_SRC_ALPHA;
        ret = pEngAlphaBlend( &so, &so2, NULL, NULL, &d, &s, &b );
        printf( "AlphaSource=%d %#lx\n", ret, bits[0][0] );
    }

    /* gradients */
    {
        TRIVERTEX v[3] = { { 0, 0, 0, 0, 0, 0 }, { 16, 1, 0xff00, 0, 0, 0 }, { 0, 16, 0, 0, 0xff00, 0 } };
        GRADIENT_RECT r = { 0, 1 };
        GRADIENT_TRIANGLE t = { 0, 1, 2 };
        clear( 0 );
        ret = pEngGradientFill( &so, NULL, NULL, v, 2, &r, 1, NULL, NULL, GRADIENT_FILL_RECT_H );
        printf( "GradientH=%d %#lx %#lx %#lx\n", ret, bits[0][0], bits[0][8], bits[0][15] );
        clear( 0x00ffffff );
        ret = pEngGradientFill( &so, NULL, NULL, v, 3, &t, 1, NULL, NULL, GRADIENT_FILL_TRIANGLE );
        printf( "GradientTriangle=%d %d %d\n", ret, bits[1][1] != 0xffffff && (bits[1][1] & 0xff0000) < 0x200000,
                bits[15][15] == 0xffffff );
        printf( "GradientBadMode=%d\n", pEngGradientFill( &so, NULL, NULL, v, 2, &r, 1, NULL, NULL, 7 ) );
    }

    /* realized brushes */
    {
        void *a = pBRUSHOBJ_pvAllocRbrush( &red, 64 ), *b;
        printf( "Rbrush=%d %d %d\n", a != NULL, pBRUSHOBJ_pvGetRbrush( &red ) == a, pBRUSHOBJ_pvGetRbrush( &blue ) == NULL );
        b = pBRUSHOBJ_pvAllocRbrush( &red, 32 );
        printf( "RbrushAgain=%d %d\n", b != NULL, pBRUSHOBJ_pvGetRbrush( &red ) == b );
        printf( "ColorTransform=%d\n", pBRUSHOBJ_hGetColorTransform( &red ) == NULL );
    }
    pEngDeleteClip( clip );
    pEngDeleteClip( clip2 );
    printf( "done=1\n" );
    return 0;
}
