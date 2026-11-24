// SPDX-License-Identifier: LGPL-2.1-or-later
/* wicpngchunks probe: the bKGD, sRGB, iCCP and iTXt chunks of a PNG are read by their own metadata readers,
 * their writers have a header in the PNG container (what Paint.NET asks before it opens a PNG) and write the
 * chunk back, and the PNG encoder writes them (wine-sg 9350). */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(GUID_ContainerFormatPng_, 0x1b7cfaf4,0x713f,0x473c,0xbb,0xcd,0x61,0x37,0x42,0x5f,0xae,0xaf);
DEFINE_GUID(GUID_MetadataFormatChunkbKGD_, 0xe14d3571,0x6b47,0x4dea,0xb6,0x0a,0x87,0xce,0x0a,0x78,0xdf,0xb7);
DEFINE_GUID(GUID_MetadataFormatChunkiTXt_, 0xc2bec729,0x0b68,0x4b77,0xaa,0x0e,0x62,0x95,0xa6,0xac,0x18,0x14);
DEFINE_GUID(GUID_MetadataFormatChunkiCCP_, 0xeb4349ab,0xb685,0x450f,0x91,0xb5,0xe8,0x02,0xe8,0x92,0x53,0x6c);
DEFINE_GUID(GUID_MetadataFormatChunksRGB_, 0xc115fd36,0xcc6f,0x4e3f,0x83,0x63,0x52,0x4b,0x87,0xc6,0xb0,0xd9);
DEFINE_GUID(GUID_MetadataFormatUnknown_, 0xa45e592f,0x9078,0x4a7c,0xad,0xb5,0x4e,0xdc,0x4f,0xd6,0x1b,0x1f);
DEFINE_GUID(IID_IWICMetadataWriterInfo_, 0xb22e3fba,0x3925,0x4323,0xb5,0xc1,0x9e,0xbf,0xc4,0x30,0xf2,0x36);
DEFINE_GUID(IID_IWICMetadataBlockReader_, 0xfeaa2a8d,0xb3f3,0x43e4,0xb2,0x5c,0xd1,0xde,0x99,0x0a,0x1a,0xe1);
DEFINE_GUID(IID_IWICMetadataBlockWriter_, 0x08fb9676,0xb444,0x41e8,0x8d,0xbe,0x6a,0x53,0xa5,0x42,0xbf,0xf1);
DEFINE_GUID(IID_IWICComponentFactory_, 0x412d0c3a,0x9650,0x44fa,0xaf,0x5b,0xdd,0x2a,0x06,0xc8,0xe8,0xfb);
DEFINE_GUID(IID_IWICPersistStream_, 0x00675040,0x6908,0x45f8,0x86,0xa3,0x49,0xc7,0xdf,0xd6,0xd9,0xad);

static const BYTE png_data[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
    0x73, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x01, 0xd9, 0xc9, 0x2c, 0x7f, 0x00, 0x00,
    0x00, 0x06, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x12, 0x00, 0x56, 0x00, 0x9a, 0x9b, 0x88, 0x57, 0xc5,
    0x00, 0x00, 0x00, 0xe0, 0x69, 0x43, 0x43, 0x50, 0x54, 0x65, 0x73, 0x74, 0x50, 0x72, 0x6f, 0x66,
    0x69, 0x6c, 0x65, 0x00, 0x00, 0x78, 0x9c, 0x01, 0xc8, 0x00, 0x37, 0xff, 0x00, 0x07, 0x0e, 0x15,
    0x1c, 0x23, 0x2a, 0x31, 0x38, 0x3f, 0x46, 0x4d, 0x54, 0x5b, 0x62, 0x69, 0x70, 0x77, 0x7e, 0x85,
    0x8c, 0x93, 0x9a, 0xa1, 0xa8, 0xaf, 0xb6, 0xbd, 0xc4, 0xcb, 0xd2, 0xd9, 0xe0, 0xe7, 0xee, 0xf5,
    0xfc, 0x03, 0x0a, 0x11, 0x18, 0x1f, 0x26, 0x2d, 0x34, 0x3b, 0x42, 0x49, 0x50, 0x57, 0x5e, 0x65,
    0x6c, 0x73, 0x7a, 0x81, 0x88, 0x8f, 0x96, 0x9d, 0xa4, 0xab, 0xb2, 0xb9, 0xc0, 0xc7, 0xce, 0xd5,
    0xdc, 0xe3, 0xea, 0xf1, 0xf8, 0xff, 0x06, 0x0d, 0x14, 0x1b, 0x22, 0x29, 0x30, 0x37, 0x3e, 0x45,
    0x4c, 0x53, 0x5a, 0x61, 0x68, 0x6f, 0x76, 0x7d, 0x84, 0x8b, 0x92, 0x99, 0xa0, 0xa7, 0xae, 0xb5,
    0xbc, 0xc3, 0xca, 0xd1, 0xd8, 0xdf, 0xe6, 0xed, 0xf4, 0xfb, 0x02, 0x09, 0x10, 0x17, 0x1e, 0x25,
    0x2c, 0x33, 0x3a, 0x41, 0x48, 0x4f, 0x56, 0x5d, 0x64, 0x6b, 0x72, 0x79, 0x80, 0x87, 0x8e, 0x95,
    0x9c, 0xa3, 0xaa, 0xb1, 0xb8, 0xbf, 0xc6, 0xcd, 0xd4, 0xdb, 0xe2, 0xe9, 0xf0, 0xf7, 0xfe, 0x05,
    0x0c, 0x13, 0x1a, 0x21, 0x28, 0x2f, 0x36, 0x3d, 0x44, 0x4b, 0x52, 0x59, 0x60, 0x67, 0x6e, 0x75,
    0x7c, 0x83, 0x8a, 0x91, 0x98, 0x9f, 0xa6, 0xad, 0xb4, 0xbb, 0xc2, 0xc9, 0xd0, 0xd7, 0xde, 0xe5,
    0xec, 0xf3, 0xfa, 0x01, 0x08, 0x0f, 0x16, 0x1d, 0x24, 0x2b, 0x32, 0x39, 0x40, 0x47, 0x4e, 0x55,
    0x5c, 0x63, 0x6a, 0x71, 0xc6, 0x50, 0x5f, 0x25, 0x17, 0x21, 0xb2, 0x91, 0x00, 0x00, 0x00, 0x16,
    0x69, 0x54, 0x58, 0x74, 0x54, 0x69, 0x74, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x66, 0x72, 0x00, 0x54,
    0x69, 0x74, 0x72, 0x65, 0x00, 0xc3, 0x89, 0x74, 0xc3, 0xa9, 0xdf, 0xfe, 0xcb, 0xa1, 0x00, 0x00,
    0x00, 0x2e, 0x69, 0x54, 0x58, 0x74, 0x43, 0x6f, 0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x78, 0x9c, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x48, 0xce, 0xcf, 0x2d, 0x28, 0x4a,
    0x2d, 0x2e, 0x4e, 0x4d, 0x51, 0x38, 0xbc, 0x27, 0xef, 0xf0, 0xfa, 0xe4, 0xfc, 0x94, 0x54, 0x00,
    0x92, 0xcc, 0x0b, 0x84, 0x9d, 0x32, 0xd9, 0xfc, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54,
    0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xc0, 0x00, 0xc2, 0x0c, 0xff, 0x81, 0x00, 0x00, 0x1f, 0xee,
    0x05, 0xfb, 0x0b, 0xd9, 0x68, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82,
};

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { printf("FAIL  " __VA_ARGS__); failures++; } printf("\n"); } while (0)

static IWICImagingFactory *factory;
static IWICComponentFactory *cfactory;

static BYTE profile[200];

/* the chunk (length, type, data, CRC) of type t in a PNG */
static const BYTE *find_chunk(const BYTE *png, ULONG len, const char *t, int nth, ULONG *size)
{
    ULONG i = 8;
    while (i + 12 <= len)
    {
        ULONG n = png[i] << 24 | png[i + 1] << 16 | png[i + 2] << 8 | png[i + 3];
        if (!memcmp(png + i + 4, t, 4) && !nth--) { *size = n + 12; return png + i; }
        i += n + 12;
    }
    return NULL;
}

static IWICBitmapFrameDecode *decode(const BYTE *data, ULONG len)
{
    IWICBitmapDecoder *dec; IWICBitmapFrameDecode *frame = NULL; IWICStream *s;
    HRESULT hr;
    IWICImagingFactory_CreateStream(factory, &s);
    IWICStream_InitializeFromMemory(s, (BYTE *)data, len);
    hr = IWICImagingFactory_CreateDecoderFromStream(factory, (IStream *)s, NULL, WICDecodeMetadataCacheOnLoad, &dec);
    if (FAILED(hr)) { printf("FAIL  decoder %#lx\n", hr); failures++; return NULL; }
    hr = IWICBitmapDecoder_GetFrame(dec, 0, &frame);
    if (FAILED(hr)) { printf("FAIL  frame %#lx\n", hr); failures++; return NULL; }
    return frame;
}

static HRESULT query(IWICMetadataQueryReader *q, const WCHAR *name, PROPVARIANT *v)
{
    PropVariantInit(v);
    return IWICMetadataQueryReader_GetMetadataByName(q, name, v);
}

static void check_values(IWICBitmapFrameDecode *frame, BOOL with_iccp, const char *what)
{
    IWICMetadataQueryReader *q;
    PROPVARIANT v;
    HRESULT hr;

    if (FAILED(hr = IWICBitmapFrameDecode_GetMetadataQueryReader(frame, &q)))
    {
        printf("FAIL  %s: query reader %#lx\n", what, hr); failures++; return;
    }
    hr = query(q, L"/sRGB/RenderingIntent", &v);
    CHECK(hr == S_OK && v.vt == VT_UI1 && v.bVal == 1, "%s: /sRGB/RenderingIntent VT_UI1 1 (hr %#lx vt %u)", what, hr, v.vt);
    PropVariantClear(&v);
    hr = query(q, L"/bKGD/BackgroundColor", &v);
    CHECK(hr == S_OK && v.vt == (VT_UI2 | VT_VECTOR) && v.caui.cElems == 3 && v.caui.pElems[0] == 0x12
          && v.caui.pElems[1] == 0x56 && v.caui.pElems[2] == 0x9a,
          "%s: /bKGD/BackgroundColor vector of 3 VT_UI2 (hr %#lx vt %#x)", what, hr, v.vt);
    PropVariantClear(&v);
    if (with_iccp)
    {
        hr = query(q, L"/iCCP/ProfileName", &v);
        CHECK(hr == S_OK && v.vt == VT_LPSTR && !strcmp(v.pszVal, "TestProfile"), "%s: /iCCP/ProfileName (hr %#lx vt %u)", what, hr, v.vt);
        PropVariantClear(&v);
        hr = query(q, L"/iCCP/ProfileData", &v);
        CHECK(hr == S_OK && v.vt == (VT_UI1 | VT_VECTOR) && v.caub.cElems == sizeof(profile)
              && !memcmp(v.caub.pElems, profile, sizeof(profile)),
              "%s: /iCCP/ProfileData is the inflated profile (hr %#lx vt %#x n %lu)", what, hr, v.vt,
              (v.vt == (VT_UI1 | VT_VECTOR)) ? v.caub.cElems : 0);
        PropVariantClear(&v);
    }
    hr = query(q, L"/[0]iTXt/Keyword", &v);
    CHECK(hr == S_OK && v.vt == VT_LPSTR && !strcmp(v.pszVal, "Title"), "%s: /[0]iTXt/Keyword (hr %#lx vt %u)", what, hr, v.vt);
    PropVariantClear(&v);
    hr = query(q, L"/[0]iTXt/LanguageTag", &v);
    CHECK(hr == S_OK && v.vt == VT_LPSTR && !strcmp(v.pszVal, "fr"), "%s: /[0]iTXt/LanguageTag (hr %#lx vt %u)", what, hr, v.vt);
    PropVariantClear(&v);
    hr = query(q, L"/[0]iTXt/TranslatedKeyword", &v);
    CHECK(hr == S_OK && v.vt == VT_LPWSTR && !wcscmp(v.pwszVal, L"Titre"), "%s: /[0]iTXt/TranslatedKeyword (hr %#lx vt %u)", what, hr, v.vt);
    PropVariantClear(&v);
    hr = query(q, L"/[0]iTXt/TextEntry", &v);
    CHECK(hr == S_OK && v.vt == VT_LPWSTR && !wcscmp(v.pwszVal, L"\x00c9t\x00e9"), "%s: /[0]iTXt/TextEntry UTF-8 decoded (hr %#lx vt %u)", what, hr, v.vt);
    PropVariantClear(&v);
    hr = query(q, L"/[1]iTXt/CompressionFlag", &v);
    CHECK(hr == S_OK && v.vt == VT_UI1 && v.bVal == 1, "%s: /[1]iTXt/CompressionFlag 1 (hr %#lx vt %u)", what, hr, v.vt);
    PropVariantClear(&v);
    hr = query(q, L"/[1]iTXt/TextEntry", &v);
    CHECK(hr == S_OK && v.vt == VT_LPWSTR && !wcscmp(v.pwszVal, L"hello compressed \x00fc" L"n\x00ef" L"code"),
          "%s: /[1]iTXt/TextEntry inflated (hr %#lx vt %u)", what, hr, v.vt);
    PropVariantClear(&v);
    IWICMetadataQueryReader_Release(q);
}

/* the writer info for a metadata format, the way a program finds it: enumerate the writers */
static IWICMetadataWriterInfo *writer_info(const GUID *format)
{
    IEnumUnknown *e; IUnknown *u; ULONG n;
    IWICImagingFactory_CreateComponentEnumerator(factory, WICMetadataWriter, WICComponentEnumerateDefault, &e);
    while (IEnumUnknown_Next(e, 1, &u, &n) == S_OK)
    {
        IWICMetadataWriterInfo *w; GUID f;
        if (SUCCEEDED(IUnknown_QueryInterface(u, &IID_IWICMetadataWriterInfo_, (void **)&w)))
        {
            IWICMetadataWriterInfo_GetMetadataFormat(w, &f);
            if (IsEqualGUID(&f, format)) { IUnknown_Release(u); IEnumUnknown_Release(e); return w; }
            IWICMetadataWriterInfo_Release(w);
        }
        IUnknown_Release(u);
    }
    IEnumUnknown_Release(e);
    return NULL;
}

int main(void)
{
    static const struct { const GUID *format; const char *type; int nth; BOOL exact; } blocks[] = {
        { &GUID_MetadataFormatChunksRGB_, "sRGB", 0, TRUE },
        { &GUID_MetadataFormatChunkbKGD_, "bKGD", 0, TRUE },
        { &GUID_MetadataFormatChunkiCCP_, "iCCP", 0, FALSE },
        { &GUID_MetadataFormatChunkiTXt_, "iTXt", 0, TRUE },
        { &GUID_MetadataFormatChunkiTXt_, "iTXt", 1, FALSE },
    };
    IWICBitmapFrameDecode *frame, *frame2;
    IWICMetadataBlockReader *br;
    UINT count = 0, i;
    HRESULT hr;

    for (i = 0; i < sizeof(profile); i++) profile[i] = (i * 7) & 255;
    CoInitialize(NULL);
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory)))
    {
        printf("FAIL  no imaging factory\n"); return 1;
    }
    IWICImagingFactory_QueryInterface(factory, &IID_IWICComponentFactory_, (void **)&cfactory);

    if (!(frame = decode(png_data, sizeof(png_data)))) goto end;

    /* each chunk has its format's reader, not the Unknown reader */
    hr = IWICBitmapFrameDecode_QueryInterface(frame, &IID_IWICMetadataBlockReader_, (void **)&br);
    IWICMetadataBlockReader_GetCount(br, &count);
    CHECK(count == 5, "five metadata blocks (%u)", count);
    for (i = 0; i < count && i < 5; i++)
    {
        IWICMetadataReader *r; IWICMetadataWriter *w; IWICMetadataWriterInfo *wi;
        GUID f = {0};
        UINT size = 0;

        IWICMetadataBlockReader_GetReaderByIndex(br, i, &r);
        IWICMetadataReader_GetMetadataFormat(r, &f);
        CHECK(IsEqualGUID(&f, blocks[i].format), "block %u is read as %s (format %08lx)", i, blocks[i].type, f.Data1);

        /* Paint.NET opening a PNG: the block's writer and the header it has in a PNG */
        wi = writer_info(blocks[i].format);
        CHECK(wi != NULL, "block %u: a %s metadata writer is registered", i, blocks[i].type);
        if (wi)
        {
            BYTE buf[256]; WICMetadataHeader *h = (WICMetadataHeader *)buf;
            hr = IWICMetadataWriterInfo_GetHeader(wi, &GUID_ContainerFormatPng_, 0, NULL, &size);
            CHECK(hr == S_OK && size > sizeof(WICMetadataHeader), "block %u: %s writer GetHeader(PNG) size (hr %#lx size %u)", i, blocks[i].type, hr, size);
            hr = IWICMetadataWriterInfo_GetHeader(wi, &GUID_ContainerFormatPng_, sizeof(buf), h, &size);
            CHECK(hr == S_OK && h->Length == 4 && h->Header && !memcmp(h->Header, blocks[i].type, 4) && h->Position.QuadPart == 4,
                  "block %u: header is the chunk type at 4 (hr %#lx)", i, hr);
            IWICMetadataWriterInfo_Release(wi);
        }

        /* the writer made from the reader writes the chunk back */
        hr = IWICComponentFactory_CreateMetadataWriterFromReader(cfactory, r, NULL, &w);
        CHECK(hr == S_OK, "block %u: writer from reader (hr %#lx)", i, hr);
        if (hr == S_OK)
        {
            IWICPersistStream *ps = NULL; IStream *st; HGLOBAL g; ULONG csize; const BYTE *orig;
            const BYTE *saved; STATSTG stat;
            IWICMetadataWriter_QueryInterface(w, &IID_IWICPersistStream_, (void **)&ps);
            if (!ps) { printf("FAIL  block %u: no IWICPersistStream\n", i); failures++; IWICMetadataWriter_Release(w); IWICMetadataReader_Release(r); continue; }
            CreateStreamOnHGlobal(NULL, TRUE, &st);
            hr = IWICPersistStream_SaveEx(ps, st, WICPersistOptionDefault, FALSE);
            IStream_Stat(st, &stat, STATFLAG_NONAME);
            GetHGlobalFromStream(st, &g);
            saved = GlobalLock(g);
            orig = find_chunk(png_data, sizeof(png_data), blocks[i].type, blocks[i].nth, &csize);
            if (!orig || !saved || hr != S_OK)
                CHECK(0, "block %u: %s chunk written back (hr %#lx)", i, blocks[i].type, hr);
            else if (blocks[i].exact)
                CHECK(hr == S_OK && stat.cbSize.QuadPart == csize && !memcmp(saved, orig, csize),
                      "block %u: %s chunk written back byte for byte (hr %#lx, %lu of %lu bytes)", i, blocks[i].type, hr,
                      (ULONG)stat.cbSize.QuadPart, csize);
            else
                CHECK(hr == S_OK && stat.cbSize.QuadPart > 12 && !memcmp(saved + 4, blocks[i].type, 4),
                      "block %u: %s chunk written (hr %#lx, %lu bytes)", i, blocks[i].type, hr, (ULONG)stat.cbSize.QuadPart);
            GlobalUnlock(g);
            IStream_Release(st);
            IWICPersistStream_Release(ps);
            IWICMetadataWriter_Release(w);
        }
        IWICMetadataReader_Release(r);
    }
    check_values(frame, TRUE, "decoded");

    /* the PNG encoder writes the chunks it has a place for */
    {
        IWICBitmapEncoder *enc; IWICBitmapFrameEncode *fe; IWICMetadataBlockWriter *bw; IStream *st;
        IPropertyBag2 *bag; WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR;
        BYTE px[2 * 3 * 2] = {0}; HGLOBAL g; STATSTG stat;

        CreateStreamOnHGlobal(NULL, TRUE, &st);
        IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng_, NULL, &enc);
        IWICBitmapEncoder_Initialize(enc, st, WICBitmapEncoderNoCache);
        IWICBitmapEncoder_CreateNewFrame(enc, &fe, &bag);
        IWICBitmapFrameEncode_Initialize(fe, bag);
        IWICBitmapFrameEncode_SetSize(fe, 2, 2);
        IWICBitmapFrameEncode_SetPixelFormat(fe, &pf);
        hr = IWICBitmapFrameEncode_QueryInterface(fe, &IID_IWICMetadataBlockWriter_, (void **)&bw);
        if (hr == S_OK) hr = IWICMetadataBlockWriter_InitializeFromBlockReader(bw, br);
        CHECK(hr == S_OK, "encoder frame takes the blocks (hr %#lx)", hr);
        IWICBitmapFrameEncode_WritePixels(fe, 2, 6, sizeof(px), px);
        IWICBitmapFrameEncode_Commit(fe);
        hr = IWICBitmapEncoder_Commit(enc);
        IStream_Stat(st, &stat, STATFLAG_NONAME);
        GetHGlobalFromStream(st, &g);
        {
            const BYTE *out = GlobalLock(g); ULONG n = stat.cbSize.LowPart, cs;
            CHECK(hr == S_OK && find_chunk(out, n, "sRGB", 0, &cs) && find_chunk(out, n, "bKGD", 0, &cs)
                  && find_chunk(out, n, "iTXt", 0, &cs) && find_chunk(out, n, "iTXt", 1, &cs),
                  "encoded PNG has sRGB, bKGD and both iTXt chunks (hr %#lx, %lu bytes)", hr, n);
            if ((frame2 = decode(out, n))) check_values(frame2, FALSE, "re-encoded");
            GlobalUnlock(g);
        }
    }
end:
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
