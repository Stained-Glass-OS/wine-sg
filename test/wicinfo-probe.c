/* windowscodecs: the MIME types and file extensions of the decoders, and
 * SetPalette with a palette that was never initialized (patches/sg/2616). */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const GUID my_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID my_png = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID my_CLSID_Bmp = {0x6b462062, 0x7cbf, 0x400d, {0x9f, 0xdb, 0x81, 0x3d, 0xd1, 0x0f, 0x27, 0x78}};
static const GUID my_CLSID_Gif = {0x381dda3c, 0x9ce9, 0x4834, {0xa2, 0x3e, 0x1f, 0x98, 0xf8, 0xfc, 0x52, 0xbe}};
static const GUID my_CLSID_Ico = {0xc61bfcdf, 0x2e0f, 0x4aad, {0xa8, 0xd7, 0xe0, 0x6b, 0xaf, 0xeb, 0xcd, 0xfe}};
static const GUID my_CLSID_Jpeg = {0x9456a480, 0xe88b, 0x43ea, {0x9e, 0x73, 0x0b, 0x2d, 0x9b, 0x71, 0xb1, 0xca}};
static const GUID my_CLSID_Png = {0x389ea17b, 0x5078, 0x4cde, {0xb6, 0xef, 0x25, 0xc1, 0x51, 0x75, 0xc7, 0x51}};
static const GUID my_CLSID_Tiff = {0xb54e85d9, 0xfe23, 0x499f, {0x8b, 0x88, 0x6a, 0xce, 0xa7, 0x13, 0x75, 0x2b}};

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

static const struct { const char *name; const GUID *clsid; const WCHAR *mime; const WCHAR *ext; } decoders[] = {
    {"bmp", &my_CLSID_Bmp, L"image/bmp", L".bmp,.dib,.rle"},
    {"gif", &my_CLSID_Gif, L"image/gif", L".gif"},
    {"ico", &my_CLSID_Ico, L"image/ico,image/x-icon", L".ico,.icon"},
    {"jpeg", &my_CLSID_Jpeg, L"image/jpeg,image/jpe,image/jpg", L".jpeg,.jpe,.jpg,.jfif,.exif"},
    {"png", &my_CLSID_Png, L"image/png", L".png"},
    {"tiff", &my_CLSID_Tiff, L"image/tiff,image/tif", L".tiff,.tif"},
};

int main(void)
{
    IWICImagingFactory *factory = NULL;
    unsigned i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) { printf("FAIL  no factory %#lx\nRESULT: FAIL\n", hr); return 1; }

    for (i = 0; i < sizeof(decoders) / sizeof(decoders[0]); i++)
    {
        IWICComponentInfo *info = NULL;
        IWICBitmapDecoderInfo *dinfo = NULL;
        WCHAR value[256] = {0};
        UINT len = 0;

        hr = IWICImagingFactory_CreateComponentInfo(factory, decoders[i].clsid, &info);
        check(hr == S_OK && info, "%s: component info (%#lx)", decoders[i].name, hr);
        if (!info) continue;
        hr = IWICComponentInfo_QueryInterface(info, &IID_IWICBitmapDecoderInfo, (void **)&dinfo);
        check(hr == S_OK, "%s: decoder info (%#lx)", decoders[i].name, hr);
        if (dinfo)
        {
            hr = IWICBitmapDecoderInfo_GetMimeTypes(dinfo, 256, value, &len);
            check(hr == S_OK && !wcscmp(value, decoders[i].mime) && len == wcslen(decoders[i].mime) + 1, "%s: mime types \"%ls\"", decoders[i].name, value);
            len = 0;
            memset(value, 0, sizeof(value));
            hr = IWICBitmapDecoderInfo_GetFileExtensions(dinfo, 256, value, &len);
            check(hr == S_OK && !wcscmp(value, decoders[i].ext) && len == wcslen(decoders[i].ext) + 1, "%s: file extensions \"%ls\"", decoders[i].name, value);
            IWICBitmapDecoderInfo_Release(dinfo);
        }
        IWICComponentInfo_Release(info);
    }

    /* SetPalette */
    {
        IWICBitmapEncoder *encoder = NULL;
        IWICBitmapFrameEncode *frame = NULL;
        IWICPalette *palette = NULL;
        IStream *stream = NULL;
        WICColor colors[4] = {0xff000000, 0xffff0000, 0xff00ff00, 0xff0000ff};

        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        IWICImagingFactory_CreateEncoder(factory, &my_png, NULL, &encoder);
        IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
        IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
        IWICImagingFactory_CreatePalette(factory, &palette);

        hr = IWICBitmapFrameEncode_SetPalette(frame, NULL);
        check(hr == E_INVALIDARG, "SetPalette(NULL) = %#lx", hr);
        hr = IWICBitmapFrameEncode_SetPalette(frame, palette);
        check(hr == WINCODEC_ERR_NOTINITIALIZED, "SetPalette before Initialize = %#lx", hr);
        hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
        check(hr == S_OK, "frame Initialize = %#lx", hr);
        hr = IWICBitmapFrameEncode_SetPalette(frame, palette);
        check(hr == WINCODEC_ERR_NOTINITIALIZED, "SetPalette with an uninitialized palette = %#lx", hr);
        hr = IWICPalette_InitializeCustom(palette, colors, 4);
        check(hr == S_OK, "palette with 4 colours = %#lx", hr);
        hr = IWICBitmapFrameEncode_SetPalette(frame, palette);
        check(hr == S_OK, "SetPalette with colours = %#lx", hr);

        IWICPalette_Release(palette);
        IWICBitmapFrameEncode_Release(frame);
        IWICBitmapEncoder_Release(encoder);
        IStream_Release(stream);
    }

    IWICImagingFactory_Release(factory);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
