// SPDX-License-Identifier: LGPL-2.1-or-later
/* wicscale16 probe: IWICBitmapScaler's filtered modes for 16-bit, float and half-float channel formats
 * (wine-sg 9202). 9353 filtered formats of 8-bit channels only; 16-bit PNG/TIFF, JPEG XR and float data
 * were still nearest neighbour ("mode 3 on {...} drawn as nearest neighbour"): a one-pixel checkerboard
 * halved stayed black-or-white instead of grey. Per format: the scaled source keeps its pixel format, a
 * checkerboard halved by Fant is mid grey, black next to white enlarged by linear has greys between, an
 * HDR float value above 1.0 is kept, and straight alpha does not darken the colour. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int failures;
static IWICImagingFactory *factory;

enum { U16, F32, F16 };

static WORD to_half(float f)
{
    /* exact for the values used here: 0, 0.5, 1, 2, 4 */
    if (f == 0.0f) return 0;
    {
        int e; float m = frexpf(f, &e);
        return (WORD)(((e + 14) << 10) | ((int)(m * 2048.0f) & 0x3ff));
    }
}

static float from_half(WORD h)
{
    int e = (h >> 10) & 0x1f, m = h & 0x3ff;
    if (!e) return ldexpf((float)m, -24);
    return ldexpf((float)(m | 0x400), e - 25);
}

static void put(BYTE *p, int type, float v)
{
    if (type == U16) *(WORD *)p = (WORD)(v * 65535.0f + 0.5f);
    else if (type == F32) *(float *)p = v;
    else *(WORD *)p = to_half(v);
}

static float get(const BYTE *p, int type)
{
    if (type == U16) return *(const WORD *)p / 65535.0f;
    if (type == F32) return *(const float *)p;
    return from_half(*(const WORD *)p);
}

static void check(int ok, const char *fmt, ...)
{
    va_list a; va_start(a, fmt);
    printf(ok ? "ok    " : "FAIL  "); vprintf(fmt, a); printf("\n");
    va_end(a);
    if (!ok) failures++;
}

/* scale a sw x sh image of n channels (cb bytes each) to dw x dh; returns the pixels or NULL */
static BYTE *scale(const GUID *fmt, const BYTE *bits, UINT sw, UINT sh, UINT n, UINT cb, UINT dw, UINT dh,
        WICBitmapInterpolationMode mode, int *same_format)
{
    IWICBitmap *bmp; IWICBitmapScaler *s = NULL; HRESULT hr; GUID out;
    BYTE *o = calloc(dw * dh, n * cb);
    hr = IWICImagingFactory_CreateBitmapFromMemory(factory, sw, sh, fmt, sw * n * cb, sw * sh * n * cb, (BYTE *)bits, &bmp);
    if (FAILED(hr)) { printf("FAIL  bitmap %#lx\n", hr); failures++; free(o); return NULL; }
    IWICImagingFactory_CreateBitmapScaler(factory, &s);
    hr = IWICBitmapScaler_Initialize(s, (IWICBitmapSource *)bmp, dw, dh, mode);
    IWICBitmap_Release(bmp);
    if (FAILED(hr)) { printf("FAIL  scaler init %#lx\n", hr); failures++; IWICBitmapScaler_Release(s); free(o); return NULL; }
    IWICBitmapScaler_GetPixelFormat(s, &out);
    *same_format = IsEqualGUID(&out, fmt);
    hr = IWICBitmapScaler_CopyPixels(s, NULL, dw * n * cb, dw * dh * n * cb, o);
    IWICBitmapScaler_Release(s);
    if (FAILED(hr)) { printf("FAIL  copy %#lx\n", hr); failures++; free(o); return NULL; }
    return o;
}

static void test_format(const char *name, const GUID *fmt, UINT n, int type, int alpha)
{
    UINT cb = type == F32 ? 4 : 2, x, y, c, same = 0;
    BYTE src[8 * 8 * 16], *o;
    float lo = 1, hi = 0, v;
    int ok;

    /* a one-pixel checkerboard of 0 and 1 (alpha opaque), halved by Fant: every colour channel 0.5 */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            for (c = 0; c < n; c++)
                put(src + ((y * 8 + x) * n + c) * cb, type, (int)c == alpha ? 1.0f : (float)((x + y) & 1));
    if ((o = scale(fmt, src, 8, 8, n, cb, 4, 4, WICBitmapInterpolationModeFant, (int *)&same)))
    {
        ok = 1;
        for (x = 0; x < 16; x++)
            for (c = 0; c < n; c++)
            {
                v = get(o + (x * n + c) * cb, type);
                if (fabsf(v - ((int)c == alpha ? 1.0f : 0.5f)) > 0.02f) ok = 0;
            }
        check(ok, "%s: a checkerboard halved by Fant is mid grey (%.3f)", name, get(o, type));
        check(same, "%s: the scaler keeps the source's pixel format", name);
        free(o);
    }

    /* black next to white enlarged 2 -> 8 by linear: values strictly between 0 and 1 in the middle */
    for (x = 0; x < 2; x++)
        for (c = 0; c < n; c++)
            put(src + (x * n + c) * cb, type, (int)c == alpha ? 1.0f : (float)x);
    if ((o = scale(fmt, src, 2, 1, n, cb, 8, 1, WICBitmapInterpolationModeLinear, (int *)&same)))
    {
        int between = 0;
        for (x = 0; x < 8; x++)
        {
            v = get(o + (x * n + (alpha == 0 ? 1 : 0)) * cb, type);
            if (v > 0.05f && v < 0.95f) between++;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        check(between >= 2 && lo < 0.05f && hi > 0.95f, "%s: black/white enlarged by linear has %d greys between", name, between);
        free(o);
    }

    if (type != U16)
    {
        /* HDR: 4.0 next to 0.0 halved by Fant is 2.0, not clamped to 1.0 */
        for (x = 0; x < 2; x++)
            for (c = 0; c < n; c++)
                put(src + (x * n + c) * cb, type, (int)c == alpha ? 1.0f : x ? 4.0f : 0.0f);
        if ((o = scale(fmt, src, 2, 1, n, cb, 1, 1, WICBitmapInterpolationModeFant, (int *)&same)))
        {
            v = get(o + (alpha == 0 ? 1 : 0) * cb, type);
            check(fabsf(v - 2.0f) < 0.02f, "%s: an HDR value above 1.0 is kept (%.3f)", name, v);
            free(o);
        }
    }

    if (alpha >= 0)
    {
        /* straight alpha: opaque red next to transparent black, halved: the colour stays red, alpha 0.5 */
        for (x = 0; x < 2; x++)
            for (c = 0; c < n; c++)
                put(src + (x * n + c) * cb, type, x ? 0.0f : (c == 0 || (int)c == alpha) ? 1.0f : 0.0f);
        if ((o = scale(fmt, src, 2, 1, n, cb, 1, 1, WICBitmapInterpolationModeFant, (int *)&same)))
        {
            float r = get(o, type), a = get(o + alpha * cb, type);
            check(fabsf(r - 1.0f) < 0.02f && fabsf(a - 0.5f) < 0.02f, "%s: straight alpha keeps the colour (r %.3f a %.3f)", name, r, a);
            free(o);
        }
    }
}

int main(void)
{
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) { printf("no_factory=1\n"); return 1; }

    test_format("64bppRGBA", &GUID_WICPixelFormat64bppRGBA, 4, U16, 3);
    test_format("48bppRGB", &GUID_WICPixelFormat48bppRGB, 3, U16, -1);
    test_format("16bppGray", &GUID_WICPixelFormat16bppGray, 1, U16, -1);
    test_format("64bppPRGBA", &GUID_WICPixelFormat64bppPRGBA, 4, U16, -1);
    test_format("128bppRGBAFloat", &GUID_WICPixelFormat128bppRGBAFloat, 4, F32, 3);
    test_format("32bppGrayFloat", &GUID_WICPixelFormat32bppGrayFloat, 1, F32, -1);
    test_format("96bppRGBFloat", &GUID_WICPixelFormat96bppRGBFloat, 3, F32, -1);
    test_format("64bppRGBAHalf", &GUID_WICPixelFormat64bppRGBAHalf, 4, F16, 3);
    test_format("16bppGrayHalf", &GUID_WICPixelFormat16bppGrayHalf, 1, F16, -1);
    test_format("48bppBGR", &GUID_WICPixelFormat48bppBGR, 3, U16, -1);
    test_format("64bppBGRA", &GUID_WICPixelFormat64bppBGRA, 4, U16, 3);
    test_format("128bppRGBFloat", &GUID_WICPixelFormat128bppRGBFloat, 4, F32, -1);

    /* the native formats Windows lists have component info (they had none: CreateBitmapFromMemory E_FAIL) */
    {
        static const struct { const char *name; const GUID *fmt; UINT bpp; } infos[] = {
            { "16bppGrayHalf", &GUID_WICPixelFormat16bppGrayHalf, 16 }, { "48bppBGR", &GUID_WICPixelFormat48bppBGR, 48 },
            { "64bppRGB", &GUID_WICPixelFormat64bppRGB, 64 }, { "64bppBGRA", &GUID_WICPixelFormat64bppBGRA, 64 },
            { "64bppPBGRA", &GUID_WICPixelFormat64bppPBGRA, 64 }, { "128bppRGBFloat", &GUID_WICPixelFormat128bppRGBFloat, 128 },
        };
        for (UINT i = 0; i < ARRAYSIZE(infos); i++)
        {
            IWICComponentInfo *ci = NULL; IWICPixelFormatInfo *pf = NULL; UINT bpp = 0;
            hr = IWICImagingFactory_CreateComponentInfo(factory, infos[i].fmt, &ci);
            if (SUCCEEDED(hr) && SUCCEEDED(IWICComponentInfo_QueryInterface(ci, &IID_IWICPixelFormatInfo, (void **)&pf)))
            {
                IWICPixelFormatInfo_GetBitsPerPixel(pf, &bpp);
                IWICPixelFormatInfo_Release(pf);
            }
            if (ci) IWICComponentInfo_Release(ci);
            check(bpp == infos[i].bpp, "%s has pixel format info (%#lx, %u bpp)", infos[i].name, hr, bpp);
        }
    }

    IWICImagingFactory_Release(factory);
    printf("failures=%d\n", failures);
    printf("done=1\n");
    return 0;
}
