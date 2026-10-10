/* windowscodecs: metadata writers reach the files (patches/sg/2623): PNG text, gamma,
 * chromaticity and time chunks, a JPEG Exif segment and the GIF graphic control,
 * comment and image position, through the frame's block writer and query writer;
 * the files are decoded again and the readers must show what was written. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <propvarutil.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const GUID my_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID my_IID_IWICComponentFactory = {0x412d0c3a, 0x9650, 0x44fa, {0xaf, 0x5b, 0xdd, 0x2a, 0x06, 0xc8, 0xe8, 0xfb}};
static const GUID my_IID_IWICMetadataBlockWriter = {0x08fb9676, 0xb444, 0x41e8, {0x8d, 0xbe, 0x6a, 0x53, 0xa5, 0x42, 0xbf, 0xf1}};
static const GUID my_IID_IWICMetadataBlockReader = {0xfeaa2a8d, 0xb3f3, 0x43e4, {0xb2, 0x5c, 0xd1, 0xde, 0x99, 0x0a, 0x1a, 0xe1}};
static const GUID my_IID_IWICMetadataWriter = {0xf7836e16, 0x3be0, 0x470b, {0x86, 0xbb, 0x16, 0x0d, 0x0a, 0xec, 0xd7, 0xde}};
static const GUID my_GUID_ContainerFormatPng = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID my_GUID_ContainerFormatJpeg = {0x19e4a5aa, 0x5662, 0x4fc5, {0xa0, 0xc0, 0x17, 0x58, 0x02, 0x8e, 0x10, 0x57}};
static const GUID my_GUID_ContainerFormatGif = {0x1f8a5601, 0x7d4d, 0x4cbd, {0x9c, 0x82, 0x1b, 0xc8, 0xd4, 0xee, 0xb9, 0xa5}};
static const GUID my_GUID_WICPixelFormat8bppGray = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x08}};
static const GUID my_GUID_WICPixelFormat8bppIndexed = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x04}};
static const GUID my_CLSID_Gama_writer = {0xff036d13, 0x5d4b, 0x46dd, {0xb1, 0x0f, 0x10, 0x66, 0x93, 0xd9, 0xfe, 0x4f}};
static const GUID my_CLSID_Time_writer = {0x1ab78400, 0xb5a3, 0x4d91, {0x8a, 0xce, 0x33, 0xfc, 0xd1, 0x49, 0x9b, 0xe6}};
static const GUID my_CLSID_Chrm_writer = {0xe23ce3eb, 0x5608, 0x4e83, {0xbc, 0xef, 0x27, 0xb1, 0x98, 0x7e, 0x51, 0xd7}};
static const GUID my_CLSID_Text_writer = {0xb5ebafb9, 0x253e, 0x4a72, {0xa7, 0x44, 0x07, 0x62, 0xd2, 0x68, 0x56, 0x83}};
static const GUID my_CLSID_Gce_writer = {0xaf95dc76, 0x16b2, 0x47f4, {0xb3, 0xea, 0x3c, 0x31, 0x79, 0x66, 0x93, 0xe7}};
static const GUID my_CLSID_Cmt_writer = {0xa02797fc, 0xc4ae, 0x418c, {0xaf, 0x95, 0xe6, 0x37, 0xc7, 0xea, 0xd2, 0xa1}};
static const GUID my_CLSID_Imd_writer = {0x8c89071f, 0x452e, 0x4e95, {0x96, 0x82, 0x9d, 0x10, 0x24, 0x62, 0x71, 0x72}};

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

static IWICImagingFactory *factory;

static PROPVARIANT pv_str(const WCHAR *s) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (WCHAR *)s; return v; }
static PROPVARIANT pv_ui1(BYTE x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI1; v.bVal = x; return v; }
static PROPVARIANT pv_ui2(USHORT x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI2; v.uiVal = x; return v; }
static PROPVARIANT pv_ui4(ULONG x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = x; return v; }
static PROPVARIANT pv_bool(int x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = x ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static PROPVARIANT pv_lpstr(const char *s) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (char *)s; return v; }

static IWICMetadataWriter *make(const GUID *clsid)
{
    IWICMetadataWriter *w = NULL;

    CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICMetadataWriter, (void **)&w);
    return w;
}

static void setv(IWICMetadataWriter *w, const PROPVARIANT *id, const PROPVARIANT *v)
{
    HRESULT hr = IWICMetadataWriter_SetValue(w, NULL, id, v);

    if (FAILED(hr)) check(0, "SetValue = %#lx", hr);
}

static IStream *mem;

static IWICBitmapFrameEncode *begin(const GUID *container, IWICBitmapEncoder **enc)
{
    IWICBitmapFrameEncode *frame = NULL;

    if (mem) { IStream_Release(mem); mem = NULL; }
    CreateStreamOnHGlobal(NULL, TRUE, &mem);
    IWICImagingFactory_CreateEncoder(factory, container, NULL, enc);
    IWICBitmapEncoder_Initialize(*enc, mem, WICBitmapEncoderNoCache);
    IWICBitmapEncoder_CreateNewFrame(*enc, &frame, NULL);
    IWICBitmapFrameEncode_Initialize(frame, NULL);
    return frame;
}

static IWICBitmapFrameDecode *decode(void)
{
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    LARGE_INTEGER zero = {{0}};

    IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
    IWICImagingFactory_CreateDecoderFromStream(factory, mem, NULL, WICDecodeMetadataCacheOnLoad, &dec);
    if (dec) IWICBitmapDecoder_GetFrame(dec, 0, &frame);
    if (dec) IWICBitmapDecoder_Release(dec);
    return frame;
}

static BOOL stream_contains(const char *needle, ULONG len)
{
    STATSTG st;
    LARGE_INTEGER zero = {{0}};
    BYTE *data;
    ULONG got = 0, i;
    BOOL found = FALSE;

    IStream_Stat(mem, &st, STATFLAG_NONAME);
    data = malloc(st.cbSize.LowPart);
    IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
    IStream_Read(mem, data, st.cbSize.LowPart, &got);
    for (i = 0; i + len <= got && !found; ++i)
        found = !memcmp(data + i, needle, len);
    free(data);
    return found;
}

static void test_png(void)
{
    IWICBitmapEncoder *enc;
    IWICBitmapFrameEncode *frame = begin(&my_GUID_ContainerFormatPng, &enc);
    IWICMetadataBlockWriter *bw = NULL;
    IWICMetadataWriter *text = make(&my_CLSID_Text_writer), *gama = make(&my_CLSID_Gama_writer), *time = make(&my_CLSID_Time_writer),
            *chrm = make(&my_CLSID_Chrm_writer);
    IWICMetadataQueryReader *qr = NULL;
    IWICBitmapFrameDecode *dframe;
    GUID fmt = my_GUID_WICPixelFormat8bppGray;
    BYTE pixel = 9;
    PROPVARIANT id, v, out;
    HRESULT hr;
    int i;

    IWICBitmapFrameEncode_QueryInterface(frame, &my_IID_IWICMetadataBlockWriter, (void **)&bw);

    id = pv_lpstr("Title"); v = pv_lpstr("Hello PNG"); setv(text, &id, &v);
    id = pv_str(L"ImageGamma"); v = pv_ui4(45455); setv(gama, &id, &v);
    {
        static const WCHAR *names[6] = { L"Year", L"Month", L"Day", L"Hour", L"Minute", L"Second" };
        static const BYTE vals[6] = { 0, 6, 3, 15, 7, 45 };

        for (i = 0; i < 6; ++i)
        {
            id = pv_str(names[i]);
            v = i ? pv_ui1(vals[i]) : pv_ui2(2005);
            setv(time, &id, &v);
        }
    }
    {
        static const WCHAR *names[8] = { L"WhitePointX", L"WhitePointY", L"RedX", L"RedY", L"GreenX", L"GreenY", L"BlueX", L"BlueY" };
        static const ULONG vals[8] = { 31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000 };
        for (i = 0; i < 8; ++i) { id = pv_str(names[i]); v = pv_ui4(vals[i]); setv(chrm, &id, &v); }
    }
    IWICMetadataBlockWriter_AddWriter(bw, text);
    IWICMetadataBlockWriter_AddWriter(bw, gama);
    IWICMetadataBlockWriter_AddWriter(bw, time);
    IWICMetadataBlockWriter_AddWriter(bw, chrm);

    IWICBitmapFrameEncode_SetSize(frame, 1, 1);
    IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    hr = IWICBitmapFrameEncode_WritePixels(frame, 1, 1, 1, &pixel);
    check(hr == S_OK, "PNG WritePixels = %#lx", hr);
    IWICBitmapFrameEncode_Commit(frame);
    IWICBitmapEncoder_Commit(enc);

    check(stream_contains("tEXtTitle", 9), "the file has the tEXt chunk");
    check(stream_contains("gAMA", 4), "the file has the gAMA chunk");
    check(stream_contains("tIME", 4), "the file has the tIME chunk");
    check(stream_contains("cHRM", 4), "the file has the cHRM chunk");

    dframe = decode();
    check(dframe != NULL, "the PNG decodes");
    if (dframe)
    {
        hr = IWICBitmapFrameDecode_GetMetadataQueryReader(dframe, &qr);
        check(hr == S_OK && qr, "query reader (%#lx)", hr);
        if (qr)
        {
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/tEXt/{str=Title}", &out);
            check(hr == S_OK && out.vt == VT_LPSTR && !strcmp(out.pszVal, "Hello PNG"), "text read back (%#lx)", hr);
            if (hr == S_OK) PropVariantClear(&out);
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/gAMA/ImageGamma", &out);
            check(hr == S_OK && out.vt == VT_UI4 && out.ulVal == 45455, "gamma read back (%#lx, %lu)", hr, out.ulVal);
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/tIME/Year", &out);
            check(hr == S_OK && out.vt == VT_UI2 && out.uiVal == 2005, "year read back (%#lx)", hr);
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/cHRM/BlueY", &out);
            check(hr == S_OK && out.ulVal == 6000, "chromaticity read back (%#lx, %lu)", hr, out.ulVal);
            IWICMetadataQueryReader_Release(qr);
        }
        IWICBitmapFrameDecode_Release(dframe);
    }
    IWICMetadataBlockWriter_Release(bw);
    IWICMetadataWriter_Release(text); IWICMetadataWriter_Release(gama); IWICMetadataWriter_Release(time); IWICMetadataWriter_Release(chrm);
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
}

static void test_png_query_writer(void)
{
    IWICBitmapEncoder *enc;
    IWICBitmapFrameEncode *frame = begin(&my_GUID_ContainerFormatPng, &enc);
    IWICMetadataQueryWriter *qw = NULL;
    IWICBitmapFrameDecode *dframe;
    IWICMetadataQueryReader *qr = NULL;
    GUID fmt = my_GUID_WICPixelFormat8bppGray;
    BYTE pixel = 9;
    PROPVARIANT v, out;
    HRESULT hr;

    hr = IWICBitmapFrameEncode_GetMetadataQueryWriter(frame, &qw);
    check(hr == S_OK, "PNG query writer (%#lx)", hr);
    v = pv_lpstr("A comment");
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/tEXt/{str=Comment}", &v);
    check(hr == S_OK, "set text through the query writer = %#lx", hr);
    IWICBitmapFrameEncode_SetSize(frame, 1, 1);
    IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    IWICBitmapFrameEncode_WritePixels(frame, 1, 1, 1, &pixel);
    IWICBitmapFrameEncode_Commit(frame);
    IWICBitmapEncoder_Commit(enc);
    dframe = decode();
    if (dframe)
    {
        IWICBitmapFrameDecode_GetMetadataQueryReader(dframe, &qr);
        if (qr)
        {
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/tEXt/{str=Comment}", &out);
            check(hr == S_OK && out.vt == VT_LPSTR && !strcmp(out.pszVal, "A comment"), "and read back from the file (%#lx)", hr);
            if (hr == S_OK) PropVariantClear(&out);
            IWICMetadataQueryReader_Release(qr);
        }
        IWICBitmapFrameDecode_Release(dframe);
    }
    IWICMetadataQueryWriter_Release(qw);
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
}

static void test_png_nothing(void)
{
    IWICBitmapEncoder *enc;
    IWICBitmapFrameEncode *frame = begin(&my_GUID_ContainerFormatPng, &enc);
    GUID fmt = my_GUID_WICPixelFormat8bppGray;
    BYTE pixel = 9;

    IWICBitmapFrameEncode_SetSize(frame, 1, 1);
    IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    IWICBitmapFrameEncode_WritePixels(frame, 1, 1, 1, &pixel);
    IWICBitmapFrameEncode_Commit(frame);
    IWICBitmapEncoder_Commit(enc);
    check(!stream_contains("tEXt", 4) && !stream_contains("gAMA", 4), "no metadata, no chunks");
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
}

static void test_jpeg(void)
{
    IWICBitmapEncoder *enc;
    IWICBitmapFrameEncode *frame = begin(&my_GUID_ContainerFormatJpeg, &enc);
    IWICMetadataQueryWriter *qw = NULL;
    IWICBitmapFrameDecode *dframe;
    IWICMetadataQueryReader *qr = NULL;
    GUID fmt = my_GUID_WICPixelFormat8bppGray;
    BYTE pixels[64];
    PROPVARIANT v, out;
    HRESULT hr;

    memset(pixels, 0x80, sizeof(pixels));
    hr = IWICBitmapFrameEncode_GetMetadataQueryWriter(frame, &qw);
    check(hr == S_OK && qw, "JPEG query writer (%#lx)", hr);
    v = pv_ui2(6);
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/{ushort=274}", &v);
    check(hr == S_OK, "orientation = %#lx", hr);
    v = pv_lpstr("Contoso");
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/{ushort=271}", &v);
    check(hr == S_OK, "make = %#lx", hr);
    v = pv_lpstr("2020:01:02 03:04:05");
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/exif/{ushort=36867}", &v);
    check(hr == S_OK, "date taken in the Exif directory = %#lx", hr);

    IWICBitmapFrameEncode_SetSize(frame, 8, 8);
    IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    hr = IWICBitmapFrameEncode_WritePixels(frame, 8, 8, 64, pixels);
    check(hr == S_OK, "JPEG WritePixels = %#lx", hr);
    IWICBitmapFrameEncode_Commit(frame);
    IWICBitmapEncoder_Commit(enc);
    check(stream_contains("Exif\0\0", 6), "the file has an Exif segment");

    dframe = decode();
    check(dframe != NULL, "the JPEG decodes");
    if (dframe)
    {
        IWICBitmapFrameDecode_GetMetadataQueryReader(dframe, &qr);
        if (qr)
        {
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/app1/ifd/{ushort=274}", &out);
            check(hr == S_OK && out.vt == VT_UI2 && out.uiVal == 6, "orientation read back (%#lx, vt %u)", hr, out.vt);
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/app1/ifd/{ushort=271}", &out);
            check(hr == S_OK && out.vt == VT_LPSTR && !strcmp(out.pszVal, "Contoso"), "make read back (%#lx)", hr);
            if (hr == S_OK) PropVariantClear(&out);
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/app1/ifd/exif/{ushort=36867}", &out);
            check(hr == S_OK && out.vt == VT_LPSTR && !strcmp(out.pszVal, "2020:01:02 03:04:05"), "date read back (%#lx)", hr);
            if (hr == S_OK) PropVariantClear(&out);
            IWICMetadataQueryReader_Release(qr);
        }
        IWICBitmapFrameDecode_Release(dframe);
    }
    IWICMetadataQueryWriter_Release(qw);
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
}

static void test_gif(void)
{
    IWICBitmapEncoder *enc;
    IWICBitmapFrameEncode *frame = begin(&my_GUID_ContainerFormatGif, &enc);
    IWICMetadataBlockWriter *bw = NULL;
    IWICMetadataWriter *gce = make(&my_CLSID_Gce_writer), *cmt = make(&my_CLSID_Cmt_writer), *imd = make(&my_CLSID_Imd_writer);
    IWICPalette *palette = NULL;
    IWICBitmapFrameDecode *dframe;
    IWICMetadataQueryReader *qr = NULL;
    GUID fmt = my_GUID_WICPixelFormat8bppIndexed;
    BYTE pixels[4] = { 0, 1, 2, 3 };
    PROPVARIANT id, v, out;
    HRESULT hr;

    IWICBitmapFrameEncode_QueryInterface(frame, &my_IID_IWICMetadataBlockWriter, (void **)&bw);
    id = pv_str(L"Delay"); v = pv_ui2(25); setv(gce, &id, &v);
    id = pv_str(L"Disposal"); v = pv_ui1(2); setv(gce, &id, &v);
    id = pv_str(L"TextEntry"); v = pv_lpstr("made by the probe"); setv(cmt, &id, &v);
    id = pv_str(L"Left"); v = pv_ui2(0); setv(imd, &id, &v);
    IWICMetadataBlockWriter_AddWriter(bw, gce);
    IWICMetadataBlockWriter_AddWriter(bw, cmt);
    (void)pv_bool;

    IWICImagingFactory_CreatePalette(factory, &palette);
    IWICPalette_InitializePredefined(palette, WICBitmapPaletteTypeFixedGray4, FALSE);
    IWICBitmapFrameEncode_SetSize(frame, 2, 2);
    IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    IWICBitmapFrameEncode_SetPalette(frame, palette);
    hr = IWICBitmapFrameEncode_WritePixels(frame, 2, 2, 2, pixels);
    check(hr == S_OK, "GIF WritePixels = %#lx", hr);
    hr = IWICBitmapFrameEncode_Commit(frame);
    check(hr == S_OK, "GIF frame Commit = %#lx", hr);
    IWICBitmapEncoder_Commit(enc);
    check(stream_contains("\x21\xf9\x04", 3), "the file has a graphic control extension");
    check(stream_contains("made by the probe", 17), "the file has the comment");

    dframe = decode();
    check(dframe != NULL, "the GIF decodes");
    if (dframe)
    {
        IWICBitmapFrameDecode_GetMetadataQueryReader(dframe, &qr);
        if (qr)
        {
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/grctlext/Delay", &out);
            check(hr == S_OK && out.vt == VT_UI2 && out.uiVal == 25, "delay read back (%#lx, %u)", hr, out.uiVal);
            PropVariantInit(&out);
            hr = IWICMetadataQueryReader_GetMetadataByName(qr, L"/grctlext/Disposal", &out);
            check(hr == S_OK && out.vt == VT_UI1 && out.bVal == 2, "disposal read back (%#lx)", hr);
            IWICMetadataQueryReader_Release(qr);
        }
        IWICBitmapFrameDecode_Release(dframe);
    }
    IWICPalette_Release(palette);
    IWICMetadataBlockWriter_Release(bw);
    IWICMetadataWriter_Release(gce); IWICMetadataWriter_Release(cmt); IWICMetadataWriter_Release(imd);
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    check(hr == S_OK, "WIC factory (%#lx)", hr);
    if (FAILED(hr)) goto done;

    test_png();
    test_png_query_writer();
    test_png_nothing();
    test_jpeg();
    test_gif();
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
