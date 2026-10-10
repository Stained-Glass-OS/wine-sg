/* windowscodecs (patches/sg/2644): IWICImagingFactory::CreateBitmapFromHBITMAP of an 8 bit
 * bitmap and a palette of 3 to 16 colours gives a 4 bit indexed image with the 16 colours of
 * a 4 bit palette (the Wine tests record Windows); more colours stay 8 bit. */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static HBITMAP make_dib(void)
{
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bmi;
    BYTE *bits = NULL;
    HBITMAP bmp;
    static const BYTE data[12] = { 0,1,2,0, 1,2,0,0, 2,1,0,0 };
    int i;

    memset(&bmi, 0, sizeof(bmi));
    bmi.h.biSize = sizeof(bmi.h); bmi.h.biWidth = 3; bmi.h.biHeight = -3; bmi.h.biPlanes = 1; bmi.h.biBitCount = 8;
    bmi.h.biClrUsed = 3;
    bmi.pal[0].rgbRed = 0xff; bmi.pal[1].rgbGreen = 0xff; bmi.pal[2].rgbBlue = 0xff;
    bmp = CreateDIBSection(0, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    for (i = 0; i < 12; i++) bits[i] = data[i];
    return bmp;
}

static HPALETTE make_pal(int n)
{
    char buf[sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * 255];
    LOGPALETTE *p = (LOGPALETTE *)buf;

    memset(buf, 0, sizeof(buf));
    p->palVersion = 0x300; p->palNumEntries = n;
    p->palPalEntry[0].peRed = 0xff; p->palPalEntry[1].peGreen = 0xff; p->palPalEntry[2].peBlue = 0xff;
    return CreatePalette(p);
}

int main(void)
{
    static const struct { int entries; const GUID *fmt; UINT count; const char *name; } tests[] =
    {
        { 3, &GUID_WICPixelFormat4bppIndexed, 16, "3 colours" },
        { 16, &GUID_WICPixelFormat4bppIndexed, 16, "16 colours" },
        { 17, &GUID_WICPixelFormat8bppIndexed, 17, "17 colours" },
        { 256, &GUID_WICPixelFormat8bppIndexed, 256, "256 colours" },
    };
    IWICImagingFactory *factory = NULL;
    unsigned int i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory);
    check(hr == S_OK, "factory (%#lx)", hr);
    if (!factory) goto done;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        HBITMAP bmp = make_dib();
        HPALETTE pal = make_pal(tests[i].entries);
        IWICBitmap *bitmap = NULL;
        IWICPalette *palette;
        WICPixelFormatGUID fmt;
        UINT count = 0;

        hr = IWICImagingFactory_CreateBitmapFromHBITMAP(factory, bmp, pal, WICBitmapIgnoreAlpha, &bitmap);
        check(hr == S_OK, "%s: bitmap (%#lx)", tests[i].name, hr);
        if (bitmap)
        {
            IWICBitmap_GetPixelFormat(bitmap, &fmt);
            check(IsEqualGUID(&fmt, tests[i].fmt), "%s: pixel format %s", tests[i].name,
                  IsEqualGUID(&fmt, &GUID_WICPixelFormat4bppIndexed) ? "4bpp" : IsEqualGUID(&fmt, &GUID_WICPixelFormat8bppIndexed) ? "8bpp" : "other");
            IWICImagingFactory_CreatePalette(factory, &palette);
            IWICBitmap_CopyPalette(bitmap, palette);
            IWICPalette_GetColorCount(palette, &count);
            check(count == tests[i].count, "%s: %u palette colours (want %u)", tests[i].name, count, tests[i].count);
            IWICPalette_Release(palette);
            if (tests[i].entries == 3)
            {
                BYTE px[12] = {0};

                hr = IWICBitmap_CopyPixels(bitmap, NULL, 4, sizeof(px), px);
                /* rows: 0 1 2 / 1 2 0 / 2 1 0 as nibbles */
                check(hr == S_OK && px[0] == 0x01 && px[1] == 0x20 && px[4] == 0x12 && px[5] == 0x00,
                      "3 colours: pixels are packed (%02x %02x / %02x %02x) (%#lx)", px[0], px[1], px[4], px[5], hr);
            }
            IWICBitmap_Release(bitmap);
        }
        DeleteObject(pal);
        DeleteObject(bmp);
    }
    IWICImagingFactory_Release(factory);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
