/* onlineid-ticket-gate.sh's probe (1474): Windows.Security.Authentication.OnlineId
 * ticket requests and the system authenticator's (refused) GetTicketAsync. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

DEFINE_GUID(IID_IAsyncInfo_, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_TicketRequestFactory, 0xbebb0a08, 0x9e73, 0x4077, 0x96, 0x14, 0x08, 0x61, 0x4c, 0x0b, 0xc2, 0x45);
DEFINE_GUID(IID_TicketRequest, 0x297445d3, 0xfb63, 0x4135, 0x89, 0x09, 0x4e, 0x35, 0x4c, 0x06, 0x14, 0x66);
DEFINE_GUID(IID_AuthStatics, 0x85047792, 0xf634, 0x41e3, 0x96, 0xa4, 0x51, 0x64, 0xe9, 0x02, 0xc7, 0x40);
DEFINE_GUID(IID_TicketResult, 0xdb0a5ff8, 0xb098, 0x4acd, 0x9d, 0x13, 0x9e, 0x64, 0x06, 0x52, 0xb5, 0xb6);
/* IAsyncOperation<OnlineIdSystemTicketResult> and its completed handler */
DEFINE_GUID(IID_OpResult, 0x162f5870, 0x5a4a, 0x503c, 0x98, 0x7f, 0xa0, 0x5a, 0x13, 0x12, 0xd8, 0xe4);
DEFINE_GUID(IID_HandlerResult, 0x05f9f2ec, 0x5950, 0x56f8, 0xb7, 0xf8, 0x22, 0xe2, 0x0b, 0x98, 0x46, 0x79);

typedef struct { void **lpVtbl; } obj;
#define CALL(o, idx, type, ...) (((type)((obj *)(o))->lpVtbl[idx])((void *)(o), ##__VA_ARGS__))
typedef HRESULT (WINAPI *qi_t)(void *, REFIID, void **);
typedef ULONG (WINAPI *rel_t)(void *);
typedef HRESULT (WINAPI *out_t)(void *, void **);
typedef HRESULT (WINAPI *str_t)(void *, HSTRING *);
typedef HRESULT (WINAPI *create_t)(void *, HSTRING, HSTRING, void **);
typedef HRESULT (WINAPI *create1_t)(void *, HSTRING, void **);
typedef HRESULT (WINAPI *req_t)(void *, void *, void **);
typedef HRESULT (WINAPI *putguid_t)(void *, GUID *);   /* a GUID by value is passed by reference on x64 */
typedef HRESULT (WINAPI *getguid_t)(void *, GUID *);
typedef HRESULT (WINAPI *int_t)(void *, int *);
typedef HRESULT (WINAPI *hr_t)(void *, HRESULT *);
typedef HRESULT (WINAPI *put_t)(void *, void *);

/* IInspectable 0-5; IOnlineIdServiceTicketRequestFactory 6 Create, 7 CreateAdvanced;
 * IOnlineIdServiceTicketRequest 6 Service, 7 Policy; IOnlineIdSystemAuthenticatorStatics 6 Default;
 * IOnlineIdSystemAuthenticatorForUser 6 GetTicketAsync, 7 put_ApplicationId, 8 get_ApplicationId;
 * IAsyncOperation 6 put_Completed, 7 get_Completed, 8 GetResults; IAsyncInfo 6 Id, 7 Status;
 * IOnlineIdSystemTicketResult 6 Identity, 7 Status, 8 ExtendedError */

struct handler { void **lpVtbl; LONG ref, calls; int status; };
static HRESULT WINAPI h_qi(void *i, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_HandlerResult)) { *out = i; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI h_addref(void *i) { return InterlockedIncrement(&((struct handler *)i)->ref); }
static ULONG WINAPI h_release(void *i) { return InterlockedDecrement(&((struct handler *)i)->ref); }
static HRESULT WINAPI h_invoke(void *i, void *op, int status)
{
    struct handler *h = i;
    h->calls++;
    h->status = status;
    return S_OK;
}
static void *handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

static int str_is(HSTRING h, const WCHAR *s)
{
    const WCHAR *buf = WindowsGetStringRawBuffer(h, NULL);
    return buf && !lstrcmpW(buf, s);
}

static void check_request(const char *name, void *req, const WCHAR *service, const WCHAR *policy)
{
    HSTRING s = NULL, p = NULL;
    HRESULT hr1 = CALL(req, 6, str_t, &s), hr2 = CALL(req, 7, str_t, &p);
    printf("%s %#lx %#lx %s %s\n", name, hr1, hr2, str_is(s, service) ? "service" : "wrongservice",
           str_is(p, policy) ? "policy" : "wrongpolicy");
}

int main(void)
{
    HSTRING cls_req = NULL, cls_auth = NULL, svc, pol;
    WCHAR *wsvc = L"ssl.live.com", *wpol = L"MBI_SSL";
    void *fact, *req, *statics, *auth, *op, *info, *res, *req2;
    struct handler h = { handler_vtbl, 1, 0, -1 };
    GUID g, g2 = { 0x12345678, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    int status = -1, rstatus = -1, i;
    HRESULT hr, err = 0;
    void *ident;

    RoInitialize(RO_INIT_MULTITHREADED);
    WindowsCreateString(L"Windows.Security.Authentication.OnlineId.OnlineIdServiceTicketRequest", lstrlenW(L"Windows.Security.Authentication.OnlineId.OnlineIdServiceTicketRequest"), &cls_req);
    WindowsCreateString(L"Windows.Security.Authentication.OnlineId.OnlineIdSystemAuthenticator", lstrlenW(L"Windows.Security.Authentication.OnlineId.OnlineIdSystemAuthenticator"), &cls_auth);
    WindowsCreateString(wsvc, lstrlenW(wsvc), &svc);
    WindowsCreateString(wpol, lstrlenW(wpol), &pol);

    hr = RoGetActivationFactory(cls_req, &IID_TicketRequestFactory, &fact);
    printf("reqfactory %#lx\n", hr);
    if (FAILED(hr)) return 0;
    req = NULL;
    hr = CALL(fact, 6, create_t, svc, pol, &req);
    printf("create %#lx %s\n", hr, req ? "set" : "null");
    if (SUCCEEDED(hr) && req) check_request("create_props", req, wsvc, wpol);
    req2 = NULL;
    hr = CALL(fact, 7, create1_t, svc, &req2);
    printf("advanced %#lx %s\n", hr, req2 ? "set" : "null");
    if (SUCCEEDED(hr) && req2) check_request("advanced_props", req2, wsvc, L"");

    hr = RoGetActivationFactory(cls_auth, &IID_AuthStatics, &statics);
    printf("authfactory %#lx\n", hr);
    if (FAILED(hr)) return 0;
    auth = NULL;
    hr = CALL(statics, 6, out_t, &auth);
    printf("default %#lx %s\n", hr, auth ? "set" : "null");
    if (FAILED(hr) || !auth) return 0;
    hr = CALL(auth, 7, putguid_t, &g2);
    memset(&g, 0, sizeof(g));
    i = CALL(auth, 8, getguid_t, &g);
    printf("appid %#lx %#x %s\n", hr, i, IsEqualGUID(&g, &g2) ? "same" : "different");

    if (!req) return 0;
    op = NULL;
    hr = CALL(auth, 6, req_t, req, &op);
    printf("getticket %#lx %s\n", hr, op ? "set" : "null");
    if (FAILED(hr) || !op) return 0;
    if (SUCCEEDED(CALL(op, 0, qi_t, &IID_IAsyncInfo_, &info)))
    {
        CALL(info, 7, int_t, &status);
        CALL(info, 2, rel_t);
    }
    printf("status %d\n", status);
    hr = CALL(op, 6, put_t, &h);
    printf("handler %#lx calls=%ld status=%d\n", hr, h.calls, h.status);
    res = NULL;
    hr = CALL(op, 8, out_t, &res);
    printf("getresults %#lx %s\n", hr, res ? "set" : "null");
    if (FAILED(hr) || !res) return 0;
    ident = (void *)1;
    CALL(res, 6, out_t, &ident);
    CALL(res, 7, int_t, &rstatus);
    CALL(res, 8, hr_t, &err);
    printf("result identity=%s status=%d error=%#lx\n", ident ? "set" : "null", rstatus, err);
    return 0;
}
