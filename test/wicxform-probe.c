/* windowscodecs: IWICBitmapFlipRotator for all eight orientations, and the
 * pixel format conversions that used to answer "unimplemented" (the sources
 * and destinations listed in patches/notes/2606.txt). Everything is built from
 * memory bitmaps; the expected values are written out, not computed by the
 * code under test. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const GUID my_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};

static GUID wf(BYTE last)
{
    GUID g = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, last}};
    return g;
}
#define F_1bppIndexed 0x01
#define F_2bppIndexed 0x02
#define F_4bppIndexed 0x03
#define F_8bppIndexed 0x04
#define F_BlackWhite 0x05
#define F_2bppGray 0x06
#define F_4bppGray 0x07
#define F_8bppGray 0x08
#define F_BGR555 0x09
#define F_BGR565 0x0a
#define F_16bppGray 0x0b
#define F_24bppBGR 0x0c
#define F_24bppRGB 0x0d
#define F_32bppBGR 0x0e
#define F_32bppBGRA 0x0f
#define F_32bppPBGRA 0x10
#define F_GrayFloat 0x11
#define F_48bppRGB 0x15
#define F_64bppRGBA 0x16
#define F_128Float 0x19
/* formats outside the common GUID family */
#define F_32bppRGB 0xf1
#define F_32bppRGBA 0xf2
#define F_32bppPRGBA 0xf3
#define F_BGRA5551 0xf4

static GUID fmt(int id)
{
    GUID g;
    switch (id)
    {
    case F_32bppRGB: { GUID x = {0xd98c6b95, 0x3efe, 0x47d6, {0xbb, 0x25, 0xeb, 0x17, 0x48, 0xab, 0x0c, 0xf1}}; return x; }
    case F_32bppRGBA: { GUID x = {0xf5c7ad2d, 0x6a8d, 0x43dd, {0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9}}; return x; }
    case F_32bppPRGBA: { GUID x = {0x3cc4a650, 0xa527, 0x4d37, {0xa9, 0x16, 0x31, 0x42, 0xc7, 0xeb, 0xed, 0xba}}; return x; }
    case F_BGRA5551: { GUID x = {0x05ec7c2b, 0xf1e6, 0x4961, {0xad, 0x46, 0xe1, 0xcc, 0x81, 0x0a, 0x87, 0xd2}}; return x; }
    }
    g = wf((BYTE)id);
    return g;
}

static int bits_of(int id)
{
    switch (id)
    {
    case F_1bppIndexed: case F_BlackWhite: return 1;
    case F_2bppIndexed: case F_2bppGray: return 2;
    case F_4bppIndexed: case F_4bppGray: return 4;
    case F_8bppIndexed: case F_8bppGray: return 8;
    case F_BGR555: case F_BGR565: case F_16bppGray: case F_BGRA5551: return 16;
    case F_24bppBGR: return 24;
    case F_48bppRGB: return 48;
    case F_64bppRGBA: return 64;
    case F_128Float: return 128;
    default: return 32;
    }
}

static int failures;
static IWICImagingFactory *factory;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static IWICBitmap *make_bitmap(int id, UINT w, UINT h, const BYTE *data)
{
    IWICBitmap *bmp = NULL;
    GUID g = fmt(id);
    UINT stride = (w * bits_of(id) + 7) / 8;
    HRESULT hr = IWICImagingFactory_CreateBitmapFromMemory(factory, w, h, &g, stride, stride * h, (BYTE *)data, &bmp);

    if (FAILED(hr)) printf("note: CreateBitmapFromMemory(%#x) hr=%#lx\n", id, hr);
    return bmp;
}

/* ---- flip / rotate ---------------------------------------------------- */

struct orient
{
    const char *name;
    WICBitmapTransformOptions opt;
    UINT w, h;
    const char *expect;     /* row major, 8bppGray source 3x2 = "123456" */
};

static const struct orient orients[] = {
    {"Rotate0", WICBitmapTransformRotate0, 3, 2, "123456"},
    {"Rotate90", WICBitmapTransformRotate90, 2, 3, "415263"},
    {"Rotate180", WICBitmapTransformRotate180, 3, 2, "654321"},
    {"Rotate270", WICBitmapTransformRotate270, 2, 3, "362514"},
    {"FlipHorizontal", WICBitmapTransformFlipHorizontal, 3, 2, "321654"},
    {"FlipVertical", WICBitmapTransformFlipVertical, 3, 2, "456123"},
    {"Rotate90+FlipH", WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal, 2, 3, "142536"},
    {"Rotate90+FlipV", WICBitmapTransformRotate90 | WICBitmapTransformFlipVertical, 2, 3, "635241"},
    {"Rotate180+FlipH", WICBitmapTransformRotate180 | WICBitmapTransformFlipHorizontal, 3, 2, "456123"},
    {"Rotate180+FlipV", WICBitmapTransformRotate180 | WICBitmapTransformFlipVertical, 3, 2, "321654"},
    {"Rotate270+FlipH", WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal, 2, 3, "635241"},
    {"Rotate270+FlipV", WICBitmapTransformRotate270 | WICBitmapTransformFlipVertical, 2, 3, "142536"},
    {"FlipH+FlipV", WICBitmapTransformFlipHorizontal | WICBitmapTransformFlipVertical, 3, 2, "654321"},
};

static IWICBitmapFlipRotator *make_rotator(IWICBitmapSource *src, WICBitmapTransformOptions opt, HRESULT *phr)
{
    IWICBitmapFlipRotator *rot = NULL;
    HRESULT hr = IWICImagingFactory_CreateBitmapFlipRotator(factory, &rot);

    if (SUCCEEDED(hr)) hr = IWICBitmapFlipRotator_Initialize(rot, src, opt);
    if (phr) *phr = hr;
    if (FAILED(hr) && rot) { IWICBitmapFlipRotator_Release(rot); rot = NULL; }
    return rot;
}

/* the destination pixel (dx, dy) of an orientation, computed from first principles for
 * the byte oriented formats: rotate clockwise first, then flip the rotated image */
static void src_pos(WICBitmapTransformOptions opt, UINT sw, UINT sh, UINT dx, UINT dy, UINT *sx, UINT *sy)
{
    UINT rot = opt & 3, dw = (rot & 1) ? sh : sw, dh = (rot & 1) ? sw : sh;

    if (opt & WICBitmapTransformFlipHorizontal) dx = dw - 1 - dx;
    if (opt & WICBitmapTransformFlipVertical) dy = dh - 1 - dy;
    switch (rot)
    {
    case 0: *sx = dx; *sy = dy; break;
    case 1: *sx = dy; *sy = sh - 1 - dx; break;
    case 2: *sx = sw - 1 - dx; *sy = sh - 1 - dy; break;
    default: *sx = sw - 1 - dy; *sy = dx; break;
    }
}

static void test_orientations(void)
{
    BYTE src[6] = {1, 2, 3, 4, 5, 6};
    IWICBitmap *bmp = make_bitmap(F_8bppGray, 3, 2, src);
    UINT i;

    for (i = 0; i < sizeof(orients) / sizeof(orients[0]); i++)
    {
        const struct orient *o = &orients[i];
        IWICBitmapFlipRotator *rot = make_rotator((IWICBitmapSource *)bmp, o->opt, NULL);
        UINT w = 0, h = 0;
        BYTE out[6];
        char got[8] = "", name[96];
        HRESULT hr;
        UINT k;

        sprintf(name, "8bppGray %s size", o->name);
        if (!rot) { check(0, name); continue; }
        IWICBitmapFlipRotator_GetSize(rot, &w, &h);
        check(w == o->w && h == o->h, name);

        memset(out, 0xee, sizeof(out));
        hr = IWICBitmapFlipRotator_CopyPixels(rot, NULL, o->w, sizeof(out), out);
        for (k = 0; k < o->w * o->h && k < 7; k++) got[k] = '0' + out[k];
        got[k] = 0;
        sprintf(name, "8bppGray %s pixels (got %s hr %#lx)", o->name, got, hr);
        check(hr == S_OK && !strcmp(got, o->expect), name);
        IWICBitmapFlipRotator_Release(rot);
    }
    IWICBitmap_Release(bmp);
}

/* every orientation for 24bppBGR, 32bppBGRA and 1 bit pixels, whole image and
 * every sub-rectangle, against src_pos() */
static int get_px(int bits, const BYTE *data, UINT stride, UINT x, UINT y, BYTE *px)
{
    int n = bits / 8, i;

    if (bits >= 8)
    {
        for (i = 0; i < n; i++) px[i] = data[y * stride + x * n + i];
        return n;
    }
    if (bits == 4)
        px[0] = (data[y * stride + x / 2] >> (x % 2 ? 0 : 4)) & 15;
    else
        px[0] = (data[y * stride + x / 8] >> (7 - x % 8)) & 1;
    return 1;
}

static void test_formats_and_rects(void)
{
    static const int ids[] = {F_BlackWhite, F_4bppGray, F_8bppGray, F_16bppGray, F_24bppBGR, F_32bppBGRA, F_64bppRGBA};
    static const UINT SW = 11, SH = 5;
    UINT fi, oi;

    for (fi = 0; fi < sizeof(ids) / sizeof(ids[0]); fi++)
    {
        int bits = bits_of(ids[fi]);
        UINT sstride = (SW * bits + 7) / 8, i;
        BYTE *src = calloc(1, sstride * SH);
        IWICBitmap *bmp;
        int bad = 0, tried = 0;

        for (i = 0; i < sstride * SH; i++) src[i] = (BYTE)(i * 37 + 11);
        bmp = make_bitmap(ids[fi], SW, SH, src);

        for (oi = 0; oi < 8 * 2; oi++)
        {
            static const WICBitmapTransformOptions rots[4] = {0, 1, 2, 3};
            WICBitmapTransformOptions opt = rots[oi & 3] | ((oi & 4) ? WICBitmapTransformFlipHorizontal : 0)
                    | ((oi & 8) ? WICBitmapTransformFlipVertical : 0);
            IWICBitmapFlipRotator *rot = make_rotator((IWICBitmapSource *)bmp, opt, NULL);
            UINT dw, dh, rx, ry, rw, rh;

            if (!rot) { bad++; continue; }
            IWICBitmapFlipRotator_GetSize(rot, &dw, &dh);
            for (ry = 0; ry < dh; ry += 2)
              for (rx = 0; rx < dw; rx += 3)
                for (rw = 1; rw <= dw - rx; rw += 4)
                  for (rh = 1; rh <= dh - ry; rh += 3)
                  {
                      WICRect rc = {rx, ry, rw, rh};
                      UINT dstride = (rw * bits + 7) / 8 + 3, x, y;
                      BYTE *out = malloc(dstride * rh);
                      HRESULT hr;

                      memset(out, 0, dstride * rh);
                      hr = IWICBitmapFlipRotator_CopyPixels(rot, &rc, dstride, dstride * rh, out);
                      tried++;
                      if (FAILED(hr)) { bad++; free(out); continue; }
                      for (y = 0; y < rh; y++)
                        for (x = 0; x < rw; x++)
                        {
                            BYTE want[16], got[16];
                            UINT sx, sy;
                            int n;

                            src_pos(opt, SW, SH, rx + x, ry + y, &sx, &sy);
                            n = get_px(bits, src, sstride, sx, sy, want);
                            get_px(bits, out, dstride, x, y, got);
                            if (memcmp(want, got, n))
                            {
                                if (!bad) printf("note: first mismatch opt %#x rect %d,%d %dx%d at %u,%u\n", opt, rc.X, rc.Y, rc.Width, rc.Height, x, y);
                                bad++;
                            }
                        }
                      free(out);
                  }
            IWICBitmapFlipRotator_Release(rot);
        }
        {
            char name[96];
            sprintf(name, "%d bit pixels: 16 orientations, %d sub-rectangles, %d wrong", bits, tried, bad);
            check(bad == 0 && tried > 100, name);
        }
        IWICBitmap_Release(bmp);
        free(src);
    }
}

static void test_rotator_errors(void)
{
    BYTE src[6] = {1, 2, 3, 4, 5, 6}, out[6];
    IWICBitmap *bmp = make_bitmap(F_8bppGray, 3, 2, src);
    IWICBitmapFlipRotator *rot = NULL;
    WICRect rc;
    HRESULT hr;

    IWICImagingFactory_CreateBitmapFlipRotator(factory, &rot);
    hr = IWICBitmapFlipRotator_CopyPixels(rot, NULL, 3, 6, out);
    check(hr == WINCODEC_ERR_WRONGSTATE, "CopyPixels before Initialize is WRONGSTATE");
    IWICBitmapFlipRotator_Release(rot);

    rot = make_rotator((IWICBitmapSource *)bmp, WICBitmapTransformRotate90, NULL);
    rc.X = 0; rc.Y = 0; rc.Width = 3; rc.Height = 2;
    hr = IWICBitmapFlipRotator_CopyPixels(rot, &rc, 3, 6, out);
    check(hr == E_INVALIDARG, "rotate90: a 3x2 rectangle does not fit the 2x3 output");
    rc.X = 1; rc.Y = 1; rc.Width = 2; rc.Height = 2;
    hr = IWICBitmapFlipRotator_CopyPixels(rot, &rc, 2, 4, out);
    check(hr == E_INVALIDARG, "rotate90: rectangle past the edge");
    rc.X = 0; rc.Y = 0; rc.Width = 2; rc.Height = 3;
    hr = IWICBitmapFlipRotator_CopyPixels(rot, &rc, 1, 6, out);
    check(hr == E_INVALIDARG, "stride smaller than a row");
    hr = IWICBitmapFlipRotator_CopyPixels(rot, &rc, 2, 5, out);
    check(hr == E_INVALIDARG, "buffer smaller than the rectangle");
    hr = IWICBitmapFlipRotator_CopyPixels(rot, &rc, 2, 6, out);
    check(hr == S_OK, "exact buffer");
    rc.X = -1; rc.Y = 0; rc.Width = 2; rc.Height = 3;
    hr = IWICBitmapFlipRotator_CopyPixels(rot, &rc, 2, 6, out);
    check(hr == E_INVALIDARG, "negative X");
    hr = IWICBitmapFlipRotator_Initialize(rot, (IWICBitmapSource *)bmp, WICBitmapTransformRotate0);
    check(hr == WINCODEC_ERR_WRONGSTATE, "second Initialize is WRONGSTATE");
    IWICBitmapFlipRotator_Release(rot);
    IWICBitmap_Release(bmp);
}

/* ---- conversions ------------------------------------------------------ */

struct conv
{
    const char *name;
    int src, dst;
    UINT w;
    const BYTE *in;
    const BYTE *want;
    UINT want_len;
    WICBitmapPaletteType pal;
    int tolerance;       /* allowed difference for 16 bit results, 0 = exact */
};

#define B(...) (const BYTE[]){__VA_ARGS__}
#define W16(x) (x) & 0xff, (x) >> 8

static const struct conv convs[] = {
    /* sources the 32bppBGRA path lacked */
    {"32bppRGB -> 32bppBGRA", F_32bppRGB, F_32bppBGRA, 1, B(10, 20, 30, 0x77), B(30, 20, 10, 255), 4, WICBitmapPaletteTypeCustom, 0},
    {"32bppPRGBA -> 32bppBGRA unpremultiplies", F_32bppPRGBA, F_32bppBGRA, 1, B(50, 100, 25, 128), B(49, 199, 99, 128), 4, WICBitmapPaletteTypeCustom, 0},
    {"32bppPRGBA opaque -> 32bppBGRA", F_32bppPRGBA, F_32bppBGRA, 1, B(1, 2, 3, 255), B(3, 2, 1, 255), 4, WICBitmapPaletteTypeCustom, 0},
    {"GrayFloat 0.5 -> 32bppBGRA", F_GrayFloat, F_32bppBGRA, 1, B(0, 0, 0, 0x3f), B(188, 188, 188, 255), 4, WICBitmapPaletteTypeCustom, 0},
    {"GrayFloat 1 and 0 -> 32bppBGRA", F_GrayFloat, F_32bppBGRA, 2, B(0, 0, 0x80, 0x3f, 0, 0, 0, 0), B(255, 255, 255, 255, 0, 0, 0, 255), 8, WICBitmapPaletteTypeCustom, 0},
    /* 24 bit destinations from sources they did not handle */
    {"8bppGray -> 24bppBGR", F_8bppGray, F_24bppBGR, 2, B(100, 7), B(100, 100, 100, 7, 7, 7), 6, WICBitmapPaletteTypeCustom, 0},
    {"16bppGray -> 24bppBGR", F_16bppGray, F_24bppBGR, 1, B(0x34, 0x12), B(0x12, 0x12, 0x12), 3, WICBitmapPaletteTypeCustom, 0},
    {"4bppGray -> 24bppRGB", F_4bppGray, F_24bppRGB, 2, B(0xf0), B(255, 255, 255, 0, 0, 0), 6, WICBitmapPaletteTypeCustom, 0},
    {"BlackWhite -> 24bppRGB", F_BlackWhite, F_24bppRGB, 2, B(0x80), B(255, 255, 255, 0, 0, 0), 6, WICBitmapPaletteTypeCustom, 0},
    {"48bppRGB -> 24bppBGR", F_48bppRGB, F_24bppBGR, 1, B(W16(0x1234), W16(0x5678), W16(0x9abc)), B(0x9a, 0x56, 0x12), 3, WICBitmapPaletteTypeCustom, 0},
    {"48bppRGB -> 24bppRGB", F_48bppRGB, F_24bppRGB, 1, B(W16(0x1234), W16(0x5678), W16(0x9abc)), B(0x12, 0x56, 0x9a), 3, WICBitmapPaletteTypeCustom, 0},
    {"32bppRGBA -> 24bppRGB", F_32bppRGBA, F_24bppRGB, 1, B(1, 2, 3, 4), B(1, 2, 3), 3, WICBitmapPaletteTypeCustom, 0},
    {"32bppRGB -> 24bppBGR", F_32bppRGB, F_24bppBGR, 1, B(1, 2, 3, 0), B(3, 2, 1), 3, WICBitmapPaletteTypeCustom, 0},
    {"128bppRGBAFloat white -> 24bppRGB", F_128Float, F_24bppRGB, 1, B(0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f), B(255, 255, 255), 3, WICBitmapPaletteTypeCustom, 0},
    /* 16 bit colour destinations */
    {"32bppBGRA -> 16bppBGRA5551", F_32bppBGRA, F_BGRA5551, 1, B(10, 20, 30, 200), B(0x41, 0x8c), 2, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR -> 16bppBGRA5551 opaque", F_24bppBGR, F_BGRA5551, 1, B(10, 20, 30), B(0x41, 0x8c), 2, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR -> 16bppBGR555", F_24bppBGR, F_BGR555, 1, B(10, 20, 30), B(0x41, 0x0c), 2, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR -> 16bppBGR565", F_24bppBGR, F_BGR565, 1, B(10, 20, 30), B(0xa1, 0x18), 2, WICBitmapPaletteTypeCustom, 0},
    {"32bppBGRA -> 16bppBGR565 white", F_32bppBGRA, F_BGR565, 1, B(255, 255, 255, 0), B(0xff, 0xff), 2, WICBitmapPaletteTypeCustom, 0},
    {"16bppBGR565 -> 16bppBGR565 copy", F_BGR565, F_BGR565, 1, B(0x34, 0x12), B(0x34, 0x12), 2, WICBitmapPaletteTypeCustom, 0},
    {"16bppBGR555 -> 16bppBGR565", F_BGR555, F_BGR565, 1, B(0x1f, 0x00), B(0x1f, 0x00), 2, WICBitmapPaletteTypeCustom, 0},
    /* 64 and 48 bit */
    {"24bppRGB -> 64bppRGBA", F_24bppBGR, F_64bppRGBA, 1, B(50, 100, 200), B(W16(51400), W16(25700), W16(12850), W16(65535)), 8, WICBitmapPaletteTypeCustom, 0},
    {"32bppBGRA -> 64bppRGBA", F_32bppBGRA, F_64bppRGBA, 1, B(50, 100, 200, 128), B(W16(51400), W16(25700), W16(12850), W16(32896)), 8, WICBitmapPaletteTypeCustom, 0},
    {"8bppGray -> 64bppRGBA", F_8bppGray, F_64bppRGBA, 1, B(10), B(W16(2570), W16(2570), W16(2570), W16(65535)), 8, WICBitmapPaletteTypeCustom, 0},
    {"16bppGray -> 64bppRGBA keeps 16 bits", F_16bppGray, F_64bppRGBA, 1, B(0x34, 0x12), B(W16(0x1234), W16(0x1234), W16(0x1234), W16(65535)), 8, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR -> 48bppRGB", F_24bppBGR, F_48bppRGB, 1, B(50, 100, 200), B(W16(51400), W16(25700), W16(12850)), 6, WICBitmapPaletteTypeCustom, 0},
    {"64bppRGBA -> 48bppRGB drops alpha", F_64bppRGBA, F_48bppRGB, 1, B(W16(0x1111), W16(0x2222), W16(0x3333), W16(0x4444)), B(W16(0x1111), W16(0x2222), W16(0x3333)), 6, WICBitmapPaletteTypeCustom, 0},
    {"48bppRGB -> 48bppRGB copy", F_48bppRGB, F_48bppRGB, 1, B(W16(1), W16(2), W16(3)), B(W16(1), W16(2), W16(3)), 6, WICBitmapPaletteTypeCustom, 0},
    {"128bppRGBAFloat white -> 48bppRGB", F_128Float, F_48bppRGB, 1, B(0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f), B(W16(65535), W16(65535), W16(65535)), 6, WICBitmapPaletteTypeCustom, 0},
    /* gray destinations: the top bits of the 8 bit gray value */
    {"24bppBGR b/g/r/black -> 8bppGray", F_24bppBGR, F_8bppGray, 4, B(255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0), B(76, 220, 127, 0), 4, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR b/g/r/black -> 4bppGray", F_24bppBGR, F_4bppGray, 4, B(255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0), B(0x4d, 0x70), 2, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR b/g/r/black -> 2bppGray", F_24bppBGR, F_2bppGray, 4, B(255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0), B(0x74), 1, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR b/g/r/black -> BlackWhite", F_24bppBGR, F_BlackWhite, 4, B(255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0), B(0x40), 1, WICBitmapPaletteTypeCustom, 0},
    {"4bppGray -> 4bppGray copy", F_4bppGray, F_4bppGray, 4, B(0xf8, 0x04), B(0xf8, 0x04), 2, WICBitmapPaletteTypeCustom, 0},
    {"4bppGray -> 2bppGray", F_4bppGray, F_2bppGray, 4, B(0xf8, 0x04), B(0xe1), 1, WICBitmapPaletteTypeCustom, 0},
    {"4bppGray -> BlackWhite", F_4bppGray, F_BlackWhite, 4, B(0xf8, 0x04), B(0xc0), 1, WICBitmapPaletteTypeCustom, 0},
    {"4bppGray -> 8bppGray exact", F_4bppGray, F_8bppGray, 4, B(0xf8, 0x04), B(255, 136, 0, 68), 4, WICBitmapPaletteTypeCustom, 0},
    {"8bppGray -> 4bppGray keeps the top bits", F_8bppGray, F_4bppGray, 4, B(0x0a, 0x1a, 0xea, 0xfa), B(0x01, 0xef), 2, WICBitmapPaletteTypeCustom, 0},
    {"2bppGray -> 8bppGray exact", F_2bppGray, F_8bppGray, 4, B(0xe4), B(255, 170, 85, 0), 4, WICBitmapPaletteTypeCustom, 0},
    {"BlackWhite -> 8bppGray exact", F_BlackWhite, F_8bppGray, 4, B(0xa0), B(255, 0, 255, 0), 4, WICBitmapPaletteTypeCustom, 0},
    {"16bppGray -> 8bppGray exact", F_16bppGray, F_8bppGray, 2, B(0xcd, 0xab, 0x00, 0x01), B(0xab, 0x01), 2, WICBitmapPaletteTypeCustom, 0},
    {"128bppRGBAFloat white/black -> 8bppGray", F_128Float, F_8bppGray, 2,
        B(0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f, 0, 0, 0x80, 0x3f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x3f),
        B(255, 0), 2, WICBitmapPaletteTypeCustom, 0},
    {"8bppGray -> 16bppGray", F_8bppGray, F_16bppGray, 2, B(0, 255), B(W16(0), W16(65535)), 4, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR white/black -> 16bppGray", F_24bppBGR, F_16bppGray, 2, B(255, 255, 255, 0, 0, 0), B(W16(65535), W16(0)), 4, WICBitmapPaletteTypeCustom, 0},
    {"24bppBGR blue -> 16bppGray matches 8bit", F_24bppBGR, F_16bppGray, 1, B(255, 0, 0), B(W16(76 * 257)), 2, WICBitmapPaletteTypeCustom, 300},
    /* palette destinations */
    {"24bppRGB -> 1bppIndexed FixedBW", F_24bppBGR, F_1bppIndexed, 4, B(0, 0, 0, 255, 255, 255, 250, 250, 250, 3, 3, 3), B(0x60), 1, WICBitmapPaletteTypeFixedBW, 0},
    {"24bppRGB -> 2bppIndexed FixedGray4", F_24bppBGR, F_2bppIndexed, 4, B(0, 0, 0, 255, 255, 255, 85, 85, 85, 170, 170, 170), B(0x36), 1, WICBitmapPaletteTypeFixedGray4, 0},
    {"24bppRGB -> 4bppIndexed FixedGray16", F_24bppBGR, F_4bppIndexed, 4, B(0, 0, 0, 0x11, 0x11, 0x11, 255, 255, 255, 0x77, 0x77, 0x77), B(0x01, 0xf7), 2, WICBitmapPaletteTypeFixedGray16, 0},
    {"32bppBGRA odd width -> 4bppIndexed FixedGray16", F_32bppBGRA, F_4bppIndexed, 3, B(0x22, 0x22, 0x22, 255, 0x33, 0x33, 0x33, 255, 0xee, 0xee, 0xee, 255), B(0x23, 0xe0), 2, WICBitmapPaletteTypeFixedGray16, 0},
    {"BlackWhite -> 4bppIndexed FixedGray16", F_BlackWhite, F_4bppIndexed, 2, B(0x80), B(0xf0), 1, WICBitmapPaletteTypeFixedGray16, 0},
};

static void test_conversion(const struct conv *c)
{
    IWICBitmap *bmp = make_bitmap(c->src, c->w, 1, c->in);
    IWICFormatConverter *conv = NULL;
    GUID g = fmt(c->dst);
    BYTE out[64];
    char name[160];
    HRESULT hr;
    UINT stride = (c->w * bits_of(c->dst) + 7) / 8, i;
    int ok = 1;

    if (!bmp) { check(0, c->name); return; }
    memset(out, 0xcd, sizeof(out));
    hr = IWICImagingFactory_CreateFormatConverter(factory, &conv);
    if (SUCCEEDED(hr))
        hr = IWICFormatConverter_Initialize(conv, (IWICBitmapSource *)bmp, &g, WICBitmapDitherTypeNone, NULL, 0.0, c->pal);
    if (FAILED(hr)) { sprintf(name, "%s: Initialize hr=%#lx", c->name, hr); check(0, name); goto done; }
    hr = IWICFormatConverter_CopyPixels(conv, NULL, stride, sizeof(out), out);
    if (FAILED(hr)) { sprintf(name, "%s: CopyPixels hr=%#lx", c->name, hr); check(0, name); goto done; }

    if (c->tolerance > 0 && c->want_len == 2)
    {
        int got = out[0] | out[1] << 8, want = c->want[0] | c->want[1] << 8;
        ok = abs(got - want) <= c->tolerance;
    }
    else
        ok = !memcmp(out, c->want, c->want_len);
    if (ok && c->want_len < sizeof(out) && out[c->want_len] != 0xcd) ok = 0;
    sprintf(name, "%s", c->name);
    if (!ok)
    {
        char dump[200] = "";
        for (i = 0; i < c->want_len; i++) sprintf(dump + strlen(dump), "%02x ", out[i]);
        sprintf(name, "%s (got %s)", c->name, dump);
    }
    check(ok, name);
done:
    if (conv) IWICFormatConverter_Release(conv);
    IWICBitmap_Release(bmp);
}

static void test_rect_and_whole(void)
{
    /* the narrow destinations through a sub-rectangle */
    BYTE in[12] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0};
    IWICBitmap *bmp = make_bitmap(F_24bppBGR, 4, 1, in);
    IWICFormatConverter *conv = NULL;
    GUID g = fmt(F_8bppGray);
    WICRect rc = {1, 0, 2, 1};
    BYTE out[2] = {0xcd, 0xcd};
    HRESULT hr;

    IWICImagingFactory_CreateFormatConverter(factory, &conv);
    hr = IWICFormatConverter_Initialize(conv, (IWICBitmapSource *)bmp, &g, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    check(hr == S_OK, "24bppBGR -> 8bppGray Initialize");
    hr = IWICFormatConverter_CopyPixels(conv, &rc, 2, 2, out);
    check(hr == S_OK && out[0] == 220 && out[1] == 127, "24bppBGR -> 8bppGray sub-rectangle");
    IWICFormatConverter_Release(conv);

    g = fmt(F_4bppGray);
    conv = NULL;
    IWICImagingFactory_CreateFormatConverter(factory, &conv);
    hr = IWICFormatConverter_Initialize(conv, (IWICBitmapSource *)bmp, &g, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    rc.X = 0; rc.Width = 4;
    out[0] = out[1] = 0xcd;
    hr = IWICFormatConverter_CopyPixels(conv, &rc, 2, 2, out);
    check(hr == S_OK && out[0] == 0x4d && out[1] == 0x70, "4bppGray whole row with a rectangle");
    IWICFormatConverter_Release(conv);
    IWICBitmap_Release(bmp);
}

static void test_palette_rules(void)
{
    static const struct { int dst; WICBitmapPaletteType pal; HRESULT hr; const char *name; } t[] = {
        {F_1bppIndexed, WICBitmapPaletteTypeFixedBW, S_OK, "1bppIndexed FixedBW"},
        {F_1bppIndexed, WICBitmapPaletteTypeMedianCut, S_OK, "1bppIndexed MedianCut"},
        {F_1bppIndexed, WICBitmapPaletteTypeFixedHalftone8, E_INVALIDARG, "1bppIndexed Halftone8 has too many colours"},
        {F_1bppIndexed, WICBitmapPaletteTypeCustom, E_INVALIDARG, "1bppIndexed Custom without a palette"},
        {F_2bppIndexed, WICBitmapPaletteTypeFixedGray4, S_OK, "2bppIndexed FixedGray4"},
        {F_2bppIndexed, WICBitmapPaletteTypeFixedGray16, E_INVALIDARG, "2bppIndexed Gray16 has too many colours"},
        {F_4bppIndexed, WICBitmapPaletteTypeFixedGray16, S_OK, "4bppIndexed FixedGray16"},
        {F_4bppIndexed, WICBitmapPaletteTypeFixedHalftone256, E_INVALIDARG, "4bppIndexed Halftone256 has too many colours"},
        {F_8bppIndexed, WICBitmapPaletteTypeFixedHalftone256, S_OK, "8bppIndexed Halftone256"},
    };
    BYTE in[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    IWICBitmap *bmp = make_bitmap(F_24bppBGR, 4, 1, in);
    UINT i;

    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        IWICFormatConverter *conv = NULL;
        GUID g = fmt(t[i].dst);
        char name[100];
        HRESULT hr;

        IWICImagingFactory_CreateFormatConverter(factory, &conv);
        hr = IWICFormatConverter_Initialize(conv, (IWICBitmapSource *)bmp, &g, WICBitmapDitherTypeNone, NULL, 0.0, t[i].pal);
        sprintf(name, "%s (hr %#lx)", t[i].name, hr);
        check(hr == t[i].hr, name);
        IWICFormatConverter_Release(conv);
    }

    /* CanConvert and WICConvertBitmapSource find the new destinations */
    {
        static const int dsts[] = {F_BlackWhite, F_2bppGray, F_4bppGray, F_16bppGray, F_BGR555, F_BGR565, F_48bppRGB, F_1bppIndexed, F_4bppIndexed};
        IWICFormatConverter *conv = NULL;
        GUID s = fmt(F_32bppBGRA);
        int bad = 0;

        IWICImagingFactory_CreateFormatConverter(factory, &conv);
        for (i = 0; i < sizeof(dsts) / sizeof(dsts[0]); i++)
        {
            GUID d = fmt(dsts[i]);
            BOOL can = FALSE;
            IWICBitmapSource *out = NULL;
            HRESULT hr = IWICFormatConverter_CanConvert(conv, &s, &d, &can);

            if (FAILED(hr) || !can) bad++;
            hr = WICConvertBitmapSource(&d, (IWICBitmapSource *)bmp, &out);
            if (FAILED(hr)) bad++;
            if (out) IWICBitmapSource_Release(out);
        }
        IWICFormatConverter_Release(conv);
        check(bad == 0, "CanConvert and WICConvertBitmapSource accept the new destinations");
    }
    IWICBitmap_Release(bmp);
}

int main(void)
{
    UINT i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) { printf("FAIL  no factory %#lx\nRESULT: FAIL\n", hr); return 1; }

    test_orientations();
    test_formats_and_rects();
    test_rotator_errors();
    for (i = 0; i < sizeof(convs) / sizeof(convs[0]); i++) test_conversion(&convs[i]);
    test_rect_and_whole();
    test_palette_rules();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
