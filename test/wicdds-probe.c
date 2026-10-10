/* windowscodecs (patches/sg/2643): the DDS codec as the Wine tests record Windows: the decoder has a
 * metadata query reader (without metadata), the encoder refuses an unknown cache option and then
 * answers the next Initialize with E_INVALIDARG, and SetParameters of the encoder takes block
 * compressed formats of 2D and 3D textures only. */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <dxgiformat.h>
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

DEFINE_GUID(CLSID_DdsDecoder_, 0x9053699f, 0xa341, 0x429d, 0x9e, 0x90, 0xee, 0x43, 0x7c, 0xf8, 0x0c, 0x73);
DEFINE_GUID(CLSID_DdsEncoder_, 0xa61dde94, 0x66ce, 0x4ac1, 0x88, 0x1b, 0x71, 0x68, 0x05, 0x88, 0x89, 0x5e);
DEFINE_GUID(GUID_ContainerFormatDds_, 0x9967cb95, 0x2e85, 0x4ac8, 0x8c, 0xa2, 0x83, 0xd7, 0xcc, 0xd4, 0x25, 0xc9);

#pragma pack(push, 1)
struct dds_file
{
    DWORD magic, size, flags, height, width, linear_size, depth, mips, reserved[11];
    DWORD pf_size, pf_flags, pf_fourcc, pf_bpp, pf_r, pf_g, pf_b, pf_a;
    DWORD caps, caps2, caps3, caps4, reserved2;
    BYTE block[8];
};
#pragma pack(pop)

int main(void)
{
    struct dds_file dds;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICImagingFactory *factory = NULL;
    IWICStream *stream;
    IWICMetadataQueryReader *reader = NULL;
    HRESULT hr;
    BYTE buffer[1024];

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory);
    check(hr == S_OK, "factory (%#lx)", hr);
    if (!factory) goto done;

    memset(&dds, 0, sizeof(dds));
    dds.magic = 0x20534444; dds.size = 124; dds.flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000;
    dds.height = 4; dds.width = 4; dds.linear_size = 8; dds.pf_size = 32; dds.pf_flags = 4;
    dds.pf_fourcc = MAKEFOURCC('D', 'X', 'T', '1'); dds.caps = 0x1000;

    hr = CoCreateInstance(&CLSID_DdsDecoder_, NULL, CLSCTX_INPROC_SERVER, &IID_IWICBitmapDecoder, (void **)&decoder);
    check(hr == S_OK, "DDS decoder (%#lx)", hr);
    if (decoder)
    {
        hr = IWICImagingFactory_CreateStream(factory, &stream);
        IWICStream_InitializeFromMemory(stream, (BYTE *)&dds, sizeof(dds));
        hr = IWICBitmapDecoder_Initialize(decoder, (IStream *)stream, WICDecodeMetadataCacheOnDemand);
        check(hr == S_OK, "decoder initialized (%#lx)", hr);
        hr = IWICBitmapDecoder_GetMetadataQueryReader(decoder, &reader);
        check(hr == S_OK && reader, "decoder query reader (%#lx)", hr);
        if (reader)
        {
            GUID fmt = {0};
            PROPVARIANT value;

            hr = IWICMetadataQueryReader_GetContainerFormat(reader, &fmt);
            check(hr == S_OK && IsEqualGUID(&fmt, &GUID_ContainerFormatDds_), "container format is DDS (%#lx)", hr);
            PropVariantInit(&value);
            hr = IWICMetadataQueryReader_GetMetadataByName(reader, L"/nothing", &value);
            check(FAILED(hr), "no metadata: query fails (%#lx)", hr);
            PropVariantClear(&value);
            IWICMetadataQueryReader_Release(reader);
        }
        hr = IWICBitmapDecoder_GetMetadataQueryReader(decoder, NULL);
        check(hr == E_INVALIDARG, "no pointer (%#lx)", hr);
        IWICStream_Release(stream);
        IWICBitmapDecoder_Release(decoder);
    }

    /* the encoder */
    hr = IWICImagingFactory_CreateStream(factory, &stream);
    IWICStream_InitializeFromMemory(stream, buffer, 1);
    hr = CoCreateInstance(&CLSID_DdsEncoder_, NULL, CLSCTX_INPROC_SERVER, &IID_IWICBitmapEncoder, (void **)&encoder);
    check(hr == S_OK && encoder, "DDS encoder (%#lx)", hr);
    if (encoder)
    {
        hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, 0xdeadbeef);
        check(hr == WINCODEC_ERR_UNSUPPORTEDOPERATION, "unknown cache option (%#lx)", hr);
        hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache);
        check(hr == E_INVALIDARG, "and then the next Initialize (%#lx)", hr);
        IWICBitmapEncoder_Release(encoder);
    }
    hr = CoCreateInstance(&CLSID_DdsEncoder_, NULL, CLSCTX_INPROC_SERVER, &IID_IWICBitmapEncoder, (void **)&encoder);
    if (encoder)
    {
        IWICDdsEncoder *dds_enc = NULL;
        static const struct { DXGI_FORMAT fmt; WICDdsDimension dim; HRESULT want; const char *name; } sets[] =
        {
            { DXGI_FORMAT_BC1_UNORM, WICDdsTexture2D, S_OK, "BC1 2D" },
            { DXGI_FORMAT_BC3_UNORM, WICDdsTexture3D, S_OK, "BC3 3D" },
            { DXGI_FORMAT_BC7_UNORM, WICDdsTexture2D, S_OK, "BC7 2D" },
            { DXGI_FORMAT_BC1_UNORM, WICDdsTextureCube, WINCODEC_ERR_BADHEADER, "BC1 cube" },
            { DXGI_FORMAT_B8G8R8A8_UNORM, WICDdsTexture2D, WINCODEC_ERR_BADHEADER, "B8G8R8A8" },
            { DXGI_FORMAT_R8_UNORM, WICDdsTexture2D, WINCODEC_ERR_BADHEADER, "R8" },
            { DXGI_FORMAT_UNKNOWN, WICDdsTexture2D, WINCODEC_ERR_BADHEADER, "unknown format" },
        };
        unsigned int i;

        hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache);
        check(hr == S_OK, "regular Initialize (%#lx)", hr);
        IWICBitmapEncoder_QueryInterface(encoder, &IID_IWICDdsEncoder, (void **)&dds_enc);
        for (i = 0; dds_enc && i < sizeof(sets) / sizeof(sets[0]); i++)
        {
            WICDdsParameters p = { 4, 4, 1, 1, 1, sets[i].fmt, sets[i].dim, WICDdsAlphaModeUnknown };

            hr = IWICDdsEncoder_SetParameters(dds_enc, &p);
            check(hr == sets[i].want, "SetParameters %s = %#lx (want %#lx)", sets[i].name, hr, sets[i].want);
        }
        if (dds_enc) IWICDdsEncoder_Release(dds_enc);
        IWICBitmapEncoder_Release(encoder);
    }
    IWICStream_Release(stream);
    IWICImagingFactory_Release(factory);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
