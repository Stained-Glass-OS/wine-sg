/* windowscodecs: Exif metadata of a JPEG file. A JPEG is built in memory (a
 * tiny image from the WIC encoder), an APP1 Exif segment is spliced in after
 * the SOI marker (little or big endian, IFD not right behind the header), and
 * the file is decoded again; the app1 block and its ifd child must show the
 * tags. Variants: other segments before the Exif one, a non-Exif APP1, an
 * invalid TIFF header, no Exif at all. */
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
static const GUID my_IID_IWICMetadataBlockReader = {0xfeaa2a8d, 0xb3f3, 0x43e4, {0xb2, 0x5c, 0xd1, 0xde, 0x99, 0x0a, 0x1a, 0xe1}};
static const GUID my_IID_IWICMetadataReader = {0x9204fe99, 0xd8fc, 0x4fd5, {0xa0, 0x01, 0x95, 0x36, 0xb0, 0x67, 0xa8, 0x99}};
static const GUID my_IID_IWICBitmapFrameDecode = {0x3b16811b, 0x6a43, 0x4ec9, {0xa8, 0x13, 0x3d, 0x93, 0x0c, 0x13, 0xb9, 0x40}};
static const GUID my_GUID_ContainerFormatJpeg = {0x19e4a5aa, 0x5662, 0x4fc5, {0xa0, 0xc0, 0x17, 0x58, 0x02, 0x8e, 0x10, 0x57}};
static const GUID my_GUID_MetadataFormatApp1 = {0x8fd3dfc3, 0xf951, 0x492b, {0x81, 0x7f, 0x69, 0xc2, 0xe6, 0xd9, 0xa5, 0xb0}};
static const GUID my_GUID_MetadataFormatIfd = {0x537396c6, 0x2d8a, 0x4bb6, {0x9b, 0xf8, 0x2f, 0x0a, 0x8e, 0x2a, 0x3a, 0xdf}};
static const GUID my_GUID_WICPixelFormat8bppGray = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x08}};

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void checkv(int ok, const char *what, unsigned long got)
{
    printf("%s  %s (got %#lx)\n", ok ? "PASS" : "FAIL", what, got);
    if (!ok) failures++;
}

static IWICImagingFactory *factory;

struct buf { BYTE *data; UINT size; };

static void put16(BYTE *p, WORD v, int be) { if (be) { p[0] = v >> 8; p[1] = v; } else { p[0] = v; p[1] = v >> 8; } }
static void put32(BYTE *p, DWORD v, int be)
{
    if (be) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
    else { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
}

/* a plain 8x8 gray JPEG from the encoder */
static BOOL make_jpeg(struct buf *out)
{
    IWICBitmapEncoder *enc = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IWICStream *stream = NULL;
    BYTE *mem = calloc(1, 65536);
    BYTE pixels[8 * 8];
    GUID fmt = my_GUID_WICPixelFormat8bppGray;
    ULARGE_INTEGER pos;
    LARGE_INTEGER zero = {{0}};
    HRESULT hr;
    UINT i;
    IStream *st;

    for (i = 0; i < sizeof(pixels); i++) pixels[i] = i * 3;
    hr = CreateStreamOnHGlobal(NULL, TRUE, &st);
    if (FAILED(hr)) return FALSE;
    hr = IWICImagingFactory_CreateEncoder(factory, &my_GUID_ContainerFormatJpeg, NULL, &enc);
    if (hr == S_OK) hr = IWICBitmapEncoder_Initialize(enc, st, WICBitmapEncoderNoCache);
    if (hr == S_OK) hr = IWICBitmapEncoder_CreateNewFrame(enc, &frame, NULL);
    if (hr == S_OK) hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
    if (hr == S_OK) hr = IWICBitmapFrameEncode_SetSize(frame, 8, 8);
    if (hr == S_OK) hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    if (hr == S_OK) hr = IWICBitmapFrameEncode_WritePixels(frame, 8, 8, sizeof(pixels), pixels);
    if (hr == S_OK) hr = IWICBitmapFrameEncode_Commit(frame);
    if (hr == S_OK) hr = IWICBitmapEncoder_Commit(enc);
    if (frame) IWICBitmapFrameEncode_Release(frame);
    if (enc) IWICBitmapEncoder_Release(enc);
    (void)stream; (void)mem; (void)pos;
    if (hr != S_OK) { IStream_Release(st); return FALSE; }

    {
        STATSTG stat;
        ULONG got;
        IStream_Stat(st, &stat, STATFLAG_NONAME);
        out->size = stat.cbSize.LowPart;
        out->data = malloc(out->size);
        IStream_Seek(st, zero, STREAM_SEEK_SET, NULL);
        IStream_Read(st, out->data, out->size, &got);
        hr = got == out->size ? S_OK : E_FAIL;
    }
    IStream_Release(st);
    free(mem);
    return hr == S_OK && out->size > 4 && out->data[0] == 0xff && out->data[1] == 0xd8;
}

/* Exif payload (after the "Exif\0\0"): Make (ascii, out of line), Orientation
 * (short), XResolution (rational, out of line); the IFD sits at ifd_off. */
static UINT make_exif(BYTE *p, int be, UINT ifd_off, BOOL good_header)
{
    UINT i, data;

    memset(p, 0xaa, 128);
    p[0] = p[1] = be ? 'M' : 'I';
    put16(p + 2, good_header ? 42 : 43, be);
    put32(p + 4, ifd_off, be);
    i = ifd_off;
    put16(p + i, 3, be); i += 2;
    data = ifd_off + 2 + 36 + 4;
    /* Make */
    put16(p + i, 271, be); put16(p + i + 2, 2, be); put32(p + i + 4, 8, be); put32(p + i + 8, data, be); i += 12;
    memcpy(p + data, "Wine-SG", 8); data += 8;
    /* Orientation */
    put16(p + i, 274, be); put16(p + i + 2, 3, be); put32(p + i + 4, 1, be); put16(p + i + 8, 6, be); put16(p + i + 10, 0, be); i += 12;
    /* XResolution */
    put16(p + i, 282, be); put16(p + i + 2, 5, be); put32(p + i + 4, 1, be); put32(p + i + 8, data, be); i += 12;
    put32(p + data, 72, be); put32(p + data + 4, 1, be); data += 8;
    put32(p + i, 0, be);
    return data;
}

enum variant { V_LE, V_BE, V_BE_OFF, V_PREFIX, V_XMP_FIRST, V_BAD_TIFF, V_NONE };

static BOOL build(struct buf *jpeg, enum variant v)
{
    struct buf base;
    BYTE seg[1024], app0[18] = { 0xff, 0xe0, 0, 16, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0 };
    UINT n = 0, exif_len;
    BYTE *p;

    if (!make_jpeg(&base)) return FALSE;
    if (v == V_NONE) { *jpeg = base; return TRUE; }

    if (v == V_PREFIX)
    {
        /* APP0 JFIF with a thumbnail-free payload, a COM and an APP2 first */
        memcpy(seg + n, app0, sizeof(app0)); n += sizeof(app0);
        seg[n++] = 0xff; seg[n++] = 0xfe; seg[n++] = 0; seg[n++] = 7; memcpy(seg + n, "hello", 5); n += 5;
        seg[n++] = 0xff; seg[n++] = 0xe2; seg[n++] = 0; seg[n++] = 5; seg[n++] = 1; seg[n++] = 2; seg[n++] = 3;
    }
    if (v == V_XMP_FIRST)
    {
        static const char xmp[] = "http://ns.adobe.com/xap/1.0/";
        seg[n++] = 0xff; seg[n++] = 0xe1; seg[n++] = 0; seg[n++] = 2 + sizeof(xmp) + 4;
        memcpy(seg + n, xmp, sizeof(xmp)); n += sizeof(xmp);
        memcpy(seg + n, "<x/>", 4); n += 4;
    }
    seg[n++] = 0xff; seg[n++] = 0xe1; seg[n++] = 0; seg[n++] = 0; /* length patched below */
    p = seg + n;
    memcpy(p, "Exif\0\0", 6);
    exif_len = make_exif(p + 6, v == V_BE || v == V_BE_OFF || v == V_BAD_TIFF, v == V_BE_OFF ? 16 : 8, v != V_BAD_TIFF) + 6;
    seg[n - 2] = (exif_len + 2) >> 8; seg[n - 1] = (exif_len + 2);
    n += exif_len;

    jpeg->size = base.size + n;
    jpeg->data = malloc(jpeg->size);
    memcpy(jpeg->data, base.data, 2);
    memcpy(jpeg->data + 2, seg, n);
    memcpy(jpeg->data + 2 + n, base.data + 2, base.size - 2);
    free(base.data);
    return TRUE;
}

static BOOL get_u16(IWICMetadataQueryReader *q, const WCHAR *name, USHORT *val)
{
    PROPVARIANT pv;
    HRESULT hr;

    PropVariantInit(&pv);
    hr = IWICMetadataQueryReader_GetMetadataByName(q, name, &pv);
    if (hr == S_OK && pv.vt == VT_UI2) *val = pv.uiVal;
    else hr = E_FAIL;
    PropVariantClear(&pv);
    return hr == S_OK;
}

static void test_variant(enum variant v, const char *label)
{
    struct buf jpeg;
    IWICStream *stream;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICMetadataBlockReader *blocks = NULL;
    IWICMetadataQueryReader *q = NULL;
    HRESULT hr;
    UINT count = 77;
    char what[160];
    BOOL has_exif = v != V_NONE;
    BOOL good = v != V_NONE && v != V_BAD_TIFF;

    if (!build(&jpeg, v)) { sprintf(what, "%s: the test image was built", label); check(0, what); return; }
    hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (hr == S_OK) hr = IWICStream_InitializeFromMemory(stream, jpeg.data, jpeg.size);
    if (hr == S_OK) hr = IWICImagingFactory_CreateDecoderFromStream(factory, (IStream *)stream, NULL, WICDecodeMetadataCacheOnDemand, &dec);
    sprintf(what, "%s: JPEG decoder created", label);
    checkv(hr == S_OK, what, hr);
    if (hr != S_OK) return;
    hr = IWICBitmapDecoder_GetFrame(dec, 0, &frame);
    sprintf(what, "%s: frame 0", label);
    checkv(hr == S_OK, what, hr);
    if (hr != S_OK) return;

    hr = IWICBitmapFrameDecode_QueryInterface(frame, &my_IID_IWICMetadataBlockReader, (void **)&blocks);
    sprintf(what, "%s: frame is a metadata block reader", label);
    checkv(hr == S_OK, what, hr);
    if (blocks)
    {
        GUID cf;

        hr = IWICMetadataBlockReader_GetCount(blocks, &count);
        sprintf(what, "%s: %u metadata block(s)", label, has_exif && v != V_BAD_TIFF + 100 ? 1 : 0);
        checkv(hr == S_OK && count == (has_exif ? 1u : 0u), what, count);
        hr = IWICMetadataBlockReader_GetContainerFormat(blocks, &cf);
        sprintf(what, "%s: block container is JPEG", label);
        check(hr == S_OK && IsEqualGUID(&cf, &my_GUID_ContainerFormatJpeg), what);

        if (good && count == 1)
        {
            IWICMetadataReader *app1 = NULL, *ifd = NULL;
            GUID fmt;
            PROPVARIANT id, val;

            hr = IWICMetadataBlockReader_GetReaderByIndex(blocks, 0, &app1);
            sprintf(what, "%s: block 0 reader", label);
            checkv(hr == S_OK && app1, what, hr);
            if (app1)
            {
                hr = IWICMetadataReader_GetMetadataFormat(app1, &fmt);
                sprintf(what, "%s: block 0 is the app1 format", label);
                check(hr == S_OK && IsEqualGUID(&fmt, &my_GUID_MetadataFormatApp1), what);
                count = 77;
                hr = IWICMetadataReader_GetCount(app1, &count);
                sprintf(what, "%s: app1 has one item", label);
                check(hr == S_OK && count == 1, what);
                PropVariantInit(&id); PropVariantInit(&val);
                hr = IWICMetadataReader_GetValueByIndex(app1, 0, NULL, &id, &val);
                sprintf(what, "%s: the app1 item is an ifd reader", label);
                check(hr == S_OK && id.vt == VT_CLSID && IsEqualGUID(id.puuid, &my_GUID_MetadataFormatIfd) && val.vt == VT_UNKNOWN, what);
                if (hr == S_OK && val.vt == VT_UNKNOWN && SUCCEEDED(IUnknown_QueryInterface(val.punkVal, &my_IID_IWICMetadataReader, (void **)&ifd)))
                {
                    count = 77;
                    hr = IWICMetadataReader_GetCount(ifd, &count);
                    sprintf(what, "%s: the ifd has 3 entries", label);
                    checkv(hr == S_OK && count == 3, what, count);
                    IWICMetadataReader_Release(ifd);
                }
                PropVariantClear(&id); PropVariantClear(&val);
                IWICMetadataReader_Release(app1);
            }
        }
        IWICMetadataBlockReader_Release(blocks);
    }

    hr = IWICBitmapFrameDecode_GetMetadataQueryReader(frame, &q);
    sprintf(what, "%s: frame query reader", label);
    checkv(hr == S_OK && q, what, hr);
    if (q)
    {
        USHORT u = 0;
        PROPVARIANT pv;

        if (good)
        {
            sprintf(what, "%s: /app1/ifd/{ushort=274} is 6", label);
            check(get_u16(q, L"/app1/ifd/{ushort=274}", &u) && u == 6, what);

            PropVariantInit(&pv);
            hr = IWICMetadataQueryReader_GetMetadataByName(q, L"/app1/ifd/{ushort=271}", &pv);
            sprintf(what, "%s: /app1/ifd/{ushort=271} is Wine-SG", label);
            check(hr == S_OK && pv.vt == VT_LPSTR && !strcmp(pv.pszVal, "Wine-SG"), what);
            PropVariantClear(&pv);

            PropVariantInit(&pv);
            hr = IWICMetadataQueryReader_GetMetadataByName(q, L"/app1/ifd/{ushort=282}", &pv);
            sprintf(what, "%s: /app1/ifd/{ushort=282} is 72/1", label);
            check(hr == S_OK && pv.vt == VT_UI8 && pv.uhVal.LowPart == 72 && pv.uhVal.HighPart == 1, what);
            PropVariantClear(&pv);
        }
        else
        {
            PropVariantInit(&pv);
            hr = IWICMetadataQueryReader_GetMetadataByName(q, L"/app1/ifd/{ushort=274}", &pv);
            sprintf(what, "%s: /app1/ifd/{ushort=274} is not found", label);
            checkv(FAILED(hr), what, hr);
            PropVariantClear(&pv);
        }
        IWICMetadataQueryReader_Release(q);
    }

    IWICBitmapFrameDecode_Release(frame);
    IWICBitmapDecoder_Release(dec);
    IWICStream_Release(stream);
    free(jpeg.data);
}

int main(void)
{
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    if (hr != S_OK)
    {
        printf("FAIL  no imaging factory (%#lx)\nRESULT: FAIL\n", hr);
        return 1;
    }

    test_variant(V_LE, "little-endian Exif");
    test_variant(V_BE, "big-endian Exif");
    test_variant(V_BE_OFF, "big-endian Exif, IFD at offset 16");
    test_variant(V_PREFIX, "Exif after JFIF, COM and APP2");
    test_variant(V_XMP_FIRST, "non-Exif APP1 before the Exif one");
    test_variant(V_BAD_TIFF, "invalid TIFF header");
    test_variant(V_NONE, "no Exif");

    IWICImagingFactory_Release(factory);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
