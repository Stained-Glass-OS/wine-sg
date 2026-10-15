/* Windows.Web.Http.Headers.HttpContentHeaderCollection and its typed header values (patch 3290), run by
 * test/fastwebhdr-gate.sh. The WinRT interfaces are called through raw vtable slots; the interface IDs are
 * the published ones (windows-rs 0.58 metadata, MIT), not taken from our headers. Uses a local HTTP server
 * for the transport part (content headers sent with a request, content headers of a response).
 *
 *   fastwebhdr-probe.exe PORT */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef HRESULT (WINAPI *F0)(void *);
typedef HRESULT (WINAPI *F1)(void *, void *);
typedef HRESULT (WINAPI *F2)(void *, void *, void *);
typedef HRESULT (WINAPI *F3)(void *, void *, void *, void *);
typedef HRESULT (WINAPI *FU64)(void *, UINT64, void *);
typedef HRESULT (WINAPI *FU64x2)(void *, UINT64, UINT64, void *);
typedef HRESULT (WINAPI *FU64x3)(void *, UINT64, UINT64, UINT64, void *);
#define SLOT(o, n) ((*(void ***)(o))[n])
#define C0(o, n) ((F0)SLOT(o, n))(o)
#define C1(o, n, a) ((F1)SLOT(o, n))(o, (void *)(a))
#define C2(o, n, a, b) ((F2)SLOT(o, n))(o, (void *)(a), (void *)(b))
#define C3(o, n, a, b, c) ((F3)SLOT(o, n))(o, (void *)(a), (void *)(b), (void *)(c))
#define REL(o) do { if (o) { ((ULONG (WINAPI *)(void *))SLOT(o, 2))(o); (o) = NULL; } } while (0)

#define GUIDDEF(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    static const GUID name = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }
GUIDDEF(IID_ContentHeaders, 0x40612a44, 0x47ae, 0x4b7e, 0x91, 0x24, 0x69, 0x62, 0x8b, 0x64, 0xaa, 0x18);
GUIDDEF(IID_MediaType, 0x16b28533, 0xe728, 0x4fcb, 0xbd, 0xb0, 0x08, 0xa4, 0x31, 0xa1, 0x48, 0x44);
GUIDDEF(IID_MediaTypeFactory, 0xbed747a8, 0xcd17, 0x42dd, 0x93, 0x67, 0xab, 0x9c, 0x5b, 0x56, 0xdd, 0x7d);
GUIDDEF(IID_MediaTypeStatics, 0xe04d83df, 0x1d41, 0x4d8c, 0xa2, 0xde, 0x6f, 0xd2, 0xed, 0x87, 0x39, 0x9b);
GUIDDEF(IID_NameValue, 0xd8ba7463, 0x5b9a, 0x4d1b, 0x93, 0xf9, 0xaa, 0x5b, 0x44, 0xec, 0xfd, 0xdf);
GUIDDEF(IID_NameValueFactory, 0x770e2267, 0xcbf8, 0x4736, 0xa9, 0x25, 0x93, 0xfb, 0xe1, 0x0c, 0x7c, 0xa8);
GUIDDEF(IID_NameValueStatics, 0xffd4030f, 0x1130, 0x4152, 0x86, 0x59, 0x25, 0x69, 0x09, 0xa9, 0xd1, 0x15);
GUIDDEF(IID_Disposition, 0xf2a2eedc, 0x2629, 0x4b49, 0x99, 0x08, 0x96, 0xa1, 0x68, 0xe9, 0x36, 0x5e);
GUIDDEF(IID_DispositionFactory, 0x9915bbc4, 0x456c, 0x4e81, 0x82, 0x95, 0xb2, 0xab, 0x3c, 0xbc, 0xf5, 0x45);
GUIDDEF(IID_DispositionStatics, 0x29c56067, 0x5a37, 0x46e4, 0xb0, 0x74, 0xc5, 0x17, 0x7d, 0x69, 0xca, 0x66);
GUIDDEF(IID_Range, 0x04d967d3, 0xa4f6, 0x495c, 0x95, 0x30, 0x85, 0x79, 0xfc, 0xba, 0x8a, 0xa9);
GUIDDEF(IID_RangeFactory, 0x3f5bd691, 0xa03c, 0x4456, 0x9a, 0x6f, 0xef, 0x27, 0xec, 0xd0, 0x3c, 0xae);
GUIDDEF(IID_RangeStatics, 0x80a346ca, 0x174c, 0x4fae, 0x82, 0x1c, 0x13, 0x4c, 0xd2, 0x94, 0xaa, 0x38);
GUIDDEF(IID_Coding, 0xbcf7f92a, 0x9376, 0x4d85, 0xbc, 0xcc, 0x9f, 0x4f, 0x9a, 0xca, 0xb4, 0x34);
GUIDDEF(IID_CodingFactory, 0xc53d2bd7, 0x332b, 0x4350, 0x85, 0x10, 0x2e, 0x67, 0xa2, 0x28, 0x9a, 0x5a);
GUIDDEF(IID_CodingStatics, 0x94d8602e, 0xf9bf, 0x42f7, 0xaa, 0x46, 0xed, 0x27, 0x2a, 0x41, 0xe2, 0x12);
GUIDDEF(IID_CodingCollection, 0x7d221721, 0xa6db, 0x436e, 0x8e, 0x83, 0x91, 0x59, 0x61, 0x92, 0x81, 0x9c);
GUIDDEF(IID_LanguageCollection, 0x9ebd7ca3, 0x8219, 0x44f6, 0x99, 0x02, 0x8c, 0x56, 0xdf, 0xd3, 0x34, 0x0c);
/* pinterface IDs, as computed for the signatures */
GUIDDEF(IID_MapSS, 0xf6d1f700, 0x49c2, 0x52ae, 0x81, 0x54, 0x82, 0x6f, 0x99, 0x08, 0x77, 0x3c);
GUIDDEF(IID_IterableKV, 0xe9bdaaf0, 0xcbf6, 0x5c72, 0xbe, 0x90, 0x29, 0xcb, 0xf3, 0xa1, 0x31, 0x9b);
GUIDDEF(IID_VectorStr, 0x98b9acc1, 0x4b56, 0x532e, 0xac, 0x73, 0x03, 0xd5, 0x29, 0x1c, 0xca, 0x90);
GUIDDEF(IID_VectorNV, 0x2cf800d4, 0x35d0, 0x545c, 0xab, 0x82, 0xf6, 0x79, 0x3b, 0xe7, 0x4c, 0xba);
GUIDDEF(IID_VectorCoding, 0xb94e539f, 0x44b6, 0x54c6, 0x9a, 0xd4, 0xe2, 0x84, 0x4b, 0x3e, 0x0b, 0xe1);
GUIDDEF(IID_RefU64, 0x6755e376, 0x53bb, 0x568b, 0xa1, 0x1d, 0x17, 0x23, 0x98, 0x68, 0x30, 0x9e);
GUIDDEF(IID_RefDate, 0x5541d8a7, 0x497c, 0x5aa4, 0x86, 0xfc, 0x77, 0x13, 0xad, 0xbf, 0x2a, 0x2c);
GUIDDEF(IID_Stringable, 0x96369f54, 0x8eb6, 0x48f0, 0xab, 0xce, 0xc1, 0xb2, 0x11, 0xe6, 0x27, 0xc3);
GUIDDEF(IID_PropertyValueStatics, 0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
GUIDDEF(IID_UriFactory, 0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
GUIDDEF(IID_UriClass, 0x9e365e57, 0x48b2, 0x4160, 0x95, 0x6f, 0xc7, 0x38, 0x51, 0x20, 0xbb, 0xfc);
GUIDDEF(IID_StringContentFactory, 0x46649d5b, 0x2e93, 0x48eb, 0x8e, 0x61, 0x19, 0x67, 0x78, 0x78, 0xe5, 0x7f);
GUIDDEF(IID_Content, 0x6b14a441, 0xfba7, 0x4bd2, 0xaf, 0x0a, 0x83, 0x9d, 0xe7, 0xc2, 0x95, 0xda);
GUIDDEF(IID_BufferFactory, 0x71af914d, 0xc10f, 0x484b, 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27);
GUIDDEF(IID_BufferByteAccess, 0x905a0fef, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
GUIDDEF(IID_ClientFactoryUnused, 0xc30c4eca, 0xe3fa, 0x4f99, 0xaf, 0xb4, 0x63, 0xcc, 0x65, 0x00, 0x94, 0x62);
GUIDDEF(IID_Client, 0x7fda1151, 0x3574, 0x4880, 0xa8, 0xba, 0xe6, 0xb1, 0xe0, 0x06, 0x1f, 0x3d);
GUIDDEF(IID_RequestFactory, 0x5bac994e, 0x3886, 0x412e, 0xae, 0xc3, 0x52, 0xec, 0x7f, 0x25, 0x61, 0x6f);
GUIDDEF(IID_MethodFactory, 0x3c51d10d, 0x36d7, 0x40f8, 0xa8, 0x6d, 0xe7, 0x59, 0xca, 0xf2, 0xf8, 0x3f);
GUIDDEF(IID_Response, 0xfee200fb, 0x8664, 0x44e0, 0x95, 0xd9, 0x42, 0x69, 0x61, 0x99, 0xbf, 0xfc);
GUIDDEF(IID_Agile, 0x94ea2b94, 0xe9cc, 0x49e0, 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90);

GUIDDEF(IID_RefU32, 0x513ef3af, 0xe784, 0x5325, 0xa9, 0x1e, 0x97, 0xc2, 0xb8, 0x11, 0x1c, 0xf3);
GUIDDEF(IID_RefI32, 0x548cefbd, 0xbc8a, 0x5fa0, 0x8d, 0xf2, 0x95, 0x74, 0x40, 0xfc, 0x8b, 0xf4);
GUIDDEF(IID_RefI64, 0x4dda9e24, 0xe69f, 0x5c6a, 0xa0, 0xa6, 0x93, 0x42, 0x73, 0x65, 0xaf, 0x2a);
GUIDDEF(IID_RefFloat, 0x719cc2ba, 0x3e76, 0x5def, 0x9f, 0x1a, 0x38, 0xd8, 0x5a, 0x14, 0x5e, 0xa8);
GUIDDEF(IID_RefGuid, 0x7d50f649, 0x632c, 0x51f9, 0x84, 0x9a, 0xee, 0x49, 0x42, 0x89, 0x33, 0xea);

#ifndef E_BOUNDS
#define E_BOUNDS ((HRESULT)0x8000000BL)
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HSTRING hs(const WCHAR *s)
{
    HSTRING h = NULL;
    WindowsCreateString(s, wcslen(s), &h);
    return h;
}
static BOOL hs_is(HSTRING h, const WCHAR *s)
{
    UINT32 len;
    const WCHAR *w = WindowsGetStringRawBuffer(h, &len);
    return len == wcslen(s) && (!len || !memcmp(w, s, len * sizeof(WCHAR)));
}
static void *factory(const WCHAR *name, const GUID *iid)
{
    HSTRING h = hs(name);
    void *f = NULL;
    RoGetActivationFactory(h, iid, &f);
    WindowsDeleteString(h);
    return f;
}
static void *activate(const WCHAR *name)
{
    HSTRING h = hs(name);
    void *obj = NULL;
    RoActivateInstance(h, (IInspectable **)&obj);
    WindowsDeleteString(h);
    return obj;
}
static void *qi(void *o, const GUID *iid)
{
    void *r = NULL;
    if (o) C2(o, 0, iid, &r);
    return r;
}

/* a string property: slot returns HSTRING */
static BOOL prop_is(void *obj, int slot, const WCHAR *want)
{
    HSTRING h = NULL;
    BOOL ok = obj && SUCCEEDED(C1(obj, slot, &h)) && hs_is(h, want);
    WindowsDeleteString(h);
    return ok;
}
static HRESULT prop_set(void *obj, int slot, const WCHAR *value)
{
    HSTRING h = value ? hs(value) : NULL;
    HRESULT hr = C1(obj, slot, h);
    WindowsDeleteString(h);
    return hr;
}
static BOOL tostring_is(void *obj, const WCHAR *want)
{
    void *s = qi(obj, &IID_Stringable);
    BOOL ok = s && prop_is(s, 6, want);
    if (s && !ok)
    {
        HSTRING h = NULL;
        C1(s, 6, &h);
        printf("      ToString = [%ls], want [%ls]\n", h ? WindowsGetStringRawBuffer(h, NULL) : L"(null)", want);
        WindowsDeleteString(h);
    }
    REL(s);
    return ok;
}
/* IMap lookup: returns TRUE and the text when present */
static BOOL map_lookup(void *map, const WCHAR *key, WCHAR *out, int cap)
{
    HSTRING k = hs(key), v = NULL;
    HRESULT hr = C2(map, 6, k, &v);
    WindowsDeleteString(k);
    if (FAILED(hr)) return FALSE;
    wcsncpy(out, v ? WindowsGetStringRawBuffer(v, NULL) : L"", cap);
    out[cap - 1] = 0;
    WindowsDeleteString(v);
    return TRUE;
}
static BOOL map_is(void *map, const WCHAR *key, const WCHAR *want)
{
    WCHAR buf[400];
    BOOL ok = map_lookup(map, key, buf, 400) && !wcscmp(buf, want);
    if (!ok) printf("      Lookup(%ls) = [%ls], want [%ls]\n", key, map_lookup(map, key, buf, 400) ? buf : L"(missing)", want);
    return ok;
}
static boolean map_has(void *map, const WCHAR *key)
{
    HSTRING k = hs(key);
    boolean has = 0;
    C2(map, 8, k, &has);
    WindowsDeleteString(k);
    return has;
}
static HRESULT append(void *coll, const WCHAR *name, const WCHAR *value)
{
    HSTRING n = hs(name), v = hs(value);
    HRESULT hr = C2(coll, 24, n, v);
    WindowsDeleteString(n);
    WindowsDeleteString(v);
    return hr;
}
static HRESULT try_append(void *coll, const WCHAR *name, const WCHAR *value, boolean *ok)
{
    HSTRING n = hs(name), v = hs(value);
    HRESULT hr = C3(coll, 25, n, v, ok);
    WindowsDeleteString(n);
    WindowsDeleteString(v);
    return hr;
}

/* IReference<UINT64> / IReference<DateTime> through PropertyValue */
static void *box_u64(UINT64 v)
{
    void *st = factory(L"Windows.Foundation.PropertyValue", &IID_PropertyValueStatics), *o = NULL, *r;
    ((HRESULT (WINAPI *)(void *, UINT64, void *))SLOT(st, 13))(st, v, &o);
    REL(st);
    r = qi(o, &IID_RefU64);
    REL(o);
    return r;
}
static void *box_date(UINT64 v)
{
    void *st = factory(L"Windows.Foundation.PropertyValue", &IID_PropertyValueStatics), *o = NULL, *r;
    ((HRESULT (WINAPI *)(void *, UINT64, void *))SLOT(st, 21))(st, v, &o);
    REL(st);
    r = qi(o, &IID_RefDate);
    REL(o);
    return r;
}
static BOOL unbox(void *ref, UINT64 *v)
{
    BOOL ok = ref && SUCCEEDED(C1(ref, 6, v));
    return ok;
}
static UINT64 http_date_ticks(int y, int mo, int d, int h, int mi, int s)
{
    SYSTEMTIME st = { y, mo, 0, d, h, mi, s, 0 };
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    return ((UINT64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static void *make_media(const WCHAR *text)
{
    void *f = factory(L"Windows.Web.Http.Headers.HttpMediaTypeHeaderValue", &IID_MediaTypeFactory), *v = NULL;
    HSTRING h = hs(text);
    if (f) C2(f, 6, h, &v);
    WindowsDeleteString(h);
    REL(f);
    return v;
}

static const WCHAR *gets_(void *coll, int slot, void **obj) { *obj = NULL; C1(coll, slot, obj); return L""; }

/* ---- the tests ---- */

/* PropertyValue boxes answer IReference<T> of their own type only (patch 3291) */
static void test_boxes(void)
{
    void *st = factory(L"Windows.Foundation.PropertyValue", &IID_PropertyValueStatics), *o = NULL, *r;
    UINT32 u32 = 0;
    INT64 i64 = 0;
    GUID g = { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } }, g2 = { 0 };
    float fl = 0;
    ULONG n = 0, i;
    IID *ids = NULL;
    BOOL listed = FALSE;

    ((HRESULT (WINAPI *)(void *, UINT32, void *))SLOT(st, 11))(st, 0xdeadbeef, &o);
    r = qi(o, &IID_RefU32);
    check(r && SUCCEEDED(C1(r, 6, &u32)) && u32 == 0xdeadbeef, "boxed UInt32 answers IReference<UINT32>");
    REL(r);
    r = qi(o, &IID_RefU64);
    check(r == NULL, "boxed UInt32 does not answer IReference<UINT64>");
    r = qi(o, &IID_RefI32);
    check(r == NULL, "boxed UInt32 does not answer IReference<INT32>");
    C2(o, 3, &n, &ids);
    for (i = 0; i < n; i++) if (IsEqualGUID(&ids[i], &IID_RefU32)) listed = TRUE;
    check(listed, "GetIids lists IReference<UINT32>");
    CoTaskMemFree(ids);
    REL(o);

    ((HRESULT (WINAPI *)(void *, INT64, void *))SLOT(st, 12))(st, -5, &o);
    r = qi(o, &IID_RefI64);
    check(r && SUCCEEDED(C1(r, 6, &i64)) && i64 == -5, "boxed Int64 answers IReference<INT64>");
    REL(r); REL(o);

    ((HRESULT (WINAPI *)(void *, float, void *))SLOT(st, 14))(st, 2.5f, &o);
    r = qi(o, &IID_RefFloat);
    check(r && SUCCEEDED(C1(r, 6, &fl)) && fl == 2.5f, "boxed Single answers IReference<FLOAT>");
    REL(r); REL(o);

    {
        void *sg = NULL;
        ((HRESULT (WINAPI *)(void *, GUID, void *))SLOT(st, 20))(st, g, &sg);
        r = qi(sg, &IID_RefGuid);
        check(r && SUCCEEDED(C1(r, 6, &g2)) && IsEqualGUID(&g, &g2), "boxed Guid answers IReference<GUID>");
        REL(r); REL(sg);
    }
    r = box_u64(77);
    check(r && prop_is(r, 4, L"Windows.Foundation.IReference`1<UInt64>"), "IReference<UINT64> reports its class name");
    REL(r);
    REL(st);
}

static void test_classes(void)
{
    void *c = activate(L"Windows.Web.Http.Headers.HttpContentHeaderCollection");
    void *map, *iter, *str, *agile, *typed;
    HSTRING name = NULL;
    ULONG n = 0;
    IID *ids = NULL;

    check(c != NULL, "HttpContentHeaderCollection activates");
    if (!c) return;
    typed = qi(c, &IID_ContentHeaders);
    map = qi(c, &IID_MapSS);
    iter = qi(c, &IID_IterableKV);
    str = qi(c, &IID_Stringable);
    agile = qi(c, &IID_Agile);
    check(typed && map && iter && str && agile, "collection answers typed, IMap, IIterable, IStringable, IAgileObject");
    check(prop_is(c, 4, L"Windows.Web.Http.Headers.HttpContentHeaderCollection"), "GetRuntimeClassName");
    C2(c, 3, &n, &ids);
    check(n >= 4, "GetIids lists the faces");
    CoTaskMemFree(ids);
    (void)name;
    check(tostring_is(c, L""), "empty collection ToString is empty");
    REL(typed); REL(map); REL(iter); REL(str); REL(agile); REL(c);
}

static void test_media_type(void)
{
    void *c = activate(L"Windows.Web.Http.Headers.HttpContentHeaderCollection"), *map = qi(c, &IID_MapSS);
    void *mt = NULL, *params = NULL, *mt2, *statics, *f, *typed = qi(c, &IID_ContentHeaders);
    UINT32 size = 0;
    HRESULT hr;
    boolean ok = 0;
    HSTRING h;

    gets_(typed, 18, &mt);
    check(mt == NULL, "ContentType is null when absent");
    check(append(c ? typed : NULL, L"Content-Type", L"text/html; charset=utf-8; q=0.5") == S_OK, "Append Content-Type");
    gets_(typed, 18, &mt);
    check(mt && prop_is(mt, 8, L"text/html"), "ContentType.MediaType parsed");
    check(mt && prop_is(mt, 6, L"utf-8"), "ContentType.CharSet parsed");
    C1(mt, 10, &params);
    check(params && SUCCEEDED(C1(params, 7, &size)) && size == 2, "Parameters has charset and q");
    check(map_is(map, L"content-type", L"text/html; charset=utf-8; q=0.5"), "Lookup renders the header (case-insensitive name)");
    check(prop_set(mt, 7, L"iso-8859-1") == S_OK && map_is(map, L"Content-Type", L"text/html; charset=iso-8859-1; q=0.5"),
          "CharSet changes the header through the live object");
    check(prop_set(mt, 7, NULL) == S_OK && map_is(map, L"Content-Type", L"text/html; q=0.5"), "CharSet empty removes the parameter");
    check(prop_set(mt, 9, L"text/plain") == S_OK && prop_is(mt, 8, L"text/plain"), "MediaType set");
    check(prop_set(mt, 9, L"nonsense") == E_INVALIDARG, "MediaType without a slash is refused");
    check(prop_set(mt, 9, L"text/p lain") == E_INVALIDARG, "MediaType with a space is refused");
    REL(params);
    {
        void *again = NULL;
        gets_(typed, 18, &again);
        check(again == mt, "ContentType returns the same object");
        REL(again);
    }
    REL(mt);

    /* assigning a value object */
    mt2 = make_media(L"application/json");
    check(mt2 != NULL, "HttpMediaTypeHeaderValue(\"application/json\")");
    check(mt2 && prop_is(mt2, 8, L"application/json") && prop_is(mt2, 6, L""), "constructed value has no charset");
    check(tostring_is(mt2, L"application/json"), "value ToString");
    check(C1(typed, 19, mt2) == S_OK && map_is(map, L"Content-Type", L"application/json"), "put ContentType");
    prop_set(mt2, 7, L"utf-8");
    check(map_is(map, L"Content-Type", L"application/json; charset=utf-8"), "later change of the assigned object shows in the header");
    check(C1(typed, 19, NULL) == S_OK && !map_has(map, L"Content-Type"), "ContentType = null removes the header");
    REL(mt2);

    f = factory(L"Windows.Web.Http.Headers.HttpMediaTypeHeaderValue", &IID_MediaTypeFactory);
    mt2 = NULL;
    h = hs(L"text/html; charset=utf-8");
    hr = C2(f, 6, h, &mt2);
    check(hr == E_INVALIDARG && !mt2, "constructor refuses parameters in the media type");
    WindowsDeleteString(h);
    h = hs(L"");
    hr = C2(f, 6, h, &mt2);
    check(hr == E_INVALIDARG, "constructor refuses an empty string");
    WindowsDeleteString(h);
    REL(f);

    statics = factory(L"Windows.Web.Http.Headers.HttpMediaTypeHeaderValue", &IID_MediaTypeStatics);
    h = hs(L"text/plain; charset=\"utf-8\"; format=flowed");
    mt2 = NULL;
    hr = C2(statics, 6, h, &mt2);
    check(hr == S_OK && mt2 && prop_is(mt2, 6, L"utf-8") && tostring_is(mt2, L"text/plain; charset=\"utf-8\"; format=flowed"),
          "Parse keeps parameters, CharSet drops quotes");
    REL(mt2);
    WindowsDeleteString(h);
    h = hs(L"text/plain;;charset=utf-8");
    hr = C2(statics, 6, h, &mt2);
    check(hr == S_OK && mt2 && tostring_is(mt2, L"text/plain; charset=utf-8"), "Parse tolerates an empty parameter");
    REL(mt2);
    WindowsDeleteString(h);
    h = hs(L"bad");
    mt2 = (void *)1;
    hr = C2(statics, 6, h, &mt2);
    check(hr == E_INVALIDARG && !mt2, "Parse of garbage fails");
    mt2 = (void *)1;
    ok = 1;
    hr = C3(statics, 7, h, &mt2, &ok);
    check(hr == S_OK && !ok && !mt2, "TryParse of garbage reports false");
    WindowsDeleteString(h);
    h = hs(L"image/png");
    hr = C3(statics, 7, h, &mt2, &ok);
    check(hr == S_OK && ok && mt2 && prop_is(mt2, 8, L"image/png"), "TryParse succeeds");
    REL(mt2);
    WindowsDeleteString(h);
    REL(statics);

    /* a value the header table rejects on Append / accepts without validation */
    check(append(typed, L"Content-Type", L"nonsense") == E_INVALIDARG, "Append of an invalid Content-Type fails");
    ok = 0;
    check(try_append(typed, L"Content-Type", L"nonsense", &ok) == S_OK && ok, "TryAppendWithoutValidation keeps it");
    {
        void *none = (void *)1;
        gets_(typed, 18, &none);
        check(none == NULL, "ContentType of an unparsable value reads null");
        check(map_is(map, L"Content-Type", L"nonsense"), "but the text is still there");
    }
    REL(typed); REL(map); REL(c);
}

static void test_scalars(void)
{
    void *c = activate(L"Windows.Web.Http.Headers.HttpContentHeaderCollection"), *map = qi(c, &IID_MapSS);
    void *typed = qi(c, &IID_ContentHeaders), *ref = NULL, *buf = NULL, *uri = NULL;
    UINT64 v = 0;
    boolean ok = 0;

    /* Content-Length */
    gets_(typed, 10, &ref);
    check(ref == NULL, "ContentLength is null on a bare collection");
    ref = box_u64(1234);
    check(C1(typed, 11, ref) == S_OK, "put ContentLength");
    REL(ref);
    check(map_is(map, L"Content-Length", L"1234"), "Content-Length header text");
    gets_(typed, 10, &ref);
    check(ref && unbox(ref, &v) && v == 1234, "ContentLength reads 1234");
    REL(ref);
    check(append(typed, L"Content-Length", L"77") == S_OK && map_is(map, L"Content-Length", L"77"), "Append replaces Content-Length");
    check(append(typed, L"Content-Length", L"abc") == E_INVALIDARG, "non-numeric Content-Length refused");
    check(try_append(typed, L"Content-Length", L"abc", &ok) == S_OK && ok, "non-numeric accepted without validation");
    gets_(typed, 10, &ref);
    check(ref == NULL, "unparsable ContentLength reads null");
    check(C1(typed, 11, NULL) == S_OK && !map_has(map, L"Content-Length"), "ContentLength = null removes it");

    /* dates */
    ref = box_date(http_date_ticks(1994, 11, 6, 8, 49, 37));
    check(C1(typed, 21, ref) == S_OK, "put Expires");
    REL(ref);
    check(map_is(map, L"Expires", L"Sun, 06 Nov 1994 08:49:37 GMT"), "Expires is written as an HTTP date");
    check(append(typed, L"Last-Modified", L"Tue, 15 Nov 1994 12:45:26 GMT") == S_OK, "Append Last-Modified");
    gets_(typed, 22, &ref);
    check(ref && unbox(ref, &v) && v == http_date_ticks(1994, 11, 15, 12, 45, 26), "LastModified parsed");
    REL(ref);
    check(append(typed, L"Last-Modified", L"yesterday") == E_INVALIDARG, "bad date refused");
    gets_(typed, 20, &ref);
    check(ref && unbox(ref, &v) && v == http_date_ticks(1994, 11, 6, 8, 49, 37), "Expires reads back");
    REL(ref);
    check(C1(typed, 21, NULL) == S_OK && !map_has(map, L"Expires"), "Expires = null removes it");

    /* Content-MD5: 16 bytes 00..0f -> AAECAwQFBgcICQoLDA0ODw== */
    check(append(typed, L"Content-MD5", L"AAECAwQFBgcICQoLDA0ODw==") == S_OK, "Append Content-MD5");
    gets_(typed, 14, &buf);
    {
        UINT32 len = 0, i;
        void *access = qi(buf, &IID_BufferByteAccess);
        BYTE *bytes = NULL;
        BOOL same = TRUE;
        if (buf) C1(buf, 7, &len);
        if (access) C1(access, 3, &bytes);
        for (i = 0; i < 16 && bytes; i++) if (bytes[i] != i) same = FALSE;
        check(buf && len == 16 && bytes && same, "ContentMD5 decodes to the 16 bytes");
        REL(access);
    }
    check(C1(typed, 15, buf) == S_OK && map_is(map, L"Content-MD5", L"AAECAwQFBgcICQoLDA0ODw=="), "put ContentMD5 re-encodes");
    REL(buf);
    check(append(typed, L"Content-MD5", L"!!!") == E_INVALIDARG, "bad base64 refused");

    /* Content-Location */
    {
        void *f = factory(L"Windows.Foundation.Uri", &IID_UriFactory), *u = NULL;
        HSTRING h = hs(L"http://example.com/a/b?x=1");
        C2(f, 6, h, &u);
        WindowsDeleteString(h);
        REL(f);
        check(u && C1(typed, 13, u) == S_OK && map_is(map, L"Content-Location", L"http://example.com/a/b?x=1"), "put ContentLocation");
        REL(u);
        gets_(typed, 12, &uri);
        {
            void *uc = qi(uri, &IID_UriClass);
            HSTRING abs = NULL;
            check(uc && SUCCEEDED(C1(uc, 6, &abs)) && hs_is(abs, L"http://example.com/a/b?x=1"), "ContentLocation reads back as a Uri");
            WindowsDeleteString(abs);
            REL(uc);
        }
        REL(uri);
        check(C1(typed, 13, NULL) == S_OK && !map_has(map, L"Content-Location"), "ContentLocation = null removes it");
    }
    REL(typed); REL(map); REL(c);
}

static void test_disposition(void)
{
    void *c = activate(L"Windows.Web.Http.Headers.HttpContentHeaderCollection"), *map = qi(c, &IID_MapSS);
    void *typed = qi(c, &IID_ContentHeaders), *d = NULL, *ref = NULL, *f, *statics, *d2 = NULL;
    UINT64 v = 0;
    HSTRING h;
    HRESULT hr;
    boolean ok = 0;

    gets_(typed, 6, &d);
    check(d == NULL, "ContentDisposition is null when absent");
    check(append(typed, L"Content-Disposition", L"attachment; filename=\"a b.txt\"; name=f; size=12") == S_OK, "Append Content-Disposition");
    gets_(typed, 6, &d);
    check(d && prop_is(d, 6, L"attachment"), "DispositionType");
    check(d && prop_is(d, 8, L"a b.txt"), "FileName without quotes");
    check(d && prop_is(d, 12, L"f"), "Name");
    {
        void *r = NULL;
        C1(d, 15, &r);
        check(r && unbox(r, &v) && v == 12, "Size parsed");
        REL(r);
    }
    check(prop_set(d, 9, L"x y.bin") == S_OK && map_is(map, L"Content-Disposition", L"attachment; filename=\"x y.bin\"; name=f; size=12"),
          "FileName with a space is quoted");
    check(prop_set(d, 9, L"plain.bin") == S_OK && map_is(map, L"Content-Disposition", L"attachment; filename=plain.bin; name=f; size=12"),
          "FileName that is a token stays bare");
    ref = box_u64(99);
    check(C1(d, 16, ref) == S_OK && map_is(map, L"Content-Disposition", L"attachment; filename=plain.bin; name=f; size=99"), "Size set");
    REL(ref);
    check(C1(d, 16, NULL) == S_OK && map_is(map, L"Content-Disposition", L"attachment; filename=plain.bin; name=f"), "Size = null removes it");
    check(prop_set(d, 11, L"\xfc.txt") == S_OK && map_is(map, L"Content-Disposition", L"attachment; filename=plain.bin; name=f; filename*=UTF-8''%C3%BC.txt"),
          "FileNameStar is percent-encoded UTF-8");
    check(prop_is(d, 10, L"\xfc.txt"), "FileNameStar decodes");
    check(prop_set(d, 7, L"inline") == S_OK && prop_is(d, 6, L"inline"), "DispositionType set");
    check(prop_set(d, 7, L"in line") == E_INVALIDARG, "DispositionType with a space refused");
    REL(d);

    f = factory(L"Windows.Web.Http.Headers.HttpContentDispositionHeaderValue", &IID_DispositionFactory);
    h = hs(L"form-data");
    hr = C2(f, 6, h, &d2);
    check(hr == S_OK && d2 && tostring_is(d2, L"form-data"), "constructor");
    prop_set(d2, 13, L"field1");
    check(tostring_is(d2, L"form-data; name=field1"), "Name set on a new value");
    check(C1(typed, 7, d2) == S_OK && map_is(map, L"Content-Disposition", L"form-data; name=field1"), "put ContentDisposition");
    REL(d2);
    WindowsDeleteString(h);
    REL(f);

    statics = factory(L"Windows.Web.Http.Headers.HttpContentDispositionHeaderValue", &IID_DispositionStatics);
    h = hs(L"attachment; filename=\"semi;colon.txt\"");
    hr = C2(statics, 6, h, &d2);
    check(hr == S_OK && d2 && prop_is(d2, 8, L"semi;colon.txt"), "Parse keeps a semicolon inside quotes");
    REL(d2);
    WindowsDeleteString(h);
    h = hs(L"a b");
    d2 = (void *)1;
    hr = C3(statics, 7, h, &d2, &ok);
    check(hr == S_OK && !ok && !d2, "TryParse of an invalid type reports false");
    WindowsDeleteString(h);
    REL(statics);
    REL(typed); REL(map); REL(c);
}

static void test_range(void)
{
    void *c = activate(L"Windows.Web.Http.Headers.HttpContentHeaderCollection"), *map = qi(c, &IID_MapSS);
    void *typed = qi(c, &IID_ContentHeaders), *r = NULL, *ref = NULL, *f, *statics;
    UINT64 v = 0;
    HSTRING h;
    HRESULT hr;
    boolean ok = 0;

    f = factory(L"Windows.Web.Http.Headers.HttpContentRangeHeaderValue", &IID_RangeFactory);
    hr = ((FU64x3)SLOT(f, 8))(f, 0, 9, 100, &r);
    check(hr == S_OK && r && tostring_is(r, L"bytes 0-9/100"), "CreateFromRangeWithLength");
    ref = NULL;
    C1(r, 6, &ref);
    check(ref && unbox(ref, &v) && v == 0, "FirstBytePosition");
    REL(ref);
    C1(r, 7, &ref);
    check(ref && unbox(ref, &v) && v == 9, "LastBytePosition");
    REL(ref);
    C1(r, 8, &ref);
    check(ref && unbox(ref, &v) && v == 100, "Length");
    REL(ref);
    check(prop_is(r, 9, L"bytes"), "Unit defaults to bytes");
    check(prop_set(r, 10, L"items") == S_OK && tostring_is(r, L"items 0-9/100"), "Unit set");
    check(C1(typed, 17, r) == S_OK && map_is(map, L"Content-Range", L"items 0-9/100"), "put ContentRange");
    REL(r);
    r = NULL;
    hr = ((FU64x2)SLOT(f, 7))(f, 5, 2, &r);
    check(hr == E_INVALIDARG && !r, "CreateFromRange with from > to is refused");
    hr = ((FU64x3)SLOT(f, 8))(f, 0, 100, 100, &r);
    check(hr == E_INVALIDARG && !r, "range beyond the length is refused");
    hr = ((FU64x2)SLOT(f, 7))(f, 3, 8, &r);
    check(hr == S_OK && r && tostring_is(r, L"bytes 3-8/*"), "CreateFromRange has an unknown length");
    ref = (void *)1;
    C1(r, 8, &ref);
    check(ref == NULL, "unknown Length is null");
    REL(r);
    hr = ((FU64)SLOT(f, 6))(f, 100, &r);
    check(hr == S_OK && r && tostring_is(r, L"bytes */100"), "CreateFromLength");
    ref = (void *)1;
    C1(r, 6, &ref);
    check(ref == NULL, "no FirstBytePosition");
    REL(r);
    REL(f);

    check(append(typed, L"Content-Range", L"bytes 200-1000/67589") == S_OK, "Append Content-Range");
    gets_(typed, 16, &r);
    check(r && tostring_is(r, L"bytes 200-1000/67589"), "ContentRange object");
    REL(r);
    check(append(typed, L"Content-Range", L"bytes 5-2/10") == E_INVALIDARG, "reversed range refused");
    check(append(typed, L"Content-Range", L"bytes 1-2") == E_INVALIDARG, "range without a slash refused");

    statics = factory(L"Windows.Web.Http.Headers.HttpContentRangeHeaderValue", &IID_RangeStatics);
    h = hs(L"bytes 10-19/*");
    hr = C2(statics, 6, h, &r);
    check(hr == S_OK && r && tostring_is(r, L"bytes 10-19/*"), "Parse bytes 10-19/*");
    REL(r);
    WindowsDeleteString(h);
    h = hs(L"bytes */*");
    r = (void *)1;
    hr = C3(statics, 7, h, &r, &ok);
    check(hr == S_OK && !ok && !r, "TryParse of */* reports false");
    WindowsDeleteString(h);
    REL(statics);
    REL(typed); REL(map); REL(c);
}

static void test_collections(void)
{
    void *c = activate(L"Windows.Web.Http.Headers.HttpContentHeaderCollection"), *map = qi(c, &IID_MapSS);
    void *typed = qi(c, &IID_ContentHeaders), *enc = NULL, *vec, *coll, *item = NULL, *str, *lang = NULL, *f;
    UINT32 n = 0;
    boolean ok = 0;
    HSTRING h;
    HRESULT hr;

    gets_(typed, 8, &enc);
    check(enc != NULL, "ContentEncoding is never null");
    vec = qi(enc, &IID_VectorCoding);
    coll = qi(enc, &IID_CodingCollection);
    check(vec && coll, "ContentEncoding answers IVector<HttpContentCodingHeaderValue> and the typed collection");
    check(vec && SUCCEEDED(C1(vec, 7, &n)) && n == 0, "starts empty");
    check(!map_has(map, L"Content-Encoding"), "an empty collection is no header");
    check(append(typed, L"Content-Encoding", L"gzip, deflate") == S_OK, "Append Content-Encoding");
    check(C1(vec, 7, &n) == S_OK && n == 2, "two codings (the Append reached the live collection)");
    check(C2(vec, 6, 0, &item) == S_OK && item && prop_is(item, 6, L"gzip"), "GetAt(0) is gzip");
    check(tostring_is(item, L"gzip"), "coding ToString");
    REL(item);
    check(map_is(map, L"Content-Encoding", L"gzip, deflate"), "header text");
    h = hs(L"br");
    check(C1(coll, 6, h) == S_OK, "ParseAdd br");
    WindowsDeleteString(h);
    check(map_is(map, L"Content-Encoding", L"gzip, deflate, br"), "ParseAdd shows in the header");
    h = hs(L"b r");
    ok = 1;
    check(C2(coll, 7, h, &ok) == S_OK && !ok, "TryParseAdd of an invalid token reports false");
    check(C1(coll, 6, h) == E_INVALIDARG, "ParseAdd of an invalid token fails");
    WindowsDeleteString(h);
    str = qi(enc, &IID_Stringable);
    check(str && prop_is(str, 6, L"gzip, deflate, br"), "collection ToString");
    REL(str);
    {
        boolean found = 0;
        UINT32 idx = 9;
        void *g = NULL;
        C2(vec, 6, 1, &g);
        hr = ((HRESULT (WINAPI *)(void *, void *, UINT32 *, boolean *))SLOT(vec, 9))(vec, g, &idx, &found);
        check(hr == S_OK && found && idx == 1, "IndexOf finds the object");
        REL(g);
    }
    check(C1(vec, 12, 0) == S_OK && C1(vec, 7, &n) == S_OK && n == 2 && map_is(map, L"Content-Encoding", L"deflate, br"), "RemoveAt(0)");
    check(C1(vec, 12, 7) == E_BOUNDS, "RemoveAt out of range");
    {
        void *bogus = make_media(L"a/b");
        check(C1(vec, 13, bogus) == E_INVALIDARG, "Append of a foreign object is refused");
        REL(bogus);
    }
    f = factory(L"Windows.Web.Http.Headers.HttpContentCodingHeaderValue", &IID_CodingFactory);
    h = hs(L"identity");
    item = NULL;
    hr = C2(f, 6, h, &item);
    check(hr == S_OK && item && prop_is(item, 6, L"identity"), "HttpContentCodingHeaderValue constructor");
    WindowsDeleteString(h);
    check(C1(vec, 13, item) == S_OK && map_is(map, L"Content-Encoding", L"deflate, br, identity"), "Append(object)");
    REL(item);
    REL(f);
    check(C0(vec, 15) == S_OK && !map_has(map, L"Content-Encoding"), "Clear removes the header text");
    REL(vec); REL(coll); REL(enc);

    /* languages: IVector<HSTRING> */
    gets_(typed, 9, &lang);
    vec = qi(lang, &IID_VectorStr);
    coll = qi(lang, &IID_LanguageCollection);
    check(vec && coll, "ContentLanguage answers IVector<HSTRING> and the language collection");
    h = hs(L"en-US");
    check(C1(coll, 6, h) == S_OK, "ParseAdd en-US");
    WindowsDeleteString(h);
    check(append(typed, L"content-language", L"de") == S_OK, "Append to Content-Language by a lower-case name");
    check(map_is(map, L"Content-Language", L"en-US, de"), "both languages in the header");
    {
        HSTRING g = NULL;
        check(C2(vec, 6, 1, &g) == S_OK && hs_is(g, L"de"), "GetAt(1) is de");
        WindowsDeleteString(g);
        h = hs(L"en-US");
        {
            UINT32 idx = 9;
            boolean found = 0;
            HRESULT r = ((HRESULT (WINAPI *)(void *, HSTRING, UINT32 *, boolean *))SLOT(vec, 9))(vec, h, &idx, &found);
            check(r == S_OK && found && idx == 0, "IndexOf finds a language string");
        }
        WindowsDeleteString(h);
        h = hs(L"fr");
        check(C1(vec, 13, h) == S_OK && map_is(map, L"Content-Language", L"en-US, de, fr"), "Append(HSTRING)");
        WindowsDeleteString(h);
    }
    REL(vec); REL(coll); REL(lang);
    REL(typed); REL(map); REL(c);
}

static void test_map(void)
{
    void *c = activate(L"Windows.Web.Http.Headers.HttpContentHeaderCollection"), *map = qi(c, &IID_MapSS);
    void *iterable = qi(c, &IID_IterableKV), *iter = NULL, *pair = NULL, *view = NULL, *typed = qi(c, &IID_ContentHeaders);
    HSTRING k, v;
    boolean replaced = 5, has = 0;
    UINT32 size = 0, seen = 0;
    HRESULT hr;
    WCHAR buf[100];

    k = hs(L"X-First"); v = hs(L"1");
    check(C3(map, 10, k, v, &replaced) == S_OK && !replaced, "Insert a new header");
    WindowsDeleteString(v);
    v = hs(L"2");
    check(C3(map, 10, k, v, &replaced) == S_OK && replaced && map_is(map, L"X-First", L"2"), "Insert replaces");
    WindowsDeleteString(v);
    WindowsDeleteString(k);
    check(append(typed, L"X-Multi", L"a") == S_OK && append(typed, L"X-Multi", L"b") == S_OK && map_is(map, L"X-Multi", L"a, b"),
          "a header appended twice carries both values");
    check(append(typed, L"bad name", L"x") == E_INVALIDARG, "Append with an invalid name fails");
    {
        boolean ok = 1;
        check(try_append(typed, L"bad name", L"x", &ok) == S_OK && !ok, "TryAppendWithoutValidation with an invalid name reports false");
    }
    check(append(typed, L"Content-Type", L"text/css") == S_OK, "known header in the middle");
    check(C1(map, 7, &size) == S_OK && size == 3, "Size counts the headers");
    check(map_lookup(map, L"nope", buf, 100) == FALSE, "Lookup of a missing header fails");
    k = hs(L"nope");
    hr = C2(map, 6, k, &v);
    check(hr == E_BOUNDS, "Lookup of a missing header is E_BOUNDS");
    check(C2(map, 8, k, &has) == S_OK && !has, "HasKey false");
    check(C1(map, 11, k) == E_BOUNDS, "Remove of a missing header is E_BOUNDS");
    WindowsDeleteString(k);

    /* iteration */
    check(C1(iterable, 6, &iter) == S_OK && iter, "First");
    has = 0;
    C1(iter, 7, &has);
    while (has)
    {
        HSTRING key = NULL, val = NULL;
        pair = NULL;
        if (SUCCEEDED(C1(iter, 6, &pair)) && pair)
        {
            C1(pair, 6, &key);
            C1(pair, 7, &val);
            if (hs_is(key, L"X-First")) seen += hs_is(val, L"2") ? 1 : 100;
            if (hs_is(key, L"X-Multi")) seen += hs_is(val, L"a, b") ? 10 : 1000;
            if (hs_is(key, L"Content-Type")) seen += hs_is(val, L"text/css") ? 1000000 : 10000000;
            WindowsDeleteString(key); WindowsDeleteString(val);
            REL(pair);
        }
        C1(iter, 8, &has);
    }
    check(seen == 1000011, "iteration yields every header once with its text");
    REL(iter);

    /* ToString: Name: value CRLF */
    {
        void *s = qi(c, &IID_Stringable);
        HSTRING h = NULL;
        C1(s, 6, &h);
        check(h && wcsstr(WindowsGetStringRawBuffer(h, NULL), L"X-First: 2\r\n") && wcsstr(WindowsGetStringRawBuffer(h, NULL), L"Content-Type: text/css\r\n"),
              "ToString lists Name: value lines");
        WindowsDeleteString(h);
        REL(s);
    }

    /* a view is a snapshot */
    check(C1(map, 9, &view) == S_OK && view, "GetView");
    k = hs(L"X-First");
    C1(map, 11, k);
    check(!map_has(map, L"X-First"), "Remove");
    has = 0;
    C2(view, 8, k, &has);
    check(has, "the view still has the removed header");
    WindowsDeleteString(k);
    REL(view);
    check(C0(map, 12) == S_OK, "Clear");
    check(C1(map, 7, &size) == S_OK && size == 0, "Size 0 after Clear");
    REL(iterable); REL(typed); REL(map); REL(c);
}

static void test_string_content(void)
{
    void *f = factory(L"Windows.Web.Http.HttpStringContent", &IID_StringContentFactory);
    void *content = NULL, *headers = NULL, *typed, *map, *mt = NULL, *ref = NULL;
    HSTRING text = hs(L"h\xe9llo"), media;
    UINT64 v = 0;

    check(SUCCEEDED(C2(f, 6, text, &content)) && content, "HttpStringContent(string)");
    check(content && SUCCEEDED(C1(content, 6, &headers)) && headers, "Headers is implemented");
    typed = qi(headers, &IID_ContentHeaders);
    map = qi(headers, &IID_MapSS);
    check(typed && map, "Headers is an HttpContentHeaderCollection");
    check(map_is(map, L"Content-Type", L"text/plain; charset=utf-8"), "default Content-Type is text/plain; charset=utf-8");
    gets_(typed, 18, &mt);
    check(mt && prop_is(mt, 6, L"utf-8") && prop_is(mt, 8, L"text/plain"), "ContentType object");
    REL(mt);
    gets_(typed, 10, &ref);
    check(ref && unbox(ref, &v) && v == 6, "ContentLength is the body length in bytes (6 for UTF-8 \"h\\xe9llo\")");
    REL(ref);
    check(map_is(map, L"Content-Length", L"6"), "reading ContentLength fills the header");
    {
        void *again = NULL;
        C1(content, 6, &again);
        check(again == headers, "Headers returns the same collection");
        REL(again);
    }
    REL(map); REL(typed); REL(headers); REL(content);

    media = hs(L"application/json");
    {
        void *r = ((HRESULT (WINAPI *)(void *, void *, int, void *, void *))SLOT(f, 8))(f, text, 1, media, &content) == S_OK ? content : NULL;
        check(r != NULL, "HttpStringContent(string, UTF-16LE, mediaType)");
    }
    C1(content, 6, &headers);
    typed = qi(headers, &IID_ContentHeaders);
    map = qi(headers, &IID_MapSS);
    check(map_is(map, L"Content-Type", L"application/json; charset=utf-16LE"), "media type and encoding charset");
    gets_(typed, 10, &ref);
    check(ref && unbox(ref, &v) && v == 10, "UTF-16 body length");
    REL(ref);
    REL(map); REL(typed); REL(headers); REL(content);
    WindowsDeleteString(media);
    WindowsDeleteString(text);
    REL(f);
}

/* ---- transport: content headers on the wire and on a response */
static void *make_uri(int port, const char *path)
{
    void *f = factory(L"Windows.Foundation.Uri", &IID_UriFactory), *uri = NULL;
    WCHAR url[200];
    HSTRING h;
    swprintf(url, 200, L"http://127.0.0.1:%d%hs", port, path);
    h = hs(url);
    C2(f, 6, h, &uri);
    WindowsDeleteString(h);
    REL(f);
    return uri;
}

static void test_transport(int port)
{
    void *client = activate(L"Windows.Web.Http.HttpClient");
    void *mf = factory(L"Windows.Web.Http.HttpMethod", &IID_MethodFactory), *method = NULL;
    void *rf = factory(L"Windows.Web.Http.HttpRequestMessage", &IID_RequestFactory), *request = NULL;
    void *sf = factory(L"Windows.Web.Http.HttpStringContent", &IID_StringContentFactory), *content = NULL, *headers = NULL;
    void *typed, *uri, *op = NULL, *response = NULL, *rcontent = NULL, *rheaders = NULL, *rtyped, *rmap, *clientI;
    void *mt, *lang, *vec, *coll, *ref = NULL;
    HSTRING h;
    UINT64 v = 0;
    HRESULT hr;

    clientI = qi(client, &IID_Client);
    h = hs(L"POST");
    C2(mf, 6, h, &method);
    WindowsDeleteString(h);
    uri = make_uri(port, "/ctype");
    hr = C3(rf, 6, method, uri, &request);
    check(hr == S_OK && request, "POST request message");
    h = hs(L"body text");
    C2(sf, 6, h, &content);
    WindowsDeleteString(h);
    C1(content, 6, &headers);
    typed = qi(headers, &IID_ContentHeaders);
    mt = make_media(L"application/x-sg");
    prop_set(mt, 7, L"utf-8");
    C1(typed, 19, mt);
    REL(mt);
    lang = NULL;
    gets_(typed, 9, &lang);
    coll = qi(lang, &IID_LanguageCollection);
    h = hs(L"en");
    C1(coll, 6, h);
    WindowsDeleteString(h);
    REL(coll); REL(lang);
    append(typed, L"X-Content-Test", L"1");
    C1(request, 7, content);
    hr = C2(clientI, 14, request, &op);
    check(hr == S_OK && op, "SendRequestAsync with content headers");
    hr = op ? C1(op, 10, &response) : E_FAIL;
    check(hr == S_OK && response, "response");
    {
        void *resp = qi(response, &IID_Response);
        C1(resp, 6, &rcontent);
        REL(resp);
    }
    check(rcontent != NULL, "response Content");
    C1(rcontent, 6, &rheaders);
    check(rheaders != NULL, "response Content.Headers");
    rtyped = qi(rheaders, &IID_ContentHeaders);
    rmap = qi(rheaders, &IID_MapSS);
    {
        void *body_op = NULL, *res = NULL;
        HSTRING body = NULL;
        void *rc = qi(rcontent, &IID_Content);
        C1(rc, 10, &body_op);                 /* ReadAsStringAsync */
        if (body_op) C1(body_op, 10, &body);
        check(body && hs_is(body, L"ct=application/x-sg; charset=utf-8|lang=en|x=1|len=9"),
              "the server received the content type, language, extra header and length");
        if (body && !hs_is(body, L"ct=application/x-sg; charset=utf-8|lang=en|x=1|len=9"))
            printf("      body = [%ls]\n", WindowsGetStringRawBuffer(body, NULL));
        WindowsDeleteString(body);
        REL(body_op); REL(res); REL(rc);
    }
    mt = NULL;
    gets_(rtyped, 18, &mt);
    check(mt && prop_is(mt, 8, L"text/x-test") && prop_is(mt, 6, L"utf-8"), "response ContentType parsed from the wire");
    REL(mt);
    lang = NULL;
    gets_(rtyped, 9, &lang);
    vec = qi(lang, &IID_VectorStr);
    {
        HSTRING g = NULL;
        check(vec && SUCCEEDED(C2(vec, 6, 0, &g)) && hs_is(g, L"de"), "response ContentLanguage");
        WindowsDeleteString(g);
    }
    REL(vec); REL(lang);
    gets_(rtyped, 22, &ref);
    check(ref && unbox(ref, &v) && v == http_date_ticks(1994, 11, 15, 12, 45, 26), "response LastModified");
    REL(ref);
    gets_(rtyped, 10, &ref);
    check(ref && unbox(ref, &v) && v == wcslen(L"ct=application/x-sg; charset=utf-8|lang=en|x=1|len=9"), "response ContentLength is the body length");
    REL(ref);

    REL(rmap); REL(rtyped); REL(rheaders); REL(rcontent); REL(response); REL(op);
    REL(typed); REL(headers); REL(content); REL(request); REL(uri); REL(method);
    REL(clientI); REL(client); REL(sf); REL(rf); REL(mf);
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 0;
    RoInitialize(RO_INIT_MULTITHREADED);
    test_boxes();
    test_classes();
    test_media_type();
    test_scalars();
    test_disposition();
    test_range();
    test_collections();
    test_map();
    test_string_content();
    if (port) test_transport(port);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
