/* Probe for patches/sg/5057: IInspectable on cryptowinrt's objects.
 * - the CryptographicBuffer and KeyCredentialManager factories (and their statics
 *   interfaces) list their interfaces, have no runtime class name of their own
 *   (E_ILLEGAL_METHOD_CALL) and are BaseTrust;
 * - IsSupportedAsync's IAsyncOperation<Boolean> lists IAsyncOperation<Boolean> and
 *   IAsyncInfo, and its class name has the string's own length (no terminator).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include "sg-inspectable.h"

DEFINE_GUID(SG_IID_ICryptographicBufferStatics, 0x320b7e22, 0x3cb0, 0x4cdf, 0x86, 0x63, 0x1d, 0x28, 0x91, 0x00, 0x65, 0xeb);
DEFINE_GUID(SG_IID_IKeyCredentialManagerStatics, 0x6aac468b, 0x0ef1, 0x4ce0, 0x82, 0x90, 0x41, 0x06, 0xda, 0x6a, 0x63, 0xb5);
DEFINE_GUID(SG_IID_IAsyncOperation_boolean, 0xcdb5efb3, 0x5788, 0x509d, 0x9b, 0xe1, 0x71, 0xcc, 0xb8, 0xa3, 0x36, 0x2a);

int main(void)
{
    static const struct { const WCHAR *cls; struct sg_expect e; const GUID *statics; } factories[] =
    {
        {L"Windows.Security.Cryptography.CryptographicBuffer",
         {"CryptographicBuffer factory", {&SG_IID_IActivationFactory, &SG_IID_ICryptographicBufferStatics}}, &SG_IID_ICryptographicBufferStatics},
        {L"Windows.Security.Credentials.KeyCredentialManager",
         {"KeyCredentialManager factory", {&SG_IID_IActivationFactory, &SG_IID_IKeyCredentialManagerStatics}}, &SG_IID_IKeyCredentialManagerStatics},
    };
    static const struct sg_expect async = {"IsSupportedAsync operation", {&SG_IID_IAsyncOperation_boolean, &SG_IID_IAsyncInfo},
                                           L"Windows.Foundation.IAsyncOperation`1<Boolean>"};
    unsigned int i;
    void *f, *statics, *op;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    for (i = 0; i < ARRAYSIZE(factories); i++)
    {
        if (!(f = sg_factory(factories[i].cls, &SG_IID_IActivationFactory))) continue;
        sg_check_inspectable(f, &factories[i].e);
        statics = NULL;
        hr = CALL(f, 0, sg_qi_t, factories[i].statics, &statics);
        check(hr == S_OK, "%s: statics interface %#lx", factories[i].e.what, hr);
        if (statics)
        {
            struct sg_expect e = factories[i].e;
            e.what = "statics interface";
            sg_check_inspectable(statics, &e);
            if (i == 1)
            {
                op = NULL;
                hr = CALL(statics, 6, sg_out_t, &op);   /* IsSupportedAsync */
                check(hr == S_OK && op, "IsSupportedAsync %#lx", hr);
                if (op)
                {
                    sg_check_inspectable(op, &async);
                    CALL(op, 2, sg_rel_t);
                }
            }
            CALL(statics, 2, sg_rel_t);
        }
        CALL(f, 2, sg_rel_t);
    }
    RoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
