/* windows.web batch (patch 3211), run by test/fastwebb1-gate.sh: HttpClient.GetBufferAsync /
 * GetInputStreamAsync / GetStringAsync, HttpContent.BufferAllAsync / ReadAsBufferAsync /
 * ReadAsInputStreamAsync / WriteToStreamAsync, HttpRequestMessage.Properties and TransportInformation,
 * HttpBaseProtocolFilter cookie manager, credentials, client certificate, ignorable certificate errors,
 * AllowAutoRedirect, HttpCookie, and the GetIids/GetRuntimeClassName of the objects.
 * The WinRT interfaces are called through raw vtable slots.
 *
 *   fastwebb1-probe.exe PORT */
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
typedef HRESULT (WINAPI *F4)(void *, void *, void *, void *, void *);
typedef HRESULT (WINAPI *FI)(void *, int);
typedef HRESULT (WINAPI *FU)(void *, UINT32, void *);
typedef HRESULT (WINAPI *FP)(void *, UINT64);
#define SLOT(o, n) ((*(void ***)(o))[n])
#define C0(o, n) ((F0)SLOT(o, n))(o)
#define C1(o, n, a) ((F1)SLOT(o, n))(o, (void *)(a))
#define C2(o, n, a, b) ((F2)SLOT(o, n))(o, (void *)(a), (void *)(b))
#define C3(o, n, a, b, c) ((F3)SLOT(o, n))(o, (void *)(a), (void *)(b), (void *)(c))
#define C4(o, n, a, b, c, d) ((F4)SLOT(o, n))(o, (void *)(a), (void *)(b), (void *)(c), (void *)(d))
#define REL(o) do { if (o) { ((ULONG (WINAPI *)(void *))SLOT(o, 2))(o); (o) = NULL; } } while (0)
#define ADDREF(o) ((ULONG (WINAPI *)(void *))SLOT(o, 1))(o)

static const GUID IID_Client = { 0x7fda1151, 0x3574, 0x4880, { 0xa8, 0xba, 0xe6, 0xb1, 0xe0, 0x06, 0x1f, 0x3d } };
static const GUID IID_ClientFactory = { 0xc30c4eca, 0xe3fa, 0x4f99, { 0xaf, 0xb4, 0x63, 0xcc, 0x65, 0x00, 0x94, 0x62 } };
static const GUID IID_UriFactory = { 0x44a9796f, 0x723e, 0x4fdf, { 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84 } };
static const GUID IID_StringContentFactory = { 0x46649d5b, 0x2e93, 0x48eb, { 0x8e, 0x61, 0x19, 0x67, 0x78, 0x78, 0xe5, 0x7f } };
static const GUID IID_Content = { 0x6b14a441, 0xfba7, 0x4bd2, { 0xaf, 0x0a, 0x83, 0x9d, 0xe7, 0xc2, 0x95, 0xda } };
static const GUID IID_Response = { 0xfee200fb, 0x8664, 0x44e0, { 0x95, 0xd9, 0x42, 0x69, 0x61, 0x99, 0xbf, 0xfc } };
static const GUID IID_RequestMessage = { 0xf5762b3c, 0x74d4, 0x4811, { 0xb5, 0xdc, 0x9f, 0x8b, 0x4e, 0x2f, 0x9a, 0xbf } };
static const GUID IID_Inspectable_ = { 0xaf86e2e0, 0xb12d, 0x4c6a, { 0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90 } };
static const GUID IID_ActivationFactory_ = { 0x00000035, 0x0000, 0x0000, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID IID_Filter = { 0x71c89b09, 0xe131, 0x4b54, { 0xa5, 0x3c, 0xeb, 0x43, 0xff, 0x37, 0xe9, 0xbb } };
static const GUID IID_HttpFilter = { 0xa4cb6dd5, 0x0902, 0x439e, { 0xbf, 0xd7, 0xe1, 0x25, 0x52, 0xb1, 0x65, 0xce } };
static const GUID IID_CookieFactory = { 0x6a0585a9, 0x931c, 0x4cd1, { 0xa9, 0x6d, 0xc2, 0x17, 0x01, 0x78, 0x5c, 0x5f } };
static const GUID IID_Cookie = { 0x1f5488e2, 0xcc2d, 0x4779, { 0x86, 0xa7, 0x88, 0xf1, 0x06, 0x87, 0xd2, 0x49 } };
static const GUID IID_CookieManager = { 0x7a431780, 0xcd4f, 0x4e57, { 0xa8, 0x4a, 0x5b, 0x0a, 0x53, 0xd6, 0xbb, 0x96 } };
static const GUID IID_Transport = { 0x70127198, 0xc6a7, 0x4ed0, { 0x83, 0x3a, 0x83, 0xfd, 0x8b, 0x8f, 0x17, 0x8d } };
static const GUID IID_PasswordCredential = { 0x6ab18989, 0xc720, 0x41a7, { 0xa6, 0xc1, 0xfe, 0xad, 0xb3, 0x63, 0x29, 0xa0 } };
static const GUID IID_BufferFactory = { 0x71af914d, 0xc10f, 0x484b, { 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27 } };
static const GUID IID_BufferByteAccess = { 0x905a0fef, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
static const GUID IID_RandomAccessStream = { 0x905a0fe1, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
static const GUID IID_OutputStream = { 0x905a0fe6, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
static const GUID IID_InputStream = { 0x905a0fe2, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
static const GUID IID_AsyncInfo = { 0x00000036, 0x0000, 0x0000, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID IID_IVector_Chain = { 0xd7828cf7, 0x4301, 0x58d3, { 0xaa, 0xb5, 0x06, 0xe5, 0xee, 0xfc, 0xf7, 0x9f } };
static const GUID IID_IVectorView_Chain = { 0xcb383486, 0xc2bc, 0x5756, { 0x91, 0x2d, 0x6a, 0x70, 0x8a, 0x07, 0xe5, 0xbd } };
static const GUID IID_ICertificate_ = { 0x333f740c, 0x04d8, 0x43b3, { 0xb2, 0x78, 0x8c, 0x5f, 0xcc, 0x9b, 0xe5, 0xa0 } };

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

static BOOL hs_equals(HSTRING h, const WCHAR *s)
{
    UINT32 len;
    const WCHAR *w = WindowsGetStringRawBuffer(h, &len);
    return w && len == wcslen(s) && !memcmp(w, s, len * sizeof(WCHAR));
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
    IInspectable *obj = NULL;
    RoActivateInstance(h, &obj);
    WindowsDeleteString(h);
    return obj;
}

static void *qi(void *o, const GUID *iid)
{
    void *r = NULL;
    if (o) C2(o, 0, iid, &r);
    return r;
}

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

static void *make_content(const WCHAR *text)
{
    void *f = factory(L"Windows.Web.Http.HttpStringContent", &IID_StringContentFactory), *content = NULL;
    HSTRING h = hs(text);
    C2(f, 6, h, &content);
    WindowsDeleteString(h);
    REL(f);
    return content;
}

static BOOL names_iid(void *obj, const GUID *iid)
{
    ULONG n = 0, i;
    IID *ids = NULL;
    BOOL found = FALSE;
    if (FAILED(C2(obj, 3, &n, &ids))) return FALSE;
    for (i = 0; i < n; i++) if (IsEqualGUID(&ids[i], iid)) found = TRUE;
    CoTaskMemFree(ids);
    return found;
}

static BOOL class_name_is(void *obj, const WCHAR *name)
{
    HSTRING h = NULL;
    BOOL ok = SUCCEEDED(C1(obj, 4, &h)) && h && hs_equals(h, name);
    WindowsDeleteString(h);
    return ok;
}

/* ---- async results */
static HRESULT results(void *op, void *out) { return C1(op, 10, out); }

static UINT32 buffer_length(void *buffer)
{
    UINT32 len = 0;
    C1(buffer, 7, &len);
    return len;
}

static BYTE *buffer_bytes(void *buffer)
{
    void *access = qi(buffer, &IID_BufferByteAccess);
    BYTE *bytes = NULL;
    if (access) { C1(access, 3, &bytes); REL(access); }
    return bytes;
}

/* read all of an IInputStream into out (capacity cap); returns the number of bytes */
static UINT32 read_stream(void *stream, BYTE *out, UINT32 cap)
{
    void *bf = factory(L"Windows.Storage.Streams.Buffer", &IID_BufferFactory), *buf = NULL, *op = NULL, *res = NULL;
    UINT32 total = 0;
    HRESULT hr;

    if (!bf) return 0;
    hr = C2(bf, 6, 64, &buf);
    REL(bf);
    if (FAILED(hr)) return 0;
    for (;;)
    {
        UINT32 got;
        res = NULL;
        if (FAILED(((HRESULT (WINAPI *)(void *, void *, UINT32, int, void *))SLOT(stream, 6))(stream, buf, 64, 0, &op))) break;
        /* the in-memory stream completes at once */
        Sleep(10);
        if (FAILED(results(op, &res))) { REL(op); break; }
        got = buffer_length(res);
        if (got && total + got <= cap) memcpy(out + total, buffer_bytes(res), got);
        total += got;
        REL(res);
        REL(op);
        if (!got) break;
    }
    REL(buf);
    return total;
}

/* ---- a PasswordCredential for the filter */
struct cred
{
    const void *const *vtbl;
    LONG ref;
    const WCHAR *user, *pass;
};
static HRESULT WINAPI cred_qi(void *s, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_PasswordCredential) || IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_Inspectable_))
    { *out = s; InterlockedIncrement(&((struct cred *)s)->ref); return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cred_addref(void *s) { return InterlockedIncrement(&((struct cred *)s)->ref); }
static ULONG WINAPI cred_release(void *s) { return InterlockedDecrement(&((struct cred *)s)->ref); }
static HRESULT WINAPI cred_iids(void *s, ULONG *n, IID **i) { return E_NOTIMPL; }
static HRESULT WINAPI cred_name(void *s, HSTRING *n) { return E_NOTIMPL; }
static HRESULT WINAPI cred_trust(void *s, int *t) { *t = 0; return S_OK; }
static HRESULT WINAPI cred_get_resource(void *s, HSTRING *v) { *v = hs(L""); return S_OK; }
static HRESULT WINAPI cred_put(void *s, HSTRING v) { return S_OK; }
static HRESULT WINAPI cred_get_user(void *s, HSTRING *v) { *v = hs(((struct cred *)s)->user); return S_OK; }
static HRESULT WINAPI cred_get_pass(void *s, HSTRING *v) { *v = hs(((struct cred *)s)->pass); return S_OK; }
static HRESULT WINAPI cred_retrieve(void *s) { return S_OK; }
static HRESULT WINAPI cred_props(void *s, void **v) { *v = NULL; return S_OK; }
static const void *const cred_vtbl[] =
{
    cred_qi, cred_addref, cred_release, cred_iids, cred_name, cred_trust,
    cred_get_resource, cred_put, cred_get_user, cred_put, cred_get_pass, cred_put, cred_retrieve, cred_props,
};

static void test_properties_and_transport(void)
{
    void *request = activate(L"Windows.Web.Http.HttpRequestMessage"), *map = NULL, *map2 = NULL, *transport = NULL;
    void *val = NULL, *view = NULL, *iterable = NULL, *it = NULL, *kvp = NULL, *errors = NULL;
    HSTRING k1 = hs(L"alpha"), k2 = hs(L"beta"), key = NULL;
    unsigned char replaced = 9, has = 9, more = 9;
    UINT32 size = 99;
    int severity = 99, n;
    HRESULT hr;
    void *v1 = activate(L"Windows.Web.Http.HttpRequestMessage");   /* any IInspectable will do as a value */

    check(request != NULL, "HttpRequestMessage activates");
    hr = C1(request, 11, &map);
    check(hr == S_OK && map, "Properties returns a map");
    hr = C1(request, 11, &map2);
    check(hr == S_OK && map2 == map, "Properties returns the same map every time");
    REL(map2);
    check(map && SUCCEEDED(C1(map, 7, &size)) && size == 0, "new property map is empty");
    hr = C3(map, 10, k1, v1, &replaced);
    check(hr == S_OK && replaced == 0, "Insert of a new key reports no replacement");
    hr = C3(map, 10, k1, NULL, &replaced);
    check(hr == S_OK && replaced == 1, "Insert of an existing key replaces it");
    C3(map, 10, k2, v1, &replaced);
    check(SUCCEEDED(C1(map, 7, &size)) && size == 2, "size counts the keys");
    hr = C2(map, 8, k1, &has);
    check(hr == S_OK && has == 1, "HasKey finds a key");
    hr = C2(map, 6, k2, &val);
    check(hr == S_OK && val == v1, "Lookup returns the stored object");
    REL(val);
    hr = C2(map, 6, k1, &val);
    check(hr == S_OK && val == NULL, "Lookup returns a NULL value that was stored");
    {
        HSTRING missing = hs(L"nope");
        hr = C2(map, 6, missing, &val);
        check(hr == E_BOUNDS, "Lookup of a missing key fails with E_BOUNDS");
        hr = C1(map, 11, missing);
        check(hr == E_BOUNDS, "Remove of a missing key fails with E_BOUNDS");
        WindowsDeleteString(missing);
    }
    check(SUCCEEDED(C1(map, 9, &view)) && view, "GetView returns a view");
    check(SUCCEEDED(C1(view, 7, &size)) && size == 2, "view has the keys");
    C1(map, 11, k1);
    check(SUCCEEDED(C1(view, 7, &size)) && size == 2, "view is a snapshot");
    check(SUCCEEDED(C1(map, 7, &size)) && size == 1, "Remove drops the key");
    REL(view);
    iterable = qi(map, &IID_IUnknown);
    {
        /* IIterable<IKeyValuePair<HSTRING, IInspectable>> through the IIDs the object names */
        ULONG cnt = 0;
        IID *ids = NULL;
        if (SUCCEEDED(C2(map, 3, &cnt, &ids)) && cnt == 2)
        {
            void *iter = qi(map, &ids[1]);
            check(iter != NULL, "map answers QI for its IIterable");
            if (iter)
            {
                n = 0;
                if (SUCCEEDED(C1(iter, 6, &it)) && it)
                {
                    more = 0;
                    C1(it, 7, &more);
                    while (more)
                    {
                        if (SUCCEEDED(C1(it, 6, &kvp)) && kvp)
                        {
                            key = NULL;
                            C1(kvp, 6, &key);
                            if (key && hs_equals(key, L"beta")) n++;
                            WindowsDeleteString(key);
                            REL(kvp);
                        }
                        more = 0;
                        C1(it, 8, &more);
                    }
                    check(n == 1, "iteration yields the remaining key/value pair");
                    REL(it);
                }
                REL(iter);
            }
        }
        else check(0, "map names its two interfaces");
        CoTaskMemFree(ids);
    }
    check(class_name_is(map, L"Windows.Foundation.Collections.IMap`2<String, Object>"), "map runtime class name");
    check(SUCCEEDED(((F0)SLOT(map, 12))(map)) && SUCCEEDED(C1(map, 7, &size)) && size == 0, "Clear empties the map");
    REL(iterable);
    REL(map);
    WindowsDeleteString(k1);
    WindowsDeleteString(k2);
    REL(v1);

    hr = C1(request, 14, &transport);
    check(hr == S_OK && transport, "TransportInformation returns an object");
    severity = 99;
    check(C1(transport, 7, &severity) == S_OK && severity == 0, "no SSL session: error severity None");
    check(C1(transport, 8, &errors) == S_OK && errors, "ServerCertificateErrors returns a view");
    size = 99;
    check(SUCCEEDED(C1(errors, 7, &size)) && size == 0, "no errors on a plain request");
    check(class_name_is(transport, L"Windows.Web.Http.HttpTransportInformation"), "transport runtime class name");
    REL(errors);
    REL(transport);
    REL(request);
}

static void test_cookies(void)
{
    void *cf = factory(L"Windows.Web.Http.HttpCookie", &IID_CookieFactory), *cookie = NULL, *bad = NULL;
    void *filter = activate(L"Windows.Web.Http.Filters.HttpBaseProtocolFilter");
    void *fi = qi(filter, &IID_Filter), *manager = NULL, *manager2 = NULL, *view = NULL, *got = NULL, *uri, *uri_other;
    HSTRING n = hs(L"sid"), d = hs(L"example.test"), p = hs(L"/app"), v = hs(L"xyz"), out = NULL, empty = hs(L"");
    unsigned char set = 9, flag = 9;
    UINT32 size = 99;
    HRESULT hr;

    check(cf != NULL, "HttpCookie factory");
    hr = C4(cf, 6, n, d, p, &cookie);
    check(hr == S_OK && cookie, "HttpCookie created from name, domain and path");
    hr = C4(cf, 6, empty, d, p, &bad);
    check(hr == E_INVALIDARG && !bad, "HttpCookie with an empty name is refused");
    check(C1(cookie, 6, &out) == S_OK && hs_equals(out, L"sid"), "cookie Name");
    WindowsDeleteString(out);
    check(C1(cookie, 7, &out) == S_OK && hs_equals(out, L"example.test"), "cookie Domain");
    WindowsDeleteString(out);
    check(C1(cookie, 8, &out) == S_OK && hs_equals(out, L"/app"), "cookie Path");
    WindowsDeleteString(out);
    check(C1(cookie, 15, &out) == S_OK && hs_equals(out, L""), "cookie Value starts empty");
    WindowsDeleteString(out);
    C1(cookie, 16, v);
    check(C1(cookie, 15, &out) == S_OK && hs_equals(out, L"xyz"), "cookie Value set");
    WindowsDeleteString(out);
    ((FI)SLOT(cookie, 14))(cookie, 1);
    flag = 0;
    check(C1(cookie, 13, &flag) == S_OK && flag == 1, "cookie Secure set");
    check(C1(cookie, 11, &flag) == S_OK && flag == 0, "cookie HttpOnly defaults to false");
    check(C1(cookie, 9, &got) == S_OK && got == NULL, "cookie Expires defaults to none");
    {
        void *str = qi(cookie, &(GUID){ 0x96369f54, 0x8eb6, 0x48f0, { 0xab, 0xce, 0xc1, 0xb2, 0x11, 0xe6, 0x27, 0xc3 } });
        check(str != NULL && SUCCEEDED(C1(str, 6, &out)) && hs_equals(out, L"sid=xyz"), "cookie ToString");
        WindowsDeleteString(out);
        REL(str);
    }

    hr = C1(fi, 13, &manager);
    check(hr == S_OK && manager, "filter CookieManager");
    hr = C1(fi, 13, &manager2);
    check(hr == S_OK && manager2 == manager, "the cookie manager is the app's one");
    REL(manager2);

    ((FI)SLOT(cookie, 14))(cookie, 0);
    uri = make_uri(80, "/app/page");
    uri_other = make_uri(80, "/elsewhere");
    hr = C2(manager, 6, cookie, &set);
    check(hr == S_OK && set == 1, "SetCookie stores the cookie");
    hr = C2(manager, 9, uri, &view);
    check(hr == S_OK && view, "GetCookies returns a view");
    check(view && SUCCEEDED(C1(view, 7, &size)) && size == 0, "cookie for another host is not returned");
    REL(view);
    {
        void *u = NULL;
        void *f = factory(L"Windows.Foundation.Uri", &IID_UriFactory);
        HSTRING h = hs(L"http://example.test/app/page");
        C2(f, 6, h, &u);
        WindowsDeleteString(h);
        REL(f);
        hr = C2(manager, 9, u, &view);
        size = 99;
        check(hr == S_OK && SUCCEEDED(C1(view, 7, &size)) && size == 1, "GetCookies finds the cookie under its path");
        got = NULL;
        check(view && SUCCEEDED(C2(view, 6, 0, &got)) && got != NULL, "GetAt returns the cookie");
        out = NULL;
        check(got && C1(got, 6, &out) == S_OK && hs_equals(out, L"sid"), "returned cookie has the name");
        WindowsDeleteString(out);
        REL(got);
        REL(view);
        REL(u);
        u = NULL;
        f = factory(L"Windows.Foundation.Uri", &IID_UriFactory);
        h = hs(L"http://sub.example.test/other");
        C2(f, 6, h, &u);
        WindowsDeleteString(h);
        REL(f);
        hr = C2(manager, 9, u, &view);
        size = 99;
        check(hr == S_OK && SUCCEEDED(C1(view, 7, &size)) && size == 0, "a path outside the cookie's is not matched");
        REL(view);
        REL(u);
        hr = C1(manager, 8, cookie);
        check(hr == S_OK, "DeleteCookie");
        u = NULL;
        f = factory(L"Windows.Foundation.Uri", &IID_UriFactory);
        h = hs(L"http://example.test/app/page");
        C2(f, 6, h, &u);
        WindowsDeleteString(h);
        REL(f);
        C2(manager, 9, u, &view);
        size = 99;
        check(SUCCEEDED(C1(view, 7, &size)) && size == 0, "deleted cookie is gone");
        REL(view);
        REL(u);
    }
    REL(uri);
    REL(uri_other);
    REL(manager);
    REL(cookie);
    REL(cf);
    REL(fi);
    REL(filter);
    WindowsDeleteString(n); WindowsDeleteString(d); WindowsDeleteString(p); WindowsDeleteString(v); WindowsDeleteString(empty);
}

static void test_filter_settings(void)
{
    void *filter = activate(L"Windows.Web.Http.Filters.HttpBaseProtocolFilter"), *fi = qi(filter, &IID_Filter);
    void *errors = NULL, *errors2 = NULL, *view = NULL, *got = NULL, *cert = NULL;
    struct cred c1 = { cred_vtbl, 1, L"u", L"p" }, c2 = { cred_vtbl, 1, L"v", L"q" };
    UINT32 size = 99, index = 99;
    int value = 99;
    unsigned char found = 9;
    HRESULT hr;

    hr = C1(fi, 19, &got);
    check(hr == S_OK && got == NULL, "ProxyCredential starts empty");
    check(C1(fi, 20, &c1) == S_OK, "put ProxyCredential");
    got = NULL;
    check(C1(fi, 19, &got) == S_OK && got == (void *)&c1, "ProxyCredential round trip");
    REL(got);
    check(C1(fi, 22, &c2) == S_OK, "put ServerCredential");
    got = NULL;
    check(C1(fi, 21, &got) == S_OK && got == (void *)&c2, "ServerCredential round trip");
    REL(got);
    check(C1(fi, 20, NULL) == S_OK, "ProxyCredential can be cleared");
    got = (void *)1;
    check(C1(fi, 19, &got) == S_OK && got == NULL, "cleared ProxyCredential reads back empty");
    C1(fi, 22, NULL);
    check(c1.ref == 1 && c2.ref == 1, "credential references are released");

    cert = activate(L"Windows.Web.Http.HttpRequestMessage");   /* any object */
    got = (void *)1;
    check(C1(fi, 14, &got) == S_OK && got == NULL, "ClientCertificate starts empty");
    check(C1(fi, 15, cert) == S_OK, "put ClientCertificate");
    got = NULL;
    check(C1(fi, 14, &got) == S_OK && got == cert, "ClientCertificate round trip");
    REL(got);
    REL(cert);
    C1(fi, 15, NULL);

    hr = C1(fi, 16, &errors);
    check(hr == S_OK && errors, "IgnorableServerCertificateErrors returns a vector");
    hr = C1(fi, 16, &errors2);
    check(hr == S_OK && errors2 == errors, "same vector every time");
    REL(errors2);
    check(SUCCEEDED(C1(errors, 7, &size)) && size == 0, "starts empty");
    check(C1(errors, 13, (void *)(INT_PTR)3) == S_OK, "Append Expired");
    check(C1(errors, 13, (void *)(INT_PTR)1) == S_OK, "Append Untrusted");
    check(SUCCEEDED(C1(errors, 7, &size)) && size == 2, "size 2");
    check(C1(errors, 13, (void *)(INT_PTR)99) == E_INVALIDARG, "an unknown value is refused");
    check(C3(errors, 9, (void *)(INT_PTR)1, &index, &found) == S_OK && found == 1 && index == 1, "IndexOf finds Untrusted at 1");
    check(C2(errors, 6, 1, &value) == S_OK && value == 1, "GetAt 1");
    check(C2(errors, 6, 5, &value) == E_BOUNDS, "GetAt out of range is E_BOUNDS");
    check(C1(errors, 8, &view) == S_OK && view, "GetView");
    check(SUCCEEDED(C1(view, 7, &size)) && size == 2, "view holds both");
    C1(errors, 15, NULL);
    check(SUCCEEDED(C1(errors, 7, &size)) && size == 0, "Clear");
    check(SUCCEEDED(C1(view, 7, &size)) && size == 2, "the view is a snapshot");
    check(names_iid(errors, &IID_IVector_Chain) && names_iid(view, &IID_IVectorView_Chain), "GetIids name the vector interfaces");
    REL(view);
    REL(errors);

    check(names_iid(fi, &IID_Filter) && names_iid(fi, &IID_HttpFilter), "filter GetIids");
    REL(fi);
    REL(filter);
}

static void test_content(void)
{
    void *content = make_content(L"h\xe9llo"), *op = NULL, *res = NULL, *stream = NULL, *mem = NULL, *out = NULL;
    UINT64 len = 0;
    BYTE data[64];
    UINT32 got;
    HRESULT hr;

    check(content != NULL, "HttpStringContent created");
    hr = C1(content, 7, &op);
    check(hr == S_OK && op, "BufferAllAsync");
    len = 0;
    check(op && results(op, &len) == S_OK && len == 6, "BufferAllAsync reports the UTF-8 length");
    REL(op);

    hr = C1(content, 8, &op);
    check(hr == S_OK && op, "ReadAsBufferAsync");
    res = NULL;
    check(op && results(op, &res) == S_OK && res, "ReadAsBufferAsync result");
    check(res && buffer_length(res) == 6 && !memcmp(buffer_bytes(res), "h\xc3\xa9llo", 6), "buffer holds the UTF-8 bytes");
    REL(res);
    REL(op);

    hr = C1(content, 9, &op);
    check(hr == S_OK && op, "ReadAsInputStreamAsync");
    stream = NULL;
    check(op && results(op, &stream) == S_OK && stream, "ReadAsInputStreamAsync result");
    got = stream ? read_stream(stream, data, sizeof(data)) : 0;
    check(got == 6 && !memcmp(data, "h\xc3\xa9llo", 6), "input stream yields the content");
    REL(stream);
    REL(op);

    mem = activate(L"Windows.Storage.Streams.InMemoryRandomAccessStream");
    out = qi(mem, &IID_OutputStream);
    check(out != NULL, "in-memory stream is an output stream");
    hr = C2(content, 12, out, &op);
    check(hr == S_OK && op, "WriteToStreamAsync");
    len = 0;
    check(op && results(op, &len) == S_OK && len == 6, "WriteToStreamAsync reports the bytes written");
    REL(op);
    {
        UINT64 size = 0;
        check(C1(mem, 6, &size) == S_OK && size == 6, "the stream holds the body");
        stream = NULL;
        check(((HRESULT (WINAPI *)(void *, UINT64, void *))SLOT(mem, 8))(mem, 0, &stream) == S_OK && stream, "GetInputStreamAt");
        got = stream ? read_stream(stream, data, sizeof(data)) : 0;
        check(got == 6 && !memcmp(data, "h\xc3\xa9llo", 6), "the body reads back from the stream");
        REL(stream);
    }
    hr = C2(content, 12, NULL, &op);
    check(hr == E_INVALIDARG, "WriteToStreamAsync with no stream is refused");
    REL(out);
    REL(mem);
    check(names_iid(content, &IID_Content), "content GetIids");
    REL(content);
}

static void test_factory_ids(void)
{
    void *f = factory(L"Windows.Web.Http.HttpMethod", &IID_IUnknown);
    void *inst = (void *)1;
    HRESULT hr;

    check(f != NULL, "HttpMethod factory");
    if (f)
    {
        check(names_iid(f, &IID_ActivationFactory_), "HttpMethod factory GetIids");
        check(class_name_is(f, L"Windows.Web.Http.HttpMethod"), "HttpMethod factory class name");
        hr = ((HRESULT (WINAPI *)(void *, void *))SLOT(f, 6))(f, &inst);
        check(hr == E_NOTIMPL && !inst, "HttpMethod has no default activation");
        REL(f);
    }
    f = factory(L"Windows.Web.Http.HttpStringContent", &IID_IUnknown);
    inst = (void *)1;
    hr = f ? ((HRESULT (WINAPI *)(void *, void *))SLOT(f, 6))(f, &inst) : 0;
    check(f && hr == E_NOTIMPL && !inst, "HttpStringContent has no default activation");
    REL(f);
    {
        HSTRING h = hs(L"Windows.Web.Http.HttpCookie");
        void *cf = NULL;
        HRESULT r = RoGetActivationFactory(h, &IID_IUnknown, &cf);
        WindowsDeleteString(h);
        check(r == S_OK && cf, "HttpCookie class is registered");
        REL(cf);
    }
}

/* ---- HttpClient against the local server */
static HRESULT get_text(void *op, WCHAR *out, size_t n)
{
    HSTRING h = NULL;
    HRESULT hr;
    out[0] = 0;
    if (FAILED(hr = results(op, &h))) return hr;
    wcsncpy(out, WindowsGetStringRawBuffer(h, NULL), n - 1);
    out[n - 1] = 0;
    WindowsDeleteString(h);
    return S_OK;
}

static void *new_client(void *filter)
{
    void *client = NULL;
    if (filter)
    {
        void *f = factory(L"Windows.Web.Http.HttpClient", &IID_ClientFactory), *hf = qi(filter, &IID_HttpFilter);
        C2(f, 6, hf, &client);
        REL(hf);
        REL(f);
    }
    else client = qi(activate(L"Windows.Web.Http.HttpClient"), &IID_Client);
    return client;
}

static void test_client(int port)
{
    void *client = new_client(NULL), *uri, *op = NULL, *res = NULL, *stream;
    WCHAR text[200];
    BYTE data[64];
    UINT32 got;
    HRESULT hr;

    check(client != NULL, "HttpClient activates");
    uri = make_uri(port, "/text");
    hr = C2(client, 11, uri, &op);
    check(hr == S_OK && op, "GetStringAsync");
    check(op && get_text(op, text, 200) == S_OK && !wcscmp(text, L"hello text"), "GetStringAsync returns the body");
    REL(op);

    hr = C2(client, 9, uri, &op);
    check(hr == S_OK && op, "GetBufferAsync");
    res = NULL;
    check(op && results(op, &res) == S_OK && res, "GetBufferAsync result");
    check(res && buffer_length(res) == 10 && !memcmp(buffer_bytes(res), "hello text", 10), "buffer has the response body");
    REL(res);
    REL(op);
    REL(uri);

    uri = make_uri(port, "/bin");
    hr = C2(client, 9, uri, &op);
    res = NULL;
    check(hr == S_OK && op && results(op, &res) == S_OK && res && buffer_length(res) == 5 &&
          !memcmp(buffer_bytes(res), "AB\0CD", 5), "GetBufferAsync keeps binary bytes");
    REL(res);
    REL(op);
    hr = C2(client, 10, uri, &op);
    check(hr == S_OK && op, "GetInputStreamAsync");
    stream = NULL;
    check(op && results(op, &stream) == S_OK && stream, "GetInputStreamAsync result");
    got = stream ? read_stream(stream, data, sizeof(data)) : 0;
    check(got == 5 && !memcmp(data, "AB\0CD", 5), "input stream yields the response body");
    REL(stream);
    REL(op);
    REL(uri);

    uri = make_uri(port, "/missing");
    hr = C2(client, 9, uri, &op);
    check(hr == S_OK && op, "GetBufferAsync of a 404 returns an operation");
    res = NULL;
    hr = op ? results(op, &res) : E_POINTER;
    check(hr == (HRESULT)0x80190194 && !res, "which fails with the HTTP status (404)");
    REL(op);
    hr = C2(client, 11, uri, &op);
    hr = op ? get_text(op, text, 200) : E_POINTER;
    check(hr == (HRESULT)0x80190194, "GetStringAsync of a 404 fails with the HTTP status");
    REL(op);
    REL(uri);
    hr = C2(client, 9, NULL, &op);
    check(hr == E_INVALIDARG && !op, "GetBufferAsync(NULL) is refused");
    hr = C2(client, 14, NULL, &op);
    check(hr == E_INVALIDARG && !op, "SendRequestAsync(NULL) is refused");
    REL(client);
}

static void test_session(int port)
{
    void *filter = activate(L"Windows.Web.Http.Filters.HttpBaseProtocolFilter"), *fi = qi(filter, &IID_Filter);
    void *client = new_client(filter), *uri, *op = NULL;
    WCHAR text[300];
    struct cred good = { cred_vtbl, 1, L"user", L"secret" };
    HRESULT hr;

    /* cookies: the Set-Cookie of one response goes back with the next request */
    uri = make_uri(port, "/echo-cookie");
    C2(client, 11, uri, &op);
    check(op && get_text(op, text, 300) == S_OK && !wcscmp(text, L"(none)"), "no cookie before the server sets one");
    REL(op);
    REL(uri);
    uri = make_uri(port, "/set-cookie");
    C2(client, 11, uri, &op);
    check(op && get_text(op, text, 300) == S_OK && !wcscmp(text, L"set"), "server sets a cookie");
    REL(op);
    REL(uri);
    uri = make_uri(port, "/echo-cookie");
    C2(client, 11, uri, &op);
    check(op && get_text(op, text, 300) == S_OK && !wcscmp(text, L"sid=abc"), "the cookie is sent back");
    REL(op);
    {
        /* and the cookie manager sees it */
        void *mgr = NULL, *view = NULL;
        UINT32 size = 99;
        C1(fi, 13, &mgr);
        C2(mgr, 9, uri, &view);
        check(view && SUCCEEDED(C1(view, 7, &size)) && size == 1, "the cookie manager lists the server's cookie");
        REL(view);
        REL(mgr);
    }
    REL(uri);

    /* server credential: a 401 challenge is answered with it */
    uri = make_uri(port, "/auth");
    C2(client, 11, uri, &op);
    hr = op ? get_text(op, text, 300) : E_POINTER;
    check(hr == (HRESULT)0x80190191, "without a credential the 401 is returned");
    REL(op);
    C1(fi, 22, &good);
    C2(client, 11, uri, &op);
    check(op && get_text(op, text, 300) == S_OK && !wcscmp(text, L"welcome user"), "with a ServerCredential the challenge is answered");
    REL(op);
    C1(fi, 22, NULL);
    REL(uri);

    /* redirects */
    uri = make_uri(port, "/redir");
    C2(client, 11, uri, &op);
    check(op && get_text(op, text, 300) == S_OK && !wcscmp(text, L"hello text"), "redirect is followed by default");
    REL(op);
    ((FI)SLOT(fi, 7))(fi, 0);
    C2(client, 11, uri, &op);
    hr = op ? get_text(op, text, 300) : E_POINTER;
    check(hr == (HRESULT)0x8019012e, "AllowAutoRedirect=false returns the 302");
    REL(op);
    REL(uri);
    REL(client);
    REL(fi);
    REL(filter);
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 0;

    RoInitialize(RO_INIT_MULTITHREADED);
    test_factory_ids();
    test_properties_and_transport();
    test_cookies();
    test_filter_settings();
    test_content();
    if (port)
    {
        test_client(port);
        test_session(port);
    }
    printf("RESULT: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures != 0;
}
