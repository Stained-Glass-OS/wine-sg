// SPDX-License-Identifier: LGPL-2.1-or-later
/* wicscale probe: IWICBitmapScaler's linear, cubic, Fant and high quality cubic modes resample (wine-sg 9353);
 * every mode was nearest neighbour ("unsupported mode 3", PDFgear's page thumbnails). A one-pixel
 * checkerboard halved is grey in the filtered modes (black or white in nearest neighbour); black next to
 * white enlarged has greys between them; a ramp reduced by Fant keeps its average; straight alpha does not
 * darken the colour. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
static IWICImagingFactory *factory;

static IWICBitmapScaler *scaler(UINT w, UINT h, const BYTE *bits, UINT sw, UINT sh, const GUID *fmt,
        WICBitmapInterpolationMode mode)
{
    IWICBitmap *bmp; IWICBitmapScaler *s = NULL; HRESULT hr;
    hr = IWICImagingFactory_CreateBitmapFromMemory(factory, sw, sh, fmt, sw * 4, sw * sh * 4, (BYTE *)bits, &bmp);
    if (FAILED(hr)) { printf("FAIL  bitmap %#lx\n", hr); failures++; return NULL; }
    IWICImagingFactory_CreateBitmapScaler(factory, &s);
    hr = IWICBitmapScaler_Initialize(s, (IWICBitmapSource *)bmp, w, h, mode);
    IWICBitmap_Release(bmp);
    if (FAILED(hr)) { printf("FAIL  scaler init %#lx\n", hr); failures++; IWICBitmapScaler_Release(s); return NULL; }
    return s;
}

static void check(int ok, const char *fmt, ...)
{
    va_list a; va_start(a, fmt);
    printf(ok ? "ok    " : "FAIL  "); vprintf(fmt, a); printf("\n");
    va_end(a);
    if (!ok) failures++;
}

int main(void)
{
    static const char *names[] = { "nearest", "linear", "cubic", "fant", "hq-cubic" };
    UINT checker[8 * 8], pair[2], ramp[256], alpha[4], out[64], x, y, m;
    IWICBitmapScaler *s;

    CoInitialize(NULL);
    CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory);
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) checker[y * 8 + x] = (x + y) & 1 ? 0xffffffff : 0xff000000;
    pair[0] = 0xff000000; pair[1] = 0xffffffff;
    for (x = 0; x < 256; x++) ramp[x] = 0xff000000 | x << 16 | x << 8 | x;

    for (m = 0; m <= 4; m++)
    {
        /* the checkerboard halved */
        if ((s = scaler(4, 4, (BYTE *)checker, 8, 8, &GUID_WICPixelFormat32bppBGR, m)))
        {
            HRESULT hr = IWICBitmapScaler_CopyPixels(s, NULL, 16, sizeof(out), (BYTE *)out);
            int g = out[5] & 0xff, grey = g > 96 && g < 160;
            check(hr == S_OK && (m == 0 ? !grey : grey), "%s: checkerboard halved, pixel (1,1) %d (%s)", names[m], g,
                  m == 0 ? "black or white" : "grey");
            /* a rectangle of the scaled image is the same pixels */
            if (m)
            {
                WICRect rc = { 1, 2, 2, 1 }; UINT part[2];
                IWICBitmapScaler_CopyPixels(s, &rc, 8, sizeof(part), (BYTE *)part);
                check(part[0] == out[2 * 4 + 1] && part[1] == out[2 * 4 + 2], "%s: a rectangle copies the same pixels", names[m]);
            }
            IWICBitmapScaler_Release(s);
        }
        /* black next to white enlarged 4 times */
        if (m && (s = scaler(8, 1, (BYTE *)pair, 2, 1, &GUID_WICPixelFormat32bppBGR, m)))
        {
            int a, b;
            IWICBitmapScaler_CopyPixels(s, NULL, 32, sizeof(out), (BYTE *)out);
            a = out[3] & 0xff; b = out[4] & 0xff;
            check(a > 16 && a < 128 && b > 128 && b < 240 && (out[0] & 0xff) < 16 && (out[7] & 0xff) > 240,
                  "%s: enlarged, greys between black and white (%d %d %d %d %d %d %d %d)", names[m],
                  out[0] & 0xff, out[1] & 0xff, out[2] & 0xff, a, b, out[5] & 0xff, out[6] & 0xff, out[7] & 0xff);
            IWICBitmapScaler_Release(s);
        }
    }
    /* a ramp 0..255 reduced to 64 by Fant: each pixel the average of four, 4 x + 1.5 */
    if ((s = scaler(64, 1, (BYTE *)ramp, 256, 1, &GUID_WICPixelFormat32bppBGR, WICBitmapInterpolationModeFant)))
    {
        int bad = 0;
        IWICBitmapScaler_CopyPixels(s, NULL, 256, sizeof(out) * 4, (BYTE *)ramp);
        for (x = 0; x < 64; x++) if (abs((int)(ramp[x] & 0xff) - (int)(4 * x + 2)) > 1) bad++;
        check(!bad, "fant: a ramp reduced keeps each run's average (%d off)", bad);
        IWICBitmapScaler_Release(s);
    }
    /* straight alpha: red opaque next to transparent black halved is red, half transparent */
    alpha[0] = 0xffff0000; alpha[1] = 0x00000000; alpha[2] = 0xffff0000; alpha[3] = 0x00000000;
    if ((s = scaler(1, 1, (BYTE *)alpha, 2, 2, &GUID_WICPixelFormat32bppBGRA, WICBitmapInterpolationModeFant)))
    {
        UINT p;
        IWICBitmapScaler_CopyPixels(s, NULL, 4, 4, (BYTE *)&p);
        check((p >> 16 & 0xff) > 250 && (p >> 24) > 120 && (p >> 24) < 136, "fant: straight alpha keeps the colour (%08x)", p);
        IWICBitmapScaler_Release(s);
    }
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
