/* emfpbrush-probe: EMF+ texture and path gradient brushes (Stained Glass OS wine-sg 9201). Recording a
 * fill or a pen with a TextureBrush or a PathGradientBrush into an EMF+ metafile failed with
 * NotImplemented (System.Drawing: "not implemented" from Graphics.FillRectangle on a Metafile's
 * Graphics), and a path gradient brush object in a metafile (BrushTypePathGradient, [MS-EMFPLUS]
 * 2.2.2.29) was not read, so its fill was dropped. As on Windows, both brushes are written and read,
 * and the played metafile draws what drawing the same brush directly draws.
 * Prints name=value lines; see test/emfpbrush-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;
typedef struct { INT X, Y, Width, Height; } GpRect;
typedef struct { float X, Y; } GpPointF;
typedef BOOL (CALLBACK *EnumProc)(int type, UINT flags, UINT size, const BYTE *data, void *ctx);

static HMODULE g;
#define F(name, sig, ...) ((GpStatus (WINAPI *) sig)GetProcAddress(g, #name))(__VA_ARGS__)
#define W 64
#define FMT_ARGB 0x26200a

static void *metafile;
static int brush_objects, brush_status = -1;
static DWORD texture_type, path_type, path_flags, texture_image_version;

static BOOL CALLBACK play(int type, UINT flags, UINT size, const BYTE *data, void *ctx)
{
    GpStatus st = F(GdipPlayMetafileRecord, (void *, int, UINT, UINT, const BYTE *), metafile, type, flags, size, data);
    if (type == 0x4008 && ((flags >> 8) & 0x7f) == 1) brush_status = st;
    return TRUE;
}

/* the recorded brush objects: their Type, the path gradient's flags, the texture's EmfPlusImage version */
static BOOL CALLBACK grab(int type, UINT flags, UINT size, const BYTE *data, void *ctx)
{
    DWORD t, f;
    if (type != 0x4008 || ((flags >> 8) & 0x7f) != 1 || size < 12) return TRUE;
    brush_objects++;
    memcpy(&t, data + 4, 4);
    memcpy(&f, data + 8, 4);
    if (t == 2) { texture_type = t; if (size >= 20 + 4) memcpy(&texture_image_version, data + 16 + ((f & 2) ? 24 : 0), 4); }
    if (t == 3) { path_type = t; path_flags = f; }
    return TRUE;
}

static void *new_bitmap(void **graphics)
{
    void *b = NULL;
    F(GdipCreateBitmapFromScan0, (INT, INT, INT, int, BYTE *, void **), W, W, 0, FMT_ARGB, NULL, &b);
    F(GdipGetImageGraphicsContext, (void *, void **), b, graphics);
    F(GdipGraphicsClear, (void *, DWORD), *graphics, 0xffffffff);
    return b;
}

static DWORD px(void *b, int x, int y)
{
    DWORD c = 0;
    F(GdipBitmapGetPixel, (void *, INT, INT, DWORD *), b, x, y, &c);
    return c;
}

/* the brushes: 0 a 4x4 texture (red left half, blue right half) tiled at 2x; 1 a path gradient over
 * the square, white centre, black edge, focus 0.4; 2 a path gradient with preset colours */
static void *make_brush(int which)
{
    void *br = NULL;
    if (which == 0)
    {
        void *tb = NULL, *tg = NULL;
        F(GdipCreateBitmapFromScan0, (INT, INT, INT, int, BYTE *, void **), 4, 4, 0, FMT_ARGB, NULL, &tb);
        F(GdipGetImageGraphicsContext, (void *, void **), tb, &tg);
        F(GdipGraphicsClear, (void *, DWORD), tg, 0xff0000ff);
        F(GdipDeleteGraphics, (void *), tg);
        for (int y = 0; y < 4; y++) for (int x = 0; x < 2; x++)
            F(GdipBitmapSetPixel, (void *, INT, INT, DWORD), tb, x, y, 0xffff0000);
        F(GdipCreateTexture, (void *, int, void **), tb, 0 /* WrapModeTile */, &br);
        F(GdipScaleTextureTransform, (void *, float, float, int), br, 2.0f, 2.0f, 0);
        F(GdipDisposeImage, (void *), tb);
    }
    else
    {
        GpPointF pts[4] = { { 0, 0 }, { W, 0 }, { W, W }, { 0, W } };
        DWORD black = 0xff000000;
        INT n = 1;
        F(GdipCreatePathGradient, (const GpPointF *, INT, int, void **), pts, 4, 0, &br);
        F(GdipSetPathGradientCenterColor, (void *, DWORD), br, 0xffffffff);
        F(GdipSetPathGradientSurroundColorsWithCount, (void *, const DWORD *, INT *), br, &black, &n);
        if (which == 1)
            F(GdipSetPathGradientFocusScales, (void *, float, float), br, 0.4f, 0.4f);
        else
        {
            DWORD cols[3] = { 0xff00ff00, 0xffff0000, 0xff0000ff };
            float pos[3] = { 0, 0.5f, 1 };
            F(GdipSetPathGradientPresetBlend, (void *, const DWORD *, const float *, INT), br, cols, pos, 3);
        }
    }
    return br;
}

static void draw(void *gr, int which, BOOL pen)
{
    void *br = make_brush(which);
    if (pen)
    {
        void *p = NULL;
        F(GdipCreatePen2, (void *, float, int, void **), br, 12.0f, 2, &p);
        F(GdipDrawLineI, (void *, void *, int, int, int, int), gr, p, 0, W / 2, W, W / 2);
        F(GdipDeletePen, (void *), p);
    }
    else
        F(GdipFillRectangleI, (void *, void *, int, int, int, int), gr, br, 0, 0, W, W);
    F(GdipDeleteBrush, (void *), br);
}

static int chan_diff(DWORD a, DWORD b)
{
    int m = 0;
    for (int s = 0; s < 32; s += 8) { int d = abs((int)((a >> s) & 0xff) - (int)((b >> s) & 0xff)); if (d > m) m = d; }
    return m;
}

/* draw the brush directly and through a recorded EMF+ metafile; print the recording status and
 * how far the two pictures differ (mean and the worst sample away from the edges) */
static void compare(const char *name, int which, BOOL pen)
{
    void *dg, *direct = new_bitmap(&dg), *rec = NULL, *rg = NULL, *pg, *played;
    float frame[4] = { 0, 0, W, W };
    GpRect dest = { 0, 0, W, W };
    HDC ref = GetDC(0);
    GpStatus st;
    long sum = 0; int worst = 0, n = 0;

    draw(dg, which, pen);
    F(GdipDeleteGraphics, (void *), dg);

    F(GdipRecordMetafile, (HDC, int, const float *, int, const WCHAR *, void **), ref, 4 /* EmfPlusOnly */,
            frame, 2 /* MetafileFrameUnitPixel */, NULL, &rec);
    ReleaseDC(0, ref);
    F(GdipGetImageGraphicsContext, (void *, void **), rec, &rg);
    {
        void *br = make_brush(which), *p = NULL;
        if (pen)
        {
            F(GdipCreatePen2, (void *, float, int, void **), br, 12.0f, 2, &p);
            st = F(GdipDrawLineI, (void *, void *, int, int, int, int), rg, p, 0, W / 2, W, W / 2);
            F(GdipDeletePen, (void *), p);
        }
        else
            st = F(GdipFillRectangleI, (void *, void *, int, int, int, int), rg, br, 0, 0, W, W);
        F(GdipDeleteBrush, (void *), br);
    }
    F(GdipDeleteGraphics, (void *), rg);
    printf("%s_recorded=%d status %d\n", name, st == 0, st);

    played = new_bitmap(&pg);
    brush_objects = 0;
    F(GdipEnumerateMetafileDestRectI, (void *, void *, const GpRect *, EnumProc, void *, void *), pg, rec, &dest, grab, NULL, NULL);
    F(GdipDrawImageRectI, (void *, void *, int, int, int, int), pg, rec, 0, 0, W, W);
    F(GdipDeleteGraphics, (void *), pg);
    for (int y = 4; y < W - 4; y += 3)
        for (int x = 4; x < W - 4; x += 3)
        {
            int d;
            if (pen && abs(y - W / 2) > 4) continue;
            d = chan_diff(px(direct, x, y), px(played, x, y));
            sum += d; n++; if (d > worst) worst = d;
        }
    printf("%s_same=%d mean %ld worst %d direct %08lx played %08lx\n", name,
            st == 0 && n && sum / n <= 6 && worst <= 48, n ? sum / n : -1, worst,
            px(direct, W / 2 + 1, W / 2 + 1), px(played, W / 2 + 1, W / 2 + 1));
    F(GdipDisposeImage, (void *), direct);
    F(GdipDisposeImage, (void *), played);
    F(GdipDisposeImage, (void *), rec);
}

/* hand-written EMF+: a path gradient brush with a point boundary (the triangle form Windows writes for
 * GdipCreatePathGradient) and a transform, filled over the frame */
static BYTE buf[4096], body[1024];
static UINT len, blen;
static void put(const void *p, UINT n) { memcpy(body + blen, p, n); blen += n; }
static void put32(DWORD v) { put(&v, 4); }
static void putf(float v) { put(&v, 4); }
static void record(WORD type, WORD flags, const void *data, UINT size)
{
    UINT padded = (size + 3) & ~3u, total = 12 + padded;
    memcpy(buf + len, &type, 2); memcpy(buf + len + 2, &flags, 2);
    memcpy(buf + len + 4, &total, 4); memcpy(buf + len + 8, &padded, 4);
    memset(buf + len + 12, 0, padded); memcpy(buf + len + 12, data, size);
    len += total;
}

static void handwritten(void)
{
    void *bitmap, *gr;
    GpRect dest = { 0, 0, W, W };
    RECT frame;
    HDC ref, edc;
    HENHMETAFILE hemf;
    DWORD c, e;

    len = 0;
    blen = 0; put32(0xdbc01002); put32(1); put32(96); put32(96);
    record(0x4001, 0, body, blen);
    /* brush 1: Version, Type 3, flags Transform, WrapMode Clamp, CenterColor red, CenterPoint (16,16)
     * before the transform, 1 surround colour blue, BoundaryPointCount 4 + points, then the matrix 2x */
    blen = 0; put32(0xdbc01002); put32(3); put32(0x02); put32(4); put32(0xffff0000); putf(16); putf(16);
    put32(1); put32(0xff0000ff);
    put32(4); putf(0); putf(0); putf(32); putf(0); putf(32); putf(32); putf(0); putf(32);
    putf(2); putf(0); putf(0); putf(2); putf(0); putf(0);
    record(0x4008, (1 << 8) | 1, body, blen);
    /* FillRects with brush 1, one integer rect over the frame */
    blen = 0; put32(1); put32(1); { short r[4] = { 0, 0, W, W }; put(r, sizeof(r)); }
    record(0x400a, 0x4000, body, blen);
    record(0x4002, 0, NULL, 0);

    ref = GetDC(0);
    SetRect(&frame, 0, 0, MulDiv(W, 2540, GetDeviceCaps(ref, LOGPIXELSX)), MulDiv(W, 2540, GetDeviceCaps(ref, LOGPIXELSY)));
    edc = CreateEnhMetaFileW(ref, NULL, &frame, NULL);
    {
        BYTE *comment = malloc(len + 4);
        DWORD sig = 0x2b464d45;
        memcpy(comment, &sig, 4); memcpy(comment + 4, buf, len);
        GdiComment(edc, len + 4, comment);
        free(comment);
    }
    hemf = CloseEnhMetaFile(edc);
    ReleaseDC(0, ref);
    metafile = NULL;
    F(GdipCreateMetafileFromEmf, (HENHMETAFILE, BOOL, void **), hemf, TRUE, &metafile);
    bitmap = new_bitmap(&gr);
    F(GdipEnumerateMetafileDestRectI, (void *, void *, const GpRect *, EnumProc, void *, void *), gr, metafile, &dest, play, NULL, NULL);
    F(GdipDeleteGraphics, (void *), gr);
    c = px(bitmap, W / 2, W / 2) & 0xffffff;
    e = px(bitmap, 2, W / 2) & 0xffffff;
    printf("handwritten_object=%d status %d\n", brush_status == 0, brush_status);
    /* the centre (16,16)x2 = (32,32) is red; next to the edge it is near blue */
    printf("handwritten_center=%d %06lx\n", ((c >> 16) & 0xff) > 0xd0 && (c & 0xff) < 0x30, c);
    printf("handwritten_edge=%d %06lx\n", (e & 0xff) > 0xa0 && ((e >> 16) & 0xff) < 0x60, e);
    F(GdipDisposeImage, (void *), bitmap);
    F(GdipDisposeImage, (void *), metafile);
}

/* drawn directly: a path gradient's focus scales and its transform (both were ignored: "path gradient
 * focus not implemented", "path gradient transform not implemented") */
static void direct_checks(void)
{
    GpPointF sq[4] = { { 0, 0 }, { W, 0 }, { W, W }, { 0, W } }, half[4] = { { 0, 0 }, { 32, 0 }, { 32, 32 }, { 0, 32 } };
    DWORD black = 0xff000000, a, b;
    void *gr, *bm, *br = NULL;
    INT n = 1;

    bm = new_bitmap(&gr);
    F(GdipCreatePathGradient, (const GpPointF *, INT, int, void **), sq, 4, 0, &br);
    F(GdipSetPathGradientCenterColor, (void *, DWORD), br, 0xffffffff);
    F(GdipSetPathGradientSurroundColorsWithCount, (void *, const DWORD *, INT *), br, &black, &n);
    F(GdipSetPathGradientFocusScales, (void *, float, float), br, 0.5f, 0.5f);
    F(GdipFillRectangleI, (void *, void *, int, int, int, int), gr, br, 0, 0, W, W);
    F(GdipDeleteBrush, (void *), br);
    F(GdipDeleteGraphics, (void *), gr);
    /* (36,32): an eighth of the way out, inside the half-size focus: the centre colour; (8,32): three
     * quarters out, half way from the focus to the boundary: mid grey */
    a = px(bm, 36, 32) & 0xff; b = px(bm, 8, 32) & 0xff;
    printf("focus_inner=%d %02lx\n", a >= 0xf6, a);
    printf("focus_mid=%d %02lx\n", b >= 0x68 && b <= 0x98, b);
    F(GdipDisposeImage, (void *), bm);

    bm = new_bitmap(&gr);
    br = NULL; n = 1;
    F(GdipCreatePathGradient, (const GpPointF *, INT, int, void **), half, 4, 0, &br);
    F(GdipSetPathGradientCenterColor, (void *, DWORD), br, 0xffffffff);
    F(GdipSetPathGradientSurroundColorsWithCount, (void *, const DWORD *, INT *), br, &black, &n);
    F(GdipScalePathGradientTransform, (void *, float, float, int), br, 2.0f, 2.0f, 0);
    F(GdipFillRectangleI, (void *, void *, int, int, int, int), gr, br, 0, 0, W, W);
    F(GdipDeleteBrush, (void *), br);
    F(GdipDeleteGraphics, (void *), gr);
    /* the 32x32 square scaled 2x covers the frame: white at (32,32), near black at (61,32) */
    a = px(bm, 32, 32) & 0xff; b = px(bm, 61, 32) & 0xff;
    printf("transform_center=%d %02lx\n", a >= 0xf0, a);
    printf("transform_edge=%d %02lx\n", b <= 0x30, b);
    F(GdipDisposeImage, (void *), bm);
}

int main(void)
{
    ULONG_PTR token; StartupInput in = { 1 };

    g = LoadLibraryA("gdiplus.dll");
    if (F(GdiplusStartup, (ULONG_PTR *, StartupInput *, void *), &token, &in, NULL)) { printf("no_gdiplus=1\n"); return 1; }

    direct_checks();
    compare("texture", 0, FALSE);
    printf("texture_object=%d type %lu image version %#lx\n", texture_type == 2 && texture_image_version == 0xdbc01002,
            texture_type, texture_image_version);
    compare("pathgrad", 1, FALSE);
    printf("pathgrad_object=%d type %lu flags %#lx\n", path_type == 3 && (path_flags & 0x41) == 0x41, path_type, path_flags);
    compare("preset", 2, FALSE);
    compare("texpen", 0, TRUE);
    handwritten();
    printf("done=1\n");
    return 0;
}
