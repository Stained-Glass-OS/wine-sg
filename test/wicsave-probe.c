/* windowscodecs: saving metadata (patches/sg/2621): IWICPersistStream::Save / SaveEx /
 * GetSizeMax of the metadata writers. PNG chunks (with their CRCs), GIF blocks, TIFF
 * directories (little and big endian, sorted, values inline and behind the table,
 * nested Exif directories), Exif APP1 data and unknown data; read back by the readers. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <propvarutil.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef struct { GUID clsid, format; } W;

static const GUID my_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID my_IID_IWICComponentFactory = {0x412d0c3a, 0x9650, 0x44fa, {0xaf, 0x5b, 0xdd, 0x2a, 0x06, 0xc8, 0xe8, 0xfb}};
static const GUID my_IID_IWICMetadataWriter = {0xf7836e16, 0x3be0, 0x470b, {0x86, 0xbb, 0x16, 0x0d, 0x0a, 0xec, 0xd7, 0xde}};
static const GUID my_IID_IWICMetadataReader = {0x9204fe99, 0xd8fc, 0x4fd5, {0xa0, 0x01, 0x95, 0x36, 0xb0, 0x67, 0xa8, 0x99}};
static const GUID my_IID_IWICPersistStream = {0x00675040, 0x6908, 0x45f8, {0x86, 0xa3, 0x49, 0xc7, 0xdf, 0xd6, 0xd9, 0xad}};

static const W unknown_w = {{0xa09cca86, 0x27ba, 0x4f39, {0x90, 0x53, 0x12, 0x1f, 0xa4, 0xdc, 0x08, 0xfc}}, {0xa45e592f, 0x9078, 0x4a7c, {0xad, 0xb5, 0x4e, 0xdc, 0x4f, 0xd6, 0x1b, 0x1f}}};
static const W ifd_w = {{0xb1ebfc28, 0xc9bd, 0x47a2, {0x8d, 0x33, 0xb9, 0x48, 0x76, 0x97, 0x77, 0xa7}}, {0x537396c6, 0x2d8a, 0x4bb6, {0x9b, 0xf8, 0x2f, 0x0a, 0x8e, 0x2a, 0x3a, 0xdf}}};
static const W exif_w = {{0xc9a14cda, 0xc339, 0x460b, {0x90, 0x78, 0xd4, 0xde, 0xbc, 0xfa, 0xbe, 0x91}}, {0x1c3c4f9d, 0xb84a, 0x467d, {0x94, 0x93, 0x36, 0xcf, 0xbd, 0x59, 0xea, 0x57}}};
static const W app1_w = {{0xee366069, 0x1832, 0x420f, {0xb3, 0x81, 0x04, 0x79, 0xad, 0x06, 0x6f, 0x19}}, {0x8fd3dfc3, 0xf951, 0x492b, {0x81, 0x7f, 0x69, 0xc2, 0xe6, 0xd9, 0xa5, 0xb0}}};
static const W chrm_w = {{0xe23ce3eb, 0x5608, 0x4e83, {0xbc, 0xef, 0x27, 0xb1, 0x98, 0x7e, 0x51, 0xd7}}, {0x9db3655b, 0x2842, 0x44b3, {0x80, 0x67, 0x12, 0xe9, 0xb3, 0x75, 0x55, 0x6a}}};
static const W gama_w = {{0xff036d13, 0x5d4b, 0x46dd, {0xb1, 0x0f, 0x10, 0x66, 0x93, 0xd9, 0xfe, 0x4f}}, {0xf00935a5, 0x1d5d, 0x4cd1, {0x81, 0xb2, 0x93, 0x24, 0xd7, 0xec, 0xa7, 0x81}}};
static const W hist_w = {{0x8a03e749, 0x672e, 0x446e, {0xbf, 0x1f, 0x2c, 0x11, 0xd2, 0x33, 0xb6, 0xff}}, {0xc59a82da, 0xdb74, 0x48a4, {0xbd, 0x6a, 0xb6, 0x9c, 0x49, 0x31, 0xef, 0x95}}};
static const W text_w = {{0xb5ebafb9, 0x253e, 0x4a72, {0xa7, 0x44, 0x07, 0x62, 0xd2, 0x68, 0x56, 0x83}}, {0x568d8936, 0xc0a9, 0x4923, {0x90, 0x5d, 0xdf, 0x2b, 0x38, 0x23, 0x8f, 0xbc}}};
static const W time_w = {{0x1ab78400, 0xb5a3, 0x4d91, {0x8a, 0xce, 0x33, 0xfc, 0xd1, 0x49, 0x9b, 0xe6}}, {0x6b00ae2d, 0xe24b, 0x460a, {0x98, 0xb6, 0x87, 0x8b, 0xd0, 0x30, 0x72, 0xfd}}};
static const W lsd_w = {{0x73c037e7, 0xe5d9, 0x4954, {0x87, 0x6a, 0x6d, 0xa8, 0x1d, 0x6e, 0x57, 0x68}}, {0xe256031e, 0x6299, 0x4929, {0xb9, 0x8d, 0x5a, 0xc8, 0x84, 0xaf, 0xba, 0x92}}};
static const W imd_w = {{0x8c89071f, 0x452e, 0x4e95, {0x96, 0x82, 0x9d, 0x10, 0x24, 0x62, 0x71, 0x72}}, {0xbd2bb086, 0x4d52, 0x48dd, {0x96, 0x77, 0xdb, 0x48, 0x3e, 0x85, 0xae, 0x8f}}};
static const W gce_w = {{0xaf95dc76, 0x16b2, 0x47f4, {0xb3, 0xea, 0x3c, 0x31, 0x79, 0x66, 0x93, 0xe7}}, {0x2a25cad8, 0xdeeb, 0x4c69, {0xa7, 0x88, 0x0e, 0xc2, 0x26, 0x6d, 0xca, 0xfd}}};
static const W ape_w = {{0xbd6edfca, 0x2890, 0x482f, {0xb2, 0x33, 0x8d, 0x73, 0x39, 0xa1, 0xcf, 0x8d}}, {0x2e043dc2, 0xc967, 0x4e05, {0x87, 0x5e, 0x61, 0x8b, 0xf6, 0x7e, 0x85, 0xc3}}};
static const W cmt_w = {{0xa02797fc, 0xc4ae, 0x418c, {0xaf, 0x95, 0xe6, 0x37, 0xc7, 0xea, 0xd2, 0xa1}}, {0xc4b6e0e0, 0xcfb4, 0x4ad3, {0xab, 0x33, 0x9a, 0xad, 0x23, 0x55, 0xa3, 0x4a}}};

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

static IWICComponentFactory *cfactory;

static IWICMetadataWriter *make(const W *w)
{
    IWICMetadataWriter *writer = NULL;

    CoCreateInstance(&w->clsid, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICMetadataWriter, (void **)&writer);
    return writer;
}

static void set(IWICMetadataWriter *w, const PROPVARIANT *id, const PROPVARIANT *value)
{
    HRESULT hr = IWICMetadataWriter_SetValue(w, NULL, id, value);

    if (FAILED(hr)) check(0, "SetValue = %#lx", hr);
}

static PROPVARIANT str_id(const WCHAR *s) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (WCHAR *)s; return v; }
static PROPVARIANT tag_id(USHORT t) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI2; v.uiVal = t; return v; }
static PROPVARIANT ui1(BYTE x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI1; v.bVal = x; return v; }
static PROPVARIANT ui2(USHORT x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI2; v.uiVal = x; return v; }
static PROPVARIANT ui4(ULONG x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = x; return v; }
static PROPVARIANT vbool(int x) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = x ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static PROPVARIANT lpstr(const char *s) { PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (char *)s; return v; }

/* saves; the bytes are in a stream that stays alive until the next call */
static IStream *last_stream;
static BYTE saved[4096];
static ULONG saved_len;

static HRESULT save(IWICMetadataWriter *w, DWORD options, BOOL clear_dirty, ULONGLONG *size_max)
{
    IWICPersistStream *ps = NULL;
    IStream *stream;
    HRESULT hr;
    STATSTG st;
    LARGE_INTEGER zero = {{0}};
    ULONG got = 0;
    ULARGE_INTEGER size;

    saved_len = 0;
    IWICMetadataWriter_QueryInterface(w, &my_IID_IWICPersistStream, (void **)&ps);
    if (!ps) return E_NOINTERFACE;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    size.QuadPart = 0xdeadbeef;
    if (size_max)
    {
        hr = IWICPersistStream_GetSizeMax(ps, &size);
        *size_max = SUCCEEDED(hr) ? size.QuadPart : ~0ull;
    }
    hr = IWICPersistStream_SaveEx(ps, stream, options, clear_dirty);
    if (SUCCEEDED(hr))
    {
        IStream_Stat(stream, &st, STATFLAG_NONAME);
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
        IStream_Read(stream, saved, min(st.cbSize.LowPart, sizeof(saved)), &got);
        saved_len = got;
    }
    IWICPersistStream_Release(ps);
    if (last_stream) IStream_Release(last_stream);
    last_stream = stream;
    return hr;
}

static void expect_bytes(const char *what, const BYTE *want, ULONG want_len, ULONGLONG size_max)
{
    int ok = saved_len == want_len && !memcmp(saved, want, want_len);
    ULONG i;

    check(ok, "%s: %lu bytes%s", what, saved_len, ok ? "" : " (differs)");
    if (!ok)
    {
        printf("      got ");
        for (i = 0; i < saved_len; ++i) printf("%02x ", saved[i]);
        printf("\n      want ");
        for (i = 0; i < want_len; ++i) printf("%02x ", want[i]);
        printf("\n");
    }
    check(size_max == want_len, "%s: GetSizeMax %I64u", what, size_max);
}

#define EXPECT(what, ...) do { static const BYTE want_[] = { __VA_ARGS__ }; expect_bytes(what, want_, sizeof(want_), size_max); } while (0)

static ULONG crc_table[256];
static ULONG crc32_of(const BYTE *data, ULONG len)
{
    ULONG c = 0xffffffff, i;

    if (!crc_table[1])
    {
        ULONG n, k, v;
        for (n = 0; n < 256; ++n) { v = n; for (k = 0; k < 8; ++k) v = (v & 1) ? 0xedb88320 ^ (v >> 1) : v >> 1; crc_table[n] = v; }
    }
    for (i = 0; i < len; ++i) c = crc_table[(c ^ data[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffff;
}

static void test_png(void)
{
    IWICMetadataWriter *w;
    ULONGLONG size_max;
    PROPVARIANT id, v;
    HRESULT hr;
    int i;

    w = make(&text_w);
    id = lpstr("Title"); v = lpstr("Hello"); set(w, &id, &v);
    hr = save(w, 0, FALSE, &size_max);
    check(hr == S_OK, "tEXt save = %#lx", hr);
    EXPECT("tEXt", 0x00, 0x00, 0x00, 0x0b, 0x74, 0x45, 0x58, 0x74, 0x54, 0x69, 0x74, 0x6c, 0x65, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0xcd, 0xcf, 0xc0, 0xcf);
    IWICMetadataWriter_Release(w);

    w = make(&text_w);
    id = str_id(L"Title"); v = lpstr("Hello"); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("tEXt with a wide keyword", 0x00, 0x00, 0x00, 0x0b, 0x74, 0x45, 0x58, 0x74, 0x54, 0x69, 0x74, 0x6c, 0x65, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0xcd, 0xcf, 0xc0, 0xcf);
    IWICMetadataWriter_Release(w);

    w = make(&text_w);
    id = lpstr(""); v = lpstr("x"); set(w, &id, &v);
    hr = save(w, 0, FALSE, NULL);
    check(hr == E_INVALIDARG, "an empty keyword = %#lx", hr);
    IWICMetadataWriter_Release(w);

    w = make(&text_w);
    {
        char longkey[90];
        memset(longkey, 'k', 80); longkey[80] = 0;
        id = lpstr(longkey); v = lpstr("x"); set(w, &id, &v);
        hr = save(w, 0, FALSE, NULL);
        check(hr == E_INVALIDARG, "a keyword of 80 characters = %#lx", hr);
    }
    IWICMetadataWriter_Release(w);

    w = make(&gama_w);
    id = str_id(L"ImageGamma"); v = ui4(45455); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("gAMA", 0x00, 0x00, 0x00, 0x04, 0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc, 0x61, 0x05);
    IWICMetadataWriter_Release(w);

    w = make(&time_w);
    {
        static const WCHAR *names[6] = { L"Year", L"Month", L"Day", L"Hour", L"Minute", L"Second" };
        static const BYTE vals[6] = { 0, 6, 3, 15, 7, 45 };

        for (i = 0; i < 6; ++i)
        {
            id = str_id(names[i]);
            v = i ? ui1(vals[i]) : ui2(2005);
            set(w, &id, &v);
        }
    }
    save(w, 0, FALSE, &size_max);
    EXPECT("tIME", 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xd5, 0x06, 0x03, 0x0f, 0x07, 0x2d, 0x12, 0x10, 0xf0, 0xfd);
    IWICMetadataWriter_Release(w);

    w = make(&hist_w);
    {
        USHORT freq[3] = {1, 2, 300};
        id = str_id(L"Frequencies");
        PropVariantInit(&v);
        v.vt = VT_UI2 | VT_VECTOR; v.caui.cElems = 3; v.caui.pElems = freq;
        set(w, &id, &v);
    }
    save(w, 0, FALSE, &size_max);
    EXPECT("hIST", 0x00, 0x00, 0x00, 0x06, 0x68, 0x49, 0x53, 0x54, 0x00, 0x01, 0x00, 0x02, 0x01, 0x2c, 0x4a, 0x9f, 0x00, 0x53);
    IWICMetadataWriter_Release(w);

    w = make(&chrm_w);
    {
        static const WCHAR *names[8] = { L"WhitePointX", L"WhitePointY", L"RedX", L"RedY", L"GreenX", L"GreenY", L"BlueX", L"BlueY" };
        for (i = 0; i < 8; ++i) { id = str_id(names[i]); v = ui4(1000 * (i + 1)); set(w, &id, &v); }
    }
    hr = save(w, 0, FALSE, &size_max);
    check(hr == S_OK && saved_len == 44 && saved[3] == 32 && !memcmp(saved + 4, "cHRM", 4)
            && saved[8 + 3] == 0xe8 && saved[8 + 2] == 3 && saved[8 + 31] == 0x40,
            "cHRM chunk (%lu bytes)", saved_len);
    check(saved_len == 44 && crc32_of(saved + 4, 36) == (ULONG)((saved[40] << 24) | (saved[41] << 16) | (saved[42] << 8) | saved[43]),
            "cHRM CRC");
    check(size_max == 44, "cHRM size %I64u", size_max);
    IWICMetadataWriter_Release(w);
}

static void test_gif(void)
{
    IWICMetadataWriter *w;
    ULONGLONG size_max;
    PROPVARIANT id, v;
    BYTE sig[6] = { 'G', 'I', 'F', '8', '9', 'a' };

    w = make(&lsd_w);
    id = str_id(L"Signature"); PropVariantInit(&v); v.vt = VT_UI1 | VT_VECTOR; v.caub.cElems = 6; v.caub.pElems = sig; set(w, &id, &v);
    id = str_id(L"Width"); v = ui2(320); set(w, &id, &v);
    id = str_id(L"Height"); v = ui2(200); set(w, &id, &v);
    id = str_id(L"GlobalColorTableFlag"); v = vbool(1); set(w, &id, &v);
    id = str_id(L"ColorResolution"); v = ui1(7); set(w, &id, &v);
    id = str_id(L"SortFlag"); v = vbool(0); set(w, &id, &v);
    id = str_id(L"GlobalColorTableSize"); v = ui1(3); set(w, &id, &v);
    id = str_id(L"BackgroundColorIndex"); v = ui1(5); set(w, &id, &v);
    id = str_id(L"PixelAspectRatio"); v = ui1(0); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("logical screen descriptor", 'G', 'I', 'F', '8', '9', 'a', 0x40, 0x01, 0xc8, 0x00, 0xf3, 0x05, 0x00);
    IWICMetadataWriter_Release(w);

    w = make(&imd_w);
    id = str_id(L"Left"); v = ui2(1); set(w, &id, &v);
    id = str_id(L"Top"); v = ui2(2); set(w, &id, &v);
    id = str_id(L"Width"); v = ui2(3); set(w, &id, &v);
    id = str_id(L"Height"); v = ui2(4); set(w, &id, &v);
    id = str_id(L"LocalColorTableFlag"); v = vbool(1); set(w, &id, &v);
    id = str_id(L"InterlaceFlag"); v = vbool(1); set(w, &id, &v);
    id = str_id(L"SortFlag"); v = vbool(0); set(w, &id, &v);
    id = str_id(L"LocalColorTableSize"); v = ui1(2); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("image descriptor", 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0xc2);
    IWICMetadataWriter_Release(w);

    w = make(&gce_w);
    id = str_id(L"Disposal"); v = ui1(2); set(w, &id, &v);
    id = str_id(L"UserInputFlag"); v = vbool(1); set(w, &id, &v);
    id = str_id(L"TransparencyFlag"); v = vbool(1); set(w, &id, &v);
    id = str_id(L"Delay"); v = ui2(10); set(w, &id, &v);
    id = str_id(L"TransparentColorIndex"); v = ui1(4); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("graphic control extension", 0x0b, 0x0a, 0x00, 0x04);
    IWICMetadataWriter_Release(w);

    w = make(&ape_w);
    {
        BYTE app[11] = { 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0' };
        BYTE data[4] = { 3, 1, 0, 0 };

        id = str_id(L"Application"); PropVariantInit(&v); v.vt = VT_UI1 | VT_VECTOR; v.caub.cElems = 11; v.caub.pElems = app; set(w, &id, &v);
        id = str_id(L"Data"); v.caub.cElems = 4; v.caub.pElems = data; set(w, &id, &v);
    }
    save(w, 0, FALSE, &size_max);
    EXPECT("application extension", 0x21, 0xff, 0x0b, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00);
    IWICMetadataWriter_Release(w);

    w = make(&cmt_w);
    id = str_id(L"TextEntry"); v = lpstr("hi"); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("comment", 0x21, 0xfe, 0x02, 'h', 'i', 0x00);
    IWICMetadataWriter_Release(w);

    w = make(&cmt_w);
    {
        char text[301];

        memset(text, 'a', 300); text[300] = 0;
        id = str_id(L"TextEntry"); v = lpstr(text); set(w, &id, &v);
        save(w, 0, FALSE, &size_max);
        check(saved_len == 2 + 1 + 255 + 1 + 45 + 1 && saved[2] == 255 && saved[2 + 256] == 45 && saved[saved_len - 1] == 0,
                "a long comment is cut into sub-blocks (%lu bytes)", saved_len);
        check(size_max == saved_len, "long comment size %I64u", size_max);
    }
    IWICMetadataWriter_Release(w);
}

static void test_unknown(void)
{
    IWICMetadataWriter *w = make(&unknown_w);
    ULONGLONG size_max;
    PROPVARIANT id, v;
    BYTE blob[5] = { 1, 2, 3, 4, 5 };

    PropVariantInit(&id);
    PropVariantInit(&v);
    v.vt = VT_BLOB; v.blob.cbSize = 5; v.blob.pBlobData = blob;
    set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("unknown metadata", 1, 2, 3, 4, 5);
    IWICMetadataWriter_Release(w);
}

static void test_ifd(void)
{
    IWICMetadataWriter *w;
    ULONGLONG size_max;
    PROPVARIANT id, v;
    HRESULT hr;

    w = make(&ifd_w);
    id = tag_id(0x0112); v = ui2(6); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("one short", 0x01, 0x00, 0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    IWICMetadataWriter_Release(w);

    w = make(&ifd_w);
    id = tag_id(0x0110); v = lpstr("ab"); set(w, &id, &v);
    id = tag_id(0x0100); v = ui4(640); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("sorted, string inline", 0x02, 0x00, 0x00, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x00,
            0x10, 0x01, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x61, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    save(w, WICPersistOptionBigEndian, FALSE, &size_max);
    EXPECT("big endian", 0x00, 0x02, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x80,
            0x01, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x61, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    IWICMetadataWriter_Release(w);

    w = make(&ifd_w);
    id = tag_id(0x010e); v = lpstr("Hello world"); set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("string behind the table", 0x01, 0x00, 0x0e, 0x01, 0x02, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', 0x00);
    IWICMetadataWriter_Release(w);

    w = make(&ifd_w);
    id = tag_id(0x011a); PropVariantInit(&v); v.vt = VT_UI8; v.uhVal.LowPart = 1; v.uhVal.HighPart = 3; set(w, &id, &v);
    save(w, 0, FALSE, &size_max);
    EXPECT("a rational", 0x01, 0x00, 0x1a, 0x01, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00);
    IWICMetadataWriter_Release(w);

    w = make(&ifd_w);
    {
        USHORT shorts[3] = {7, 8, 9};

        id = tag_id(0x0102); PropVariantInit(&v); v.vt = VT_UI2 | VT_VECTOR; v.caui.cElems = 3; v.caui.pElems = shorts; set(w, &id, &v);
    }
    save(w, 0, FALSE, &size_max);
    EXPECT("three shorts", 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x07, 0x00, 0x08, 0x00, 0x09, 0x00);
    IWICMetadataWriter_Release(w);

    /* a nested Exif directory */
    {
        IWICMetadataWriter *exif = make(&exif_w), *ifd = make(&ifd_w);
        PROPVARIANT idv, val;

        idv = tag_id(0x9003); val = lpstr("2020:01:02 03:04:05"); set(exif, &idv, &val);
        idv = tag_id(0x8769); PropVariantInit(&val); val.vt = VT_UNKNOWN; val.punkVal = (IUnknown *)exif;
        hr = IWICMetadataWriter_SetValue(ifd, NULL, &idv, &val);
        check(hr == S_OK, "an Exif writer as an item = %#lx", hr);
        hr = save(ifd, 0, FALSE, &size_max);
        check(hr == S_OK && saved_len > 18 + 18 && saved[0] == 1, "IFD with an Exif directory (%lu bytes, %#lx)", saved_len, hr);
        if (saved_len > 30)
        {
            ULONG offset = saved[10] | (saved[11] << 8);

            check(saved[2] == 0x69 && saved[3] == 0x87 && saved[4] == 4, "the pointer is a LONG tag 0x8769");
            check(offset >= 18 && offset + 18 <= saved_len && saved[offset] == 1 && saved[offset + 2] == 0x03 && saved[offset + 3] == 0x90,
                    "and leads to a directory with tag 0x9003 (offset %lu)", offset);
        }
        check(size_max == saved_len, "nested size %I64u", size_max);

        /* read back */
        {
            IWICMetadataReader *reader = NULL;
            PROPVARIANT out;
            IStream *copy = last_stream;

            IStream_AddRef(copy);
            {
                LARGE_INTEGER zero = {{0}};
                IStream_Seek(copy, zero, STREAM_SEEK_SET, NULL);
            }
            hr = IWICComponentFactory_CreateMetadataReader(cfactory, &ifd_w.format, NULL, 0, copy, &reader);
            check(hr == S_OK && reader, "read back the IFD (%#lx)", hr);
            if (reader)
            {
                PropVariantInit(&out);
                idv = tag_id(0x8769);
                hr = IWICMetadataReader_GetValue(reader, NULL, &idv, &out);
                check(hr == S_OK && out.vt == VT_UNKNOWN, "the Exif item is a reader (%#lx, vt %u)", hr, out.vt);
                if (hr == S_OK && out.vt == VT_UNKNOWN)
                {
                    IWICMetadataReader *nested = NULL;
                    PROPVARIANT s;

                    IUnknown_QueryInterface(out.punkVal, &my_IID_IWICMetadataReader, (void **)&nested);
                    PropVariantInit(&s);
                    idv = tag_id(0x9003);
                    hr = nested ? IWICMetadataReader_GetValue(nested, NULL, &idv, &s) : E_FAIL;
                    check(hr == S_OK && s.vt == VT_LPSTR && !strcmp(s.pszVal, "2020:01:02 03:04:05"), "with the date (%#lx)", hr);
                    if (hr == S_OK) PropVariantClear(&s);
                    if (nested) IWICMetadataReader_Release(nested);
                    PropVariantClear(&out);
                }
                IWICMetadataReader_Release(reader);
            }
            IStream_Release(copy);
        }
        IWICMetadataWriter_Release(ifd);
        IWICMetadataWriter_Release(exif);
    }
}

static void test_app1(void)
{
    IWICMetadataWriter *ifd = make(&ifd_w), *app1 = make(&app1_w);
    ULONGLONG size_max;
    PROPVARIANT id, v;
    HRESULT hr;

    id = tag_id(0x0112); v = ui2(6); set(ifd, &id, &v);
    id = tag_id(0); PropVariantInit(&v); v.vt = VT_UNKNOWN; v.punkVal = (IUnknown *)ifd;
    hr = IWICMetadataWriter_SetValue(app1, NULL, &id, &v);
    check(hr == S_OK, "an IFD writer as the App1 item = %#lx", hr);
    hr = save(app1, 0, FALSE, &size_max);
    check(hr == S_OK, "App1 save = %#lx", hr);
    EXPECT("App1, little endian", 'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 0x2a, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    save(app1, WICPersistOptionBigEndian, FALSE, &size_max);
    EXPECT("App1, big endian", 'E', 'x', 'i', 'f', 0, 0, 'M', 'M', 0x00, 0x2a, 0x00, 0x00, 0x00, 0x08,
            0x00, 0x01, 0x01, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

    save(app1, 0, FALSE, &size_max);
    {
        IWICMetadataReader *reader = NULL;
        LARGE_INTEGER zero = {{0}};
        PROPVARIANT out;

        IStream_Seek(last_stream, zero, STREAM_SEEK_SET, NULL);
        hr = IWICComponentFactory_CreateMetadataReader(cfactory, &app1_w.format, NULL, 0, last_stream, &reader);
        check(hr == S_OK && reader, "read back the App1 data (%#lx)", hr);
        if (reader)
        {
            PropVariantInit(&out);
            id = tag_id(0);
            hr = IWICMetadataReader_GetValue(reader, NULL, &id, &out);
            check(hr == S_OK && out.vt == VT_UNKNOWN, "App1 holds an IFD (%#lx)", hr);
            if (hr == S_OK) PropVariantClear(&out);
            IWICMetadataReader_Release(reader);
        }
    }
    IWICMetadataWriter_Release(app1);
    IWICMetadataWriter_Release(ifd);
}

static void test_state(void)
{
    IWICMetadataWriter *w = make(&gama_w);
    IWICPersistStream *ps = NULL;
    PROPVARIANT id, v;
    IStream *stream;
    HRESULT hr;

    IWICMetadataWriter_QueryInterface(w, &my_IID_IWICPersistStream, (void **)&ps);
    id = str_id(L"ImageGamma"); v = ui4(1); set(w, &id, &v);
    hr = IWICPersistStream_IsDirty(ps);
    check(hr == S_OK, "dirty after SetValue = %#lx", hr);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = IWICPersistStream_SaveEx(ps, stream, 0, FALSE);
    check(hr == S_OK, "SaveEx = %#lx", hr);
    check(IWICPersistStream_IsDirty(ps) == S_OK, "still dirty when not cleared");
    hr = IWICPersistStream_Save(ps, stream, TRUE);
    check(hr == S_OK, "Save = %#lx", hr);
    check(IWICPersistStream_IsDirty(ps) == S_FALSE, "clean after saving with clear");
    hr = IWICPersistStream_SaveEx(ps, NULL, 0, TRUE);
    check(hr == E_INVALIDARG, "no stream = %#lx", hr);
    IStream_Release(stream);
    IWICPersistStream_Release(ps);
    IWICMetadataWriter_Release(w);
}

int main(void)
{
    HRESULT hr;
    IWICImagingFactory *factory;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    check(hr == S_OK, "WIC factory (%#lx)", hr);
    if (FAILED(hr)) goto done;
    hr = IWICImagingFactory_QueryInterface(factory, &my_IID_IWICComponentFactory, (void **)&cfactory);
    check(hr == S_OK, "component factory (%#lx)", hr);
    if (FAILED(hr)) goto done;

    test_png();
    test_gif();
    test_unknown();
    test_ifd();
    test_app1();
    test_state();
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
