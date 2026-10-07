/* webauthcore-gate.sh's probe (1475): Windows.Security.Authentication.Web.Core's
 * WebAuthenticationCoreManager. Account providers are found (any non-empty
 * id), and token requests go to the native sign-in program, here the stub
 * SG_WAM_HELPER names, which picks its answer by the request's LoginHint.
 * Raw vtable calls; every result is one "key value..." line for the gate. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

DEFINE_GUID(IID_IAsyncInfo_, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_Statics, 0x6aca7c92, 0xa581, 0x4479, 0x9c, 0x10, 0x75, 0x2e, 0xff, 0x44, 0xfd, 0x34);
DEFINE_GUID(IID_Statics4, 0x54e633fe, 0x96e0, 0x41e8, 0x98, 0x32, 0x12, 0x98, 0x89, 0x7c, 0x2a, 0xaf);
DEFINE_GUID(IID_ReqFactory, 0x6cf2141c, 0x0ff0, 0x4c67, 0xb8, 0x4f, 0x99, 0xdd, 0xbe, 0x4a, 0x72, 0xc9);
DEFINE_GUID(IID_ReqResult, 0xc12a8305, 0xd1f8, 0x4483, 0x8d, 0x54, 0x38, 0xfe, 0x29, 0x27, 0x84, 0xff);
DEFINE_GUID(IID_Response, 0x67a7c5ca, 0x83f6, 0x44c6, 0xa3, 0xb1, 0x0e, 0xb6, 0x9e, 0x41, 0xfa, 0x8a);
DEFINE_GUID(IID_Request, 0xb77b4d68, 0xadcb, 0x4673, 0xb3, 0x64, 0x0c, 0xf7, 0xb3, 0x5c, 0xaf, 0x97);
DEFINE_GUID(IID_Account, 0x69473eb2, 0x8031, 0x49be, 0x80, 0xbb, 0x96, 0xcb, 0x46, 0xd9, 0x9a, 0xba);
DEFINE_GUID(IID_Account2, 0x7b56d6f8, 0x990b, 0x4eb5, 0x94, 0xa7, 0x56, 0x21, 0xf3, 0xa8, 0xb8, 0x24);
DEFINE_GUID(IID_Provider2, 0x4a01eb05, 0x4e42, 0x41d4, 0xb5, 0x18, 0xe0, 0x08, 0xa5, 0x16, 0x36, 0x14);

typedef struct { void **lpVtbl; } obj;
#define CALL(o, idx, type, ...) (((type)((obj *)(o))->lpVtbl[idx])((void *)(o), ##__VA_ARGS__))
typedef HRESULT (WINAPI *qi_t)(void *, REFIID, void **);
typedef ULONG (WINAPI *rel_t)(void *);
typedef HRESULT (WINAPI *find_t)(void *, HSTRING, void **);
typedef HRESULT (WINAPI *find2_t)(void *, HSTRING, HSTRING, void **);
typedef HRESULT (WINAPI *ptr_t)(void *, void **);
typedef HRESULT (WINAPI *ptr2_t)(void *, void *, void **);
typedef HRESULT (WINAPI *ptr3_t)(void *, void *, void *, void **);
typedef HRESULT (WINAPI *status_t)(void *, int *);
typedef HRESULT (WINAPI *str_t)(void *, HSTRING *);
typedef HRESULT (WINAPI *u32_t)(void *, UINT *);
typedef HRESULT (WINAPI *at_t)(void *, UINT, void **);
typedef HRESULT (WINAPI *lookup_t)(void *, HSTRING, HSTRING *);
typedef HRESULT (WINAPI *insert_t)(void *, HSTRING, HSTRING, BOOLEAN *);
typedef HRESULT (WINAPI *create_scope_t)(void *, void *, HSTRING, void **);

/* IInspectable 0-5. Statics: 6 GetTokenSilentlyAsync, 7 ...WithWebAccountAsync, 8 RequestTokenAsync,
 * 9 ...WithWebAccountAsync, 11 FindAccountProviderAsync, 12 ...WithAuthorityAsync.
 * Statics4: 6 FindAllAccountsAsync, 8 FindSystemAccountProviderAsync.
 * IAsyncInfo: 7 Status. IAsyncOperation: 8 GetResults. IMap: 6 Lookup, 10 Insert.
 * IVectorView: 6 GetAt, 7 Size. IWebAccountProvider: 6 Id. IWebAccountProvider2: 7 Authority.
 * IWebAccount: 7 UserName. IWebAccount2: 6 Id. IWebTokenRequestFactory: 9 CreateWithScope.
 * IWebTokenRequest: 10 Properties. IWebTokenRequestResult: 6 ResponseData, 7 ResponseStatus.
 * IWebTokenResponse: 6 Token, 8 WebAccount, 9 Properties. */

static HSTRING hs(const WCHAR *s)
{
    HSTRING h;
    WindowsCreateString(s, lstrlenW(s), &h);
    return h;
}

/* the text of an HSTRING (one line, or "-" when null) */
static const WCHAR *txt(HSTRING h)
{
    return h ? WindowsGetStringRawBuffer(h, NULL) : L"-";
}

static const char *wait_result(void *op, void **result)
{
    void *info;
    int status = 0, i;

    *result = NULL;
    if (FAILED(CALL(op, 0, qi_t, &IID_IAsyncInfo_, &info))) return "noinfo";
    for (i = 0; i < 6000; i++)   /* the stub answers at once; leave room for a loaded machine */
    {
        CALL(info, 7, status_t, &status);
        if (status) break;
        Sleep(20);
    }
    CALL(info, 2, rel_t);
    if (status != 1) return "notcompleted";
    if (FAILED(CALL(op, 8, ptr_t, result))) return "noresults";
    return "completed";
}

/* a provider for (id, authority); NULL when none */
static void *find_provider(void *statics, const WCHAR *id, const WCHAR *authority, HRESULT *hr_out, const char **how)
{
    void *op = NULL, *result = NULL;
    HSTRING i = hs(id), a = authority ? hs(authority) : NULL;
    HRESULT hr = authority ? CALL(statics, 12, find2_t, i, a, &op) : CALL(statics, 11, find_t, i, &op);

    *how = SUCCEEDED(hr) ? wait_result(op, &result) : "failed";
    *hr_out = hr;
    WindowsDeleteString(i);
    if (a) WindowsDeleteString(a);
    return result;
}

/* a token request on provider with a LoginHint (NULL: none) */
static void *make_request(void *factory, void *provider, const WCHAR *hint)
{
    void *request = NULL, *props = NULL;
    HSTRING scope = hs(L"User.Read openid");

    if (FAILED(CALL(factory, 9, create_scope_t, provider, scope, &request))) request = NULL;
    WindowsDeleteString(scope);
    if (request && hint && SUCCEEDED(CALL(request, 10, ptr_t, &props)))
    {
        HSTRING k = hs(L"LoginHint"), v = hs(hint);
        BOOLEAN replaced;
        CALL(props, 10, insert_t, k, v, &replaced);
        WindowsDeleteString(k);
        WindowsDeleteString(v);
        CALL(props, 2, rel_t);
    }
    return request;
}

/* run a token request through statics method idx (0 silent, 2 interactive, +1: with an account);
 * returns the WebTokenRequestResult or NULL, and its status */
static void *token(void *statics, int idx, void *request, void *account, int *status, HRESULT *hr_out)
{
    void *op = NULL, *result = NULL, *res = NULL;
    HRESULT hr;

    *status = -1;
    if (idx & 1) hr = CALL(statics, 6 + idx, ptr3_t, request, account, &op);
    else hr = CALL(statics, 6 + idx, ptr2_t, request, &op);
    *hr_out = hr;
    if (FAILED(hr)) return NULL;
    if (strcmp(wait_result(op, &result), "completed") || !result) return NULL;
    if (FAILED(CALL(result, 0, qi_t, &IID_ReqResult, &res))) return NULL;
    CALL(res, 7, status_t, status);
    return res;
}

static void prop(void *props, const WCHAR *name)
{
    HSTRING k = hs(name), v = NULL;
    HRESULT hr = CALL(props, 6, lookup_t, k, &v);
    printf("prop %ls %ls\n", name, SUCCEEDED(hr) ? txt(v) : L"(missing)");
    WindowsDeleteString(k);
    if (v) WindowsDeleteString(v);
}

int main(void)
{
    HSTRING cls = hs(L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager");
    HSTRING reqcls = hs(L"Windows.Security.Authentication.Web.Core.WebTokenRequest");
    HSTRING aad = hs(L"https://login.microsoft.com");
    void *statics, *statics4, *factory, *provider, *provider2, *op, *result, *res, *request;
    const char *how;
    HRESULT hr;
    int status;
    HSTRING s;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(cls, &IID_Statics, &statics);
    printf("factory %#lx\n", hr);
    if (FAILED(hr)) return 0;
    hr = RoGetActivationFactory(reqcls, &IID_ReqFactory, &factory);
    printf("reqfactory %#lx\n", hr);
    if (FAILED(hr)) return 0;

    /* providers */
    provider = find_provider(statics, L"https://login.microsoft.com", L"organizations", &hr, &how);
    printf("provider %#lx %s %s\n", hr, how, provider ? "some" : "none");
    if (!provider) return 0;
    s = NULL;
    CALL(provider, 6, str_t, &s);
    printf("providerid %ls\n", txt(s));
    if (SUCCEEDED(CALL(provider, 0, qi_t, &IID_Provider2, &provider2)))
    {
        s = NULL;
        CALL(provider2, 7, str_t, &s);
        printf("providerauthority %ls\n", txt(s));
        CALL(provider2, 2, rel_t);
    }
    else printf("providerauthority (no IWebAccountProvider2)\n");

    result = find_provider(statics, L"https://login.microsoft.com", NULL, &hr, &how);
    printf("noauthority %#lx %s %s\n", hr, how, result ? "some" : "none");

    op = (void *)1;
    hr = CALL(statics, 11, find_t, NULL, &op);
    printf("emptyid %#lx %s\n", hr, op ? "set" : "null");

    hr = CALL(statics, 0, qi_t, &IID_Statics4, &statics4);
    printf("statics4 %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        op = NULL; result = NULL;
        hr = CALL(statics4, 8, find_t, aad, &op);
        how = SUCCEEDED(hr) ? wait_result(op, &result) : "failed";
        printf("system %#lx %s %s\n", hr, how, result ? "some" : "none");
        op = (void *)1;
        hr = CALL(statics4, 6, ptr2_t, NULL, &op);
        printf("allaccounts %#lx %s\n", hr, op ? "set" : "null");
    }

    /* token requests; the stub answers by LoginHint */
    request = make_request(factory, provider, L"interact@contoso.com");
    res = token(statics, 0, request, NULL, &status, &hr);
    printf("silent %#lx %d\n", hr, status);

    request = make_request(factory, provider, L"hint-ok@contoso.com");
    res = token(statics, 2, request, NULL, &status, &hr);
    printf("interactive %#lx %d\n", hr, status);
    if (res)
    {
        void *vec = NULL, *resp = NULL, *props = NULL, *acct = NULL, *acct2 = NULL;
        UINT n = 0;

        CALL(res, 6, ptr_t, &vec);
        if (vec) CALL(vec, 7, u32_t, &n);
        printf("responses %u\n", n);
        if (n && SUCCEEDED(CALL(vec, 6, at_t, 0, &resp)))
        {
            void *r = NULL;
            if (SUCCEEDED(CALL(resp, 0, qi_t, &IID_Response, &r)))
            {
                s = NULL; CALL(r, 6, str_t, &s);
                printf("token %ls\n", txt(s));
                if (SUCCEEDED(CALL(r, 8, ptr_t, &acct)) && acct)
                {
                    void *a1 = NULL;
                    if (SUCCEEDED(CALL(acct, 0, qi_t, &IID_Account, &a1)))
                    {
                        s = NULL; CALL(a1, 7, str_t, &s);
                        printf("username %ls\n", txt(s));
                    }
                    if (SUCCEEDED(CALL(acct, 0, qi_t, &IID_Account2, &acct2)))
                    {
                        s = NULL; CALL(acct2, 6, str_t, &s);
                        printf("accountid %ls\n", txt(s));
                    }
                }
                else printf("account (none)\n");
                if (SUCCEEDED(CALL(r, 9, ptr_t, &props)))
                {
                    HSTRING k = hs(L"TokenExpiresOn"), v = NULL;
                    if (SUCCEEDED(CALL(props, 6, lookup_t, k, &v)))
                    {
                        unsigned long long t = _wcstoui64(txt(v), NULL, 10);
                        printf("expireson %s\n", t > 11644473600ULL ? "ok" : "bad");
                        printf("prop TokenExpiresOn %ls\n", txt(v));
                    }
                    else printf("expireson (missing)\n");
                    prop(props, L"expires_in");
                    prop(props, L"wamcompat_id_token");
                    prop(props, L"wamcompat_client_info");
                    prop(props, L"wamcompat_scopes");
                }
            }
        }

        /* the same account through the WithWebAccount variants */
        if (acct)
        {
            request = make_request(factory, provider, L"interact@contoso.com");
            token(statics, 1, request, acct, &status, &hr);
            printf("silentacct %#lx %d\n", hr, status);
            request = make_request(factory, provider, L"hint-ok@contoso.com");
            res = token(statics, 3, request, acct, &status, &hr);
            printf("interactiveacct %#lx %d\n", hr, status);
            /* no LoginHint: the account's UserName and Id go to the program */
            request = make_request(factory, provider, NULL);
            token(statics, 1, request, acct, &status, &hr);
            printf("silentacctnohint %#lx %d\n", hr, status);
            request = make_request(factory, provider, NULL);
            token(statics, 3, request, acct, &status, &hr);
            printf("interactiveacctnohint %#lx %d\n", hr, status);
        }
    }

    /* a silent request that succeeds */
    request = make_request(factory, provider, L"hint-silent@contoso.com");
    token(statics, 0, request, NULL, &status, &hr);
    printf("silentok %#lx %d\n", hr, status);

    /* cancellations */
    request = make_request(factory, provider, L"deny@contoso.com");
    token(statics, 2, request, NULL, &status, &hr);
    printf("denied %#lx %d\n", hr, status);
    request = make_request(factory, provider, L"cancel@contoso.com");
    token(statics, 2, request, NULL, &status, &hr);
    printf("cancelled %#lx %d\n", hr, status);
    request = make_request(factory, provider, L"usercancelled@contoso.com");
    token(statics, 2, request, NULL, &status, &hr);
    printf("cancelledcase %#lx %d\n", hr, status);

    /* the hint reaches the program */
    request = make_request(factory, provider, L"loginhint-marker@contoso.com");
    token(statics, 2, request, NULL, &status, &hr);
    printf("hint %#lx %d\n", hr, status);
    return 0;
}
