/* urlmon URL parsing and helpers batch (patches/sg/2003), run by
 * test/urlmonparse-gate.sh. Families: CoInternetParseUrl actions that
 * were E_NOTIMPL, IUri objects that did not come from urlmon (the
 * combine/parse/builder/IsEqual entry points returned E_NOTIMPL for
 * them), IsValidURL, RegisterMediaTypes, GetClassFileOrMime and
 * CopyStgMedium for GDI-style media.
 *
 *   urlmonparse-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <urlmon.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

#define PA_CANON 1
#define PA_FRIENDLY 2
#define PA_DOCUMENT 5
#define PA_ANCHOR 6
#define PA_ENCODE 7
#define PA_DECODE 8
#define PA_URLFROMPATH 10
#define PA_MIME 11
#define PA_SERVER 12
#define PA_SITE 14
#define PA_LOCATION 16
#define PA_ESCAPE 18
#define PA_UNESCAPE 19

/* ---- CoInternetParseUrl table ---- */
static const struct {
    const WCHAR *url; int action; HRESULT hr; const WCHAR *out;
} parse_tests[] = {
    { L"http://www.example.com/a/b.htm?q=1#frag", PA_FRIENDLY, S_OK, L"http://www.example.com/a/b.htm?q=1#frag" },
    { L"http://www.example.com/a/b.htm?q=1#frag", PA_DOCUMENT, S_OK, L"http://www.example.com/a/b.htm?q=1" },
    { L"http://www.example.com/a/b.htm?q=1#frag", PA_ANCHOR,   S_OK, L"#frag" },
    { L"http://www.example.com/a/b.htm?q=1#frag", PA_LOCATION, S_OK, L"#frag" },
    { L"http://www.example.com/a/b.htm?q=1#frag", PA_SITE,     S_OK, L"www.example.com" },
    { L"http://www.example.com/a/b.htm?q=1#frag", PA_MIME,     E_FAIL, L"" },
    { L"http://www.example.com/a/b.htm?q=1#frag", PA_SERVER,   E_FAIL, L"" },
    { L"C:\\dir\\file.txt",                       PA_URLFROMPATH, S_OK, L"file:///C:/dir/file.txt" },
    { L"http://a/b c%20d",                        PA_DECODE,   S_OK, L"http://a/b%20c%20d" },
    { L"http://a/b c%20d",                        PA_ESCAPE,   S_OK, L"http://a/b%20c%20d" },
    { L"http://a/b c%20d",                        PA_UNESCAPE, S_OK, L"http://a/b c d" },
    { L"http://a/b c%20d",                        PA_ENCODE,   S_OK, L"http://a/b c d" },
};

static void test_parse_url(void)
{
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(parse_tests); i++)
    {
        WCHAR buf[300] = {0}, small[4];
        DWORD len = 77;
        HRESULT hr = CoInternetParseUrl(parse_tests[i].url, parse_tests[i].action, 0, buf,
                                        ARRAY_SIZE(buf), &len, 0);
        char what[200];
        snprintf(what, sizeof(what), "ParseUrl action %d on %ls: hr %08lx", parse_tests[i].action,
                 parse_tests[i].url, (unsigned long)hr);
        check(hr == parse_tests[i].hr, what);
        if (FAILED(parse_tests[i].hr)) continue;
        snprintf(what, sizeof(what), "ParseUrl action %d result '%ls'", parse_tests[i].action, buf);
        check(!wcscmp(buf, parse_tests[i].out) && len == wcslen(parse_tests[i].out), what);
        len = 0;
        hr = CoInternetParseUrl(parse_tests[i].url, parse_tests[i].action, 0, small,
                                ARRAY_SIZE(small), &len, 0);
        snprintf(what, sizeof(what), "ParseUrl action %d small buffer: hr %08lx len %lu",
                 parse_tests[i].action, (unsigned long)hr, (unsigned long)len);
        check(hr == E_POINTER && len >= wcslen(parse_tests[i].out), what);
    }
}

/* ---- an IUri that is not urlmon's: forwards the string getters ---- */
typedef struct { IUri IUri_iface; LONG ref; IUri *real; } Foreign;
static Foreign *impl(IUri *i) { return (Foreign *)i; }
static HRESULT WINAPI f_qi(IUri *i, REFIID riid, void **p)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IUri))
    { *p = i; IUri_AddRef(i); return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_addref(IUri *i) { return InterlockedIncrement(&impl(i)->ref); }
static ULONG WINAPI f_release(IUri *i)
{
    LONG r = InterlockedDecrement(&impl(i)->ref);
    if (!r) { IUri_Release(impl(i)->real); HeapFree(GetProcessHeap(), 0, impl(i)); }
    return r;
}
static HRESULT WINAPI f_notimpl(void) { return E_NOTIMPL; }
static HRESULT WINAPI f_absolute(IUri *i, BSTR *s) { return IUri_GetAbsoluteUri(impl(i)->real, s); }
static HRESULT WINAPI f_raw(IUri *i, BSTR *s) { return IUri_GetRawUri(impl(i)->real, s); }
static HRESULT WINAPI f_display(IUri *i, BSTR *s) { return IUri_GetDisplayUri(impl(i)->real, s); }
static HRESULT WINAPI f_prop_bstr(IUri *i, Uri_PROPERTY p, BSTR *s, DWORD f)
{ return IUri_GetPropertyBSTR(impl(i)->real, p, s, f); }
static HRESULT WINAPI f_prop_len(IUri *i, Uri_PROPERTY p, DWORD *d, DWORD f)
{ return IUri_GetPropertyLength(impl(i)->real, p, d, f); }
static HRESULT WINAPI f_prop_dw(IUri *i, Uri_PROPERTY p, DWORD *d, DWORD f)
{ return IUri_GetPropertyDWORD(impl(i)->real, p, d, f); }
static HRESULT WINAPI f_has(IUri *i, Uri_PROPERTY p, BOOL *b)
{ return IUri_HasProperty(impl(i)->real, p, b); }

static IUriVtbl foreign_vtbl;
static IUri *make_foreign(const WCHAR *url)
{
    Foreign *f = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*f));
    HRESULT hr = CreateUri(url, Uri_CREATE_ALLOW_RELATIVE, 0, &f->real);
    if (FAILED(hr)) { HeapFree(GetProcessHeap(), 0, f); return NULL; }
    f->IUri_iface.lpVtbl = &foreign_vtbl;
    f->ref = 1;
    return &f->IUri_iface;
}
static void init_foreign(void)
{
    void **v = (void **)&foreign_vtbl;
    int i;
    for (i = 0; i < (int)(sizeof(foreign_vtbl) / sizeof(void *)); i++) v[i] = (void *)f_notimpl;
    foreign_vtbl.QueryInterface = f_qi;
    foreign_vtbl.AddRef = f_addref;
    foreign_vtbl.Release = f_release;
    foreign_vtbl.GetPropertyBSTR = f_prop_bstr;
    foreign_vtbl.GetPropertyLength = f_prop_len;
    foreign_vtbl.GetPropertyDWORD = f_prop_dw;
    foreign_vtbl.HasProperty = f_has;
    foreign_vtbl.GetAbsoluteUri = f_absolute;
    foreign_vtbl.GetRawUri = f_raw;
    foreign_vtbl.GetDisplayUri = f_display;
}

static int bstr_is(BSTR s, const WCHAR *w) { return s && !wcscmp(s, w); }

static void test_foreign_iuri(void)
{
    static const WCHAR base_url[] = L"http://www.example.com/a/b/c.htm?q#f";
    IUri *foreign = make_foreign(base_url), *real, *other, *result;
    IUriBuilder *builder;
    WCHAR buf[300];
    DWORD len;
    BSTR bstr;
    BOOL equal;
    HRESULT hr;

    CHECK(foreign != NULL);
    if (!foreign) return;
    CreateUri(base_url, 0, 0, &real);
    CreateUri(L"http://www.example.com/other", 0, 0, &other);

    /* IsEqual: the receiver is urlmon's, the argument is not */
    equal = 2;
    hr = IUri_IsEqual(real, foreign, &equal);
    check(hr == S_OK && equal == TRUE, "IsEqual(real, foreign same url) is TRUE");
    equal = 2;
    IUri_Release(foreign);
    foreign = make_foreign(L"http://www.example.com/other");
    hr = IUri_IsEqual(real, foreign, &equal);
    check(hr == S_OK && equal == FALSE, "IsEqual(real, foreign other url) is FALSE");
    IUri_Release(foreign);
    foreign = make_foreign(base_url);

    /* CoInternetParseIUri */
    len = 0;
    hr = CoInternetParseIUri(foreign, PARSE_CANONICALIZE, 0, buf, ARRAY_SIZE(buf), &len, 0);
    check(hr == S_OK && !wcscmp(buf, base_url), "CoInternetParseIUri(foreign, CANONICALIZE)");
    len = 0;
    hr = CoInternetParseIUri(foreign, PARSE_SITE, 0, buf, ARRAY_SIZE(buf), &len, 0);
    check(hr == S_OK && !wcscmp(buf, L"www.example.com"), "CoInternetParseIUri(foreign, SITE)");

    /* combine with a foreign base */
    result = NULL;
    hr = CoInternetCombineUrlEx(foreign, L"../x.htm", 0, &result, 0);
    check(hr == S_OK && result, "CoInternetCombineUrlEx(foreign base) succeeds");
    if (result)
    {
        bstr = NULL;
        IUri_GetAbsoluteUri(result, &bstr);
        check(bstr_is(bstr, L"http://www.example.com/a/x.htm"), "CombineUrlEx foreign base result");
        SysFreeString(bstr);
        IUri_Release(result);
    }
    {
        IUri *rel = NULL;
        CreateUri(L"y/z.htm", Uri_CREATE_ALLOW_RELATIVE, 0, &rel);
        result = NULL;
        hr = CoInternetCombineIUri(foreign, rel, 0, &result, 0);
        check(hr == S_OK && result, "CoInternetCombineIUri(foreign base, real relative)");
        if (result)
        {
            bstr = NULL;
            IUri_GetAbsoluteUri(result, &bstr);
            check(bstr_is(bstr, L"http://www.example.com/a/b/y/z.htm"), "CombineIUri result");
            SysFreeString(bstr);
            IUri_Release(result);
        }
        IUri_Release(rel);
        {
            IUri *frel = make_foreign(L"/root.htm");
            result = NULL;
            hr = CoInternetCombineIUri(real, frel, 0, &result, 0);
            check(hr == S_OK && result, "CoInternetCombineIUri(real base, foreign relative)");
            if (result)
            {
                bstr = NULL;
                IUri_GetAbsoluteUri(result, &bstr);
                check(bstr_is(bstr, L"http://www.example.com/root.htm"), "CombineIUri foreign relative result");
                SysFreeString(bstr);
                IUri_Release(result);
            }
            IUri_Release(frel);
        }
    }

    /* builders */
    builder = NULL;
    hr = CreateIUriBuilder(foreign, 0, 0, &builder);
    check(hr == S_OK && builder, "CreateIUriBuilder(foreign)");
    if (builder)
    {
        result = NULL;
        hr = IUriBuilder_CreateUri(builder, 0, 0, 0, &result);
        check(hr == S_OK && result, "builder from foreign creates a Uri");
        if (result)
        {
            bstr = NULL;
            IUri_GetAbsoluteUri(result, &bstr);
            check(bstr_is(bstr, base_url), "builder from foreign keeps the url");
            SysFreeString(bstr);
            IUri_Release(result);
        }
        IUriBuilder_Release(builder);
    }
    CreateIUriBuilder(NULL, 0, 0, &builder);
    if (builder)
    {
        IUri *f2 = make_foreign(L"ftp://ftp.example.org/pub/file.txt");
        hr = IUriBuilder_SetIUri(builder, f2);
        check(hr == S_OK, "IUriBuilder_SetIUri(foreign)");
        result = NULL;
        IUriBuilder_CreateUri(builder, 0, 0, 0, &result);
        if (result)
        {
            bstr = NULL;
            IUri_GetAbsoluteUri(result, &bstr);
            check(bstr_is(bstr, L"ftp://ftp.example.org/pub/file.txt"), "SetIUri(foreign) url");
            SysFreeString(bstr);
            IUri_Release(result);
        } else check(0, "SetIUri(foreign) builder creates a Uri");
        IUri_Release(f2);
        IUriBuilder_Release(builder);
    }
    IUri_Release(other);
    IUri_Release(real);
    IUri_Release(foreign);
}

static void test_is_valid_url(void)
{
    CHECK(IsValidURL(NULL, L"http://a/b", 0) == S_OK);
    CHECK(IsValidURL(NULL, L"ftp://host/file.txt", 0) == S_OK);
    CHECK(IsValidURL(NULL, L"file:///C:/x", 0) == S_OK);
    CHECK(IsValidURL(NULL, L"foo", 0) == S_FALSE);
    CHECK(IsValidURL(NULL, L"", 0) == S_FALSE);
    CHECK(IsValidURL(NULL, L"http://a", 1) == E_INVALIDARG);
    CHECK(IsValidURL(NULL, NULL, 0) == E_INVALIDARG);
}

static void test_register_media_types(void)
{
    const char *names[] = { "application/x-sg-test-one", "application/x-sg-test-two" };
    CLIPFORMAT cf[2] = { 0, 0 };
    CHECK(RegisterMediaTypes(2, names, cf) == S_OK);
    CHECK(cf[0] >= 0xc000 && cf[1] >= 0xc000 && cf[0] != cf[1]);
    CHECK(cf[0] == RegisterClipboardFormatA(names[0]));
    CHECK(RegisterMediaTypes(2, NULL, cf) == E_INVALIDARG);
    CHECK(RegisterMediaTypes(2, names, NULL) == E_INVALIDARG);
}

static void test_class_file_or_mime(void)
{
    static const WCHAR clsid_str[] = L"{5e600777-aaaa-bbbb-cccc-0123456789ab}";
    static const WCHAR file[] = L"C:\\sgtest.sgtxt";
    static const CLSID want = {0x5e600777, 0xaaaa, 0xbbbb, {0xcc, 0xcc, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab}};
    HKEY key;
    CLSID got;
    HANDLE h;
    HRESULT hr;

    if (!RegCreateKeyExW(HKEY_CLASSES_ROOT, L"MIME\\Database\\Content Type\\application/x-sg-test-mime",
                         0, NULL, 0, KEY_WRITE, NULL, &key, NULL))
    {
        RegSetValueExW(key, L"CLSID", 0, REG_SZ, (const BYTE *)clsid_str, sizeof(clsid_str));
        RegCloseKey(key);
    } else check(0, "create MIME database key");
    if (!RegCreateKeyExW(HKEY_CLASSES_ROOT, L".sgtxt", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL))
    {
        RegSetValueExW(key, NULL, 0, REG_SZ, (const BYTE *)L"sgtxtfile", sizeof(L"sgtxtfile"));
        RegCloseKey(key);
    }
    if (!RegCreateKeyExW(HKEY_CLASSES_ROOT, L"sgtxtfile\\CLSID", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL))
    {
        RegSetValueExW(key, NULL, 0, REG_SZ, (const BYTE *)clsid_str, sizeof(clsid_str));
        RegCloseKey(key);
    }
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; WriteFile(h, "hello", 5, &n, NULL); CloseHandle(h); }

    memset(&got, 0xff, sizeof(got));
    hr = GetClassFileOrMime(NULL, NULL, NULL, 0, L"application/x-sg-test-mime", 0, &got);
    check(hr == S_OK && IsEqualGUID(&got, &want), "GetClassFileOrMime by MIME type");
    memset(&got, 0xff, sizeof(got));
    hr = GetClassFileOrMime(NULL, file, NULL, 0, NULL, 0, &got);
    check(hr == S_OK && IsEqualGUID(&got, &want), "GetClassFileOrMime by file extension");
    hr = GetClassFileOrMime(NULL, NULL, NULL, 0, L"application/x-sg-no-such-mime", 0, &got);
    check(hr == REGDB_E_CLASSNOTREG, "GetClassFileOrMime unknown MIME is CLASSNOTREG");
    hr = GetClassFileOrMime(NULL, NULL, NULL, 0, NULL, 0, &got);
    check(hr == E_INVALIDARG, "GetClassFileOrMime with nothing is E_INVALIDARG");

    DeleteFileW(file);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"MIME\\Database\\Content Type\\application/x-sg-test-mime");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L".sgtxt");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"sgtxtfile");
}

static void test_copy_stgmedium(void)
{
    STGMEDIUM src, dst;
    HRESULT hr;
    BITMAP bm;
    HDC hdc;
    HENHMETAFILE emf;

    /* GDI: a bitmap must be duplicated, not shared */
    memset(&src, 0, sizeof(src));
    src.tymed = TYMED_GDI;
    src.hBitmap = CreateBitmap(8, 4, 1, 32, NULL);
    memset(&dst, 0, sizeof(dst));
    hr = CopyStgMedium(&src, &dst);
    check(hr == S_OK && dst.tymed == TYMED_GDI, "CopyStgMedium(TYMED_GDI) succeeds");
    check(dst.hBitmap && dst.hBitmap != src.hBitmap, "CopyStgMedium(TYMED_GDI) duplicates the bitmap");
    memset(&bm, 0, sizeof(bm));
    check(dst.hBitmap && GetObjectW(dst.hBitmap, sizeof(bm), &bm) && bm.bmWidth == 8 && bm.bmHeight == 4,
          "copied bitmap has the source dimensions");
    ReleaseStgMedium(&dst);
    DeleteObject(src.hBitmap);

    /* ENHMF */
    hdc = CreateEnhMetaFileW(NULL, NULL, NULL, NULL);
    Rectangle(hdc, 0, 0, 10, 10);
    emf = CloseEnhMetaFile(hdc);
    memset(&src, 0, sizeof(src));
    src.tymed = TYMED_ENHMF;
    src.hEnhMetaFile = emf;
    memset(&dst, 0, sizeof(dst));
    hr = CopyStgMedium(&src, &dst);
    check(hr == S_OK && dst.hEnhMetaFile && dst.hEnhMetaFile != emf, "CopyStgMedium(TYMED_ENHMF) duplicates");
    check(dst.hEnhMetaFile && GetEnhMetaFileBits(dst.hEnhMetaFile, 0, NULL) == GetEnhMetaFileBits(emf, 0, NULL),
          "copied metafile has the same size");
    ReleaseStgMedium(&dst);
    DeleteEnhMetaFile(emf);

    /* HGLOBAL still copies the bytes */
    memset(&src, 0, sizeof(src));
    src.tymed = TYMED_HGLOBAL;
    src.hGlobal = GlobalAlloc(GMEM_MOVEABLE, 16);
    memcpy(GlobalLock(src.hGlobal), "0123456789abcdef", 16);
    GlobalUnlock(src.hGlobal);
    memset(&dst, 0, sizeof(dst));
    hr = CopyStgMedium(&src, &dst);
    check(hr == S_OK && dst.hGlobal && dst.hGlobal != src.hGlobal &&
          !memcmp(GlobalLock(dst.hGlobal), "0123456789abcdef", 16), "CopyStgMedium(HGLOBAL) copies bytes");
    GlobalUnlock(dst.hGlobal);
    ReleaseStgMedium(&dst);
    GlobalFree(src.hGlobal);
}

int main(void)
{
    CoInitialize(NULL);
    init_foreign();
    test_parse_url();
    test_foreign_iuri();
    test_is_valid_url();
    test_register_media_types();
    test_class_file_or_mime();
    test_copy_stgmedium();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
