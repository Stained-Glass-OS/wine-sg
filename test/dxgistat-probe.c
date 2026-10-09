/* DXGI factory/adapter/device long tail (patches/sg/2607): creation flags,
 * occlusion/stereo/adapters-changed registrations with cookies, adapter video
 * memory reservation, device GPU thread priority, residency, offer/reclaim,
 * EnqueueSetEvent and CreateSoftwareAdapter. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <stdio.h>

static const GUID iid_factory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID iid_factory3 = {0x25483823, 0xcd46, 0x4c7d, {0x86, 0xca, 0x47, 0xaa, 0x95, 0xb8, 0x37, 0xbd}};
static const GUID iid_factory7 = {0xa4966eed, 0x76db, 0x44da, {0x84, 0xc1, 0xee, 0x9a, 0x7a, 0xfb, 0x20, 0xa8}};
static const GUID iid_adapter3 = {0x645967a4, 0x1392, 0x4310, {0xa7, 0x98, 0x80, 0x53, 0xce, 0x3e, 0x93, 0xfd}};
static const GUID iid_device = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID iid_device1 = {0x77db970f, 0x6276, 0x48ba, {0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c}};
static const GUID iid_device2 = {0x05008617, 0xfbfd, 0x4051, {0xa7, 0x90, 0x14, 0x48, 0x84, 0xb4, 0xf6, 0xa9}};
static const GUID iid_device3 = {0x6007896c, 0x3244, 0x4afd, {0xbf, 0x18, 0xa6, 0xd3, 0xbe, 0xda, 0x50, 0x23}};

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void check_hr(HRESULT hr, HRESULT expected, const char *what)
{
    printf("%s  %s (hr %#lx)\n", hr == expected ? "PASS" : "FAIL", what, hr);
    if (hr != expected) failures++;
}

static void test_factory(UINT flags, HWND window)
{
    IDXGIFactory2 *f2 = NULL;
    IDXGIFactory3 *f3 = NULL;
    IDXGIFactory7 *f7 = NULL;
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD c1, c2, c3, c4, c5;
    IDXGIAdapter *sw = (IDXGIAdapter *)0x1;
    HRESULT hr;
    char name[96];

    hr = CreateDXGIFactory2(flags, &iid_factory2, (void **)&f2);
    snprintf(name, sizeof(name), "CreateDXGIFactory2(%u) (hr %#lx)", flags, hr);
    check(SUCCEEDED(hr), name);
    if (FAILED(hr)) return;
    IDXGIFactory2_QueryInterface(f2, &iid_factory3, (void **)&f3);
    IDXGIFactory2_QueryInterface(f2, &iid_factory7, (void **)&f7);
    check(f3 && f7, "factory exposes IDXGIFactory3 and IDXGIFactory7");
    if (!f3 || !f7) return;

    snprintf(name, sizeof(name), "GetCreationFlags returns %u", flags);
    check(IDXGIFactory3_GetCreationFlags(f3) == flags, name);
    check(IDXGIFactory2_IsWindowedStereoEnabled(f2) == FALSE, "windowed stereo is not enabled");
    check(IDXGIFactory2_IsCurrent(f2) == TRUE, "a fresh factory is current");

    /* occlusion */
    c1 = c2 = 0;
    check_hr(IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, &c1), S_OK, "RegisterOcclusionStatusEvent");
    check_hr(IDXGIFactory2_RegisterOcclusionStatusWindow(f2, window, WM_USER, &c2), S_OK, "RegisterOcclusionStatusWindow");
    check(c1 && c2 && c1 != c2, "occlusion cookies are non-zero and distinct");
    check_hr(IDXGIFactory2_RegisterOcclusionStatusEvent(f2, NULL, &c3), DXGI_ERROR_INVALID_CALL, "occlusion NULL event is rejected");
    check_hr(IDXGIFactory2_RegisterOcclusionStatusEvent(f2, (HANDLE)0x12345670, &c3), DXGI_ERROR_INVALID_CALL, "occlusion bogus handle is rejected");
    check_hr(IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, NULL), DXGI_ERROR_INVALID_CALL, "occlusion NULL cookie is rejected");
    check_hr(IDXGIFactory2_RegisterOcclusionStatusWindow(f2, (HWND)0x12345670, WM_USER, &c3), DXGI_ERROR_INVALID_CALL, "occlusion bad window is rejected");
    IDXGIFactory2_UnregisterOcclusionStatus(f2, c1);
    IDXGIFactory2_UnregisterOcclusionStatus(f2, c2);

    /* stereo */
    c3 = c4 = 0;
    check_hr(IDXGIFactory2_RegisterStereoStatusEvent(f2, ev, &c3), S_OK, "RegisterStereoStatusEvent");
    check_hr(IDXGIFactory2_RegisterStereoStatusWindow(f2, window, WM_USER, &c4), S_OK, "RegisterStereoStatusWindow");
    check(c3 && c4 && c3 != c4, "stereo cookies are non-zero and distinct");
    check_hr(IDXGIFactory2_RegisterStereoStatusEvent(f2, NULL, &c5), DXGI_ERROR_INVALID_CALL, "stereo NULL event is rejected");
    IDXGIFactory2_UnregisterStereoStatus(f2, c3);
    IDXGIFactory2_UnregisterStereoStatus(f2, c4);

    /* adapters changed (factory 7) */
    c1 = c5 = 0;
    check_hr(IDXGIFactory7_RegisterAdaptersChangedEvent(f7, ev, &c1), S_OK, "RegisterAdaptersChangedEvent");
    check(c1 != 0, "adapters-changed cookie is non-zero");
    check_hr(IDXGIFactory7_RegisterAdaptersChangedEvent(f7, NULL, &c5), DXGI_ERROR_INVALID_CALL, "adapters-changed NULL event is rejected");
    check_hr(IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, c1), S_OK, "UnregisterAdaptersChangedEvent");
    check_hr(IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, c1), DXGI_ERROR_INVALID_CALL, "unregistering a cookie twice fails");
    c2 = 0;
    IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, &c2);
    check_hr(IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, c2), DXGI_ERROR_INVALID_CALL, "an occlusion cookie is not an adapters-changed cookie");
    IDXGIFactory2_UnregisterOcclusionStatus(f2, c2);

    /* software adapter */
    check_hr(IDXGIFactory2_CreateSoftwareAdapter(f2, NULL, &sw), DXGI_ERROR_INVALID_CALL, "CreateSoftwareAdapter(NULL module)");
    check_hr(IDXGIFactory2_CreateSoftwareAdapter(f2, GetModuleHandleW(NULL), NULL), DXGI_ERROR_INVALID_CALL, "CreateSoftwareAdapter(NULL out)");
    sw = (IDXGIAdapter *)0x1;
    hr = IDXGIFactory2_CreateSoftwareAdapter(f2, GetModuleHandleW(NULL), &sw);
    check(FAILED(hr) && hr != E_NOTIMPL, "CreateSoftwareAdapter(real module) fails with a defined DXGI error");

    IDXGIFactory3_Release(f3);
    IDXGIFactory7_Release(f7);
    IDXGIFactory2_Release(f2);
    CloseHandle(ev);
}

static void test_adapter(IDXGIDevice *dxgi_device)
{
    IDXGIAdapter *adapter = NULL;
    IDXGIAdapter3 *a3 = NULL;
    DXGI_QUERY_VIDEO_MEMORY_INFO info;
    UINT64 avail;
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD c = 0;
    HRESULT hr;

    hr = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    if (FAILED(hr)) { check(0, "GetAdapter"); return; }
    IDXGIAdapter_QueryInterface(adapter, &iid_adapter3, (void **)&a3);
    check(a3 != NULL, "adapter exposes IDXGIAdapter3");
    if (!a3) return;

    hr = IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
    check_hr(hr, S_OK, "QueryVideoMemoryInfo(local)");
    avail = info.AvailableForReservation;
    printf("available for reservation: %llu\n", (unsigned long long)avail);
    check(info.CurrentReservation == 0, "no reservation to begin with");
    if (avail >= 2)
    {
        check_hr(IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, avail / 2), S_OK, "SetVideoMemoryReservation(half)");
        IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
        check(info.CurrentReservation == avail / 2, "the reservation reads back");
        IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info);
        check(info.CurrentReservation == 0, "the other segment group is unaffected");
        IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, avail * 4 + 1);
        IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
        check(info.CurrentReservation == info.AvailableForReservation, "an excessive reservation is capped at what is available");
        IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 0);
        IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
        check(info.CurrentReservation == 0, "reservation 0 releases it");
    }
    else
        printf("SKIP  no memory is available for reservation\n");
    check_hr(IDXGIAdapter3_SetVideoMemoryReservation(a3, 1, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 1), DXGI_ERROR_INVALID_CALL, "reservation on node 1 is rejected");
    check_hr(IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, (DXGI_MEMORY_SEGMENT_GROUP)2, 1), DXGI_ERROR_INVALID_CALL, "reservation on segment group 2 is rejected");

    check_hr(IDXGIAdapter3_RegisterHardwareContentProtectionTeardownStatusEvent(a3, ev, &c), S_OK, "RegisterHardwareContentProtectionTeardownStatusEvent");
    check(c != 0, "teardown cookie is non-zero");
    check_hr(IDXGIAdapter3_RegisterHardwareContentProtectionTeardownStatusEvent(a3, NULL, &c), DXGI_ERROR_INVALID_CALL, "teardown NULL event is rejected");
    IDXGIAdapter3_UnregisterHardwareContentProtectionTeardownStatus(a3, c);

    IDXGIAdapter3_Release(a3);
    IDXGIAdapter_Release(adapter);
    CloseHandle(ev);
}

static void test_device(ID3D11Device *d3d)
{
    IDXGIDevice *dev = NULL;
    IDXGIDevice1 *dev1 = NULL;
    IDXGIDevice2 *dev2 = NULL;
    IDXGIDevice3 *dev3 = NULL;
    ID3D11Texture2D *tex;
    D3D11_TEXTURE2D_DESC desc = {16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0};
    IUnknown *res[2];
    DXGI_RESIDENCY resid[2] = {(DXGI_RESIDENCY)0, (DXGI_RESIDENCY)0};
    BOOL discarded[2] = {TRUE, TRUE};
    HANDLE ev;
    INT prio = 99;
    HRESULT hr;
    int i;

    ID3D11Device_QueryInterface(d3d, &iid_device, (void **)&dev);
    IDXGIDevice_QueryInterface(dev, &iid_device1, (void **)&dev1);
    IDXGIDevice_QueryInterface(dev, &iid_device2, (void **)&dev2);
    IDXGIDevice_QueryInterface(dev, &iid_device3, (void **)&dev3);
    check(dev && dev1 && dev2 && dev3, "device exposes IDXGIDevice 0-3");
    if (!dev3) return;
    test_adapter(dev);

    /* GPU thread priority */
    check_hr(IDXGIDevice_GetGPUThreadPriority(dev, &prio), S_OK, "GetGPUThreadPriority");
    check(prio == 0, "the default GPU thread priority is 0");
    check_hr(IDXGIDevice_SetGPUThreadPriority(dev, 5), S_OK, "SetGPUThreadPriority(5)");
    prio = 99;
    IDXGIDevice_GetGPUThreadPriority(dev, &prio);
    check(prio == 5, "the priority reads back");
    check_hr(IDXGIDevice_SetGPUThreadPriority(dev, -7), S_OK, "SetGPUThreadPriority(-7)");
    IDXGIDevice_GetGPUThreadPriority(dev, &prio);
    check(prio == -7, "the lowest priority reads back");
    check_hr(IDXGIDevice_SetGPUThreadPriority(dev, 8), DXGI_ERROR_INVALID_CALL, "priority 8 is out of range");
    check_hr(IDXGIDevice_SetGPUThreadPriority(dev, -8), DXGI_ERROR_INVALID_CALL, "priority -8 is out of range");
    IDXGIDevice_GetGPUThreadPriority(dev, &prio);
    check(prio == -7, "a rejected priority leaves the old one");
    check_hr(IDXGIDevice_GetGPUThreadPriority(dev, NULL), DXGI_ERROR_INVALID_CALL, "GetGPUThreadPriority(NULL)");

    /* residency / offer / reclaim */
    hr = ID3D11Device_CreateTexture2D(d3d, &desc, NULL, &tex);
    check(SUCCEEDED(hr), "create a texture");
    if (FAILED(hr)) return;
    res[0] = (IUnknown *)tex;
    res[1] = (IUnknown *)tex;
    check_hr(IDXGIDevice_QueryResourceResidency(dev, res, resid, 2), S_OK, "QueryResourceResidency");
    check(resid[0] == DXGI_RESIDENCY_FULLY_RESIDENT && resid[1] == DXGI_RESIDENCY_FULLY_RESIDENT, "resources are fully resident");
    check_hr(IDXGIDevice_QueryResourceResidency(dev, NULL, resid, 2), DXGI_ERROR_INVALID_CALL, "residency NULL resources");
    check_hr(IDXGIDevice_QueryResourceResidency(dev, res, NULL, 2), DXGI_ERROR_INVALID_CALL, "residency NULL output");
    check_hr(IDXGIDevice_QueryResourceResidency(dev, res, resid, 0), DXGI_ERROR_INVALID_CALL, "residency zero count");
    res[1] = NULL;
    check_hr(IDXGIDevice_QueryResourceResidency(dev, res, resid, 2), DXGI_ERROR_INVALID_CALL, "residency NULL entry");
    res[1] = (IUnknown *)tex;

    {
        IDXGIResource *r[2];
        ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&r[0]);
        r[1] = r[0];
        check_hr(IDXGIDevice2_OfferResources(dev2, 2, r, DXGI_OFFER_RESOURCE_PRIORITY_NORMAL), S_OK, "OfferResources");
        check_hr(IDXGIDevice2_OfferResources(dev2, 2, r, (DXGI_OFFER_RESOURCE_PRIORITY)0), DXGI_ERROR_INVALID_CALL, "offer priority 0");
        check_hr(IDXGIDevice2_OfferResources(dev2, 2, r, (DXGI_OFFER_RESOURCE_PRIORITY)4), DXGI_ERROR_INVALID_CALL, "offer priority 4");
        check_hr(IDXGIDevice2_OfferResources(dev2, 0, r, DXGI_OFFER_RESOURCE_PRIORITY_LOW), DXGI_ERROR_INVALID_CALL, "offer zero resources");
        check_hr(IDXGIDevice2_OfferResources(dev2, 2, NULL, DXGI_OFFER_RESOURCE_PRIORITY_LOW), DXGI_ERROR_INVALID_CALL, "offer NULL array");
        check_hr(IDXGIDevice2_ReclaimResources(dev2, 2, r, discarded), S_OK, "ReclaimResources");
        check(!discarded[0] && !discarded[1], "nothing was discarded");
        check_hr(IDXGIDevice2_ReclaimResources(dev2, 2, r, NULL), S_OK, "ReclaimResources without the discarded array");
        check_hr(IDXGIDevice2_ReclaimResources(dev2, 0, r, discarded), DXGI_ERROR_INVALID_CALL, "reclaim zero resources");
        check_hr(IDXGIDevice2_ReclaimResources(dev2, 2, NULL, discarded), DXGI_ERROR_INVALID_CALL, "reclaim NULL array");
        IDXGIResource_Release(r[0]);
    }

    /* EnqueueSetEvent */
    check_hr(IDXGIDevice2_EnqueueSetEvent(dev2, NULL), DXGI_ERROR_INVALID_CALL, "EnqueueSetEvent(NULL)");
    ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    check_hr(IDXGIDevice2_EnqueueSetEvent(dev2, ev), S_OK, "EnqueueSetEvent");
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "the event is signalled once the queued work has finished");
    for (i = 0; i < 3; i++)
    {
        ResetEvent(ev);
        IDXGIDevice2_EnqueueSetEvent(dev2, ev);
        if (WaitForSingleObject(ev, 10000) != WAIT_OBJECT_0) break;
    }
    check(i == 3, "repeated EnqueueSetEvent calls all signal");
    CloseHandle(ev);

    IDXGIDevice3_Trim(dev3);
    check(1, "Trim returns");

    ID3D11Texture2D_Release(tex);
    IDXGIDevice3_Release(dev3);
    IDXGIDevice2_Release(dev2);
    IDXGIDevice1_Release(dev1);
    IDXGIDevice_Release(dev);
}

int main(void)
{
    ID3D11Device *device;
    D3D_FEATURE_LEVEL fl;
    HWND window;
    HRESULT hr;

    window = CreateWindowA("static", "dxgistat", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    test_factory(0, window);
    test_factory(DXGI_CREATE_FACTORY_DEBUG, window);

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &device, &fl, NULL);
    if (FAILED(hr))
    {
        printf("FAIL  no D3D11 device (%#lx)\nRESULT: FAIL\n", hr);
        return 1;
    }
    test_device(device);
    ID3D11Device_Release(device);
    DestroyWindow(window);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
