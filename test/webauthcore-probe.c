/* webauthcore-gate.sh's probe (0512): Windows.Security.Authentication.Web.Core's
 * WebAuthenticationCoreManager with no account provider installed. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

DEFINE_GUID(IID_IActivationFactory_, 0x00000035, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_Statics, 0x6aca7c92, 0xa581, 0x4479, 0x9c, 0x10, 0x75, 0x2e, 0xff, 0x44, 0xfd, 0x34);
DEFINE_GUID(IID_Statics4, 0x54e633fe, 0x96e0, 0x41e8, 0x98, 0x32, 0x12, 0x98, 0x89, 0x7c, 0x2a, 0xaf);
/* IAsyncOperation<Windows.Security.Credentials.WebAccountProvider> */
DEFINE_GUID(IID_OpProvider, 0x88c66009, 0x12f7, 0x58e2, 0x8d, 0xbe, 0x6e, 0xfc, 0x62, 0x0c, 0x85, 0xba);
DEFINE_GUID(IID_IAsyncInfo_, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

typedef struct { void **lpVtbl; } obj;
#define CALL(o, idx, type, ...) (((type)((obj *)(o))->lpVtbl[idx])((void *)(o), ##__VA_ARGS__))
typedef HRESULT (WINAPI *qi_t)(void *, REFIID, void **);
typedef ULONG (WINAPI *rel_t)(void *);
typedef HRESULT (WINAPI *find_t)(void *, HSTRING, void **);
typedef HRESULT (WINAPI *find2_t)(void *, HSTRING, HSTRING, void **);
typedef HRESULT (WINAPI *findacc_t)(void *, void *, void **);
typedef HRESULT (WINAPI *status_t)(void *, int *);
typedef HRESULT (WINAPI *results_t)(void *, void **);

/* IInspectable: 0-5; IWebAuthenticationCoreManagerStatics: 6 GetTokenSilentlyAsync ...
 * 11 FindAccountProviderAsync, 12 FindAccountProviderWithAuthorityAsync;
 * Statics4: 6 FindAllAccountsAsync, 8 FindSystemAccountProviderAsync */

static const char *wait_result(void *op, void **result)
{
    void *info;
    int status = 0, i;

    *result = (void *)1;
    if (FAILED(CALL(op, 0, qi_t, &IID_OpProvider, &info))) return "noiid";
    CALL(info, 2, rel_t);
    if (FAILED(CALL(op, 0, qi_t, &IID_IAsyncInfo_, &info))) return "noinfo";
    for (i = 0; i < 100; i++)
    {
        CALL(info, 7, status_t, &status);  /* IAsyncInfo::get_Status */
        if (status) break;
        Sleep(20);
    }
    CALL(info, 2, rel_t);
    if (status != 1) return "notcompleted";
    if (FAILED(CALL(op, 8, results_t, result))) return "noresults";   /* IAsyncOperation::GetResults */
    return "completed";
}

static HSTRING hs(const WCHAR *s)
{
    HSTRING h;
    WindowsCreateString(s, lstrlenW(s), &h);
    return h;
}

int main(void)
{
    HSTRING cls = hs(L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager");
    HSTRING aad = hs(L"https://login.microsoft.com"), org = hs(L"organizations");
    void *statics, *statics4, *op, *result;
    const char *how;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(cls, &IID_Statics, &statics);
    printf("factory %#lx\n", hr);
    if (FAILED(hr)) return 0;

    hr = CALL(statics, 11, find_t, aad, &op);
    how = SUCCEEDED(hr) ? wait_result(op, &result) : "failed";
    printf("provider %#lx %s %s\n", hr, how, result ? "some" : "none");

    hr = CALL(statics, 12, find2_t, aad, org, &op);
    how = SUCCEEDED(hr) ? wait_result(op, &result) : "failed";
    printf("authority %#lx %s %s\n", hr, how, result ? "some" : "none");

    op = (void *)1;
    hr = CALL(statics, 11, find_t, NULL, &op);
    printf("emptyid %#lx %s\n", hr, op ? "set" : "null");

    hr = CALL(statics, 0, qi_t, &IID_Statics4, &statics4);
    printf("statics4 %#lx\n", hr);
    if (FAILED(hr)) return 0;
    hr = CALL(statics4, 8, find_t, aad, &op);
    how = SUCCEEDED(hr) ? wait_result(op, &result) : "failed";
    printf("system %#lx %s %s\n", hr, how, result ? "some" : "none");

    op = (void *)1;
    hr = CALL(statics4, 6, findacc_t, NULL, &op);
    printf("allaccounts %#lx %s\n", hr, op ? "set" : "null");
    return 0;
}
