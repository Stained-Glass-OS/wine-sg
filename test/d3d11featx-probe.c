/* D3D11 ID3D11Device::CheckFeatureSupport long-tail cases and
 * ID3D10Multithread::Set/GetMultithreadProtected (patches/sg/26xx).
 * CheckFeatureSupport returned E_NOTIMPL for every D3D11_FEATURE value past
 * FORMAT_SUPPORT (shadow sampling, min shader precision, instancing, shader
 * markers/cache, GPU virtual address size, OPTIONS4/5); Set/GetMultithreadProtected
 * always claimed TRUE and SetMultithreadProtected never returned the previous
 * state. */
#define COBJMACROS
#include <windows.h>
#include <d3d11_4.h>
#include <stdio.h>

/* libuuid in some mingw-w64 releases lacks this IID. */
static const GUID iid_d3d11_multithread = {0x9b7e4e00, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void check_hr(HRESULT hr, const char *what)
{
    printf("%s  %s (hr %#lx)\n", SUCCEEDED(hr) ? "PASS" : "FAIL", what, hr);
    if (FAILED(hr)) failures++;
}

int main(void)
{
    ID3D11Device *device;
    ID3D10Multithread *mt = NULL;
    ID3D11Multithread *mt11 = NULL;
    ID3D11DeviceContext *ctx;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr;
    D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT shadow;
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT minprec;
    D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT inst;
    D3D11_FEATURE_DATA_MARKER_SUPPORT marker;
    D3D11_FEATURE_DATA_D3D9_OPTIONS1 opts1;
    D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT gpuva;
    D3D11_FEATURE_DATA_SHADER_CACHE cache;
    struct { BOOL ExtendedNV12SharedTextureSupported; } opts4; /* not in every mingw header */
    D3D11_FEATURE_DATA_D3D11_OPTIONS5 opts5;
    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 fs2;
    BOOL prev;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &device, &fl, NULL);
    if (FAILED(hr))
    {
        printf("FAIL  no D3D11 device (%#lx)\nRESULT: FAIL\n", hr);
        return 1;
    }
    printf("feature level %#x\n", fl);

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &shadow, sizeof(shadow));
    check_hr(hr, "D3D9_SHADOW_SUPPORT query succeeds");
    check(shadow.SupportsDepthAsTextureWithLessEqualComparisonFilter, "depth-as-texture shadow sampling is supported");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &minprec, sizeof(minprec));
    check_hr(hr, "SHADER_MIN_PRECISION_SUPPORT query succeeds");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT, &inst, sizeof(inst));
    check_hr(hr, "D3D9_SIMPLE_INSTANCING_SUPPORT query succeeds");
    check(inst.SimpleInstancingSupported, "instancing is supported");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_MARKER_SUPPORT, &marker, sizeof(marker));
    check_hr(hr, "MARKER_SUPPORT query succeeds");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_D3D9_OPTIONS1, &opts1, sizeof(opts1));
    check_hr(hr, "D3D9_OPTIONS1 query succeeds");
    check(opts1.FullNonPow2TextureSupported && opts1.SimpleInstancingSupported, "D3D9_OPTIONS1 reports hardware features");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &gpuva, sizeof(gpuva));
    check_hr(hr, "GPU_VIRTUAL_ADDRESS_SUPPORT query succeeds");
    check(gpuva.MaxGPUVirtualAddressBitsPerResource > 0 && gpuva.MaxGPUVirtualAddressBitsPerProcess > 0,
          "GPU_VIRTUAL_ADDRESS_SUPPORT gives non-zero address widths");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_SHADER_CACHE, &cache, sizeof(cache));
    check_hr(hr, "SHADER_CACHE query succeeds");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_D3D11_OPTIONS4, &opts4, sizeof(opts4));
    check_hr(hr, "D3D11_OPTIONS4 query succeeds");

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_D3D11_OPTIONS5, &opts5, sizeof(opts5));
    check_hr(hr, "D3D11_OPTIONS5 query succeeds");

    fs2.InFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_FORMAT_SUPPORT2, &fs2, sizeof(fs2));
    check_hr(hr, "FORMAT_SUPPORT2 query succeeds");

    /* Wrong size must be rejected, not silently accepted. */
    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &shadow, sizeof(shadow) - 1);
    check(hr == E_INVALIDARG, "D3D9_SHADOW_SUPPORT rejects a too-small buffer");

    hr = ID3D11Device_QueryInterface(device, &IID_ID3D10Multithread, (void **)&mt);
    check_hr(hr, "QueryInterface(ID3D10Multithread) succeeds");
    ID3D11Device_GetImmediateContext(device, &ctx);
    hr = ID3D11DeviceContext_QueryInterface(ctx, &iid_d3d11_multithread, (void **)&mt11);
    check_hr(hr, "QueryInterface(ID3D11Multithread) succeeds");
    if (SUCCEEDED(hr) && mt)
    {
        /* The two interfaces share one flag, FALSE until turned on. */
        check(!ID3D10Multithread_GetMultithreadProtected(mt), "device starts unprotected (D3D10 interface)");
        check(!ID3D11Multithread_GetMultithreadProtected(mt11), "device starts unprotected (D3D11 interface)");
        prev = ID3D10Multithread_SetMultithreadProtected(mt, TRUE);
        check(!prev, "SetMultithreadProtected(TRUE) returns the previous (FALSE) state");
        check(ID3D11Multithread_GetMultithreadProtected(mt11), "...and the D3D11 interface sees it");
        prev = ID3D11Multithread_SetMultithreadProtected(mt11, FALSE);
        check(prev, "D3D11 SetMultithreadProtected(FALSE) returns the previous (TRUE) state");
        check(!ID3D10Multithread_GetMultithreadProtected(mt), "...and the D3D10 interface sees it");
        prev = ID3D11Multithread_SetMultithreadProtected(mt11, FALSE);
        check(!prev, "setting FALSE again returns FALSE");
    }
    if (mt11) ID3D11Multithread_Release(mt11);
    if (mt) ID3D10Multithread_Release(mt);
    ID3D11DeviceContext_Release(ctx);

    ID3D11Device_Release(device);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
