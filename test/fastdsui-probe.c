#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID CLSID_DS = {0x1ab4a8c0, 0x6a0b, 0x11d2, {0xad, 0x49, 0x00, 0xc0, 0x4f, 0xa3, 0x1a, 0x86}};
static const GUID CLSID_CQ = {0x83bc5ec0, 0x6f2a, 0x11d0, {0xa1, 0xc4, 0x00, 0xaa, 0x00, 0xc1, 0x6e, 0x65}};
static const GUID IID_CQ = {0xab50dec0, 0x6f1d, 0x11d0, {0xa1, 0xc4, 0x00, 0xaa, 0x00, 0xc1, 0x6e, 0x65}};

int main(void)
{
    void *ds = NULL, *cq = NULL, *obj = (void *)1;
    WCHAR buf[64];
    HRESULT hr;
    int type;
    DWORD qw[32] = { sizeof(void *) == 8 ? 64 : 52 };

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_DS, NULL, CLSCTX_INPROC_SERVER, &CLSID_DS, &ds);
    CHECK(hr == S_OK && ds, "create dsuiext %lx", hr);
    if (ds)
    {
        hr = CALL(ds, 3, HRESULT, L"server", NULL, NULL, 0); CHECK(hr == S_OK, "SetServer %lx", hr);
        hr = CALL(ds, 4, HRESULT, 0x409); CHECK(hr == S_OK, "SetLanguageID %lx", hr);
        hr = CALL(ds, 8, HRESULT, L"user", buf, 64); CHECK(hr == S_OK && !wcscmp(buf, L"user"), "GetFriendlyClassName %lx", hr);
        hr = CALL(ds, 9, HRESULT, L"user", L"sn", buf, 64); CHECK(hr == S_OK && !wcscmp(buf, L"sn"), "GetFriendlyAttributeName %lx", hr);
        hr = CALL(ds, 5, HRESULT, L"user", &CLSID_DS, &obj); CHECK(hr == HRESULT_FROM_WIN32(ERROR_NO_SUCH_DOMAIN) && !obj, "GetDisplaySpecifier %lx", hr);
        type = (int)(INT_PTR)CALL(ds, 13, INT_PTR, L"objectGUID"); CHECK(type == 8, "ADsType objectGUID %d", type);
        type = (int)(INT_PTR)CALL(ds, 13, INT_PTR, L"displayName"); CHECK(type == 3, "ADsType displayName %d", type);
    }
    {
        void *cf = NULL;
        HMODULE mod = LoadLibraryA("dsquery.dll");
        HRESULT (WINAPI *get_class)(const GUID *, const GUID *, void **) = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
        static const GUID IID_CF = {0x00000001, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
        hr = get_class ? get_class(&CLSID_CQ, &IID_CF, &cf) : E_FAIL;
        if (hr == S_OK) hr = CALL(cf, 3, HRESULT, NULL, &IID_CQ, &cq);
    }
    CHECK(hr == S_OK && cq, "create dsquery %lx", hr);
    if (cq)
    {
        obj = (void *)1;
        hr = CALL(cq, 3, HRESULT, NULL, qw, &obj); CHECK(hr == HRESULT_FROM_WIN32(ERROR_NO_SUCH_DOMAIN), "OpenQueryWindow %lx", hr);
    }
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
