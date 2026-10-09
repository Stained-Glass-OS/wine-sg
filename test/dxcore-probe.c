/* DXCore (patches/sg/1667), run by test/dxcore-gate.sh: the adapter
 * factory (one per process), adapter lists by attribute, sorted; each
 * adapter's properties against DXGI's description of it, sizes and
 * errors; the memory budget; the same adapter object by LUID; event
 * registration. DXCoreCreateAdapterFactory was a stub (E_NOINTERFACE). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dxgi1_6.h>
#include <d3dcommon.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

DEFINE_GUID(IID_IDXCoreAdapterFactory, 0x78ee5945, 0xc36e, 0x4b13, 0xa6, 0x69, 0x00, 0x5d, 0xd1, 0x1c, 0x0f, 0x06);
DEFINE_GUID(IID_IDXCoreAdapterList, 0x526c7776, 0x40e9, 0x459b, 0xb7, 0x11, 0xf3, 0x2a, 0xd7, 0x6d, 0xfc, 0x28);
DEFINE_GUID(IID_IDXCoreAdapter, 0xf0db4c7f, 0xfe5a, 0x42a2, 0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e);
DEFINE_GUID(ATTR_D3D11, 0x8c47866b, 0x7583, 0x450d, 0xf0, 0xf0, 0x6b, 0xad, 0xa8, 0x95, 0xaf, 0x4b);
DEFINE_GUID(ATTR_D3D12, 0x0c9ece4d, 0x2f6e, 0x4f01, 0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1);

typedef struct { void **vtbl; } obj;
#define M(o, i) (((obj *)(o))->vtbl[i])
#define RELEASE(o) ((ULONG (WINAPI *)(void *))M(o, 2))(o)

/* properties */
enum { InstanceLuid, DriverVersion, DriverDescription, HardwareID, KmdModelVersion, ComputePreemption,
       GraphicsPreemption, DedicatedAdapterMemory, DedicatedSystemMemory, SharedSystemMemory, AcgCompatible,
       IsHardware, IsIntegrated, IsDetachable, HardwareIDParts };

static HRESULT get_property(void *a, int prop, size_t size, void *buf)
{
    return ((HRESULT (WINAPI *)(void *, int, size_t, void *))M(a, 6))(a, prop, size, buf);
}

static void WINAPI dummy_callback(int type, IUnknown *object, void *ctx) { }

int main(void)
{
    HRESULT (WINAPI *create)(REFIID, void **);
    void *factory = NULL, *factory2 = NULL, *list = NULL, *list12 = NULL, *adapter = NULL, *same = NULL, *f3 = NULL;
    IDXGIFactory1 *dxgi_factory;
    IDXGIAdapter1 *dxgi;
    DXGI_ADAPTER_DESC1 desc;
    UINT dxgi_count = 0, count, i;
    LUID luid;
    char text[256], want[256];
    uint32_t ids[4];
    uint64_t mem;
    size_t size;
    bool b;
    uint32_t cookie, cookie2;
    int prefs[1] = { 2 };
    struct { uint32_t node; int group; } group = { 0, 0 };
    uint64_t budget[4];
    HMODULE dxcore = LoadLibraryA("dxcore.dll");
    HRESULT hr;

    create = dxcore ? (void *)GetProcAddress(dxcore, "DXCoreCreateAdapterFactory") : NULL;
    check(create != NULL, "DXCoreCreateAdapterFactory");
    if (!create) goto done;
    hr = create(&IID_IDXCoreAdapterFactory, &factory);
    check(hr == S_OK && factory, "the factory");
    if (!factory) goto done;
    check(create(&IID_IDXCoreAdapterFactory, &factory2) == S_OK && factory2 == factory, "is one per process");
    RELEASE(factory2);

    CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&dxgi_factory);
    while (IDXGIFactory1_EnumAdapters1(dxgi_factory, dxgi_count, &dxgi) == S_OK)
    {
        if (!dxgi_count) IDXGIAdapter1_GetDesc1(dxgi, &desc);
        IDXGIAdapter1_Release(dxgi);
        dxgi_count++;
    }
    IDXGIFactory1_Release(dxgi_factory);
    printf("      %u DXGI adapters\n", dxgi_count);

    check(((HRESULT (WINAPI *)(void *, uint32_t, const GUID *, REFIID, void **))M(factory, 3))(factory, 0, &ATTR_D3D11,
          &IID_IDXCoreAdapterList, &list) == E_INVALIDARG, "a list of no attributes: E_INVALIDARG");
    hr = ((HRESULT (WINAPI *)(void *, uint32_t, const GUID *, REFIID, void **))M(factory, 3))(factory, 1, &ATTR_D3D11,
          &IID_IDXCoreAdapterList, &list);
    check(hr == S_OK && list, "the Direct3D 11 adapters");
    if (!list) goto done;
    count = ((uint32_t (WINAPI *)(void *))M(list, 4))(list);
    check(count == dxgi_count && count >= 1, "every DXGI adapter");
    check(!((BOOL (WINAPI *)(void *))M(list, 5))(list), "the list is not stale");
    check(((HRESULT (WINAPI *)(void *, uint32_t, const int *))M(list, 7))(list, 1, prefs) == S_OK &&
          ((BOOL (WINAPI *)(void *, int))M(list, 8))(list, 2), "sorted for performance");
    if (!count) goto done;

    /* the first DXGI adapter, wherever the sort put it */
    for (i = 0; i < count; i++)
    {
        void *a;
        ((HRESULT (WINAPI *)(void *, uint32_t, REFIID, void **))M(list, 3))(list, i, &IID_IDXCoreAdapter, &a);
        get_property(a, InstanceLuid, sizeof(luid), &luid);
        if (luid.LowPart == desc.AdapterLuid.LowPart && luid.HighPart == desc.AdapterLuid.HighPart) { adapter = a; break; }
        RELEASE(a);
    }
    check(adapter != NULL, "an adapter with the DXGI adapter's LUID");
    if (!adapter) goto done;
    check(((BOOL (WINAPI *)(void *))M(adapter, 3))(adapter), "IsValid");
    WideCharToMultiByte(CP_ACP, 0, desc.Description, -1, want, sizeof(want), NULL, NULL);
    check(((HRESULT (WINAPI *)(void *, int, size_t *))M(adapter, 7))(adapter, DriverDescription, &size) == S_OK &&
          size == strlen(want) + 1, "the description's size");
    check(get_property(adapter, DriverDescription, sizeof(text), text) == S_OK && !strcmp(text, want),
          "the description");
    printf("      %s\n", text);
    check(get_property(adapter, DriverDescription, 2, text) == E_INVALIDARG, "too small a buffer: E_INVALIDARG");
    check(get_property(adapter, 99, sizeof(text), text) == DXGI_ERROR_INVALID_CALL,
          "an unknown property: DXGI_ERROR_INVALID_CALL");
    check(get_property(adapter, HardwareID, sizeof(ids), ids) == S_OK && ids[0] == desc.VendorId &&
          ids[1] == desc.DeviceId && ids[2] == desc.SubSysId && ids[3] == desc.Revision, "the hardware ID");
    check(get_property(adapter, DedicatedAdapterMemory, sizeof(mem), &mem) == S_OK && mem == desc.DedicatedVideoMemory,
          "the dedicated memory");
    check(get_property(adapter, SharedSystemMemory, sizeof(mem), &mem) == S_OK && mem == desc.SharedSystemMemory,
          "the shared memory");
    check(get_property(adapter, IsHardware, sizeof(b), &b) == S_OK && b == !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE),
          "IsHardware");
    check(get_property(adapter, KmdModelVersion, sizeof(ids[0]), &ids[0]) == S_OK && ids[0] >= 2000,
          "the driver model");
    check(((HRESULT (WINAPI *)(void *, int, size_t, const void *, size_t, void *))M(adapter, 9))(adapter, 1,
          sizeof(group), &group, sizeof(budget), budget) == S_OK, "the memory budget");
    check(!((BOOL (WINAPI *)(void *, int))M(adapter, 10))(adapter, 1) &&
          ((HRESULT (WINAPI *)(void *, int, size_t, const void *, size_t, const void *))M(adapter, 11))(adapter, 1,
          sizeof(group), &group, sizeof(budget), budget) == DXGI_ERROR_UNSUPPORTED, "no state is set");
    check(((HRESULT (WINAPI *)(void *, const LUID *, REFIID, void **))M(factory, 4))(factory, &desc.AdapterLuid,
          &IID_IDXCoreAdapter, &same) == S_OK && same == adapter, "GetAdapterByLuid: the same adapter");
    check(((HRESULT (WINAPI *)(void *, REFIID, void **))M(adapter, 12))(adapter, &IID_IDXCoreAdapterFactory, &f3) == S_OK &&
          f3 == factory, "the adapter's factory");

    /* Direct3D 12: as d3d12 says */
    {
        HRESULT (WINAPI *create12)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
        HMODULE d3d12 = LoadLibraryA("d3d12.dll");
        BOOL want12 = FALSE, has12;
        create12 = d3d12 ? (void *)GetProcAddress(d3d12, "D3D12CreateDevice") : NULL;
        CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&dxgi_factory);
        IDXGIFactory1_EnumAdapters1(dxgi_factory, 0, &dxgi);
        if (create12) want12 = SUCCEEDED(create12((IUnknown *)dxgi, D3D_FEATURE_LEVEL_11_0, &IID_IUnknown, NULL));
        IDXGIAdapter1_Release(dxgi);
        IDXGIFactory1_Release(dxgi_factory);
        has12 = ((BOOL (WINAPI *)(void *, const GUID *))M(adapter, 4))(adapter, &ATTR_D3D12);
        printf("      Direct3D 12: %d\n", want12);
        check(has12 == want12, "the Direct3D 12 attribute");
        ((HRESULT (WINAPI *)(void *, uint32_t, const GUID *, REFIID, void **))M(factory, 3))(factory, 1, &ATTR_D3D12,
            &IID_IDXCoreAdapterList, &list12);
        check(list12 && (((uint32_t (WINAPI *)(void *))M(list12, 4))(list12) > 0) == want12, "and its list");
    }

    check(((HRESULT (WINAPI *)(void *, IUnknown *, int, void *, void *, uint32_t *))M(factory, 6))(factory,
          list, 0, dummy_callback, NULL, &cookie) == S_OK, "a list-stale event");
    check(((HRESULT (WINAPI *)(void *, IUnknown *, int, void *, void *, uint32_t *))M(factory, 6))(factory,
          list, 1, dummy_callback, NULL, &cookie2) == DXGI_ERROR_INVALID_CALL, "an adapter event on a list: refused");
    check(((HRESULT (WINAPI *)(void *, uint32_t))M(factory, 7))(factory, cookie) == S_OK &&
          ((HRESULT (WINAPI *)(void *, uint32_t))M(factory, 7))(factory, cookie) == DXGI_ERROR_NOT_FOUND,
          "unregistered once");

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
