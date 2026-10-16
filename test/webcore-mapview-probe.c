/* Probe for patches/sg/5061: IMap<String, String>::GetView on the StringMap a
 * WebTokenRequest's Properties hands out.
 * - the view has the map's pairs (Size, Lookup, HasKey, a missing key E_BOUNDS);
 * - it is a snapshot: inserting, changing and removing keys in the map afterwards
 *   does not reach it;
 * - it is IIterable<IKeyValuePair<String, String>> and walks its own pairs.
 * IMap: 6 Lookup, 7 Size, 8 HasKey, 9 GetView, 10 Insert, 11 Remove. IMapView: 6 Lookup,
 * 7 Size, 8 HasKey. IIterable: 6 First. IIterator: 7 HasCurrent, 8 MoveNext.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include "sg-inspectable.h"

DEFINE_GUID(IID_Statics, 0x6aca7c92, 0xa581, 0x4479, 0x9c, 0x10, 0x75, 0x2e, 0xff, 0x44, 0xfd, 0x34);
DEFINE_GUID(IID_ReqFactory, 0x6cf2141c, 0x0ff0, 0x4c67, 0xb8, 0x4f, 0x99, 0xdd, 0xbe, 0x4a, 0x72, 0xc9);
DEFINE_GUID(IID_MapView, 0xac7f26f2, 0xfeb7, 0x5b2a, 0x8a, 0xc4, 0x34, 0x5b, 0xc6, 0x2c, 0xae, 0xde);
DEFINE_GUID(IID_IterableKvp, 0xe9bdaaf0, 0xcbf6, 0x5c72, 0xbe, 0x90, 0x29, 0xcb, 0xf3, 0xa1, 0x31, 0x9b);

typedef HRESULT (WINAPI *find2_t)(void *, HSTRING, HSTRING, void **);
typedef HRESULT (WINAPI *create_scope_t)(void *, void *, HSTRING, void **);
typedef HRESULT (WINAPI *lookup_t)(void *, HSTRING, HSTRING *);
typedef HRESULT (WINAPI *u32_t)(void *, UINT32 *);
typedef HRESULT (WINAPI *haskey_t)(void *, HSTRING, BOOLEAN *);
typedef HRESULT (WINAPI *insert_t)(void *, HSTRING, HSTRING, BOOLEAN *);
typedef HRESULT (WINAPI *remove_t)(void *, HSTRING);
typedef HRESULT (WINAPI *bool_t)(void *, BOOLEAN *);
typedef HRESULT (WINAPI *status_t)(void *, int *);

static HSTRING hs(const WCHAR *s)
{
    HSTRING h;
    WindowsCreateString(s, lstrlenW(s), &h);
    return h;
}

static void *wait_result(void *op)
{
    void *info, *result = NULL;
    int status = 0, i;
    if (FAILED(CALL(op, 0, sg_qi_t, &SG_IID_IAsyncInfo, &info))) return NULL;
    for (i = 0; i < 6000 && !status; i++) { CALL(info, 7, status_t, &status); if (!status) Sleep(20); }
    CALL(info, 2, sg_rel_t);
    if (status == 1) CALL(op, 8, sg_out_t, &result);
    return result;
}

static void put(void *map, const WCHAR *k, const WCHAR *v)
{
    HSTRING hk = hs(k), hv = hs(v);
    BOOLEAN replaced;
    CALL(map, 10, insert_t, hk, hv, &replaced);
    WindowsDeleteString(hk);
    WindowsDeleteString(hv);
}

int main(void)
{
    /* the view after the map changed: key, the value the view must still have (NULL: absent) */
    static const struct { const WCHAR *key, *value; } expect[] =
    {
        {L"LoginHint", L"someone@contoso.com"},
        {L"prompt", L"login"},
        {L"later", NULL},
        {L"nothere", NULL},
    };
    void *statics = NULL, *factory = NULL, *op = NULL, *provider = NULL, *request = NULL, *props = NULL;
    void *view = NULL, *iterable = NULL, *iter = NULL;
    HSTRING id = hs(L"https://login.microsoft.com"), auth = hs(L"organizations"), scope = hs(L"User.Read");
    unsigned int i;
    UINT32 size = 0;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    statics = sg_factory(L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager", &IID_Statics);
    factory = sg_factory(L"Windows.Security.Authentication.Web.Core.WebTokenRequest", &IID_ReqFactory);
    if (statics && SUCCEEDED(CALL(statics, 12, find2_t, id, auth, &op)) && op) provider = wait_result(op);
    check(provider != NULL, "an account provider");
    if (provider) CALL(factory, 9, create_scope_t, provider, scope, &request);
    check(request != NULL, "a token request");
    if (request) CALL(request, 10, sg_out_t, &props);
    check(props != NULL, "its Properties map");
    if (!props) goto done;

    put(props, L"LoginHint", L"someone@contoso.com");
    put(props, L"prompt", L"login");
    hr = CALL(props, 9, sg_out_t, &view);
    check(hr == S_OK && view, "GetView %#lx", hr);
    if (!view) goto done;

    /* change the map behind the view */
    put(props, L"later", L"x");
    put(props, L"prompt", L"consent");
    { HSTRING k = hs(L"LoginHint"); CALL(props, 11, remove_t, k); WindowsDeleteString(k); }

    hr = CALL(view, 7, u32_t, &size);
    check(hr == S_OK && size == 2, "the view's Size %#lx %u", hr, size);
    for (i = 0; i < ARRAYSIZE(expect); i++)
    {
        HSTRING k = hs(expect[i].key), v = NULL;
        BOOLEAN found = 2;
        hr = CALL(view, 6, lookup_t, k, &v);
        if (expect[i].value)
            check(hr == S_OK && v && !lstrcmpW(WindowsGetStringRawBuffer(v, NULL), expect[i].value),
                  "view Lookup %ls %#lx \"%ls\"", expect[i].key, hr, v ? WindowsGetStringRawBuffer(v, NULL) : L"");
        else
            check(hr == E_BOUNDS, "view Lookup %ls (absent) %#lx", expect[i].key, hr);
        hr = CALL(view, 8, haskey_t, k, &found);
        check(hr == S_OK && found == !!expect[i].value, "view HasKey %ls %#lx %d", expect[i].key, hr, found);
        WindowsDeleteString(v);
        WindowsDeleteString(k);
    }

    hr = CALL(view, 0, sg_qi_t, &IID_MapView, &iterable);
    check(hr == S_OK, "the view is an IMapView<String, String> %#lx", hr);
    if (iterable) CALL(iterable, 2, sg_rel_t);
    iterable = NULL;
    hr = CALL(view, 0, sg_qi_t, &IID_IterableKvp, &iterable);
    check(hr == S_OK && iterable, "the view is IIterable<IKeyValuePair<String, String>> %#lx", hr);
    if (iterable && SUCCEEDED(CALL(iterable, 6, sg_out_t, &iter)) && iter)
    {
        BOOLEAN has = 0;
        unsigned int n = 0;
        CALL(iter, 7, bool_t, &has);
        while (has && n < 10) { n++; CALL(iter, 8, bool_t, &has); }
        check(n == 2, "the view walks %u pairs", n);
        CALL(iter, 2, sg_rel_t);
    }
    else check(0, "the view's First");
    if (iterable) CALL(iterable, 2, sg_rel_t);
    CALL(view, 2, sg_rel_t);
done:
    RoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
