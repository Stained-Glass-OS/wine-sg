/* windowscodecs: metadata writers (patches/sg/2614): the writer classes, the
 * stream provider interface of readers and writers, SetValue / RemoveValue, the
 * factory's CreateMetadataWriter and CreateMetadataWriterFromReader, and the Exif
 * and GPS IFDs that an IFD holds as readers of their own. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <propvarutil.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* not in the mingw-w64 headers */
typedef struct IWICStreamProvider IWICStreamProvider;
typedef struct IWICStreamProviderVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICStreamProvider *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICStreamProvider *);
    ULONG (STDMETHODCALLTYPE *Release)(IWICStreamProvider *);
    HRESULT (STDMETHODCALLTYPE *GetStream)(IWICStreamProvider *, IStream **);
    HRESULT (STDMETHODCALLTYPE *GetPersistOptions)(IWICStreamProvider *, DWORD *);
    HRESULT (STDMETHODCALLTYPE *GetPreferredVendorGUID)(IWICStreamProvider *, GUID *);
    HRESULT (STDMETHODCALLTYPE *RefreshStream)(IWICStreamProvider *);
} IWICStreamProviderVtbl;
struct IWICStreamProvider { const IWICStreamProviderVtbl *lpVtbl; };
#define IWICStreamProvider_Release(p) (p)->lpVtbl->Release(p)
#define IWICStreamProvider_GetStream(p, a) (p)->lpVtbl->GetStream(p, a)
#define IWICStreamProvider_GetPersistOptions(p, a) (p)->lpVtbl->GetPersistOptions(p, a)
#define IWICStreamProvider_GetPreferredVendorGUID(p, a) (p)->lpVtbl->GetPreferredVendorGUID(p, a)
#define IWICStreamProvider_RefreshStream(p) (p)->lpVtbl->RefreshStream(p)

static const struct { const char *name; GUID clsid; GUID format; int writer; } handlers[] = {
    {"Unknown Writer", {0xa09cca86, 0x27ba, 0x4f39, {0x90, 0x53, 0x12, 0x1f, 0xa4, 0xdc, 0x08, 0xfc}}, {0xa45e592f, 0x9078, 0x4a7c, {0xad, 0xb5, 0x4e, 0xdc, 0x4f, 0xd6, 0x1b, 0x1f}}, 1},
    {"Ifd Writer", {0xb1ebfc28, 0xc9bd, 0x47a2, {0x8d, 0x33, 0xb9, 0x48, 0x76, 0x97, 0x77, 0xa7}}, {0x537396c6, 0x2d8a, 0x4bb6, {0x9b, 0xf8, 0x2f, 0x0a, 0x8e, 0x2a, 0x3a, 0xdf}}, 1},
    {"Exif Writer", {0xc9a14cda, 0xc339, 0x460b, {0x90, 0x78, 0xd4, 0xde, 0xbc, 0xfa, 0xbe, 0x91}}, {0x1c3c4f9d, 0xb84a, 0x467d, {0x94, 0x93, 0x36, 0xcf, 0xbd, 0x59, 0xea, 0x57}}, 1},
    {"Gps Writer", {0xcb8c13e4, 0x62b5, 0x4c96, {0xa4, 0x8b, 0x6b, 0xa6, 0xac, 0xe3, 0x9c, 0x76}}, {0x7134ab8a, 0x9351, 0x44ad, {0xaf, 0x62, 0x44, 0x8d, 0xb6, 0xb5, 0x02, 0xec}}, 1},
    {"App1 Writer", {0xee366069, 0x1832, 0x420f, {0xb3, 0x81, 0x04, 0x79, 0xad, 0x06, 0x6f, 0x19}}, {0x8fd3dfc3, 0xf951, 0x492b, {0x81, 0x7f, 0x69, 0xc2, 0xe6, 0xd9, 0xa5, 0xb0}}, 1},
    {"cHRM Writer", {0xe23ce3eb, 0x5608, 0x4e83, {0xbc, 0xef, 0x27, 0xb1, 0x98, 0x7e, 0x51, 0xd7}}, {0x9db3655b, 0x2842, 0x44b3, {0x80, 0x67, 0x12, 0xe9, 0xb3, 0x75, 0x55, 0x6a}}, 1},
    {"gAMA Writer", {0xff036d13, 0x5d4b, 0x46dd, {0xb1, 0x0f, 0x10, 0x66, 0x93, 0xd9, 0xfe, 0x4f}}, {0xf00935a5, 0x1d5d, 0x4cd1, {0x81, 0xb2, 0x93, 0x24, 0xd7, 0xec, 0xa7, 0x81}}, 1},
    {"hIST Writer", {0x8a03e749, 0x672e, 0x446e, {0xbf, 0x1f, 0x2c, 0x11, 0xd2, 0x33, 0xb6, 0xff}}, {0xc59a82da, 0xdb74, 0x48a4, {0xbd, 0x6a, 0xb6, 0x9c, 0x49, 0x31, 0xef, 0x95}}, 1},
    {"tEXt Writer", {0xb5ebafb9, 0x253e, 0x4a72, {0xa7, 0x44, 0x07, 0x62, 0xd2, 0x68, 0x56, 0x83}}, {0x568d8936, 0xc0a9, 0x4923, {0x90, 0x5d, 0xdf, 0x2b, 0x38, 0x23, 0x8f, 0xbc}}, 1},
    {"tIME Writer", {0x1ab78400, 0xb5a3, 0x4d91, {0x8a, 0xce, 0x33, 0xfc, 0xd1, 0x49, 0x9b, 0xe6}}, {0x6b00ae2d, 0xe24b, 0x460a, {0x98, 0xb6, 0x87, 0x8b, 0xd0, 0x30, 0x72, 0xfd}}, 1},
    {"LSD Writer", {0x73c037e7, 0xe5d9, 0x4954, {0x87, 0x6a, 0x6d, 0xa8, 0x1d, 0x6e, 0x57, 0x68}}, {0xe256031e, 0x6299, 0x4929, {0xb9, 0x8d, 0x5a, 0xc8, 0x84, 0xaf, 0xba, 0x92}}, 1},
    {"IMD Writer", {0x8c89071f, 0x452e, 0x4e95, {0x96, 0x82, 0x9d, 0x10, 0x24, 0x62, 0x71, 0x72}}, {0xbd2bb086, 0x4d52, 0x48dd, {0x96, 0x77, 0xdb, 0x48, 0x3e, 0x85, 0xae, 0x8f}}, 1},
    {"GCE Writer", {0xaf95dc76, 0x16b2, 0x47f4, {0xb3, 0xea, 0x3c, 0x31, 0x79, 0x66, 0x93, 0xe7}}, {0x2a25cad8, 0xdeeb, 0x4c69, {0xa7, 0x88, 0x0e, 0xc2, 0x26, 0x6d, 0xca, 0xfd}}, 1},
    {"APE Writer", {0xbd6edfca, 0x2890, 0x482f, {0xb2, 0x33, 0x8d, 0x73, 0x39, 0xa1, 0xcf, 0x8d}}, {0x2e043dc2, 0xc967, 0x4e05, {0x87, 0x5e, 0x61, 0x8b, 0xf6, 0x7e, 0x85, 0xc3}}, 1},
    {"GIF comment Writer", {0xa02797fc, 0xc4ae, 0x418c, {0xaf, 0x95, 0xe6, 0x37, 0xc7, 0xea, 0xd2, 0xa1}}, {0xc4b6e0e0, 0xcfb4, 0x4ad3, {0xab, 0x33, 0x9a, 0xad, 0x23, 0x55, 0xa3, 0x4a}}, 1},
    {"Exif Reader", {0xd9403860, 0x297f, 0x4a49, {0xbf, 0x9b, 0x77, 0x89, 0x81, 0x50, 0xa4, 0x42}}, {0x1c3c4f9d, 0xb84a, 0x467d, {0x94, 0x93, 0x36, 0xcf, 0xbd, 0x59, 0xea, 0x57}}, 0},
    {"Gps Reader", {0x3697790b, 0x223b, 0x484e, {0x99, 0x25, 0xc4, 0x86, 0x92, 0x18, 0xf1, 0x7a}}, {0x7134ab8a, 0x9351, 0x44ad, {0xaf, 0x62, 0x44, 0x8d, 0xb6, 0xb5, 0x02, 0xec}}, 0},
    {"App1 Reader", {0xdde33513, 0x774e, 0x4bcd, {0xae, 0x79, 0x02, 0xf4, 0xad, 0xfe, 0x62, 0xfc}}, {0x8fd3dfc3, 0xf951, 0x492b, {0x81, 0x7f, 0x69, 0xc2, 0xe6, 0xd9, 0xa5, 0xb0}}, 0},
};

static const GUID my_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_IID_IWICComponentFactory = {0x412d0c3a, 0x9650, 0x44fa, {0xaf, 0x5b, 0xdd, 0x2a, 0x06, 0xc8, 0xe8, 0xfb}};
static const GUID my_IID_IWICMetadataWriter = {0xf7836e16, 0x3be0, 0x470b, {0x86, 0xbb, 0x16, 0x0d, 0x0a, 0xec, 0xd7, 0xde}};
static const GUID my_IID_IWICMetadataReader = {0x9204fe99, 0xd8fc, 0x4fd5, {0xa0, 0x01, 0x95, 0x36, 0xb0, 0x67, 0xa8, 0x99}};
static const GUID my_IID_IWICPersistStream = {0x00675040, 0x6908, 0x45f8, {0x86, 0xa3, 0x49, 0xc7, 0xdf, 0xd6, 0xd9, 0xad}};
static const GUID my_IID_IWICStreamProvider = {0x449494bc, 0xb468, 0x4927, {0x96, 0xd7, 0xba, 0x90, 0xd3, 0x1a, 0xb5, 0x05}};
static const GUID my_IID_IWICMetadataWriterInfo = {0xb22e3fba, 0x3925, 0x4323, {0xb5, 0xc1, 0x9e, 0xbf, 0xc4, 0x30, 0xf2, 0x36}};
static const GUID my_IID_IPersistStream = {0x00000109, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID my_vendor_ms = {0xf0e749ca, 0xedef, 0x4589, {0xa7, 0x3a, 0xee, 0x0e, 0x62, 0x6a, 0x2a, 0x2b}};
static const GUID my_vendor_other = {0x11223344, 0x5566, 0x7788, {0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00}};
static const GUID my_format_text = {0x568d8936, 0xc0a9, 0x4923, {0x90, 0x5d, 0xdf, 0x2b, 0x38, 0x23, 0x8f, 0xbc}};
static const GUID my_format_unknown = {0xa45e592f, 0x9078, 0x4a7c, {0xad, 0xb5, 0x4e, 0xdc, 0x4f, 0xd6, 0x1b, 0x1f}};
static const GUID my_format_exif = {0x1c3c4f9d, 0xb84a, 0x467d, {0x94, 0x93, 0x36, 0xcf, 0xbd, 0x59, 0xea, 0x57}};
static const GUID my_format_gps = {0x7134ab8a, 0x9351, 0x44ad, {0xaf, 0x62, 0x44, 0x8d, 0xb6, 0xb5, 0x02, 0xec}};
static const GUID my_format_ifd = {0x537396c6, 0x2d8a, 0x4bb6, {0x9b, 0xf8, 0x2f, 0x0a, 0x8e, 0x2a, 0x3a, 0xdf}};
static const GUID my_format_app1 = {0x8fd3dfc3, 0xf951, 0x492b, {0x81, 0x7f, 0x69, 0xc2, 0xe6, 0xd9, 0xa5, 0xb0}};

static int failures;
static IWICComponentFactory *factory;

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

static IStream *make_stream(const void *data, UINT size)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, size);
    IStream *stream = NULL;

    memcpy(GlobalLock(h), data, size);
    GlobalUnlock(h);
    CreateStreamOnHGlobal(h, TRUE, &stream);
    return stream;
}

static const BYTE text_chunk[] = {
    0, 0, 0, 14, 't', 'E', 'X', 't', 'w', 'i', 'n', 'e', 't', 'e', 's', 't', 0, 'v', 'a', 'l', 'u', 'e', 0x3f, 0x64, 0x19, 0xf3
};

/* ---- the classes ------------------------------------------------------ */

static void test_classes(void)
{
    unsigned i;

    for (i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++)
    {
        IUnknown *unk = NULL, *other = NULL;
        IWICMetadataReader *reader = NULL;
        IWICMetadataWriter *writer = NULL;
        IWICMetadataHandlerInfo *info = NULL;
        IWICMetadataWriterInfo *winfo = NULL;
        IWICPersistStream *persist = NULL;
        CLSID clsid;
        WICComponentType type = 0;
        GUID format;
        UINT count = 77;
        HRESULT hr;
        const char *n = handlers[i].name;

        hr = CoCreateInstance(&handlers[i].clsid, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICMetadataReader, (void **)&reader);
        check(hr == S_OK && reader, "%s: CoCreateInstance (%#lx)", n, hr);
        if (!reader) continue;
        unk = (IUnknown *)reader;

        hr = IUnknown_QueryInterface(unk, &my_IID_IWICMetadataWriter, (void **)&writer);
        check(handlers[i].writer ? (hr == S_OK && writer) : (hr == E_NOINTERFACE && !writer), "%s: IWICMetadataWriter %s (%#lx)", n,
              handlers[i].writer ? "present" : "absent", hr);
        if (writer) IWICMetadataWriter_Release(writer);

        hr = IUnknown_QueryInterface(unk, &my_IID_IWICStreamProvider, (void **)&other);
        check(hr == S_OK, "%s: IWICStreamProvider (%#lx)", n, hr);
        if (other) IUnknown_Release(other);
        hr = IUnknown_QueryInterface(unk, &my_IID_IWICPersistStream, (void **)&persist);
        check(hr == S_OK, "%s: IWICPersistStream (%#lx)", n, hr);

        if (persist)
        {
            memset(&clsid, 0, sizeof(clsid));
            hr = IPersistStream_GetClassID((IPersistStream *)persist, &clsid);
            check(hr == S_OK && IsEqualGUID(&clsid, &handlers[i].clsid), "%s: GetClassID is the class (%#lx)", n, hr);
            hr = IPersistStream_IsDirty((IPersistStream *)persist);
            check(hr == S_FALSE, "%s: not dirty when new (%#lx)", n, hr);
            IWICPersistStream_Release(persist);
        }

        hr = IWICMetadataReader_GetMetadataFormat(reader, &format);
        check(hr == S_OK && IsEqualGUID(&format, &handlers[i].format), "%s: format (%#lx)", n, hr);
        hr = IWICMetadataReader_GetCount(reader, &count);
        check(hr == S_OK && count == 0, "%s: no items yet (%#lx, %u)", n, hr, count);

        hr = IWICMetadataReader_GetMetadataHandlerInfo(reader, &info);
        check(hr == S_OK && info, "%s: handler info (%#lx)", n, hr);
        if (info)
        {
            hr = IWICMetadataHandlerInfo_GetComponentType(info, &type);
            check(hr == S_OK && type == (handlers[i].writer ? WICMetadataWriter : WICMetadataReader), "%s: component type %#x", n, type);
            hr = IWICMetadataHandlerInfo_QueryInterface(info, &my_IID_IWICMetadataWriterInfo, (void **)&winfo);
            check(handlers[i].writer ? (hr == S_OK) : (hr == E_NOINTERFACE), "%s: IWICMetadataWriterInfo (%#lx)", n, hr);
            if (winfo)
            {
                IWICMetadataWriter *created = NULL;

                hr = IWICMetadataWriterInfo_CreateInstance(winfo, &created);
                check(hr == S_OK && created, "%s: the info creates a writer (%#lx)", n, hr);
                if (created) IWICMetadataWriter_Release(created);
                IWICMetadataWriterInfo_Release(winfo);
            }
            IWICMetadataHandlerInfo_Release(info);
        }

        IWICMetadataReader_Release(reader);
    }
}

/* ---- SetValue, RemoveValue --------------------------------------------- */

static PROPVARIANT lpstr(const char *s)
{
    PROPVARIANT v;

    PropVariantInit(&v);
    v.vt = VT_LPSTR;
    v.pszVal = CoTaskMemAlloc(strlen(s) + 1);
    strcpy(v.pszVal, s);
    return v;
}

static int value_is(IWICMetadataWriter *w, UINT index, const char *id, const char *value)
{
    PROPVARIANT i, v;
    int ok;

    PropVariantInit(&i);
    PropVariantInit(&v);
    ok = SUCCEEDED(IWICMetadataWriter_GetValueByIndex(w, index, NULL, &i, &v)) && i.vt == VT_LPSTR && v.vt == VT_LPSTR &&
         !strcmp(i.pszVal, id) && !strcmp(v.pszVal, value);
    PropVariantClear(&i);
    PropVariantClear(&v);
    return ok;
}

static void test_set_remove(void)
{
    IWICMetadataWriter *w = NULL;
    IPersistStream *ps = NULL;
    PROPVARIANT id1 = lpstr("one"), id2 = lpstr("two"), val_a = lpstr("a"), val_b = lpstr("b"), val_c = lpstr("c"), got;
    UINT count;
    HRESULT hr;

    hr = CoCreateInstance(&handlers[8].clsid, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICMetadataWriter, (void **)&w);
    check(hr == S_OK, "tEXt writer for SetValue (%#lx)", hr);
    if (!w) return;
    IWICMetadataWriter_QueryInterface(w, &my_IID_IPersistStream, (void **)&ps);

    hr = IWICMetadataWriter_SetValue(w, NULL, NULL, &val_a);
    check(hr == E_INVALIDARG, "SetValue without an id = %#lx", hr);
    hr = IWICMetadataWriter_SetValue(w, NULL, &id1, NULL);
    check(hr == E_INVALIDARG, "SetValue without a value = %#lx", hr);

    hr = IWICMetadataWriter_SetValue(w, NULL, &id1, &val_a);
    IWICMetadataWriter_GetCount(w, &count);
    check(hr == S_OK && count == 1 && value_is(w, 0, "one", "a"), "SetValue adds an item (%#lx, %u)", hr, count);
    check(IPersistStream_IsDirty(ps) == S_OK, "the writer is dirty after SetValue");

    hr = IWICMetadataWriter_SetValue(w, NULL, &id1, &val_b);
    IWICMetadataWriter_GetCount(w, &count);
    check(hr == S_OK && count == 1 && value_is(w, 0, "one", "b"), "SetValue with the same id replaces the value (%#lx, %u)", hr, count);

    hr = IWICMetadataWriter_SetValue(w, NULL, &id2, &val_c);
    IWICMetadataWriter_GetCount(w, &count);
    check(hr == S_OK && count == 2 && value_is(w, 1, "two", "c"), "a second id is a second item (%#lx, %u)", hr, count);

    PropVariantInit(&got);
    hr = IWICMetadataWriter_GetValue(w, NULL, &id2, &got);
    check(hr == S_OK && got.vt == VT_LPSTR && !strcmp(got.pszVal, "c"), "GetValue finds it (%#lx)", hr);
    PropVariantClear(&got);

    hr = IWICMetadataWriter_SetValueByIndex(w, 5, NULL, &id1, &val_a);
    check(hr == E_INVALIDARG, "SetValueByIndex past the end = %#lx", hr);
    hr = IWICMetadataWriter_SetValueByIndex(w, 0, NULL, &id2, &val_a);
    IWICMetadataWriter_GetCount(w, &count);
    check(hr == S_OK && count == 2 && value_is(w, 0, "two", "a"), "SetValueByIndex replaces the item (%#lx)", hr);

    hr = IWICMetadataWriter_RemoveValue(w, NULL, &id1);
    check(hr == WINCODEC_ERR_PROPERTYNOTFOUND, "RemoveValue of an unknown id = %#lx", hr);
    hr = IWICMetadataWriter_RemoveValue(w, NULL, NULL);
    check(hr == E_INVALIDARG, "RemoveValue without an id = %#lx", hr);
    hr = IWICMetadataWriter_RemoveValue(w, NULL, &id2);
    IWICMetadataWriter_GetCount(w, &count);
    check(hr == S_OK && count == 1 && value_is(w, 0, "two", "c"), "RemoveValue removes the first match (%#lx, %u)", hr, count);

    hr = IWICMetadataWriter_RemoveValueByIndex(w, 3);
    check(hr == E_INVALIDARG, "RemoveValueByIndex past the end = %#lx", hr);
    hr = IWICMetadataWriter_RemoveValueByIndex(w, 0);
    IWICMetadataWriter_GetCount(w, &count);
    check(hr == S_OK && count == 0, "RemoveValueByIndex (%#lx, %u)", hr, count);

    PropVariantClear(&id1); PropVariantClear(&id2); PropVariantClear(&val_a); PropVariantClear(&val_b); PropVariantClear(&val_c);
    if (ps) IPersistStream_Release(ps);
    IWICMetadataWriter_Release(w);
}

/* ---- the stream provider ----------------------------------------------- */

static void test_stream_provider(void)
{
    IWICMetadataWriter *w = NULL;
    IWICPersistStream *persist = NULL;
    IWICStreamProvider *sp = NULL;
    IStream *stream = make_stream(text_chunk, sizeof(text_chunk)), *got;
    GUID vendor;
    DWORD options;
    UINT count;
    HRESULT hr;

    CoCreateInstance(&handlers[8].clsid, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICMetadataWriter, (void **)&w);
    IWICMetadataWriter_QueryInterface(w, &my_IID_IWICPersistStream, (void **)&persist);
    IWICMetadataWriter_QueryInterface(w, &my_IID_IWICStreamProvider, (void **)&sp);

    hr = IWICStreamProvider_GetPersistOptions(sp, NULL);
    check(hr == E_INVALIDARG, "GetPersistOptions(NULL) = %#lx", hr);
    hr = IWICStreamProvider_GetPreferredVendorGUID(sp, NULL);
    check(hr == E_INVALIDARG, "GetPreferredVendorGUID(NULL) = %#lx", hr);
    hr = IWICStreamProvider_GetStream(sp, NULL);
    check(hr == E_INVALIDARG, "GetStream(NULL) = %#lx", hr);
    options = 123;
    hr = IWICStreamProvider_GetPersistOptions(sp, &options);
    check(hr == S_OK && options == 0, "options before any load (%#lx, %lu)", hr, options);
    memset(&vendor, 0, sizeof(vendor));
    hr = IWICStreamProvider_GetPreferredVendorGUID(sp, &vendor);
    check(hr == S_OK && IsEqualGUID(&vendor, &my_vendor_ms), "vendor before any load is the default one (%#lx)", hr);
    got = (IStream *)0xdeadbeef;
    hr = IWICStreamProvider_GetStream(sp, &got);
    check(hr == WINCODEC_ERR_STREAMNOTAVAILABLE && got == (IStream *)0xdeadbeef, "no stream before any load (%#lx)", hr);

    /* cached */
    hr = IWICPersistStream_LoadEx(persist, stream, &my_vendor_other, WICPersistOptionPreferUTF8);
    check(hr == S_OK, "LoadEx = %#lx", hr);
    IWICMetadataWriter_GetCount(w, &count);
    check(count == 1, "one item loaded (%u)", count);
    options = ~0u;
    IWICStreamProvider_GetPersistOptions(sp, &options);
    check(options == WICPersistOptionPreferUTF8, "options are kept (%#lx)", options);
    memset(&vendor, 0, sizeof(vendor));
    IWICStreamProvider_GetPreferredVendorGUID(sp, &vendor);
    check(IsEqualGUID(&vendor, &my_vendor_other), "the preferred vendor is kept");
    got = NULL;
    hr = IWICStreamProvider_GetStream(sp, &got);
    check(hr == S_OK && got == stream, "the stream is kept (%#lx)", hr);
    if (got) IStream_Release(got);

    /* a load without a stream keeps the items and drops the stream */
    hr = IWICPersistStream_LoadEx(persist, NULL, NULL, 0);
    IWICMetadataWriter_GetCount(w, &count);
    check(hr == S_OK && count == 1, "LoadEx(NULL) keeps the items (%#lx, %u)", hr, count);
    got = (IStream *)0xdeadbeef;
    hr = IWICStreamProvider_GetStream(sp, &got);
    check(hr == WINCODEC_ERR_STREAMNOTAVAILABLE, "and has no stream (%#lx)", hr);
    memset(&vendor, 0, sizeof(vendor));
    IWICStreamProvider_GetPreferredVendorGUID(sp, &vendor);
    check(IsEqualGUID(&vendor, &my_vendor_ms), "vendor back to the default one");

    /* not cached */
    {
        LARGE_INTEGER zero = {{0}};
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    }
    hr = IWICPersistStream_LoadEx(persist, stream, NULL, WICPersistOptionNoCacheStream);
    check(hr == S_OK, "LoadEx with NoCacheStream = %#lx", hr);
    got = (IStream *)0xdeadbeef;
    hr = IWICStreamProvider_GetStream(sp, &got);
    check(hr == WINCODEC_ERR_STREAMNOTAVAILABLE && got == (IStream *)0xdeadbeef, "NoCacheStream: no stream (%#lx)", hr);
    IWICStreamProvider_GetPersistOptions(sp, &options);
    check(options == WICPersistOptionNoCacheStream, "NoCacheStream is the option (%#lx)", options);

    hr = IWICStreamProvider_RefreshStream(sp);
    check(hr == WINCODEC_ERR_STREAMNOTAVAILABLE, "RefreshStream without a stream = %#lx", hr);

    IWICStreamProvider_Release(sp);
    IWICPersistStream_Release(persist);
    IWICMetadataWriter_Release(w);
    IStream_Release(stream);
}

/* ---- the factory ------------------------------------------------------- */

static void test_factory(void)
{
    IWICMetadataWriter *writer = (IWICMetadataWriter *)0xdeadbeef;
    IWICMetadataReader *reader = NULL;
    IWICStreamProvider *sp;
    IStream *stream, *got;
    GUID format;
    UINT count;
    HRESULT hr;

    hr = IWICComponentFactory_CreateMetadataWriter(factory, NULL, NULL, 0, &writer);
    check(hr == E_INVALIDARG, "CreateMetadataWriter(NULL format) = %#lx", hr);
    hr = IWICComponentFactory_CreateMetadataWriter(factory, &my_format_text, NULL, 0, NULL);
    check(hr == E_INVALIDARG, "CreateMetadataWriter(NULL result) = %#lx", hr);
    hr = IWICComponentFactory_CreateMetadataWriter(factory, &my_format_app1, NULL, WICPersistOptionBigEndian, &writer);
    check(hr == E_INVALIDARG, "CreateMetadataWriter with BigEndian = %#lx", hr);
    hr = IWICComponentFactory_CreateMetadataWriter(factory, &my_format_ifd, NULL, WICPersistOptionNoCacheStream, &writer);
    check(hr == E_INVALIDARG, "CreateMetadataWriter with NoCacheStream = %#lx", hr);
    hr = IWICComponentFactory_CreateMetadataWriter(factory, &my_format_app1, NULL, 0x100, &writer);
    check(hr == E_INVALIDARG, "CreateMetadataWriter with a stray option = %#lx", hr);

    writer = NULL;
    hr = IWICComponentFactory_CreateMetadataWriter(factory, &my_format_text, NULL, 0, &writer);
    check(hr == S_OK && writer, "CreateMetadataWriter(tEXt) = %#lx", hr);
    if (writer)
    {
        hr = IWICMetadataWriter_GetMetadataFormat(writer, &format);
        check(hr == S_OK && IsEqualGUID(&format, &my_format_text), "it is a tEXt writer");
        IWICMetadataWriter_QueryInterface(writer, &my_IID_IWICStreamProvider, (void **)&sp);
        got = (IStream *)0xdeadbeef;
        hr = IWICStreamProvider_GetStream(sp, &got);
        check(hr == WINCODEC_ERR_STREAMNOTAVAILABLE, "and has no stream");
        IWICStreamProvider_Release(sp);
        IWICMetadataWriter_Release(writer);
    }

    writer = NULL;
    hr = IWICComponentFactory_CreateMetadataWriter(factory, &IID_IUnknown, NULL, 0, &writer);
    check(hr == S_OK && writer, "an unknown format gives a writer (%#lx)", hr);
    if (writer)
    {
        hr = IWICMetadataWriter_GetMetadataFormat(writer, &format);
        check(hr == S_OK && IsEqualGUID(&format, &my_format_unknown), "which is the unknown one");
        IWICMetadataWriter_Release(writer);
    }
    writer = (IWICMetadataWriter *)0xdeadbeef;
    hr = IWICComponentFactory_CreateMetadataWriter(factory, &IID_IUnknown, NULL, WICMetadataCreationFailUnknown, &writer);
    check(hr == WINCODEC_ERR_COMPONENTNOTFOUND && writer == (IWICMetadataWriter *)0xdeadbeef, "FailUnknown (%#lx)", hr);

    /* from a reader */
    hr = IWICComponentFactory_CreateMetadataWriterFromReader(factory, NULL, NULL, &writer);
    check(hr == E_INVALIDARG, "FromReader(NULL reader) = %#lx", hr);
    stream = make_stream(text_chunk, sizeof(text_chunk));
    hr = IWICComponentFactory_CreateMetadataReader(factory, &my_format_text, NULL, 0, stream, &reader);
    check(hr == S_OK && reader, "a tEXt reader (%#lx)", hr);
    hr = IWICComponentFactory_CreateMetadataWriterFromReader(factory, reader, NULL, NULL);
    check(hr == E_INVALIDARG, "FromReader(NULL result) = %#lx", hr);
    writer = NULL;
    hr = IWICComponentFactory_CreateMetadataWriterFromReader(factory, reader, NULL, &writer);
    check(hr == S_OK && writer, "FromReader = %#lx", hr);
    if (writer)
    {
        hr = IWICMetadataWriter_GetMetadataFormat(writer, &format);
        check(hr == S_OK && IsEqualGUID(&format, &my_format_text), "the writer has the format of the reader");
        IWICMetadataWriter_GetCount(writer, &count);
        check(count == 1 && value_is(writer, 0, "winetest", "value"), "and its items (%u)", count);
        IWICMetadataWriter_QueryInterface(writer, &my_IID_IWICStreamProvider, (void **)&sp);
        got = NULL;
        hr = IWICStreamProvider_GetStream(sp, &got);
        check(hr == S_OK && got && got != stream, "it keeps a stream of its own (%#lx)", hr);
        if (got) IStream_Release(got);
        IWICStreamProvider_Release(sp);
        IWICMetadataWriter_Release(writer);
    }
    IWICMetadataReader_Release(reader);
    IStream_Release(stream);
}

/* ---- nested IFDs --------------------------------------------------------- */

#pragma pack(push, 2)
struct ifd_entry { USHORT id; USHORT type; ULONG count; ULONG value; };
static const struct app1
{
    BYTE header[6];
    BYTE bom[2];
    USHORT marker;
    ULONG ifd0_offset;
    USHORT ifd0_count;
    struct ifd_entry ifd0[4];
    ULONG next;
    USHORT exif_count;
    struct ifd_entry exif[1];
    ULONG next2;
    USHORT gps_count;
    struct ifd_entry gps[1];
    ULONG next3;
} app1 = {
    {'E', 'x', 'i', 'f', 0, 0}, {'I', 'I'}, 0x2a, 8,
    4, {{0x100, 4, 1, 222}, {0x101, 4, 1, 333},
        {0x8769, 4, 1, FIELD_OFFSET(struct app1, exif_count) - 6},
        {0x8825, 4, 1, FIELD_OFFSET(struct app1, gps_count) - 6}}, 0,
    1, {{0x200, 3, 1, 444}}, 0,
    1, {{0x300, 3, 1, 555}}, 0,
};
#pragma pack(pop)

static IWICMetadataReader *nested_reader(IWICMetadataReader *parent, UINT index)
{
    PROPVARIANT value;
    IWICMetadataReader *reader = NULL;

    PropVariantInit(&value);
    if (SUCCEEDED(IWICMetadataReader_GetValueByIndex(parent, index, NULL, NULL, &value)) && value.vt == VT_UNKNOWN && value.punkVal)
        IUnknown_QueryInterface(value.punkVal, &my_IID_IWICMetadataReader, (void **)&reader);
    PropVariantClear(&value);
    return reader;
}

static void test_nested(void)
{
    IWICMetadataReader *app1_reader = NULL, *ifd, *exif, *gps;
    IWICMetadataWriter *writer = NULL, *ifd_writer = NULL;
    IStream *stream = make_stream(&app1, sizeof(app1));
    PROPVARIANT id, value;
    GUID format;
    DWORD options;
    UINT count;
    IWICStreamProvider *sp;
    HRESULT hr;

    hr = IWICComponentFactory_CreateMetadataReader(factory, &my_format_app1, NULL, 0, stream, &app1_reader);
    check(hr == S_OK && app1_reader, "an App1 reader (%#lx)", hr);
    if (!app1_reader) return;

    PropVariantInit(&id);
    PropVariantInit(&value);
    hr = IWICMetadataReader_GetValueByIndex(app1_reader, 0, NULL, &id, &value);
    check(hr == S_OK && id.vt == VT_UI2 && id.uiVal == 0 && value.vt == VT_UNKNOWN, "the App1 item is the IFD number 0 (vt %u)", id.vt);
    PropVariantClear(&value);

    ifd = nested_reader(app1_reader, 0);
    check(ifd != NULL, "the IFD is a reader");
    if (ifd)
    {
        IWICMetadataReader_GetCount(ifd, &count);
        check(count == 4, "the IFD has four items (%u)", count);
        PropVariantInit(&value);
        IWICMetadataReader_GetValueByIndex(ifd, 0, NULL, &id, &value);
        check(id.uiVal == 0x100 && value.vt == VT_UI4 && value.ulVal == 222, "a plain item stays a plain item");
        PropVariantClear(&value);

        exif = nested_reader(ifd, 2);
        check(exif != NULL, "the Exif pointer holds a reader");
        if (exif)
        {
            hr = IWICMetadataReader_GetMetadataFormat(exif, &format);
            check(hr == S_OK && IsEqualGUID(&format, &my_format_exif), "which is an Exif IFD");
            IWICMetadataReader_GetCount(exif, &count);
            PropVariantInit(&value);
            IWICMetadataReader_GetValueByIndex(exif, 0, NULL, &id, &value);
            check(count == 1 && id.uiVal == 0x200 && value.ulVal == 444, "with its own items (%u)", count);
            PropVariantClear(&value);
            IWICMetadataReader_QueryInterface(exif, &my_IID_IWICStreamProvider, (void **)&sp);
            options = 99;
            IWICStreamProvider_GetPersistOptions(sp, &options);
            check(options == 0, "and persist options 0 (%#lx)", options);
            IWICStreamProvider_Release(sp);
            IWICMetadataReader_Release(exif);
        }
        gps = nested_reader(ifd, 3);
        check(gps != NULL, "the GPS pointer holds a reader");
        if (gps)
        {
            hr = IWICMetadataReader_GetMetadataFormat(gps, &format);
            check(hr == S_OK && IsEqualGUID(&format, &my_format_gps), "which is a GPS IFD");
            IWICMetadataReader_Release(gps);
        }
    }

    hr = IWICComponentFactory_CreateMetadataWriterFromReader(factory, app1_reader, NULL, &writer);
    check(hr == S_OK && writer, "an App1 writer from the reader (%#lx)", hr);
    if (writer)
    {
        ifd_writer = NULL;
        {
            IWICMetadataReader *r = nested_reader((IWICMetadataReader *)writer, 0);
            if (r)
            {
                hr = IWICMetadataReader_QueryInterface(r, &my_IID_IWICMetadataWriter, (void **)&ifd_writer);
                check(hr == S_OK && ifd_writer, "the IFD in it is a writer (%#lx)", hr);
                IWICMetadataReader_Release(r);
            }
            else
                check(0, "the writer holds a reader for the IFD");
        }
        if (ifd_writer)
        {
            IWICMetadataReader *r = nested_reader((IWICMetadataReader *)ifd_writer, 2);

            if (r)
            {
                IWICMetadataWriter *w2 = NULL;

                hr = IWICMetadataReader_QueryInterface(r, &my_IID_IWICMetadataWriter, (void **)&w2);
                check(hr == S_OK && w2, "and so is the Exif IFD below it (%#lx)", hr);
                if (w2) IWICMetadataWriter_Release(w2);
                IWICMetadataReader_Release(r);
            }
            else
                check(0, "the converted IFD still holds the Exif IFD");
            IWICMetadataWriter_Release(ifd_writer);
        }
        IWICMetadataWriter_Release(writer);
    }

    if (ifd) IWICMetadataReader_Release(ifd);
    IWICMetadataReader_Release(app1_reader);
    IStream_Release(stream);
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICComponentFactory, (void **)&factory);
    if (FAILED(hr)) { printf("FAIL  no factory %#lx\nRESULT: FAIL\n", hr); return 1; }

    test_classes();
    test_set_remove();
    test_stream_provider();
    test_factory();
    test_nested();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
