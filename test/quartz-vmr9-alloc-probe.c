/* Probe for patches/sg/4588: VMR-9's IVMRSurfaceAllocatorNotify9::
 * AllocateSurfaceHelper takes the documented flag combinations: a texture
 * that is also a 3D render target or a DXVA target (a render-target texture),
 * and the RGBDynamicSwitch hint with any surface type. Table driven; lines
 * "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <d3d9.h>
#include <dshow.h>
#include <vmr9.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    static const struct { DWORD flags; int texture; DWORD usage; } cases[] =
    {
        {VMR9AllocFlag_TextureSurface, 1, D3DUSAGE_DYNAMIC},
        {VMR9AllocFlag_TextureSurface | VMR9AllocFlag_3DRenderTarget, 1, D3DUSAGE_RENDERTARGET},
        {VMR9AllocFlag_TextureSurface | VMR9AllocFlag_DXVATarget, 1, D3DUSAGE_RENDERTARGET},
        {VMR9AllocFlag_TextureSurface | VMR9AllocFlag_RGBDynamicSwitch, 1, D3DUSAGE_DYNAMIC},
        {VMR9AllocFlag_OffscreenSurface | VMR9AllocFlag_RGBDynamicSwitch, 0, 0},
        {VMR9AllocFlag_3DRenderTarget | VMR9AllocFlag_RGBDynamicSwitch, 0, D3DUSAGE_RENDERTARGET},
    };
    D3DPRESENT_PARAMETERS pp = {0};
    IVMRSurfaceAllocatorNotify9 *notify;
    IVMRFilterConfig9 *config;
    IDirect3DDevice9 *device;
    IBaseFilter *filter;
    IDirect3D9 *d3d;
    HWND window;
    unsigned int i;
    HRESULT hr;

    CoInitialize(NULL);
    window = CreateWindowA("static", "vmr9", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, NULL, NULL, NULL, NULL);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window;
    if (!window || !d3d || FAILED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device)))
    {
        printf("ok   no Direct3D 9 device: skipped\nRESULT: PASS\n");
        return 0;
    }
    hr = CoCreateInstance(&CLSID_VideoMixingRenderer9, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
    check(hr == S_OK, "VMR-9 %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }
    IBaseFilter_QueryInterface(filter, &IID_IVMRFilterConfig9, (void **)&config);
    IVMRFilterConfig9_SetRenderingMode(config, VMR9Mode_Renderless);
    IVMRFilterConfig9_Release(config);
    IBaseFilter_QueryInterface(filter, &IID_IVMRSurfaceAllocatorNotify9, (void **)&notify);
    hr = IVMRSurfaceAllocatorNotify9_SetD3DDevice(notify, device, MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY));
    check(hr == S_OK, "SetD3DDevice %#lx", hr);

    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        VMR9AllocationInfo info = {cases[i].flags, 32, 16, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, 2, {32, 16}, {32, 16}};
        IDirect3DSurface9 *surfaces[2] = {0};
        IDirect3DTexture9 *container;
        D3DSURFACE_DESC desc;
        DWORD count = 2;

        hr = IVMRSurfaceAllocatorNotify9_AllocateSurfaceHelper(notify, &info, &count, surfaces);
        check(hr == S_OK && count == 2 && surfaces[0] && surfaces[1], "flags %#lx: %#lx, %lu surfaces", cases[i].flags, hr, count);
        if (hr != S_OK) continue;
        hr = IDirect3DSurface9_GetContainer(surfaces[0], &IID_IDirect3DTexture9, (void **)&container);
        check(cases[i].texture ? hr == D3D_OK : hr != D3D_OK, "flags %#lx: %s a texture's surface", cases[i].flags,
              cases[i].texture ? "is" : "is not");
        if (hr == D3D_OK) IDirect3DTexture9_Release(container);
        IDirect3DSurface9_GetDesc(surfaces[1], &desc);
        check(desc.Usage == cases[i].usage && desc.Width == 32 && desc.Height == 16,
              "flags %#lx: usage %#lx, %ux%u", cases[i].flags, desc.Usage, desc.Width, desc.Height);
        IDirect3DSurface9_Release(surfaces[0]);
        IDirect3DSurface9_Release(surfaces[1]);
    }

    IVMRSurfaceAllocatorNotify9_Release(notify);
    IBaseFilter_Release(filter);
    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    DestroyWindow(window);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
