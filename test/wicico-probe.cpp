/* windowscodecs (patches/sg/2673): the ICO decoder accepts a file whose reserved or type field is not the usual
 * one (the Wine test records that native does). C++: mingw's wincodec.h is. */
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

#pragma pack(push, 1)
struct ICONHEADER
{
    WORD idReserved;
    WORD idType;
    WORD idCount;
};

struct ICONDIRENTRY
{
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwDIBSize;
    DWORD dwDIBOffset;
};

struct test_ico
{
    struct ICONHEADER header;
    struct ICONDIRENTRY direntry;
    BITMAPINFOHEADER bmi;
    unsigned char data[512];
};

static const struct test_ico ico_1 =
{
    /* ICONHEADER */
    {
      0, /* reserved */
      1, /* type */
      1, /* count */
    },
    /* ICONDIRENTRY */
    {
      16, /* width */
      16, /* height */
      2, /* color count */
      0, /* reserved */
      1, /* planes */
      8, /* bitcount*/
      40 + 2*4 + 16 * 16 + 16 * 4, /* data size */
      22 /* data offset */
    },
    /* BITMAPINFOHEADER */
    {
      sizeof(BITMAPINFOHEADER), /* header size */
      16, /* width */
      2*16, /* height (XOR+AND rows) */
      1, /* planes */
      8, /* bit count */
      0, /* compression */
      0, /* sizeImage */
      0, /* x pels per meter */
      0, /* y pels per meter */
      2, /* clrUsed */
      0, /* clrImportant */
    },
    {
      /* palette */
      0,0,0,0,
      0xFF,0xFF,0xFF,0,
      /* XOR mask */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
      0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
      0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
      0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,
      0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,
      0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,
      0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
      0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
      0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
      0,0,0,0,1,0,1,0,1,0,1,0,0,0,0,0,
      0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      /* AND mask */
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0
    }
};



#pragma pack(pop)

static HRESULT try_ico(IWICImagingFactory *factory, void *data, DWORD size)
{
    IWICBitmapDecoder *decoder = NULL;
    IStream *stream = NULL;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
    HRESULT hr;

    memcpy(GlobalLock(mem), data, size);
    GlobalUnlock(mem);
    CreateStreamOnHGlobal(mem, TRUE, &stream);
    hr = CoCreateInstance(CLSID_WICIcoDecoder, NULL, CLSCTX_INPROC_SERVER, IID_IWICBitmapDecoder, (void **)&decoder);
    if (SUCCEEDED(hr))
    {
        hr = decoder->Initialize(stream, WICDecodeMetadataCacheOnDemand);
        decoder->Release();
    }
    stream->Release();
    return hr;
}

int main()
{
    IWICImagingFactory *factory = NULL;
    struct test_ico ico = ico_1;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void **)&factory);
    check(hr == S_OK, "factory (%#lx)", hr);
    hr = try_ico(factory, &ico, sizeof(ico));
    check(hr == S_OK, "a regular icon (%#lx)", hr);
    ico.header.idReserved = 1;
    hr = try_ico(factory, &ico, sizeof(ico));
    check(hr == S_OK, "a reserved field of 1 (%#lx)", hr);
    ico.header.idReserved = 0;
    ico.header.idType = 100;
    hr = try_ico(factory, &ico, sizeof(ico));
    check(hr == S_OK, "a type of 100 (%#lx)", hr);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
