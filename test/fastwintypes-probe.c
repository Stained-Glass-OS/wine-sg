#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *HSTRING_;
typedef HRESULT (WINAPI *WCS)(const WCHAR *, UINT32, HSTRING_ *);
typedef HRESULT (WINAPI *WDS)(HSTRING_);
typedef const WCHAR *(WINAPI *WGR)(HSTRING_, UINT32 *);
typedef HRESULT (WINAPI *RGAF)(HSTRING_, const GUID *, void **);
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID IID_IPropertyValueStatics_ = {0x629bdbc8, 0xd932, 0x4ff4, {0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58}};
static const GUID IID_IPropertyValue_ = {0x4bd682dd, 0x7554, 0x40e9, {0x9a, 0x9b, 0x82, 0x65, 0x4e, 0xde, 0x7e, 0x62}};

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll"), wintypes = LoadLibraryA("wintypes.dll");
    WCS create = (WCS)GetProcAddress(combase, "WindowsCreateString");
    WDS del = (WDS)GetProcAddress(combase, "WindowsDeleteString");
    WGR raw = (WGR)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    RGAF get_factory = (RGAF)GetProcAddress(combase, "RoGetActivationFactory");
    HRESULT (WINAPI *ro_init)(int) = (void *)GetProcAddress(combase, "RoInitialize");
    HRESULT (WINAPI *contract)(const WCHAR *, UINT16, UINT16, BOOL *) = (void *)GetProcAddress(wintypes, "RoIsApiContractPresent");
    void *statics = NULL, *boxed = NULL, *pv = NULL, *array_obj = NULL;
    HSTRING_ name = NULL, str = NULL, got = NULL, cls = NULL;
    HRESULT hr;
    UINT32 count = 0;
    void **objects = NULL;
    BOOL present = FALSE;
    ULONG n = 0;
    GUID *iids = NULL;

    ro_init(1);
    create(L"Windows.Foundation.PropertyValue", 32, &name);
    hr = get_factory(name, &IID_IPropertyValueStatics_, &statics);
    CHECK(hr == S_OK, "statics %lx", hr);
    if (hr != S_OK) return 1;

    /* the boxed string must survive deleting the original HSTRING */
    create(L"survivor", 8, &str);
    hr = CALL(statics, 18, HRESULT, str, &boxed); CHECK(hr == S_OK && boxed, "CreateString %lx", hr);
    del(str);
    hr = CALL(boxed, 0, HRESULT, &IID_IPropertyValue_, &pv); CHECK(hr == S_OK, "QI IPropertyValue");
    hr = CALL(pv, 19, HRESULT, &got); CHECK(hr == S_OK && got && !wcscmp(raw(got, NULL), L"survivor"), "GetString %lx", hr);
    hr = CALL(boxed, 4, HRESULT, &cls); CHECK(hr == S_OK && cls && !wcscmp(raw(cls, NULL), L"Windows.Foundation.IReference`1<String>"), "class name %lx", hr);
    hr = CALL(boxed, 3, HRESULT, &n, &iids); CHECK(hr == S_OK && n == 2, "GetIids %lx %lu", hr, n);

    /* inspectable arrays */
    {
        void *items[2];
        items[0] = items[1] = boxed;
        hr = CALL(statics, 38, HRESULT, 2, items, &array_obj); CHECK(hr == S_OK && array_obj, "CreateInspectableArray %lx", hr);
        hr = CALL(array_obj, 0, HRESULT, &IID_IPropertyValue_, &pv); CHECK(hr == S_OK, "QI array");
        hr = CALL(pv, 38, HRESULT, &count, &objects); CHECK(hr == S_OK && count == 2 && objects && objects[0] == boxed, "GetInspectableArray %lx", hr);
    }

    hr = contract(L"Windows.Foundation.UniversalApiContract", 1, 0, &present); CHECK(hr == S_OK && present, "contract %lx", hr);
    hr = contract(L"No.Such.Contract", 1, 0, &present); CHECK(hr == S_OK && !present, "contract2 %lx", hr);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
