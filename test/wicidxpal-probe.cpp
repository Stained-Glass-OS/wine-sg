/* windowscodecs (patches/sg/2667): WICConvertBitmapSource from a paletted format to 8bppIndexed gives the
 * destination palette the source's colors in the order they first appear in the image (the values the Wine
 * converter tests record from native). C++: mingw's wincodec.h is. */
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

static void run(IWICImagingFactory *factory, const char *name, REFWICPixelFormatGUID fmt, UINT bpp, const BYTE *bits, UINT stride,
        const WICColor *colors, UINT ncolors, const BYTE *expect)
{
    IWICBitmap *bitmap = NULL;
    IWICPalette *palette = NULL;
    IWICBitmapSource *dst = NULL;
    BYTE out[64];
    HRESULT hr;

    hr = factory->CreateBitmapFromMemory(8, 2, fmt, stride, stride * 2, (BYTE *)bits, &bitmap);
    check(hr == S_OK && bitmap, "%s: bitmap (%#lx)", name, hr);
    if (!bitmap) return;
    factory->CreatePalette(&palette);
    palette->InitializeCustom((WICColor *)colors, ncolors);
    bitmap->SetPalette(palette);
    hr = WICConvertBitmapSource(GUID_WICPixelFormat8bppIndexed, bitmap, &dst);
    check(hr == S_OK && dst, "%s: converted (%#lx)", name, hr);
    if (dst)
    {
        WICRect rc = { 0, 0, 8, 2 };
        memset(out, 0xaa, sizeof(out));
        hr = dst->CopyPixels(&rc, 8, 16, out);
        check(hr == S_OK && !memcmp(out, expect, 16), "%s: indices %02x %02x %02x %02x %02x | %02x %02x %02x %02x %02x", name,
              out[0], out[1], out[2], out[3], out[4], out[8], out[9], out[10], out[11], out[12]);
        dst->Release();
    }
    palette->Release();
    bitmap->Release();
}

int main()
{
    IWICImagingFactory *factory = NULL;
    HRESULT hr;
    /* 2bpp: row 0 is 3 1 2 3 3 1 2 3, row 1 is 0 2 1 0 0 2 1 0 */
    static const BYTE bits2[] = { 0xdb, 0xdb, 0x24, 0x24 };
    static const WICColor pal4[4] = { 0xff000000, 0xff555555, 0xffaaaaaa, 0xffffffff };
    static const BYTE exp2[16] = { 0, 1, 2, 0, 0, 1, 2, 0, 3, 2, 1, 3, 3, 2, 1, 3 };
    /* 1bpp: 0 1 0 1 ... and 1 0 1 0 ... */
    static const BYTE bits1[] = { 0x55, 0xaa };
    static const WICColor pal2[2] = { 0xff000000, 0xffffffff };
    static const BYTE exp1[16] = { 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0 };

    CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void **)&factory);
    check(hr == S_OK && factory, "factory (%#lx)", hr);
    if (!factory) return 1;
    run(factory, "2bpp", GUID_WICPixelFormat2bppIndexed, 2, bits2, 2, pal4, 4, exp2);
    run(factory, "1bpp", GUID_WICPixelFormat1bppIndexed, 1, bits1, 1, pal2, 2, exp1);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
