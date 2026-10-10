/* windowscodecs: the metadata block writer of encoder frames and the metadata query
 * writer (patches/sg/2622): adding, setting, removing and reading writers, making
 * blocks from a block reader, and SetMetadataByName / RemoveMetadataByName creating
 * the blocks on the way. */
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
static const GUID my_IID_IWICMetadataQueryWriter = {0xa721791a, 0x0def, 0x4d06, {0xbd, 0x91, 0x21, 0x18, 0xbf, 0x1d, 0xb1, 0x0b}};
static const GUID my_IID_IWICMetadataReader = {0x9204fe99, 0xd8fc, 0x4fd5, {0xa0, 0x01, 0x95, 0x36, 0xb0, 0x67, 0xa8, 0x99}};
static const GUID my_IID_IWICMetadataWriter = {0xf7836e16, 0x3be0, 0x470b, {0x86, 0xbb, 0x16, 0x0d, 0x0a, 0xec, 0xd7, 0xde}};
static const GUID my_IID_IWICPersistStream = {0x00675040, 0x6908, 0x45f8, {0x86, 0xa3, 0x49, 0xc7, 0xdf, 0xd6, 0xd9, 0xad}};
static const GUID my_GUID_ContainerFormatJpeg = {0x19e4a5aa, 0x5662, 0x4fc5, {0xa0, 0xc0, 0x17, 0x58, 0x02, 0x8e, 0x10, 0x57}};
static const GUID my_GUID_ContainerFormatGif = {0x1f8a5601, 0x7d4d, 0x4cbd, {0x9c, 0x82, 0x1b, 0xc8, 0xd4, 0xee, 0xb9, 0xa5}};
static const GUID my_GUID_MetadataFormatIfd = {0x537396c6, 0x2d8a, 0x4bb6, {0x9b, 0xf8, 0x2f, 0x0a, 0x8e, 0x2a, 0x3a, 0xdf}};
static const GUID my_GUID_MetadataFormatExif = {0x1c3c4f9d, 0xb84a, 0x467d, {0x94, 0x93, 0x36, 0xcf, 0xbd, 0x59, 0xea, 0x57}};
static const GUID my_GUID_MetadataFormatApp1 = {0x8fd3dfc3, 0xf951, 0x492b, {0x81, 0x7f, 0x69, 0xc2, 0xe6, 0xd9, 0xa5, 0xb0}};
static const GUID my_GUID_MetadataFormatIMD = {0xbd2bb086, 0x4d52, 0x48dd, {0x96, 0x77, 0xdb, 0x48, 0x3e, 0x85, 0xae, 0x8f}};
static const GUID my_GUID_MetadataFormatGCE = {0x2a25cad8, 0xdeeb, 0x4c69, {0xa7, 0x88, 0x0e, 0xc2, 0x26, 0x6d, 0xca, 0xfd}};
static const GUID my_CLSID_Ifd_writer = {0xb1ebfc28, 0xc9bd, 0x47a2, {0x8d, 0x33, 0xb9, 0x48, 0x76, 0x97, 0x77, 0xa7}};
static const GUID my_CLSID_Exif_writer = {0xc9a14cda, 0xc339, 0x460b, {0x90, 0x78, 0xd4, 0xde, 0xbc, 0xfa, 0xbe, 0x91}};

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
static IWICComponentFactory *cfactory;

static const BYTE gif[] =
{
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x21, 0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
};

static IWICBitmapFrameEncode *new_frame(const GUID *container, IWICBitmapEncoder **enc_out, IStream **stream_out)
{
    IWICBitmapEncoder *enc = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IStream *stream = NULL;

    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IWICImagingFactory_CreateEncoder(factory, container, NULL, &enc);
    IWICBitmapEncoder_Initialize(enc, stream, WICBitmapEncoderNoCache);
    IWICBitmapEncoder_CreateNewFrame(enc, &frame, NULL);
    IWICBitmapFrameEncode_Initialize(frame, NULL);
    *enc_out = enc;
    *stream_out = stream;
    return frame;
}

static void format_is(IWICMetadataReader *r, const GUID *want, const char *what)
{
    GUID got;
    HRESULT hr = IWICMetadataReader_GetMetadataFormat(r, &got);

    check(hr == S_OK && IsEqualGUID(&got, want), "%s", what);
}

static void test_block_writer(const GUID *container, const char *name)
{
    IWICBitmapEncoder *enc;
    IStream *stream;
    IWICBitmapFrameEncode *frame = new_frame(container, &enc, &stream);
    IWICMetadataBlockWriter *bw = NULL;
    IWICMetadataWriter *ifd = NULL, *exif = NULL, *got = NULL;
    IWICMetadataReader *reader = NULL;
    IEnumUnknown *en = NULL;
    IUnknown *unk;
    ULONG fetched;
    GUID format;
    UINT count;
    HRESULT hr;

    hr = IWICBitmapFrameEncode_QueryInterface(frame, &my_IID_IWICMetadataBlockWriter, (void **)&bw);
    check(hr == S_OK && bw, "%s: block writer (%#lx)", name, hr);
    if (!bw) goto done;

    hr = IWICMetadataBlockWriter_GetContainerFormat(bw, &format);
    check(hr == S_OK && IsEqualGUID(&format, container), "%s: container format (%#lx)", name, hr);
    hr = IWICMetadataBlockWriter_GetContainerFormat(bw, NULL);
    check(hr == E_INVALIDARG, "%s: container format to NULL = %#lx", name, hr);
    hr = IWICMetadataBlockWriter_GetCount(bw, NULL);
    check(hr == E_INVALIDARG, "%s: count to NULL = %#lx", name, hr);
    count = 99;
    hr = IWICMetadataBlockWriter_GetCount(bw, &count);
    check(hr == S_OK && count == 0, "%s: empty (%#lx, %u)", name, hr, count);
    hr = IWICMetadataBlockWriter_GetWriterByIndex(bw, 0, &got);
    check(hr == WINCODEC_ERR_VALUEOUTOFRANGE, "%s: no writer 0 = %#lx", name, hr);
    hr = IWICMetadataBlockWriter_GetReaderByIndex(bw, 0, &reader);
    check(hr == WINCODEC_ERR_VALUEOUTOFRANGE, "%s: no reader 0 = %#lx", name, hr);

    CoCreateInstance(&my_CLSID_Ifd_writer, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICMetadataWriter, (void **)&ifd);
    CoCreateInstance(&my_CLSID_Exif_writer, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICMetadataWriter, (void **)&exif);

    hr = IWICMetadataBlockWriter_AddWriter(bw, ifd);
    check(hr == S_OK, "%s: AddWriter = %#lx", name, hr);
    IWICMetadataBlockWriter_GetCount(bw, &count);
    check(count == 1, "%s: one writer", name);
    hr = IWICMetadataBlockWriter_GetWriterByIndex(bw, 0, &got);
    check(hr == S_OK && got == ifd, "%s: the same writer (%#lx)", name, hr);
    if (got) IWICMetadataWriter_Release(got);
    hr = IWICMetadataBlockWriter_GetReaderByIndex(bw, 0, &reader);
    check(hr == S_OK && reader, "%s: reader of the writer (%#lx)", name, hr);
    if (reader) { format_is(reader, &my_GUID_MetadataFormatIfd, "and its format"); IWICMetadataReader_Release(reader); }

    hr = IWICMetadataBlockWriter_SetWriterByIndex(bw, 0, exif);
    check(hr == S_OK, "%s: SetWriterByIndex = %#lx", name, hr);
    IWICMetadataBlockWriter_GetReaderByIndex(bw, 0, &reader);
    if (reader) { format_is(reader, &my_GUID_MetadataFormatExif, "replaced by the Exif writer"); IWICMetadataReader_Release(reader); }
    hr = IWICMetadataBlockWriter_SetWriterByIndex(bw, 3, exif);
    check(hr == WINCODEC_ERR_VALUEOUTOFRANGE, "%s: SetWriterByIndex past the end = %#lx", name, hr);
    hr = IWICMetadataBlockWriter_AddWriter(bw, ifd);

    hr = IWICMetadataBlockWriter_GetEnumerator(bw, &en);
    check(hr == S_OK && en, "%s: enumerator (%#lx)", name, hr);
    if (en)
    {
        fetched = 0;
        hr = IEnumUnknown_Next(en, 1, &unk, &fetched);
        check(hr == S_OK && fetched == 1, "%s: first of the enumeration (%#lx)", name, hr);
        if (fetched)
        {
            hr = IUnknown_QueryInterface(unk, &my_IID_IWICMetadataReader, (void **)&reader);
            check(hr == S_OK, "%s: it is a reader", name);
            if (reader) { format_is(reader, &my_GUID_MetadataFormatExif, "of the first writer"); IWICMetadataReader_Release(reader); }
            IUnknown_Release(unk);
        }
        hr = IEnumUnknown_Skip(en, 1);
        fetched = 7;
        hr = IEnumUnknown_Next(en, 1, &unk, &fetched);
        check(hr == S_FALSE && fetched == 0, "%s: nothing after the second (%#lx)", name, hr);
        IEnumUnknown_Reset(en);
        hr = IEnumUnknown_Next(en, 1, &unk, &fetched);
        check(hr == S_OK && fetched == 1, "%s: Reset starts again", name);
        if (fetched) IUnknown_Release(unk);
        IEnumUnknown_Release(en);
    }

    hr = IWICMetadataBlockWriter_RemoveWriterByIndex(bw, 9);
    check(hr == WINCODEC_ERR_VALUEOUTOFRANGE, "%s: remove past the end = %#lx", name, hr);
    hr = IWICMetadataBlockWriter_RemoveWriterByIndex(bw, 0);
    check(hr == S_OK, "%s: remove = %#lx", name, hr);
    IWICMetadataBlockWriter_GetCount(bw, &count);
    check(count == 1, "%s: one left", name);
    IWICMetadataBlockWriter_GetReaderByIndex(bw, 0, &reader);
    if (reader) { format_is(reader, &my_GUID_MetadataFormatIfd, "the Ifd one"); IWICMetadataReader_Release(reader); }

    if (ifd) IWICMetadataWriter_Release(ifd);
    if (exif) IWICMetadataWriter_Release(exif);
    IWICMetadataBlockWriter_Release(bw);
done:
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
    IStream_Release(stream);
}

static void test_from_reader(void)
{
    IWICBitmapEncoder *enc;
    IStream *stream;
    IWICBitmapFrameEncode *frame = new_frame(&my_GUID_ContainerFormatGif, &enc, &stream);
    IWICMetadataBlockWriter *bw = NULL;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *dframe = NULL;
    IWICMetadataBlockReader *br = NULL;
    IWICStream *gif_stream = NULL;
    IWICMetadataReader *reader = NULL;
    UINT count = 0, rcount = 0;
    HRESULT hr;

    IWICBitmapFrameEncode_QueryInterface(frame, &my_IID_IWICMetadataBlockWriter, (void **)&bw);
    IWICImagingFactory_CreateStream(factory, &gif_stream);
    IWICStream_InitializeFromMemory(gif_stream, (BYTE *)gif, sizeof(gif));
    IWICImagingFactory_CreateDecoderFromStream(factory, (IStream *)gif_stream, NULL, WICDecodeMetadataCacheOnDemand, &dec);
    IWICBitmapDecoder_GetFrame(dec, 0, &dframe);
    IWICBitmapFrameDecode_QueryInterface(dframe, &my_IID_IWICMetadataBlockReader, (void **)&br);
    IWICMetadataBlockReader_GetCount(br, &rcount);

    hr = IWICMetadataBlockWriter_InitializeFromBlockReader(bw, br);
    check(hr == S_OK, "InitializeFromBlockReader = %#lx", hr);
    IWICMetadataBlockWriter_GetCount(bw, &count);
    check(count == rcount && count >= 1, "as many writers as readers (%u, %u)", count, rcount);
    IWICMetadataBlockWriter_GetReaderByIndex(bw, 0, &reader);
    if (reader) { format_is(reader, &my_GUID_MetadataFormatIMD, "the first is the image descriptor"); IWICMetadataReader_Release(reader); }
    hr = IWICMetadataBlockWriter_InitializeFromBlockReader(bw, NULL);
    check(hr == E_INVALIDARG, "no reader = %#lx", hr);
    IWICMetadataBlockWriter_GetCount(bw, &count);
    check(count == rcount, "and nothing changes");

    IWICMetadataBlockReader_Release(br);
    IWICBitmapFrameDecode_Release(dframe);
    IWICBitmapDecoder_Release(dec);
    IWICStream_Release(gif_stream);
    IWICMetadataBlockWriter_Release(bw);
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
    IStream_Release(stream);
}

static void test_query_writer(void)
{
    IWICBitmapEncoder *enc;
    IStream *stream;
    IWICBitmapFrameEncode *frame = new_frame(&my_GUID_ContainerFormatJpeg, &enc, &stream);
    IWICMetadataQueryWriter *qw = NULL;
    IWICMetadataBlockWriter *bw = NULL;
    PROPVARIANT v, out;
    UINT count = 0;
    HRESULT hr;

    hr = IWICBitmapFrameEncode_GetMetadataQueryWriter(frame, &qw);
    check(hr == S_OK && qw, "query writer (%#lx)", hr);
    if (!qw) goto done;
    IWICBitmapFrameEncode_QueryInterface(frame, &my_IID_IWICMetadataBlockWriter, (void **)&bw);

    PropVariantInit(&out);
    hr = IWICMetadataQueryWriter_GetMetadataByName(qw, L"/app1/ifd/{ushort=274}", &out);
    check(hr == WINCODEC_ERR_PROPERTYNOTFOUND, "nothing there yet = %#lx", hr);

    PropVariantInit(&v);
    v.vt = VT_UI2; v.uiVal = 6;
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/{ushort=274}", &v);
    check(hr == S_OK, "set the orientation, making app1 and ifd = %#lx", hr);
    IWICMetadataBlockWriter_GetCount(bw, &count);
    check(count == 1, "one block writer was made (%u)", count);

    PropVariantInit(&out);
    hr = IWICMetadataQueryWriter_GetMetadataByName(qw, L"/app1/ifd/{ushort=274}", &out);
    check(hr == S_OK && out.vt == VT_UI2 && out.uiVal == 6, "read it back (%#lx, vt %u)", hr, out.vt);
    PropVariantClear(&out);

    v.uiVal = 8;
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/{ushort=274}", &v);
    check(hr == S_OK, "set it again = %#lx", hr);
    PropVariantInit(&out);
    hr = IWICMetadataQueryWriter_GetMetadataByName(qw, L"/app1/ifd/{ushort=274}", &out);
    check(hr == S_OK && out.uiVal == 8, "changed (%#lx)", hr);
    PropVariantClear(&out);
    IWICMetadataBlockWriter_GetCount(bw, &count);
    check(count == 1, "still one block writer (%u)", count);

    /* a second item in the same directory, with a string */
    {
        char text[] = "Fabrikam";

        PropVariantInit(&v);
        v.vt = VT_LPSTR; v.pszVal = text;
        hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/{ushort=271}", &v);
        check(hr == S_OK, "set the make = %#lx", hr);
        PropVariantInit(&out);
        hr = IWICMetadataQueryWriter_GetMetadataByName(qw, L"/app1/ifd/{ushort=271}", &out);
        check(hr == S_OK && out.vt == VT_LPSTR && !strcmp(out.pszVal, "Fabrikam"), "make read back (%#lx)", hr);
        PropVariantClear(&out);
    }

    /* the exif directory inside the ifd */
    PropVariantInit(&v);
    v.vt = VT_UI2; v.uiVal = 100;
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/exif/{ushort=34855}", &v);
    check(hr == S_OK, "set the ISO in the Exif directory = %#lx", hr);
    PropVariantInit(&out);
    hr = IWICMetadataQueryWriter_GetMetadataByName(qw, L"/app1/ifd/exif/{ushort=34855}", &out);
    check(hr == S_OK && out.uiVal == 100, "ISO read back (%#lx)", hr);
    PropVariantClear(&out);

    /* what was written is what Save writes */
    {
        IWICMetadataWriter *w = NULL;
        IWICPersistStream *ps = NULL;
        IStream *mem;
        STATSTG st;
        BYTE bytes[512];
        ULONG got = 0;
        LARGE_INTEGER zero = {{0}};

        IWICMetadataBlockWriter_GetWriterByIndex(bw, 0, &w);
        IWICMetadataWriter_QueryInterface(w, &my_IID_IWICPersistStream, (void **)&ps);
        CreateStreamOnHGlobal(NULL, TRUE, &mem);
        hr = IWICPersistStream_Save(ps, mem, FALSE);
        check(hr == S_OK, "Save of the App1 writer = %#lx", hr);
        IStream_Stat(mem, &st, STATFLAG_NONAME);
        IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
        IStream_Read(mem, bytes, sizeof(bytes), &got);
        check(got > 30 && !memcmp(bytes, "Exif\0\0II*\0", 10), "starts with the Exif header (%lu bytes)", got);
        check(got > 30 && bytes[14] == 2 /* two entries: orientation and make */ + 1 /* and the Exif pointer */, "three entries in the directory (%u)", bytes[14]);
        IStream_Release(mem);
        IWICPersistStream_Release(ps);
        IWICMetadataWriter_Release(w);
    }

    hr = IWICMetadataQueryWriter_RemoveMetadataByName(qw, L"/app1/ifd/{ushort=274}");
    check(hr == S_OK, "remove the orientation = %#lx", hr);
    PropVariantInit(&out);
    hr = IWICMetadataQueryWriter_GetMetadataByName(qw, L"/app1/ifd/{ushort=274}", &out);
    check(hr == WINCODEC_ERR_PROPERTYNOTFOUND, "gone = %#lx", hr);
    hr = IWICMetadataQueryWriter_RemoveMetadataByName(qw, L"/app1/ifd/{ushort=274}");
    check(FAILED(hr), "removing it again fails = %#lx", hr);
    hr = IWICMetadataQueryWriter_RemoveMetadataByName(qw, L"/app1");
    check(hr == S_OK, "remove the whole App1 block = %#lx", hr);
    IWICMetadataBlockWriter_GetCount(bw, &count);
    check(count == 0, "no blocks left (%u)", count);

    /* wrong spellings */
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"app1/ifd/{ushort=274}", &v);
    check(hr == WINCODEC_ERR_PROPERTYNOTSUPPORTED, "no leading slash = %#lx", hr);
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/ifd/[*]{ushort=274}", &v);
    check(FAILED(hr), "[*] on a set fails = %#lx", hr);
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, NULL, &v);
    check(hr == E_INVALIDARG, "no name = %#lx", hr);
    hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/app1/ifd/{ushort=274}", NULL);
    check(hr == E_INVALIDARG, "no value = %#lx", hr);

    {
        GUID container;

        hr = IWICMetadataQueryWriter_GetContainerFormat(qw, &container);
        check(hr == S_OK && IsEqualGUID(&container, &my_GUID_ContainerFormatJpeg), "container format of the writer (%#lx)", hr);
    }

    if (bw) IWICMetadataBlockWriter_Release(bw);
    IWICMetadataQueryWriter_Release(qw);
done:
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
    IStream_Release(stream);
}

static void test_gif_query_writer(void)
{
    IWICBitmapEncoder *enc;
    IStream *stream;
    IWICBitmapFrameEncode *frame = new_frame(&my_GUID_ContainerFormatGif, &enc, &stream);
    IWICMetadataQueryWriter *qw = NULL;
    PROPVARIANT v, out;
    HRESULT hr;

    hr = IWICBitmapFrameEncode_GetMetadataQueryWriter(frame, &qw);
    check(hr == S_OK && qw, "GIF query writer (%#lx)", hr);
    if (qw)
    {
        PropVariantInit(&v);
        v.vt = VT_UI2; v.uiVal = 10;
        hr = IWICMetadataQueryWriter_SetMetadataByName(qw, L"/grctlext/Delay", &v);
        check(hr == S_OK, "set the delay = %#lx", hr);
        PropVariantInit(&out);
        hr = IWICMetadataQueryWriter_GetMetadataByName(qw, L"/grctlext/Delay", &out);
        check(hr == S_OK && out.vt == VT_UI2 && out.uiVal == 10, "read back (%#lx)", hr);
        PropVariantClear(&out);
        IWICMetadataQueryWriter_Release(qw);
    }
    IWICBitmapFrameEncode_Release(frame);
    IWICBitmapEncoder_Release(enc);
    IStream_Release(stream);
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    check(hr == S_OK, "WIC factory (%#lx)", hr);
    if (FAILED(hr)) goto done;
    hr = IWICImagingFactory_QueryInterface(factory, &my_IID_IWICComponentFactory, (void **)&cfactory);
    check(hr == S_OK, "component factory (%#lx)", hr);
    if (FAILED(hr)) goto done;

    test_block_writer(&my_GUID_ContainerFormatJpeg, "jpeg");
    test_block_writer(&my_GUID_ContainerFormatGif, "gif");
    test_from_reader();
    test_query_writer();
    test_gif_query_writer();
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
