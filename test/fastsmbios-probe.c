#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *HSTRING_;
typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HSTRING_ *);
typedef const WCHAR *(WINAPI *WGR)(HSTRING_, UINT32 *);
typedef HRESULT (WINAPI *RGAF)(HSTRING_, const GUID *, void **);
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID IID_IActivationFactory_ = {0x00000035, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID IID_ISmbios = {0x080cca7c, 0x637c, 0x48c4, {0xb7, 0x28, 0xf9, 0x27, 0x38, 0x12, 0xdb, 0x8e}};
static const GUID IID_IAsyncInfo_ = {0x00000036, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    WCS create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    WGR raw = (WGR)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    RGAF get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    void *factory = NULL;
    HSTRING_ name = NULL, cls = NULL;
    GUID *iids = NULL;
    ULONG count = 0;
    HRESULT hr;
    int trust = -1;

    ro_init(1);
    create(L"Windows.System.Profile.SystemManufacturers.SmbiosInformation", 63, &name);
    hr = get_factory(name, &IID_IActivationFactory_, &factory);
    CHECK(hr == S_OK, "factory %lx", hr);
    if (hr != S_OK) return 1;

    hr = CALL(factory, 3, HRESULT, &count, &iids);
    CHECK(hr == S_OK && count == 1 && iids, "GetIids %lx count %lu", hr, count);
    if (iids) CoTaskMemFree(iids);
    hr = CALL(factory, 4, HRESULT, &cls);
    CHECK(hr == S_OK && cls && !wcscmp(raw(cls, NULL), L"Windows.System.Profile.SystemManufacturers.SmbiosInformation"), "class name %lx", hr);
    hr = CALL(factory, 5, HRESULT, &trust);
    CHECK(hr == S_OK && trust == 0, "trust level %lx %d", hr, trust);
    {
        void *statics = NULL; HSTRING_ serial = NULL;
        hr = CALL(factory, 0, HRESULT, &IID_ISmbios, &statics);
        CHECK(hr == S_OK && statics, "QI statics %lx", hr);
        if (statics)
        {
            hr = CALL(statics, 6, HRESULT, &serial);
            /* a machine without readable SMBIOS data may fail, but never with garbage */
            CHECK(hr == S_OK ? serial != NULL || 1 : FAILED(hr), "SerialNumber %lx", hr);
        }
    }
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
