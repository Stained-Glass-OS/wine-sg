/* windowscodecs: results recorded from Windows by Wine's tests (patches/sg/2620):
 * the metadata query grammar (signs and wildcards in indexes, hexadecimal
 * numbers, indexes on items), readers refusing data of another format, and
 * block readers asked past their end. A GIF and a PNG are built in memory. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <propvarutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const GUID my_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID my_IID_IWICComponentFactory = {0x412d0c3a, 0x9650, 0x44fa, {0xaf, 0x5b, 0xdd, 0x2a, 0x06, 0xc8, 0xe8, 0xfb}};
static const GUID my_IID_IWICMetadataBlockReader = {0xfeaa2a8d, 0xb3f3, 0x43e4, {0xb2, 0x5c, 0xd1, 0xde, 0x99, 0x0a, 0x1a, 0xe1}};
static const GUID my_GUID_MetadataFormatApp1 = {0x8fd3dfc3, 0xf951, 0x492b, {0x81, 0x7f, 0x69, 0xc2, 0xe6, 0xd9, 0xa5, 0xb0}};
static const GUID my_GUID_ContainerFormatPng = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID my_GUID_WICPixelFormat8bppGray = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x08}};

static int failures;

static void checkv(int ok, const char *what, unsigned long got)
{
    printf("%s  %s (got %#lx)\n", ok ? "PASS" : "FAIL", what, got);
    if (!ok) failures++;
}

static IWICImagingFactory *factory;
static IWICComponentFactory *cfactory;

static const BYTE gif[] =
{
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x21, 0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
};

static IWICBitmapFrameDecode *frame_of(const BYTE *data, UINT size)
{
    IWICStream *stream = NULL;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *frame = NULL;

    IWICImagingFactory_CreateStream(factory, &stream);
    IWICStream_InitializeFromMemory(stream, (BYTE *)data, size);
    IWICImagingFactory_CreateDecoderFromStream(factory, (IStream *)stream, NULL, WICDecodeMetadataCacheOnDemand, &dec);
    if (dec) IWICBitmapDecoder_GetFrame(dec, 0, &frame);
    if (dec) IWICBitmapDecoder_Release(dec);
    IWICStream_Release(stream);
    return frame;
}

static HRESULT query(IWICMetadataQueryReader *reader, const WCHAR *q, PROPVARIANT *v)
{
    PropVariantInit(v);
    return IWICMetadataQueryReader_GetMetadataByName(reader, q, v);
}

static void test_queries(void)
{
    static const struct { const WCHAR *query; HRESULT hr; const char *what; } tests[] =
    {
        {L"/imgdesc/Left", S_OK, "a plain item"},
        {L"/[0]imgdesc/Left", S_OK, "an indexed block"},
        {L"/[+1]imgdesc/Left", WINCODEC_ERR_INVALIDQUERYCHARACTER, "a plus sign in an index"},
        {L"/[-1]imgdesc/Left", WINCODEC_ERR_INVALIDQUERYCHARACTER, "a minus sign in an index"},
        {L"/imgdesc/{uint=0x1234}", DISP_E_TYPEMISMATCH, "a hexadecimal number"},
        {L"/imgdesc/{ushort=0X10}", DISP_E_TYPEMISMATCH, "a hexadecimal number, capital X"},
        {L"/imgdesc/[0]Left", E_INVALIDARG, "an index on an item"},
        {L"/imgdesc/[*]Left", WINCODEC_ERR_REQUESTONLYVALIDATMETADATAROOT, "[*] on an item"},
        {L"/imgdesc/{ushort=0}", WINCODEC_ERR_PROPERTYNOTFOUND, "a decimal number is looked up"},
    };
    IWICBitmapFrameDecode *frame = frame_of(gif, sizeof(gif));
    IWICMetadataQueryReader *reader = NULL;
    HRESULT hr;
    unsigned int i;

    if (!frame) { checkv(0, "GIF frame", 0); return; }
    hr = IWICBitmapFrameDecode_GetMetadataQueryReader(frame, &reader);
    checkv(hr == S_OK && reader, "query reader", hr);
    if (!reader) { IWICBitmapFrameDecode_Release(frame); return; }
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    {
        PROPVARIANT v;

        hr = query(reader, tests[i].query, &v);
        checkv(hr == tests[i].hr, tests[i].what, hr);
        if (SUCCEEDED(hr)) PropVariantClear(&v);
    }
    IWICMetadataQueryReader_Release(reader);
    IWICBitmapFrameDecode_Release(frame);
}

static void test_reader_header(void)
{
    static const BYTE text[] = "tEXt chunk, not an Exif header";
    IWICStream *stream = NULL;
    IWICMetadataReader *reader = (void *)0xdeadbeef;
    HRESULT hr;

    IWICImagingFactory_CreateStream(factory, &stream);
    IWICStream_InitializeFromMemory(stream, (BYTE *)text, sizeof(text));
    hr = IWICComponentFactory_CreateMetadataReader(cfactory, &my_GUID_MetadataFormatApp1, NULL, 0, (IStream *)stream, &reader);
    checkv(hr == WINCODEC_ERR_BADMETADATAHEADER, "an App1 reader on other data", hr);
    if (SUCCEEDED(hr)) IWICMetadataReader_Release(reader);

    reader = NULL;
    IWICStream_InitializeFromMemory(stream, (BYTE *)text, sizeof(text));
    hr = IWICComponentFactory_CreateMetadataReader(cfactory, &my_GUID_MetadataFormatApp1, NULL, WICMetadataCreationFailUnknown, (IStream *)stream, &reader);
    checkv(FAILED(hr), "and when unknown readers are refused", hr);
    if (SUCCEEDED(hr)) IWICMetadataReader_Release(reader);

    /* a format nobody reads is still the unknown reader */
    {
        static const GUID nothing = {0x12345678, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};

        reader = NULL;
        IWICStream_InitializeFromMemory(stream, (BYTE *)text, sizeof(text));
        hr = IWICComponentFactory_CreateMetadataReader(cfactory, &nothing, NULL, 0, (IStream *)stream, &reader);
        checkv(hr == S_OK && reader, "an unregistered format is the unknown reader", hr);
        if (reader) IWICMetadataReader_Release(reader);
    }
    IWICStream_Release(stream);
}

static void test_block_range(void)
{
    IWICBitmapEncoder *enc = NULL;
    IWICBitmapFrameEncode *fe = NULL;
    IWICBitmapFrameDecode *frame;
    IWICBitmapDecoder *dec = NULL;
    IWICMetadataBlockReader *blocks = NULL;
    IWICMetadataReader *reader;
    IWICStream *stream = NULL;
    IStream *mem = NULL;
    GUID fmt = my_GUID_WICPixelFormat8bppGray;
    BYTE pixel = 7;
    UINT count = 99;
    HRESULT hr;

    CreateStreamOnHGlobal(NULL, TRUE, &mem);
    IWICImagingFactory_CreateEncoder(factory, &my_GUID_ContainerFormatPng, NULL, &enc);
    IWICBitmapEncoder_Initialize(enc, mem, WICBitmapEncoderNoCache);
    IWICBitmapEncoder_CreateNewFrame(enc, &fe, NULL);
    IWICBitmapFrameEncode_Initialize(fe, NULL);
    IWICBitmapFrameEncode_SetSize(fe, 1, 1);
    IWICBitmapFrameEncode_SetPixelFormat(fe, &fmt);
    IWICBitmapFrameEncode_WritePixels(fe, 1, 1, 1, &pixel);
    IWICBitmapFrameEncode_Commit(fe);
    IWICBitmapEncoder_Commit(enc);
    IWICBitmapFrameEncode_Release(fe);
    IWICBitmapEncoder_Release(enc);

    {
        LARGE_INTEGER zero = {{0}};
        IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
    }
    hr = IWICImagingFactory_CreateDecoderFromStream(factory, mem, NULL, WICDecodeMetadataCacheOnDemand, &dec);
    checkv(hr == S_OK, "decoder of the PNG", hr);
    if (!dec) goto done;
    IWICBitmapDecoder_GetFrame(dec, 0, &frame);
    hr = IWICBitmapFrameDecode_QueryInterface(frame, &my_IID_IWICMetadataBlockReader, (void **)&blocks);
    checkv(hr == S_OK, "block reader", hr);
    if (blocks)
    {
        IWICMetadataBlockReader_GetCount(blocks, &count);
        reader = (void *)0xdeadbeef;
        hr = IWICMetadataBlockReader_GetReaderByIndex(blocks, count, &reader);
        checkv(hr == WINCODEC_ERR_VALUEOUTOFRANGE, "the reader just past the last one", hr);
        hr = IWICMetadataBlockReader_GetReaderByIndex(blocks, count + 5, &reader);
        checkv(hr == WINCODEC_ERR_VALUEOUTOFRANGE, "and far past it", hr);
        IWICMetadataBlockReader_Release(blocks);
    }
    IWICBitmapFrameDecode_Release(frame);
    IWICBitmapDecoder_Release(dec);
done:
    IStream_Release(mem);
    (void)stream;
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    checkv(hr == S_OK, "WIC factory", hr);
    if (FAILED(hr)) goto done;
    hr = IWICImagingFactory_QueryInterface(factory, &my_IID_IWICComponentFactory, (void **)&cfactory);
    checkv(hr == S_OK, "component factory", hr);
    if (FAILED(hr)) goto done;

    test_queries();
    test_reader_header();
    test_block_range();
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
