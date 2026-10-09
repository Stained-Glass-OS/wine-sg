/* DXGI swap chain long tail (patches/sg/2608): frame statistics, background
 * colour, rotation, source size, the frame latency waitable object, tearing,
 * colour space and HDR metadata on a window swap chain and on a composition
 * swap chain. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <stdio.h>
#include <string.h>

static const GUID iid_factory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID iid_swapchain4 = {0x3d585d5a, 0xbd4a, 0x489e, {0xb1, 0xf4, 0x3d, 0xbc, 0xb6, 0x45, 0x2f, 0xfb}};
static const GUID iid_dxgidevice = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID iid_swapchain1 = {0x790a45f7, 0x0d42, 0x4876, {0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa}};

#define W 320
#define H 200

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

static int waits(HANDLE h)
{
    return WaitForSingleObject(h, 0) == WAIT_OBJECT_0;
}

/* Everything that works the same on both kinds of swap chain. */
static void test_common(IDXGISwapChain4 *sc, const char *kind, BOOL window)
{
    char name[160];
    DXGI_RGBA colour = {0.25f, 0.5f, 0.75f, 1.0f}, got;
    DXGI_MODE_ROTATION rotation;
    UINT w, h, support;
    DXGI_HDR_METADATA_HDR10 hdr;
    DXGI_MATRIX_3X2_F matrix = {2.0f, 0.0f, 0.0f, 0.5f, 3.0f, 4.0f}, gm;
    IDXGIOutput *output;
    HRESULT hr;

#define N(s) (snprintf(name, sizeof(name), "%s: %s", kind, s), name)

    /* background colour */
    memset(&got, 0xaa, sizeof(got));
    check_hr(IDXGISwapChain4_GetBackgroundColor(sc, &got), S_OK, N("GetBackgroundColor"));
    check(got.r == 0.0f && got.g == 0.0f && got.b == 0.0f && got.a == 0.0f, N("background defaults to all zero"));
    check_hr(IDXGISwapChain4_SetBackgroundColor(sc, &colour), S_OK, N("SetBackgroundColor"));
    memset(&got, 0, sizeof(got));
    IDXGISwapChain4_GetBackgroundColor(sc, &got);
    check(!memcmp(&got, &colour, sizeof(got)), N("background colour round trips"));
    check_hr(IDXGISwapChain4_SetBackgroundColor(sc, NULL), E_INVALIDARG, N("SetBackgroundColor(NULL)"));
    check_hr(IDXGISwapChain4_GetBackgroundColor(sc, NULL), E_INVALIDARG, N("GetBackgroundColor(NULL)"));

    /* rotation */
    rotation = 99;
    check_hr(IDXGISwapChain4_GetRotation(sc, &rotation), S_OK, N("GetRotation"));
    check(rotation == DXGI_MODE_ROTATION_IDENTITY, N("rotation defaults to identity"));
    check_hr(IDXGISwapChain4_SetRotation(sc, DXGI_MODE_ROTATION_ROTATE180), S_OK, N("SetRotation(180)"));
    IDXGISwapChain4_GetRotation(sc, &rotation);
    check(rotation == DXGI_MODE_ROTATION_ROTATE180, N("rotation round trips"));
    check_hr(IDXGISwapChain4_SetRotation(sc, DXGI_MODE_ROTATION_UNSPECIFIED), DXGI_ERROR_INVALID_CALL, N("SetRotation(unspecified)"));
    check_hr(IDXGISwapChain4_SetRotation(sc, 5), DXGI_ERROR_INVALID_CALL, N("SetRotation(5)"));
    IDXGISwapChain4_GetRotation(sc, &rotation);
    check(rotation == DXGI_MODE_ROTATION_ROTATE180, N("a rejected rotation is not stored"));
    check_hr(IDXGISwapChain4_GetRotation(sc, NULL), E_INVALIDARG, N("GetRotation(NULL)"));
    IDXGISwapChain4_SetRotation(sc, DXGI_MODE_ROTATION_IDENTITY);

    /* source size */
    w = h = 0;
    check_hr(IDXGISwapChain4_GetSourceSize(sc, &w, &h), S_OK, N("GetSourceSize"));
    check(w == W && h == H, N("source size defaults to the buffer size"));
    check_hr(IDXGISwapChain4_SetSourceSize(sc, W / 2, H / 4), S_OK, N("SetSourceSize(half)"));
    w = h = 0;
    IDXGISwapChain4_GetSourceSize(sc, &w, &h);
    check(w == W / 2 && h == H / 4, N("source size round trips"));
    check_hr(IDXGISwapChain4_SetSourceSize(sc, 0, 10), DXGI_ERROR_INVALID_CALL, N("SetSourceSize(0, 10)"));
    check_hr(IDXGISwapChain4_SetSourceSize(sc, 10, 0), DXGI_ERROR_INVALID_CALL, N("SetSourceSize(10, 0)"));
    check_hr(IDXGISwapChain4_SetSourceSize(sc, W + 1, H), DXGI_ERROR_INVALID_CALL, N("SetSourceSize(wider than the buffer)"));
    check_hr(IDXGISwapChain4_SetSourceSize(sc, W, H + 1), DXGI_ERROR_INVALID_CALL, N("SetSourceSize(taller than the buffer)"));
    w = h = 0;
    IDXGISwapChain4_GetSourceSize(sc, &w, &h);
    check(w == W / 2 && h == H / 4, N("a rejected source size is not stored"));
    check_hr(IDXGISwapChain4_GetSourceSize(sc, NULL, &h), E_INVALIDARG, N("GetSourceSize(NULL width)"));
    IDXGISwapChain4_SetSourceSize(sc, W, H);

    /* transform */
    if (window)
    {
        check_hr(IDXGISwapChain4_SetMatrixTransform(sc, &matrix), DXGI_ERROR_INVALID_CALL, N("SetMatrixTransform on a window swap chain"));
        check_hr(IDXGISwapChain4_GetMatrixTransform(sc, &gm), DXGI_ERROR_INVALID_CALL, N("GetMatrixTransform on a window swap chain"));
    }
    else
    {
        memset(&gm, 0, sizeof(gm));
        check_hr(IDXGISwapChain4_GetMatrixTransform(sc, &gm), S_OK, N("GetMatrixTransform"));
        check(gm._11 == 1.0f && gm._22 == 1.0f && gm._12 == 0.0f && gm._21 == 0.0f && gm._31 == 0.0f && gm._32 == 0.0f,
                N("transform defaults to identity"));
        check_hr(IDXGISwapChain4_SetMatrixTransform(sc, &matrix), S_OK, N("SetMatrixTransform"));
        memset(&gm, 0, sizeof(gm));
        IDXGISwapChain4_GetMatrixTransform(sc, &gm);
        check(!memcmp(&gm, &matrix, sizeof(gm)), N("transform round trips"));
        check_hr(IDXGISwapChain4_SetMatrixTransform(sc, NULL), E_INVALIDARG, N("SetMatrixTransform(NULL)"));
        check_hr(IDXGISwapChain4_GetMatrixTransform(sc, NULL), E_INVALIDARG, N("GetMatrixTransform(NULL)"));
        matrix._11 = 1.0f; matrix._22 = 1.0f; matrix._31 = matrix._32 = matrix._12 = matrix._21 = 0.0f;
        IDXGISwapChain4_SetMatrixTransform(sc, &matrix);
    }

    /* colour space */
    support = 0xff;
    check_hr(IDXGISwapChain4_CheckColorSpaceSupport(sc, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &support), S_OK, N("CheckColorSpaceSupport(sRGB)"));
    check(support == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT, N("sRGB can be presented"));
    support = 0xff;
    check_hr(IDXGISwapChain4_CheckColorSpaceSupport(sc, DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &support), S_OK, N("CheckColorSpaceSupport(HDR10)"));
    check(support == 0, N("HDR10 is not supported"));
    check_hr(IDXGISwapChain4_CheckColorSpaceSupport(sc, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, NULL), E_INVALIDARG, N("CheckColorSpaceSupport(NULL)"));
    check_hr(IDXGISwapChain4_SetColorSpace1(sc, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709), S_OK, N("SetColorSpace1(sRGB)"));
    check_hr(IDXGISwapChain4_SetColorSpace1(sc, DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020), E_INVALIDARG, N("SetColorSpace1(HDR10)"));

    /* HDR metadata */
    memset(&hdr, 0, sizeof(hdr));
    hdr.MaxMasteringLuminance = 1000;
    check_hr(IDXGISwapChain4_SetHDRMetaData(sc, DXGI_HDR_METADATA_TYPE_NONE, 0, NULL), S_OK, N("SetHDRMetaData(none)"));
    check_hr(IDXGISwapChain4_SetHDRMetaData(sc, DXGI_HDR_METADATA_TYPE_NONE, 4, &hdr), E_INVALIDARG, N("SetHDRMetaData(none, with data)"));
    check_hr(IDXGISwapChain4_SetHDRMetaData(sc, DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdr), &hdr), S_OK, N("SetHDRMetaData(HDR10)"));
    check_hr(IDXGISwapChain4_SetHDRMetaData(sc, DXGI_HDR_METADATA_TYPE_HDR10, 4, &hdr), E_INVALIDARG, N("SetHDRMetaData(HDR10, short)"));
    check_hr(IDXGISwapChain4_SetHDRMetaData(sc, DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdr), NULL), E_INVALIDARG, N("SetHDRMetaData(HDR10, NULL)"));
    check_hr(IDXGISwapChain4_SetHDRMetaData(sc, 7, sizeof(hdr), &hdr), E_INVALIDARG, N("SetHDRMetaData(unknown type)"));

    /* misc */
    check(IDXGISwapChain4_IsTemporaryMonoSupported(sc) == FALSE, N("temporary mono is not supported"));
    output = (IDXGIOutput *)0x1;
    hr = IDXGISwapChain4_GetRestrictToOutput(sc, &output);
    check(hr == S_OK && output == NULL, N("GetRestrictToOutput gives S_OK and no output"));
    check_hr(IDXGISwapChain4_GetRestrictToOutput(sc, NULL), E_INVALIDARG, N("GetRestrictToOutput(NULL)"));
    check(IDXGISwapChain4_GetCurrentBackBufferIndex(sc) == 0, N("current back buffer index is 0"));
#undef N
}

static void test_statistics(IDXGISwapChain4 *sc, const char *kind)
{
    DXGI_FRAME_STATISTICS a, b;
    UINT last = 0;
    char name[160];
    int i;

#define N(s) (snprintf(name, sizeof(name), "%s: %s", kind, s), name)
    memset(&a, 0xff, sizeof(a));
    check_hr(IDXGISwapChain4_GetFrameStatistics(sc, &a), S_OK, N("GetFrameStatistics"));
    IDXGISwapChain4_GetLastPresentCount(sc, &last);
    check(a.PresentCount == last, N("PresentCount is the last present count"));
    check(last > 0, N("the presents so far were counted"));
    check(a.SyncQPCTime.QuadPart != 0, N("SyncQPCTime is set"));
    for (i = 0; i < 3; ++i) IDXGISwapChain4_Present(sc, 0, 0);
    Sleep(300);
    memset(&b, 0xff, sizeof(b));
    IDXGISwapChain4_GetFrameStatistics(sc, &b);
    check(b.PresentCount == a.PresentCount + 3, N("PresentCount follows the presents"));
    check(b.SyncRefreshCount > a.SyncRefreshCount, N("SyncRefreshCount advances with time"));
    check(b.SyncQPCTime.QuadPart > a.SyncQPCTime.QuadPart, N("SyncQPCTime advances"));
    check_hr(IDXGISwapChain4_GetFrameStatistics(sc, NULL), E_INVALIDARG, N("GetFrameStatistics(NULL)"));
#undef N
}

static void test_window(IDXGIFactory2 *factory, ID3D11Device *device, HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC1 desc = {0}, got1;
    DXGI_SWAP_CHAIN_DESC got;
    IDXGISwapChain1 *sc1 = NULL;
    IDXGISwapChain4 *sc = NULL;
    HANDLE a, b;
    UINT latency;
    HRESULT hr;
    int i;

    /* A swap chain with the latency object and tearing. */
    desc.Width = W;
    desc.Height = H;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)device, hwnd, &desc, NULL, NULL, &sc1);
    check_hr(hr, S_OK, "window: CreateSwapChainForHwnd with the latency and tearing flags");
    if (FAILED(hr)) return;
    IDXGISwapChain1_QueryInterface(sc1, &iid_swapchain4, (void **)&sc);
    IDXGISwapChain1_Release(sc1);
    if (!sc) { check(0, "window: IDXGISwapChain4"); return; }

    memset(&got1, 0, sizeof(got1));
    IDXGISwapChain4_GetDesc1(sc, &got1);
    check(got1.Flags == desc.Flags, "window: GetDesc1 reports the creation flags");
    memset(&got, 0, sizeof(got));
    IDXGISwapChain4_GetDesc(sc, &got);
    check(got.Flags == desc.Flags, "window: GetDesc reports the creation flags");

    /* latency object: the application may start one frame, a present gives it back */
    latency = 99;
    check_hr(IDXGISwapChain4_GetMaximumFrameLatency(sc, &latency), S_OK, "window: GetMaximumFrameLatency");
    check(latency == 1, "window: the default maximum frame latency is 1");
    a = IDXGISwapChain4_GetFrameLatencyWaitableObject(sc);
    b = IDXGISwapChain4_GetFrameLatencyWaitableObject(sc);
    check(a && b, "window: GetFrameLatencyWaitableObject");
    check(a != b, "window: every call returns a new handle");
    if (a && b)
    {
        check(waits(a), "window: the object starts signalled");
        check(!waits(b), "window: the other handle is the same object (taken)");
        check_hr(IDXGISwapChain4_Present(sc, 0, 0), S_OK, "window: Present");
        check(waits(b), "window: a present signals the object again");
        check_hr(IDXGISwapChain4_Present(sc, 0, 0), S_OK, "window: Present again");
        check_hr(IDXGISwapChain4_Present(sc, 0, 0), S_OK, "window: Present a third time without waiting");
        check(waits(a), "window: unwaited presents leave one frame available");
        check(!waits(b), "window: and no more than one");

        check_hr(IDXGISwapChain4_SetMaximumFrameLatency(sc, 3), S_OK, "window: SetMaximumFrameLatency(3)");
        IDXGISwapChain4_GetMaximumFrameLatency(sc, &latency);
        check(latency == 3, "window: the maximum frame latency round trips");
        for (i = 0; i < 3 && waits(a); ++i) ;
        check(i == 2, "window: a larger latency allows more frames in flight");
        check(!waits(a), "window: and then blocks");
        check_hr(IDXGISwapChain4_SetMaximumFrameLatency(sc, 1), S_OK, "window: SetMaximumFrameLatency(1)");
        check_hr(IDXGISwapChain4_Present(sc, 0, 0), S_OK, "window: Present with latency 1");
        check(waits(a) && !waits(a), "window: shrinking the latency shrinks the frames in flight");
        CloseHandle(a);
        CloseHandle(b);
    }
    check_hr(IDXGISwapChain4_SetMaximumFrameLatency(sc, 0), DXGI_ERROR_INVALID_CALL, "window: SetMaximumFrameLatency(0)");
    check_hr(IDXGISwapChain4_SetMaximumFrameLatency(sc, 17), DXGI_ERROR_INVALID_CALL, "window: SetMaximumFrameLatency(17)");
    check_hr(IDXGISwapChain4_GetMaximumFrameLatency(sc, NULL), DXGI_ERROR_INVALID_CALL, "window: GetMaximumFrameLatency(NULL)");

    /* tearing */
    check_hr(IDXGISwapChain4_Present(sc, 0, DXGI_PRESENT_ALLOW_TEARING), S_OK, "window: Present(0, ALLOW_TEARING)");
    check_hr(IDXGISwapChain4_Present(sc, 1, DXGI_PRESENT_ALLOW_TEARING), DXGI_ERROR_INVALID_CALL, "window: Present(1, ALLOW_TEARING)");
    check_hr(IDXGISwapChain4_Present(sc, 0, DXGI_PRESENT_ALLOW_TEARING | DXGI_PRESENT_TEST), S_OK, "window: Present(0, ALLOW_TEARING | TEST)");

    test_common(sc, "window", TRUE);
    IDXGISwapChain4_Present(sc, 0, 0);
    test_statistics(sc, "window");
    {
        void *cw = (void *)1;
        hr = IDXGISwapChain4_GetCoreWindow(sc, &iid_factory2, &cw);
        check(hr == DXGI_ERROR_INVALID_CALL && !cw, "window: GetCoreWindow has no core window");
    }
    IDXGISwapChain4_Release(sc);

    /* A swap chain without them. */
    desc.Flags = 0;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.BufferCount = 1;
    sc1 = NULL;
    hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)device, hwnd, &desc, NULL, NULL, &sc1);
    check_hr(hr, S_OK, "plain: CreateSwapChainForHwnd");
    if (FAILED(hr)) return;
    IDXGISwapChain1_QueryInterface(sc1, &iid_swapchain4, (void **)&sc);
    IDXGISwapChain1_Release(sc1);
    memset(&got1, 0xff, sizeof(got1));
    IDXGISwapChain4_GetDesc1(sc, &got1);
    check(got1.Flags == 0, "plain: GetDesc1 reports no flags");
    check(IDXGISwapChain4_GetFrameLatencyWaitableObject(sc) == NULL, "plain: no latency object");
    check_hr(IDXGISwapChain4_GetMaximumFrameLatency(sc, &latency), DXGI_ERROR_INVALID_CALL, "plain: GetMaximumFrameLatency");
    check_hr(IDXGISwapChain4_SetMaximumFrameLatency(sc, 2), DXGI_ERROR_INVALID_CALL, "plain: SetMaximumFrameLatency");
    check_hr(IDXGISwapChain4_Present(sc, 0, DXGI_PRESENT_ALLOW_TEARING), DXGI_ERROR_INVALID_CALL, "plain: Present(ALLOW_TEARING) without the creation flag");
    check_hr(IDXGISwapChain4_Present(sc, 0, 0), S_OK, "plain: Present");
    test_common(sc, "plain", TRUE);
    IDXGISwapChain4_Release(sc);
}

static void test_composition(IDXGIFactory2 *factory, ID3D11Device *device)
{
    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    IDXGISwapChain1 *sc1 = NULL;
    IDXGISwapChain4 *sc = NULL;
    HANDLE a;
    UINT latency = 0;
    HRESULT hr;
    int i;

    desc.Width = W;
    desc.Height = H;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    hr = IDXGIFactory2_CreateSwapChainForComposition(factory, (IUnknown *)device, &desc, NULL, &sc1);
    check_hr(hr, S_OK, "composition: CreateSwapChainForComposition");
    if (FAILED(hr)) return;
    IDXGISwapChain1_QueryInterface(sc1, &iid_swapchain4, (void **)&sc);
    IDXGISwapChain1_Release(sc1);

    check_hr(IDXGISwapChain4_GetMaximumFrameLatency(sc, &latency), S_OK, "composition: GetMaximumFrameLatency");
    check(latency == 3, "composition: the default maximum frame latency is 3");
    a = IDXGISwapChain4_GetFrameLatencyWaitableObject(sc);
    check(a != NULL, "composition: GetFrameLatencyWaitableObject");
    if (a)
    {
        for (i = 0; i < 3 && waits(a); ++i) ;
        check(i == 3 && !waits(a), "composition: three frames may be started");
        IDXGISwapChain4_Present(sc, 0, 0);
        check(waits(a), "composition: a present signals the object");
        CloseHandle(a);
    }
    check_hr(IDXGISwapChain4_Present(sc, 0, DXGI_PRESENT_ALLOW_TEARING), S_OK, "composition: Present(0, ALLOW_TEARING)");
    check_hr(IDXGISwapChain4_Present(sc, 2, DXGI_PRESENT_ALLOW_TEARING), DXGI_ERROR_INVALID_CALL, "composition: Present(2, ALLOW_TEARING)");
    test_common(sc, "composition", FALSE);
    test_statistics(sc, "composition");
    IDXGISwapChain4_Release(sc);

    desc.Flags = 0;
    sc1 = NULL;
    hr = IDXGIFactory2_CreateSwapChainForComposition(factory, (IUnknown *)device, &desc, NULL, &sc1);
    check_hr(hr, S_OK, "composition (plain): CreateSwapChainForComposition");
    if (FAILED(hr)) return;
    IDXGISwapChain1_QueryInterface(sc1, &iid_swapchain4, (void **)&sc);
    IDXGISwapChain1_Release(sc1);
    check(IDXGISwapChain4_GetFrameLatencyWaitableObject(sc) == NULL, "composition (plain): no latency object");
    check_hr(IDXGISwapChain4_Present(sc, 0, DXGI_PRESENT_ALLOW_TEARING), DXGI_ERROR_INVALID_CALL, "composition (plain): Present(ALLOW_TEARING) without the creation flag");
    IDXGISwapChain4_Release(sc);
}

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    IDXGIFactory2 *factory = NULL;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    ID3D11Device *device = NULL;
    HWND hwnd;
    HRESULT hr;

    hwnd = CreateWindowW(L"static", L"dxgiswap", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, W + 16, H + 40, NULL, NULL, NULL, NULL);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
            D3D11_SDK_VERSION, &device, NULL, NULL);
    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
                D3D11_SDK_VERSION, &device, NULL, NULL);
    check(SUCCEEDED(hr), "D3D11CreateDevice");
    if (FAILED(hr)) goto done;
    ID3D11Device_QueryInterface(device, &iid_dxgidevice, (void **)&dxgi_device);
    IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    hr = IDXGIAdapter_GetParent(adapter, &iid_factory2, (void **)&factory);
    check_hr(hr, S_OK, "the adapter's parent is an IDXGIFactory2");
    if (FAILED(hr)) goto done;

    test_window(factory, device, hwnd);
    test_composition(factory, device);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
