/* windowscodecs, fast mode (patch 3500): component info capabilities and MIME
 * matching, a right-to-left TGA, a planar TIFF, the metadata block
 * enumerators of a GIF, a standalone query writer, and the exports
 * WICMatchMetadataContent / WICSerializeMetadataContent.
 *
 *   fastb-wic-probe.exe */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static IWICImagingFactory *factory;

static IWICBitmapDecoder *decoder_from_bytes(const BYTE *data, UINT size)
{
    IWICStream *stream;
    IWICBitmapDecoder *decoder = NULL;

    if (FAILED(IWICImagingFactory_CreateStream(factory, &stream))) return NULL;
    IWICStream_InitializeFromMemory(stream, (BYTE *)data, size);
    if (FAILED(IWICImagingFactory_CreateDecoderFromStream(factory, (IStream *)stream, NULL,
            WICDecodeMetadataCacheOnDemand, &decoder))) decoder = NULL;
    IWICStream_Release(stream);
    return decoder;
}

static void test_info(void)
{
    IWICComponentInfo *info;
    IWICBitmapDecoderInfo *dinfo;
    IWICBitmapEncoderInfo *einfo;
    BOOL b;
    DWORD status;
    WCHAR buf[64];
    UINT len;
    HRESULT hr;

    hr = IWICImagingFactory_CreateComponentInfo(factory, &CLSID_WICPngDecoder, &info);
    check(hr == S_OK, "png decoder info");
    IWICComponentInfo_QueryInterface(info, &IID_IWICBitmapDecoderInfo, (void **)&dinfo);
    IWICComponentInfo_Release(info);

    b = TRUE; hr = IWICBitmapDecoderInfo_DoesSupportLossless(dinfo, &b);
    check(hr == S_OK && b, "png is lossless");
    b = TRUE; hr = IWICBitmapDecoderInfo_DoesSupportMultiframe(dinfo, &b);
    check(hr == S_OK && !b, "png is not multiframe");
    b = TRUE; hr = IWICBitmapDecoderInfo_DoesSupportAnimation(dinfo, &b);
    check(hr == S_OK && !b, "png has no animation");
    hr = IWICBitmapDecoderInfo_DoesSupportLossless(dinfo, NULL);
    check(hr == E_INVALIDARG, "null lossless pointer");
    b = FALSE; hr = IWICBitmapDecoderInfo_MatchesMimeType(dinfo, L"image/png", &b);
    check(hr == S_OK && b, "png matches image/png");
    b = TRUE; hr = IWICBitmapDecoderInfo_MatchesMimeType(dinfo, L"image/gif", &b);
    check(hr == S_OK && !b, "png does not match image/gif");
    b = FALSE; hr = IWICBitmapDecoderInfo_MatchesMimeType(dinfo, L"IMAGE/PNG", &b);
    check(hr == S_OK && b, "mime match ignores case");
    status = 0; hr = IWICBitmapDecoderInfo_GetSigningStatus(dinfo, &status);
    check(hr == S_OK && status == WICComponentSigned, "signing status");
    len = 0; hr = IWICBitmapDecoderInfo_GetColorManagementVersion(dinfo, 64, buf, &len);
    check(hr == S_OK && len > 1, "colour management version");
    IWICBitmapDecoderInfo_Release(dinfo);

    hr = IWICImagingFactory_CreateComponentInfo(factory, &CLSID_WICGifDecoder, &info);
    IWICComponentInfo_QueryInterface(info, &IID_IWICBitmapDecoderInfo, (void **)&dinfo);
    IWICComponentInfo_Release(info);
    b = FALSE; IWICBitmapDecoderInfo_DoesSupportAnimation(dinfo, &b);
    check(b, "gif has animation");
    b = FALSE; IWICBitmapDecoderInfo_DoesSupportMultiframe(dinfo, &b);
    check(b, "gif is multiframe");
    IWICBitmapDecoderInfo_Release(dinfo);

    hr = IWICImagingFactory_CreateComponentInfo(factory, &CLSID_WICJpegEncoder, &info);
    IWICComponentInfo_QueryInterface(info, &IID_IWICBitmapEncoderInfo, (void **)&einfo);
    IWICComponentInfo_Release(info);
    b = TRUE; hr = IWICBitmapEncoderInfo_DoesSupportLossless(einfo, &b);
    check(hr == S_OK && !b, "jpeg encoder is not lossless");
    b = FALSE; IWICBitmapEncoderInfo_MatchesMimeType(einfo, L"image/jpeg", &b);
    check(b, "jpeg encoder matches image/jpeg");
    IWICBitmapEncoderInfo_Release(einfo);
}

static void test_tga(void)
{
    /* 2x1 24-bit true colour, stored right to left: pixels (B,G,R) are given
     * as the right pixel first */
    static const BYTE tga[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 24, 0x30,
        0x10, 0x20, 0x30,   /* right pixel */
        0x01, 0x02, 0x03,   /* left pixel */
    };
    IWICBitmapDecoder *decoder = decoder_from_bytes(tga, sizeof(tga));
    IWICBitmapFrameDecode *frame;
    BYTE px[6] = {0};
    double dx = 0, dy = 0;
    HRESULT hr;

    check(decoder != NULL, "right-to-left tga decodes");
    if (!decoder) return;
    IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    hr = IWICBitmapFrameDecode_CopyPixels(frame, NULL, 6, 6, px);
    check(hr == S_OK, "tga CopyPixels");
    check(px[0] == 0x01 && px[1] == 0x02 && px[2] == 0x03 && px[3] == 0x10 && px[4] == 0x20 && px[5] == 0x30,
          "tga pixels in left-to-right order");
    hr = IWICBitmapFrameDecode_GetResolution(frame, &dx, &dy);
    check(hr == S_OK && dx == 96.0 && dy == 96.0, "tga resolution");
    IWICBitmapFrameDecode_Release(frame);
    {
        IWICPalette *palette;
        IWICImagingFactory_CreatePalette(factory, &palette);
        hr = IWICBitmapDecoder_CopyPalette(decoder, palette);
        check(hr == WINCODEC_ERR_PALETTEUNAVAILABLE, "tga decoder palette unavailable");
        IWICPalette_Release(palette);
    }
    IWICBitmapDecoder_Release(decoder);
}

static void test_tiff_planar(void)
{
    BYTE tiff[170];
    BYTE *p = tiff;
    IWICBitmapDecoder *decoder;
    IWICBitmapFrameDecode *frame;
    WICPixelFormatGUID fmt;
    BYTE px[6] = {0};
    HRESULT hr;
    int i;
    static const struct { WORD tag, type; DWORD count, value; } ent[] =
    {
        { 256, 3, 1, 2 }, { 257, 3, 1, 1 }, { 258, 3, 3, 134 }, { 259, 3, 1, 1 }, { 262, 3, 1, 2 },
        { 273, 4, 3, 140 }, { 277, 3, 1, 3 }, { 278, 3, 1, 1 }, { 279, 4, 3, 152 }, { 284, 3, 1, 2 },
    };

    memset(tiff, 0, sizeof(tiff));
    memcpy(p, "II\x2a\0\x08\0\0\0", 8); p += 8;
    *(WORD *)p = 10; p += 2;
    for (i = 0; i < 10; i++)
    {
        *(WORD *)p = ent[i].tag; *(WORD *)(p + 2) = ent[i].type;
        *(DWORD *)(p + 4) = ent[i].count;
        if (ent[i].type == 3 && ent[i].count == 1) *(WORD *)(p + 8) = ent[i].value;
        else *(DWORD *)(p + 8) = ent[i].value;
        p += 12;
    }
    p += 4; /* next IFD: none */
    *(WORD *)p = 8; *(WORD *)(p + 2) = 8; *(WORD *)(p + 4) = 8;   /* at 134 */
    *(DWORD *)(tiff + 140) = 164; *(DWORD *)(tiff + 144) = 166; *(DWORD *)(tiff + 148) = 168;
    *(DWORD *)(tiff + 152) = 2; *(DWORD *)(tiff + 156) = 2; *(DWORD *)(tiff + 160) = 2;
    tiff[164] = 0x11; tiff[165] = 0x44;   /* red plane   */
    tiff[166] = 0x22; tiff[167] = 0x55;   /* green plane */
    tiff[168] = 0x33; tiff[169] = 0x66;   /* blue plane  */

    decoder = decoder_from_bytes(tiff, sizeof(tiff));
    check(decoder != NULL, "planar tiff decodes");
    if (!decoder) return;
    IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    IWICBitmapFrameDecode_GetPixelFormat(frame, &fmt);
    hr = IWICBitmapFrameDecode_CopyPixels(frame, NULL, 6, 6, px);
    check(hr == S_OK, "planar tiff CopyPixels");
    if (IsEqualGUID(&fmt, &GUID_WICPixelFormat24bppRGB))
        check(px[0] == 0x11 && px[1] == 0x22 && px[2] == 0x33 && px[3] == 0x44 && px[4] == 0x55 && px[5] == 0x66,
              "planes woven into pixels (RGB)");
    else
        check(px[2] == 0x11 && px[1] == 0x22 && px[0] == 0x33 && px[5] == 0x44 && px[4] == 0x55 && px[3] == 0x66,
              "planes woven into pixels (BGR)");
    IWICBitmapFrameDecode_Release(frame);
    IWICBitmapDecoder_Release(decoder);
}

static void test_gif_blocks(void)
{
    static const BYTE gif[] = {
        'G','I','F','8','9','a', 1,0, 1,0, 0x80, 0, 0, 0,0,0, 255,255,255,
        0x21, 0xfe, 3, 'h','i','!', 0,                          /* comment extension */
        0x2c, 0,0,0,0, 1,0, 1,0, 0, 2, 2, 0x44, 1, 0, 0x3b };
    IWICBitmapDecoder *decoder = decoder_from_bytes(gif, sizeof(gif));
    IWICBitmapFrameDecode *frame;
    IWICMetadataBlockReader *blocks;
    IEnumUnknown *enumerator = NULL;
    IUnknown *item;
    UINT count = 0, n = 0;
    ULONG got;
    HRESULT hr;

    check(decoder != NULL, "gif decodes");
    if (!decoder) return;
    IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    hr = IWICBitmapFrameDecode_QueryInterface(frame, &IID_IWICMetadataBlockReader, (void **)&blocks);
    check(hr == S_OK, "frame block reader");
    if (hr == S_OK)
    {
        IWICMetadataBlockReader_GetCount(blocks, &count);
        hr = IWICMetadataBlockReader_GetEnumerator(blocks, &enumerator);
        check(hr == S_OK, "GetEnumerator");
        if (enumerator)
        {
            while (IEnumUnknown_Next(enumerator, 1, &item, &got) == S_OK) { n++; IUnknown_Release(item); }
            check(n == count, "enumerator yields every block");
            hr = IEnumUnknown_Reset(enumerator);
            check(hr == S_OK, "Reset");
            hr = IEnumUnknown_Skip(enumerator, count + 1);
            check(hr == S_FALSE, "Skip past the end");
            IEnumUnknown_Release(enumerator);
        }
        IWICMetadataBlockReader_Release(blocks);
    }
    {
        IWICMetadataQueryReader *qreader = NULL;
        IWICComponentFactory *cf;
        IWICMetadataQueryWriter *qwriter = NULL;

        hr = IWICBitmapFrameDecode_GetMetadataQueryReader(frame, &qreader);
        check(hr == S_OK, "frame query reader");
        if (qreader)
        {
            IWICImagingFactory_QueryInterface(factory, &IID_IWICComponentFactory, (void **)&cf);
            hr = IWICComponentFactory_CreateQueryWriterFromReader(cf, qreader, NULL, &qwriter);
            check(hr == S_OK && qwriter, "query writer from a decoder's query reader");
            if (qwriter) IWICMetadataQueryWriter_Release(qwriter);
            IWICComponentFactory_Release(cf);
            IWICMetadataQueryReader_Release(qreader);
        }
    }
    IWICBitmapFrameDecode_Release(frame);
    IWICBitmapDecoder_Release(decoder);
}

static void test_query_writer(void)
{
    IWICComponentFactory *cf;
    IWICMetadataQueryWriter *writer = NULL, *copy = NULL;
    GUID container;
    HRESULT hr;

    IWICImagingFactory_QueryInterface(factory, &IID_IWICComponentFactory, (void **)&cf);
    hr = IWICComponentFactory_CreateQueryWriter(cf, &GUID_MetadataFormatIfd, NULL, &writer);
    check(hr == S_OK, "CreateQueryWriter");
    if (hr == S_OK)
    {
        memset(&container, 0, sizeof(container));
        hr = IWICMetadataQueryWriter_GetContainerFormat(writer, &container);
        check(hr == S_OK && IsEqualGUID(&container, &GUID_MetadataFormatIfd), "query writer container");
        hr = IWICComponentFactory_CreateQueryWriterFromReader(cf, (IWICMetadataQueryReader *)writer, NULL, &copy);
        check(hr == S_OK, "CreateQueryWriterFromReader");
        if (copy) IWICMetadataQueryWriter_Release(copy);
        IWICMetadataQueryWriter_Release(writer);
    }
    hr = IWICComponentFactory_CreateQueryWriter(cf, &GUID_MetadataFormatIfd, NULL, NULL);
    check(hr == E_INVALIDARG, "null query writer pointer");
    IWICComponentFactory_Release(cf);
}

static void test_exports(void)
{
    HMODULE mod = LoadLibraryA("windowscodecs.dll");
    HRESULT (WINAPI *match)(const GUID *, const GUID *, IStream *, GUID *);
    HRESULT (WINAPI *size)(const GUID *, IWICMetadataWriter *, ULARGE_INTEGER *);
    HRESULT (WINAPI *serialize)(const GUID *, IWICMetadataWriter *, DWORD, IStream *);
    HRESULT (WINAPI *next)(IEnumString *, ULONG, LPOLESTR *, ULONG *);

    match = (void *)GetProcAddress(mod, "WICMatchMetadataContent");
    size = (void *)GetProcAddress(mod, "WICGetMetadataContentSize");
    serialize = (void *)GetProcAddress(mod, "WICSerializeMetadataContent");
    next = (void *)GetProcAddress(mod, "IEnumString_Next_WIC_Proxy");
    check(match && size && serialize && next, "exports present");
    if (match)
    {
        GUID fmt;
        HRESULT hr = match(&GUID_ContainerFormatPng, NULL, NULL, &fmt);
        check(hr == E_INVALIDARG, "WICMatchMetadataContent argument check");
    }
    if (size)
    {
        ULARGE_INTEGER sz;
        HRESULT hr = size(&GUID_ContainerFormatPng, NULL, &sz);
        check(hr == E_INVALIDARG, "WICGetMetadataContentSize argument check");
    }
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                          (void **)&factory);
    if (FAILED(hr)) { printf("FAIL  no factory %lx\n", hr); printf("RESULT: FAIL\n"); return 1; }
    test_info();
    test_tga();
    test_tiff_planar();
    test_gif_blocks();
    test_query_writer();
    test_exports();
    IWICImagingFactory_Release(factory);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
