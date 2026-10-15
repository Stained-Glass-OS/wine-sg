/* Probe for patches/sg/5060: IInspectable on Windows.Security.Authentication.Web.Core's
 * WebAuthenticationCoreManager statics and its IAsyncOperation objects.
 * - the factory and its statics interfaces list their interfaces, have no runtime
 *   class name (E_ILLEGAL_METHOD_CALL, the Interop interface too) and are BaseTrust;
 * - FindAccountProviderAsync's operation lists IAsyncOperation<WebAccountProvider> and
 *   IAsyncInfo, is named IAsyncOperation`1<Windows.Security.Credentials.WebAccountProvider>
 *   and is BaseTrust;
 * - IAsyncInfo::get_Id is S_OK with a non-zero id until Close, E_ILLEGAL_METHOD_CALL after.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include "sg-inspectable.h"

DEFINE_GUID(IID_Statics, 0x6aca7c92, 0xa581, 0x4479, 0x9c, 0x10, 0x75, 0x2e, 0xff, 0x44, 0xfd, 0x34);
DEFINE_GUID(IID_Statics4, 0x54e633fe, 0x96e0, 0x41e8, 0x98, 0x32, 0x12, 0x98, 0x89, 0x7c, 0x2a, 0xaf);
DEFINE_GUID(IID_Interop, 0xf4b8e804, 0x811e, 0x4436, 0xb6, 0x9c, 0x44, 0xcb, 0x67, 0xb7, 0x20, 0x84);
DEFINE_GUID(IID_OpProvider, 0x88c66009, 0x12f7, 0x58e2, 0x8d, 0xbe, 0x6e, 0xfc, 0x62, 0x0c, 0x85, 0xba);

typedef HRESULT (WINAPI *find_t)(void *, HSTRING, void **);
typedef HRESULT (WINAPI *id_t)(void *, UINT32 *);
typedef HRESULT (WINAPI *close_t)(void *);
typedef HRESULT (WINAPI *status_t)(void *, int *);

int main(void)
{
    static const struct sg_expect expect[] =
    {
        {"manager factory", {&SG_IID_IActivationFactory, &IID_Statics, &IID_Statics4}},
        {"manager statics", {&SG_IID_IActivationFactory, &IID_Statics, &IID_Statics4}},
        {"manager statics4", {&SG_IID_IActivationFactory, &IID_Statics, &IID_Statics4}},
        {"FindAccountProviderAsync operation", {&IID_OpProvider, &SG_IID_IAsyncInfo},
         L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Credentials.WebAccountProvider>"},
        {"its IAsyncInfo", {&IID_OpProvider, &SG_IID_IAsyncInfo},
         L"Windows.Foundation.IAsyncOperation`1<Windows.Security.Credentials.WebAccountProvider>"},
    };
    void *obj[ARRAYSIZE(expect)] = {0}, *interop = NULL;
    HSTRING id;
    unsigned int i;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    WindowsCreateString(L"https://login.microsoft.com", 27, &id);
    obj[0] = sg_factory(L"Windows.Security.Authentication.Web.Core.WebAuthenticationCoreManager", &SG_IID_IActivationFactory);
    if (obj[0])
    {
        CALL(obj[0], 0, sg_qi_t, &IID_Statics, &obj[1]);
        CALL(obj[0], 0, sg_qi_t, &IID_Statics4, &obj[2]);
        CALL(obj[0], 0, sg_qi_t, &IID_Interop, &interop);
    }
    if (obj[1]) CALL(obj[1], 11, find_t, id, &obj[3]);   /* FindAccountProviderAsync */
    if (obj[3]) CALL(obj[3], 0, sg_qi_t, &SG_IID_IAsyncInfo, &obj[4]);

    for (i = 0; i < ARRAYSIZE(expect); i++)
    {
        check(obj[i] != NULL, "%s exists", expect[i].what);
        if (obj[i]) sg_check_inspectable(obj[i], &expect[i]);
    }
    check(interop != NULL, "Interop interface");
    if (interop)
    {
        HSTRING name = NULL;
        hr = CALL(interop, 4, sg_str_t, &name);
        check(hr == E_ILLEGAL_METHOD_CALL, "Interop GetRuntimeClassName %#lx", hr);
        CALL(interop, 2, sg_rel_t);
    }

    if (obj[4])
    {
        UINT32 got = 0;
        int status = -1;
        for (i = 0; i < 200; i++)   /* let the operation finish first */
        {
            CALL(obj[4], 7, status_t, &status);
            if (status) break;
            Sleep(10);
        }
        hr = CALL(obj[4], 6, id_t, &got);
        check(hr == S_OK && got, "get_Id %#lx id %u (status %d)", hr, got, status);
        hr = CALL(obj[4], 10, close_t);
        check(hr == S_OK, "Close %#lx", hr);
        hr = CALL(obj[4], 6, id_t, &got);
        check(hr == E_ILLEGAL_METHOD_CALL, "get_Id after Close %#lx", hr);
    }
    for (i = ARRAYSIZE(expect); i > 0; i--) if (obj[i - 1]) CALL(obj[i - 1], 2, sg_rel_t);
    WindowsDeleteString(id);
    RoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
