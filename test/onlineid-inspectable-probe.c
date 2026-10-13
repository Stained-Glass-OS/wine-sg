/* Probe for patches/sg/5059: IInspectable on Windows.Security.Authentication.OnlineId's objects.
 * - the OnlineIdSystemAuthenticator and OnlineIdServiceTicketRequest factories (and their
 *   statics interfaces) list their interfaces, have no runtime class name (E_ILLEGAL_METHOD_CALL)
 *   and are BaseTrust;
 * - the authenticator, a ticket request, GetTicketAsync's operation (also through its IAsyncInfo)
 *   and its result list their interfaces, carry their class names and are BaseTrust.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include "sg-inspectable.h"

DEFINE_GUID(IID_TicketRequestFactory, 0xbebb0a08, 0x9e73, 0x4077, 0x96, 0x14, 0x08, 0x61, 0x4c, 0x0b, 0xc2, 0x45);
DEFINE_GUID(IID_TicketRequest, 0x297445d3, 0xfb63, 0x4135, 0x89, 0x09, 0x4e, 0x35, 0x4c, 0x06, 0x14, 0x66);
DEFINE_GUID(IID_AuthStatics, 0x85047792, 0xf634, 0x41e3, 0x96, 0xa4, 0x51, 0x64, 0xe9, 0x02, 0xc7, 0x40);
DEFINE_GUID(IID_AuthForUser, 0x5798befb, 0x1de4, 0x4186, 0xa2, 0xe6, 0xb5, 0x63, 0xf8, 0x6a, 0xaf, 0x44);
DEFINE_GUID(IID_TicketResult, 0xdb0a5ff8, 0xb098, 0x4acd, 0x9d, 0x13, 0x9e, 0x64, 0x06, 0x52, 0xb5, 0xb6);
DEFINE_GUID(IID_OpResult, 0x162f5870, 0x5a4a, 0x503c, 0x98, 0x7f, 0xa0, 0x5a, 0x13, 0x12, 0xd8, 0xe4);

typedef HRESULT (WINAPI *create_t)(void *, HSTRING, HSTRING, void **);
typedef HRESULT (WINAPI *req_t)(void *, void *, void **);

#define NS L"Windows.Security.Authentication.OnlineId."

int main(void)
{
    static const struct sg_expect expect[] =
    {
        {"authenticator factory", {&SG_IID_IActivationFactory, &IID_AuthStatics}},
        {"authenticator statics", {&SG_IID_IActivationFactory, &IID_AuthStatics}},
        {"ticket request factory", {&SG_IID_IActivationFactory, &IID_TicketRequestFactory}},
        {"ticket request factory statics", {&SG_IID_IActivationFactory, &IID_TicketRequestFactory}},
        {"authenticator", {&IID_AuthForUser}, NS L"OnlineIdSystemAuthenticatorForUser"},
        {"ticket request", {&IID_TicketRequest}, NS L"OnlineIdServiceTicketRequest"},
        {"GetTicketAsync operation", {&IID_OpResult, &SG_IID_IAsyncInfo},
         L"Windows.Foundation.IAsyncOperation`1<" NS L"OnlineIdSystemTicketResult>"},
        {"GetTicketAsync operation's IAsyncInfo", {&IID_OpResult, &SG_IID_IAsyncInfo},
         L"Windows.Foundation.IAsyncOperation`1<" NS L"OnlineIdSystemTicketResult>"},
        {"ticket result", {&IID_TicketResult}, NS L"OnlineIdSystemTicketResult"},
    };
    void *obj[ARRAYSIZE(expect)] = {0};
    HSTRING svc, pol;
    unsigned int i;

    RoInitialize(RO_INIT_MULTITHREADED);
    WindowsCreateString(L"ssl.live.com", 12, &svc);
    WindowsCreateString(L"MBI_SSL", 7, &pol);
    obj[0] = sg_factory(NS L"OnlineIdSystemAuthenticator", &SG_IID_IActivationFactory);
    if (obj[0]) CALL(obj[0], 0, sg_qi_t, &IID_AuthStatics, &obj[1]);
    obj[2] = sg_factory(NS L"OnlineIdServiceTicketRequest", &SG_IID_IActivationFactory);
    if (obj[2]) CALL(obj[2], 0, sg_qi_t, &IID_TicketRequestFactory, &obj[3]);
    if (obj[1]) CALL(obj[1], 6, sg_out_t, &obj[4]);                 /* Default */
    if (obj[3]) CALL(obj[3], 6, create_t, svc, pol, &obj[5]);       /* CreateOnlineIdServiceTicketRequest */
    if (obj[4] && obj[5]) CALL(obj[4], 6, req_t, obj[5], &obj[6]);  /* GetTicketAsync */
    if (obj[6]) CALL(obj[6], 0, sg_qi_t, &SG_IID_IAsyncInfo, &obj[7]);
    if (obj[6]) CALL(obj[6], 8, sg_out_t, &obj[8]);                 /* GetResults */

    for (i = 0; i < ARRAYSIZE(expect); i++)
    {
        check(obj[i] != NULL, "%s exists", expect[i].what);
        if (obj[i]) sg_check_inspectable(obj[i], &expect[i]);
    }
    for (i = ARRAYSIZE(expect); i > 0; i--) if (obj[i - 1]) CALL(obj[i - 1], 2, sg_rel_t);
    RoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
