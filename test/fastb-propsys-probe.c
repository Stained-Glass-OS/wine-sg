#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <objbase.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <stdio.h>
#include <wchar.h>

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef HRESULT (WINAPI *fn_desc)(const PROPERTYKEY *, REFIID, void **);
typedef HRESULT (WINAPI *fn_descname)(PCWSTR, REFIID, void **);
typedef HRESULT (WINAPI *fn_enum)(int, REFIID, void **);
typedef HRESULT (WINAPI *fn_list)(PCWSTR, REFIID, void **);
typedef HRESULT (WINAPI *fn_fmt)(const PROPERTYKEY *, const PROPVARIANT *, int, PWSTR, DWORD);
typedef HRESULT (WINAPI *fn_fmtalloc)(const PROPERTYKEY *, const PROPVARIANT *, int, PWSTR *);
typedef HRESULT (WINAPI *fn_coerce)(const PROPERTYKEY *, PROPVARIANT *);
typedef HRESULT (WINAPI *fn_simple)(int, const PROPERTYKEY *, const PROPVARIANT *, REFIID, void **);
typedef HRESULT (WINAPI *fn_array)(const PROPERTYKEY *, const int *, const PROPVARIANT *, UINT, REFIID, void **);
typedef HRESULT (WINAPI *fn_mux)(IUnknown **, DWORD, REFIID, void **);
typedef HRESULT (WINAPI *fn_adapter)(IPropertyStore *, REFIID, void **);
typedef HRESULT (WINAPI *fn_fromobj)(IUnknown *, DWORD, REFIID, void **);
typedef HRESULT (WINAPI *fn_sps)(const void *, DWORD, const PROPERTYKEY *, PROPVARIANT *);
typedef HRESULT (WINAPI *fn_spsn)(const void *, DWORD, PCWSTR, PROPVARIANT *);
typedef HRESULT (WINAPI *fn_lookup)(PCWSTR, CLSID *);
typedef HRESULT (WINAPI *fn_change)(PROPVARIANT *, const PROPVARIANT *, int, VARTYPE);

static const PROPERTYKEY key_title = {{0xf29f85e0,0x4ff9,0x1068,{0xab,0x91,0x08,0x00,0x2b,0x27,0xb3,0xd9}}, 2};
static const PROPERTYKEY key_size = {{0xb725f130,0x47ef,0x101a,{0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac}}, 12};
static const PROPERTYKEY key_dmod = {{0xb725f130,0x47ef,0x101a,{0xa5,0xf1,0x02,0x60,0x8c,0x9e,0xeb,0xac}}, 14};
static const PROPERTYKEY key_author = {{0xf29f85e0,0x4ff9,0x1068,{0xab,0x91,0x08,0x00,0x2b,0x27,0xb3,0xd9}}, 4};
static const PROPERTYKEY key_bogus = {{0x12345678,0x1234,0x1234,{1,2,3,4,5,6,7,8}}, 77};

static void test_descriptions(HMODULE h)
{
    fn_desc get = (fn_desc)GetProcAddress(h, "PSGetPropertyDescription");
    fn_descname byname = (fn_descname)GetProcAddress(h, "PSGetPropertyDescriptionByName");
    fn_enum en = (fn_enum)GetProcAddress(h, "PSEnumeratePropertyDescriptions");
    fn_list lst = (fn_list)GetProcAddress(h, "PSGetPropertyDescriptionListFromString");
    IPropertyDescription *d = NULL;
    IPropertyDescriptionList *l = NULL;
    PROPERTYKEY k;
    WCHAR *s;
    VARTYPE vt;
    HRESULT hr;
    UINT n;
    PROPDESC_TYPE_FLAGS tf;

    CHECK(get && byname && en && lst, "exports");
    hr = get(&key_title, &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == S_OK && d, "title desc %lx", hr);
    if (d)
    {
        hr = IPropertyDescription_GetCanonicalName(d, &s);
        CHECK(hr == S_OK && !wcscmp(s, L"System.Title"), "canonical name");
        CoTaskMemFree(s);
        hr = IPropertyDescription_GetPropertyType(d, &vt);
        CHECK(hr == S_OK && vt == VT_LPWSTR, "title vt %u", vt);
        hr = IPropertyDescription_GetDisplayName(d, &s);
        CHECK(hr == S_OK && !wcscmp(s, L"Title"), "display name");
        CoTaskMemFree(s);
        hr = IPropertyDescription_GetPropertyKey(d, &k);
        CHECK(hr == S_OK && IsEqualPropertyKey(k, key_title), "key");
        hr = IPropertyDescription_GetTypeFlags(d, PDTF_MASK_ALL, &tf);
        CHECK(hr == S_OK && (tf & PDTF_ISSYSTEMPROPERTY) && !(tf & PDTF_MULTIPLEVALUES), "type flags %x", tf);
        IPropertyDescription_Release(d);
    }
    hr = get(&key_author, &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == S_OK, "author");
    if (d)
    {
        IPropertyDescription_GetPropertyType(d, &vt);
        CHECK(vt == (VT_LPWSTR | VT_VECTOR), "author vt %x", vt);
        IPropertyDescription_GetTypeFlags(d, PDTF_MASK_ALL, &tf);
        CHECK(tf & PDTF_MULTIPLEVALUES, "author multi");
        IPropertyDescription_Release(d);
    }
    hr = get(&key_size, &IID_IPropertyDescription, (void **)&d);
    if (d)
    {
        IPropertyDescription_GetPropertyType(d, &vt);
        CHECK(vt == VT_UI8, "size vt");
        IPropertyDescription_Release(d);
    }
    hr = get(&key_bogus, &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND && !d, "bogus %lx", hr);
    hr = get(&key_title, &IID_IStream, (void **)&d);
    CHECK(hr == E_NOINTERFACE, "wrong iid %lx", hr);
    hr = get(&key_title, &IID_IPropertyDescription, NULL);
    CHECK(hr == E_POINTER, "null out %lx", hr);

    hr = byname(L"System.DateModified", &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == S_OK, "by name %lx", hr);
    if (d)
    {
        hr = IPropertyDescription_GetPropertyKey(d, &k);
        CHECK(IsEqualPropertyKey(k, key_dmod), "dmod key");
        IPropertyDescription_GetPropertyType(d, &vt);
        CHECK(vt == VT_FILETIME, "dmod vt");
        IPropertyDescription_Release(d);
    }
    hr = byname(L"System.NoSuchProperty", &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND, "unknown name %lx", hr);

    hr = lst(L"prop:System.Title;System.Size", &IID_IPropertyDescriptionList, (void **)&l);
    CHECK(hr == S_OK && l, "list %lx", hr);
    if (l)
    {
        hr = IPropertyDescriptionList_GetCount(l, &n);
        CHECK(hr == S_OK && n == 2, "list count %u", n);
        hr = IPropertyDescriptionList_GetAt(l, 1, &IID_IPropertyDescription, (void **)&d);
        CHECK(hr == S_OK, "list at");
        if (d) { IPropertyDescription_GetPropertyKey(d, &k); CHECK(IsEqualPropertyKey(k, key_size), "list key order"); IPropertyDescription_Release(d); }
        hr = IPropertyDescriptionList_GetAt(l, 2, &IID_IPropertyDescription, (void **)&d);
        CHECK(FAILED(hr), "list at bound");
        IPropertyDescriptionList_Release(l);
    }
    hr = lst(L"System.Title;Bogus.Name", &IID_IPropertyDescriptionList, (void **)&l);
    CHECK(FAILED(hr), "list bogus %lx", hr);

    hr = en(PDEF_ALL, &IID_IPropertyDescriptionList, (void **)&l);
    CHECK(hr == S_OK && l, "enum all %lx", hr);
    if (l)
    {
        hr = IPropertyDescriptionList_GetCount(l, &n);
        CHECK(n > 500, "enum all count %u", n);
        IPropertyDescriptionList_Release(l);
    }
    hr = en(PDEF_NONSYSTEM, &IID_IPropertyDescriptionList, (void **)&l);
    if (l) { IPropertyDescriptionList_GetCount(l, &n); CHECK(n == 0, "nonsystem count %u", n); IPropertyDescriptionList_Release(l); }
}

static void test_format(HMODULE h)
{
    fn_fmt fmt = (fn_fmt)GetProcAddress(h, "PSFormatForDisplay");
    fn_fmtalloc fa = (fn_fmtalloc)GetProcAddress(h, "PSFormatForDisplayAlloc");
    fn_coerce co = (fn_coerce)GetProcAddress(h, "PSCoerceToCanonicalValue");
    PROPVARIANT pv;
    WCHAR buf[128], *s;
    HRESULT hr;

    PropVariantInit(&pv);
    pv.vt = VT_UI8; pv.uhVal.QuadPart = 2048;
    hr = fa(&key_size, &pv, 0, &s);
    CHECK(hr == S_OK && s && wcsstr(s, L"KB"), "size fmt %lx %ls", hr, s ? s : L"(null)");
    CoTaskMemFree(s);
    hr = fmt(&key_size, &pv, 4 /* ALWAYSKB */, buf, 128);
    CHECK(hr == S_OK && wcsstr(buf, L"2"), "size kb %ls", buf);
    hr = fmt(&key_size, &pv, 0, buf, 3);
    CHECK(FAILED(hr), "small buffer %lx", hr);

    pv.vt = VT_LPWSTR; pv.pwszVal = (WCHAR *)L"Hello";
    hr = fmt(&key_title, &pv, 0, buf, 128);
    CHECK(hr == S_OK && !wcscmp(buf, L"Hello"), "string fmt %ls", buf);
    hr = fmt(&key_title, &pv, 1 /* PREFIXNAME */, buf, 128);
    CHECK(hr == S_OK && !wcscmp(buf, L"Title: Hello"), "prefix fmt %ls", buf);

    {
        SYSTEMTIME st = { 2024, 3, 0, 9, 14, 5, 0, 0 };
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        pv.vt = VT_FILETIME; pv.filetime = ft;
        hr = fmt(&key_dmod, &pv, 0x200 /* HIDEDATE */, buf, 128);
        CHECK(hr == S_OK && wcschr(buf, ':'), "time only %ls", buf);
        hr = fmt(&key_dmod, &pv, 0x40 /* HIDETIME */, buf, 128);
        CHECK(hr == S_OK && wcschr(buf, '2') && !wcschr(buf, ':'), "date only %ls", buf);
    }
    pv.vt = VT_EMPTY;
    hr = fmt(&key_title, &pv, 0, buf, 128);
    CHECK(hr == S_OK && !buf[0], "empty fmt");
    hr = fmt(&key_bogus, &pv, 0, buf, 128);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND, "bogus fmt %lx", hr);

    /* coerce */
    pv.vt = VT_I4; pv.lVal = 77;
    hr = co(&key_size, &pv);
    CHECK(hr == S_OK && pv.vt == VT_UI8 && pv.uhVal.QuadPart == 77, "coerce size vt %u", pv.vt);
    PropVariantClear(&pv);
    pv.vt = VT_LPWSTR; pv.pwszVal = CoTaskMemAlloc(6 * 2); wcscpy(pv.pwszVal, L"Alice");
    hr = co(&key_author, &pv);
    CHECK(hr == S_OK && pv.vt == (VT_LPWSTR | VT_VECTOR) && pv.calpwstr.cElems == 1 && !wcscmp(pv.calpwstr.pElems[0], L"Alice"), "coerce author");
    PropVariantClear(&pv);
}

static void test_changetype(HMODULE h)
{
    fn_change ct = (fn_change)GetProcAddress(h, "PropVariantChangeType");
    PROPVARIANT src, dst;
    HRESULT hr;
    GUID g;

    PropVariantInit(&dst);
    src.vt = VT_I4; src.lVal = 5;
    hr = ct(&dst, &src, 0, VT_BOOL);
    CHECK(hr == S_OK && dst.vt == VT_BOOL && dst.boolVal == VARIANT_TRUE, "i4->bool %lx", hr);
    src.lVal = 0;
    hr = ct(&dst, &src, 0, VT_BOOL);
    CHECK(hr == S_OK && dst.boolVal == VARIANT_FALSE, "0->bool");
    src.vt = VT_I4; src.lVal = 3;
    hr = ct(&dst, &src, 0, VT_R8);
    CHECK(hr == S_OK && dst.vt == VT_R8 && dst.dblVal == 3.0, "i4->r8");
    hr = ct(&dst, &src, 0, VT_R4);
    CHECK(hr == S_OK && dst.vt == VT_R4 && dst.fltVal == 3.0f, "i4->r4");
    src.vt = VT_R8; src.dblVal = 2.5;
    hr = ct(&dst, &src, 0, VT_I4);
    CHECK(hr == S_OK && dst.lVal == 3 || dst.lVal == 2, "r8->i4 %ld", dst.lVal);
    src.vt = VT_LPWSTR; src.pwszVal = (WCHAR *)L"{00000001-0002-0003-0405-060708090a0b}";
    hr = ct(&dst, &src, 0, VT_CLSID);
    CHECK(hr == S_OK && dst.vt == VT_CLSID && dst.puuid && dst.puuid->Data1 == 1, "str->clsid %lx", hr);
    PropVariantClear(&dst);
    src.pwszVal = (WCHAR *)L"nonsense";
    hr = ct(&dst, &src, 0, VT_CLSID);
    CHECK(FAILED(hr), "bad clsid");
    src.vt = VT_BOOL; src.boolVal = VARIANT_TRUE;
    hr = ct(&dst, &src, PVCHF_ALPHABOOL, VT_LPWSTR);
    CHECK(hr == S_OK && !wcscmp(dst.pwszVal, L"true"), "bool alpha");
    PropVariantClear(&dst);
    hr = ct(&dst, &src, 0, VT_BSTR);
    CHECK(hr == S_OK && dst.vt == VT_BSTR && dst.bstrVal && SysStringLen(dst.bstrVal) > 0, "bool->bstr vt %u", dst.vt);
    PropVariantClear(&dst);
    {
        SYSTEMTIME st = { 2020, 6, 0, 15, 12, 30, 45, 0 };
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        src.vt = VT_FILETIME; src.filetime = ft;
        hr = ct(&dst, &src, 0, VT_LPWSTR);
        CHECK(hr == S_OK && !wcscmp(dst.pwszVal, L"2020/06/15:12:30:45.000"), "ft->str %ls", hr == S_OK ? dst.pwszVal : L"");
        PropVariantClear(&dst);
        hr = ct(&dst, &src, 0, VT_UI8);
        CHECK(hr == S_OK && dst.uhVal.LowPart == ft.dwLowDateTime, "ft->ui8");
        src.vt = VT_UI8; src.uhVal.LowPart = ft.dwLowDateTime; src.uhVal.HighPart = ft.dwHighDateTime;
        hr = ct(&dst, &src, 0, VT_FILETIME);
        CHECK(hr == S_OK && dst.vt == VT_FILETIME && dst.filetime.dwLowDateTime == ft.dwLowDateTime, "ui8->ft");
        src.vt = VT_LPWSTR; src.pwszVal = (WCHAR *)L"2020-06-15 12:30:45";
        hr = ct(&dst, &src, PVCHF_NOUSEROVERRIDE, VT_FILETIME);
        CHECK(hr == S_OK && dst.vt == VT_FILETIME, "str->ft %lx", hr);
    }
    (void)g;
}

static void test_changes(HMODULE h)
{
    fn_simple simple = (fn_simple)GetProcAddress(h, "PSCreateSimplePropertyChange");
    fn_array arr = (fn_array)GetProcAddress(h, "PSCreatePropertyChangeArray");
    IPropertyChange *c = NULL;
    IPropertyChangeArray *a = NULL;
    PROPVARIANT in, add, out;
    PROPERTYKEY keys[2] = { key_title, key_size };
    int flags[2] = { 0, 2 };
    PROPVARIANT vals[2];
    HRESULT hr;
    UINT n;

    in.vt = VT_LPWSTR; in.pwszVal = (WCHAR *)L"old";
    add.vt = VT_LPWSTR; add.pwszVal = (WCHAR *)L"new";
    hr = simple(0, &key_title, &add, &IID_IPropertyChange, (void **)&c);
    CHECK(hr == S_OK && c, "simple set %lx", hr);
    if (c)
    {
        hr = IPropertyChange_ApplyToPropVariant(c, &in, &out);
        CHECK(hr == S_OK && out.vt == VT_LPWSTR && !wcscmp(out.pwszVal, L"new"), "apply set");
        PropVariantClear(&out);
        IPropertyChange_Release(c);
    }
    hr = simple(1, &key_author, &add, &IID_IPropertyChange, (void **)&c);
    CHECK(hr == S_OK, "append");
    if (c)
    {
        PROPVARIANT empty = { 0 };
        hr = IPropertyChange_ApplyToPropVariant(c, &empty, &out);
        CHECK(hr == S_OK && out.vt == (VT_LPWSTR | VT_VECTOR) && out.calpwstr.cElems == 1, "append to empty");
        PropVariantClear(&out);
        hr = IPropertyChange_ApplyToPropVariant(c, &in, &out);
        CHECK(hr == S_OK && out.vt == (VT_LPWSTR | VT_VECTOR) && out.calpwstr.cElems == 2, "append to scalar vt %x", out.vt);
        PropVariantClear(&out);
        IPropertyChange_Release(c);
    }
    hr = simple(2, &key_title, &add, &IID_IPropertyChange, (void **)&c);
    if (c)
    {
        hr = IPropertyChange_ApplyToPropVariant(c, &in, &out);
        CHECK(hr == S_OK && out.vt == VT_EMPTY, "delete");
        IPropertyChange_Release(c);
    }
    hr = simple(7, &key_title, &add, &IID_IPropertyChange, (void **)&c);
    CHECK(FAILED(hr), "bad flag");

    vals[0] = add;
    vals[1].vt = VT_UI8; vals[1].uhVal.QuadPart = 5;
    hr = arr(keys, flags, vals, 2, &IID_IPropertyChangeArray, (void **)&a);
    CHECK(hr == S_OK && a, "array %lx", hr);
    if (a)
    {
        hr = IPropertyChangeArray_GetCount(a, &n);
        CHECK(n == 2, "array count");
        CHECK(IPropertyChangeArray_IsKeyInArray(a, &key_size) == S_OK, "in array");
        CHECK(IPropertyChangeArray_IsKeyInArray(a, &key_author) == S_FALSE, "not in array");
        hr = simple(0, &key_title, &add, &IID_IPropertyChange, (void **)&c);
        hr = IPropertyChangeArray_AppendOrReplace(a, c);
        IPropertyChangeArray_GetCount(a, &n);
        CHECK(hr == S_OK && n == 2, "replace keeps count %u", n);
        hr = IPropertyChangeArray_Append(a, c);
        IPropertyChangeArray_GetCount(a, &n);
        CHECK(n == 3, "append count");
        IPropertyChange_Release(c);
        hr = IPropertyChangeArray_RemoveAt(a, 0);
        IPropertyChangeArray_GetCount(a, &n);
        CHECK(hr == S_OK && n == 2, "remove");
        CHECK(FAILED(IPropertyChangeArray_RemoveAt(a, 9)), "remove bound");
        IPropertyChangeArray_Release(a);
    }
}

static void test_stores(HMODULE h)
{
    fn_mux mux = (fn_mux)GetProcAddress(h, "PSCreateMultiplexPropertyStore");
    fn_adapter adapter = (fn_adapter)GetProcAddress(h, "PSCreateAdapterFromPropertyStore");
    fn_fromobj fromobj = (fn_fromobj)GetProcAddress(h, "PSCreatePropertyStoreFromObject");
    IPropertyStore *a = NULL, *b = NULL, *m = NULL, *ad = NULL;
    IUnknown *units[2], *unk = NULL;
    PROPVARIANT pv, out;
    DWORD n;
    HRESULT hr;

    HRESULT (WINAPI *mem)(REFIID, void **) = (void *)GetProcAddress(h, "PSCreateMemoryPropertyStore");
    hr = mem(&IID_IPropertyStore, (void **)&a);
    hr = mem(&IID_IPropertyStore, (void **)&b);
    pv.vt = VT_LPWSTR; pv.pwszVal = (WCHAR *)L"in-a";
    IPropertyStore_SetValue(a, &key_title, &pv);
    pv.pwszVal = (WCHAR *)L"in-b";
    IPropertyStore_SetValue(b, &key_title, &pv);
    pv.vt = VT_UI4; pv.ulVal = 9;
    IPropertyStore_SetValue(b, &key_size, &pv);
    units[0] = (IUnknown *)a; units[1] = (IUnknown *)b;
    hr = mux(units, 2, &IID_IPropertyStore, (void **)&m);
    CHECK(hr == S_OK && m, "mux %lx", hr);
    if (m)
    {
        hr = IPropertyStore_GetCount(m, &n);
        CHECK(hr == S_OK && n == 3, "mux count %lu", n);
        PropVariantInit(&out);
        IPropertyStore_GetValue(m, &key_title, &out);
        CHECK(out.vt == VT_LPWSTR && !wcscmp(out.pwszVal, L"in-a"), "mux first wins");
        PropVariantClear(&out);
        IPropertyStore_GetValue(m, &key_size, &out);
        CHECK(out.vt == VT_UI4 && out.ulVal == 9, "mux falls through");
        pv.vt = VT_UI4; pv.ulVal = 11;
        hr = IPropertyStore_SetValue(m, &key_size, &pv);
        PropVariantInit(&out);
        IPropertyStore_GetValue(b, &key_size, &out);
        CHECK(hr == S_OK && out.ulVal == 11, "mux set goes to holder");
        IPropertyStore_Release(m);
    }
    hr = adapter(a, &IID_IPropertyStore, (void **)&ad);
    CHECK(hr == S_OK && ad && ad != a, "adapter");
    if (ad)
    {
        PropVariantInit(&out);
        IPropertyStore_GetValue(ad, &key_title, &out);
        CHECK(out.vt == VT_LPWSTR && !wcscmp(out.pwszVal, L"in-a"), "adapter value");
        PropVariantClear(&out);
        IPropertyStore_Release(ad);
    }
    hr = fromobj((IUnknown *)a, STGM_READWRITE, &IID_IUnknown, (void **)&unk);
    CHECK(hr == S_OK && unk && unk != (IUnknown *)a, "from object wrapper %lx", hr);
    if (unk) IUnknown_Release(unk);
    IPropertyStore_Release(a);
    IPropertyStore_Release(b);
}

#pragma pack(push, 1)
struct sps_v { DWORD size; DWORD id; BYTE reserved; DWORD type; DWORD data; };
struct sps { DWORD size; DWORD version; GUID fmtid; struct sps_v v; DWORD zero; };
#pragma pack(pop)

static void test_serialized(HMODULE h)
{
    fn_sps get = (fn_sps)GetProcAddress(h, "PSGetPropertyFromPropertyStorage");
    fn_spsn getn = (fn_spsn)GetProcAddress(h, "PSGetNamedPropertyFromPropertyStorage");
    struct sps s;
    PROPVARIANT pv;
    HRESULT hr;

    memset(&s, 0, sizeof(s));
    s.size = sizeof(s) - sizeof(DWORD);
    s.version = 0x53505331;
    s.fmtid = key_title.fmtid;
    s.v.size = sizeof(s.v);
    s.v.id = 42;
    s.v.type = VT_UI4;
    s.v.data = 1234;
    {
        PROPERTYKEY k = { key_title.fmtid, 42 }, k2 = { key_title.fmtid, 43 };
        hr = get(&s, sizeof(s), &k, &pv);
        CHECK(hr == S_OK && pv.vt == VT_UI4 && pv.ulVal == 1234, "serialized get %lx", hr);
        hr = get(&s, sizeof(s), &k2, &pv);
        CHECK(FAILED(hr), "serialized missing %lx", hr);
        s.version = 7;
        hr = get(&s, sizeof(s), &k, &pv);
        CHECK(FAILED(hr), "bad version");
    }
    (void)getn;
}

static void test_schema_file(HMODULE h)
{
    static const char xml[] =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<schema xmlns=\"http://schemas.microsoft.com/windows/2006/propertydescription\" schemaVersion=\"1.0\">\n"
        "<propertyDescriptionList publisher=\"Contoso\" product=\"Widgets\">\n"
        "<propertyDescription name=\"Contoso.Widget.Weight\" formatID=\"{6B1C3A22-5A5B-4C47-8F0F-1D3F4E1A9C01}\" propID=\"100\">\n"
        "<description>Weight</description>\n"
        "<searchInfo inInvertedIndex=\"false\" isColumn=\"true\"/>\n"
        "<typeInfo type=\"UInt32\" isInnate=\"true\" isViewable=\"true\"/>\n"
        "<labelInfo label=\"Weight &amp; Balance\" invitationText=\"Type a weight\"/>\n"
        "<displayInfo defaultColumnWidth=\"12\" displayType=\"Number\" alignment=\"Right\"/>\n"
        "</propertyDescription>\n"
        "<propertyDescription name=\"Contoso.Widget.Tags\" formatID=\"{6B1C3A22-5A5B-4C47-8F0F-1D3F4E1A9C01}\" propID=\"101\">\n"
        "<typeInfo type=\"String\" multipleValues=\"true\"/>\n"
        "</propertyDescription>\n"
        "</propertyDescriptionList></schema>\n";
    WCHAR path[MAX_PATH];
    HANDLE f;
    DWORD w;
    fn_descname byname = (fn_descname)GetProcAddress(h, "PSGetPropertyDescriptionByName");
    HRESULT (WINAPI *reg)(PCWSTR) = (void *)GetProcAddress(h, "PSRegisterPropertySchema");
    HRESULT (WINAPI *unreg)(PCWSTR) = (void *)GetProcAddress(h, "PSUnregisterPropertySchema");
    HRESULT (WINAPI *name_from_key)(const PROPERTYKEY *, PWSTR *) = (void *)GetProcAddress(h, "PSGetNameFromPropertyKey");
    IPropertyDescription *d = NULL;
    HRESULT hr;
    WCHAR *s;
    UINT u;
    VARTYPE vt;
    PROPDESC_TYPE_FLAGS tf;
    PROPDESC_DISPLAYTYPE dt;
    PROPDESC_VIEW_FLAGS vf;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"contoso.propdesc");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, xml, sizeof(xml) - 1, &w, NULL);
    CloseHandle(f);

    hr = byname(L"Contoso.Widget.Weight", &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND, "before register %lx", hr);
    hr = reg(path);
    CHECK(hr == S_OK, "register %lx", hr);
    hr = byname(L"Contoso.Widget.Weight", &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == S_OK && d, "after register %lx", hr);
    if (d)
    {
        IPropertyDescription_GetPropertyType(d, &vt);
        CHECK(vt == VT_UI4, "custom vt %u", vt);
        hr = IPropertyDescription_GetDisplayName(d, &s);
        CHECK(hr == S_OK && !wcscmp(s, L"Weight & Balance"), "custom label %ls", s);
        CoTaskMemFree(s);
        hr = IPropertyDescription_GetEditInvitation(d, &s);
        CHECK(hr == S_OK && !wcscmp(s, L"Type a weight"), "custom invitation");
        CoTaskMemFree(s);
        IPropertyDescription_GetTypeFlags(d, PDTF_MASK_ALL, &tf);
        CHECK((tf & PDTF_ISINNATE) && (tf & PDTF_ISVIEWABLE) && !(tf & PDTF_ISSYSTEMPROPERTY), "custom flags %x", tf);
        IPropertyDescription_GetDefaultColumnWidth(d, &u);
        CHECK(u == 12, "custom width");
        IPropertyDescription_GetDisplayType(d, &dt);
        CHECK(dt == PDDT_NUMBER, "custom display type");
        IPropertyDescription_GetViewFlags(d, &vf);
        CHECK(vf & PDVF_RIGHTALIGN, "custom right align");
        IPropertyDescription_Release(d);
    }
    hr = byname(L"Contoso.Widget.Tags", &IID_IPropertyDescription, (void **)&d);
    if (d)
    {
        IPropertyDescription_GetPropertyType(d, &vt);
        CHECK(vt == (VT_LPWSTR | VT_VECTOR), "custom multi vt %x", vt);
        IPropertyDescription_Release(d);
    }
    {
        PROPERTYKEY k = {{0x6B1C3A22,0x5A5B,0x4C47,{0x8F,0x0F,0x1D,0x3F,0x4E,0x1A,0x9C,0x01}}, 100};
        hr = name_from_key(&k, &s);
        CHECK(hr == S_OK && !wcscmp(s, L"Contoso.Widget.Weight"), "name from custom key");
        if (hr == S_OK) CoTaskMemFree(s);
    }
    hr = unreg(path);
    CHECK(hr == S_OK, "unregister %lx", hr);
    hr = byname(L"Contoso.Widget.Weight", &IID_IPropertyDescription, (void **)&d);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND, "after unregister %lx", hr);
    DeleteFileW(path);
}

static void test_lookup(HMODULE h)
{
    fn_lookup look = (fn_lookup)GetProcAddress(h, "PSLookupPropertyHandlerCLSID");
    CLSID c;
    HKEY k;
    HRESULT hr;

    hr = look(L"C:\\x\\file.sgtestext", &c);
    CHECK(FAILED(hr), "unknown ext %lx", hr);
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.sgtestext", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL))
    {
        static const WCHAR g[] = L"{11111111-2222-3333-4444-555555555555}";
        RegSetValueExW(k, NULL, 0, REG_SZ, (const BYTE *)g, sizeof(g));
        RegCloseKey(k);
        hr = look(L"C:\\x\\file.sgtestext", &c);
        CHECK(hr == S_OK && c.Data1 == 0x11111111, "handler clsid %lx", hr);
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.sgtestext");
    }
}

int main(void)
{
    HMODULE h;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    h = LoadLibraryW(L"propsys.dll");
    if (!h) { printf("no propsys\n"); return 1; }
    test_descriptions(h);
    test_format(h);
    test_changetype(h);
    test_changes(h);
    test_stores(h);
    test_serialized(h);
    test_schema_file(h);
    test_lookup(h);
    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
