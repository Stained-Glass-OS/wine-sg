/* DXVA-HD (wine-sg 8207), for test/dxvahd-gate.sh: DXVAHD_CreateDevice on a Direct3D 9Ex device, its
 * capabilities and formats, video surfaces, a video processor, its states, and a blit: the background
 * colour around the stream, the stream (scaled) in its destination rectangle. Prints KEY=VALUE.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

typedef struct { void **vtbl; } Obj;
#define CALL(o, i, ret, ...) ((ret (__stdcall *)(Obj*, ##__VA_ARGS__))((o)->vtbl[i]))

typedef struct { int InputFrameFormat; UINT fn, fd, InputWidth, InputHeight, on, od, OutputWidth, OutputHeight; } content_desc;
typedef struct { int DeviceType; UINT DeviceCaps, FeatureCaps, FilterCaps, InputFormatCaps; D3DPOOL InputPool;
                 UINT OutputFormatCount, InputFormatCount, VideoProcessorCount, MaxInputStreams, MaxStreamStates; } devcaps;
typedef struct { GUID VPGuid; UINT PastFrames, FutureFrames, ProcessorCaps, ITelecineCaps, CustomRateCount; } vpcaps;
typedef struct { BOOL Enable; UINT OutputIndex, InputFrameOrField, PastFrames, FutureFrames;
                 IDirect3DSurface9 **ppPastSurfaces; IDirect3DSurface9 *pInputSurface; IDirect3DSurface9 **ppFutureSurfaces; } stream_data;
typedef struct { BOOL YCbCr; float Color[4]; } background;
typedef struct { BOOL Enable; RECT Rect; } rect_state;

static DWORD pixel(IDirect3DDevice9Ex *dev, IDirect3DSurface9 *rt, int x, int y)
{
    IDirect3DSurface9 *sys = NULL;
    D3DLOCKED_RECT lr;
    D3DSURFACE_DESC d;
    DWORD v = 0xdeadbeef;
    IDirect3DSurface9_GetDesc(rt, &d);
    if (FAILED(IDirect3DDevice9Ex_CreateOffscreenPlainSurface(dev, d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, NULL))) return v;
    if (SUCCEEDED(IDirect3DDevice9Ex_GetRenderTargetData(dev, rt, sys)) && SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY)))
    {
        v = ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x] & 0xffffff;
        IDirect3DSurface9_UnlockRect(sys);
    }
    IDirect3DSurface9_Release(sys);
    return v;
}

int main(void)
{
    HRESULT (WINAPI *create9ex)(UINT, IDirect3D9Ex **) = (void *)GetProcAddress(LoadLibraryA("d3d9.dll"), "Direct3DCreate9Ex");
    HRESULT (WINAPI *createhd)(IDirect3DDevice9Ex *, const content_desc *, int, void *, Obj **) =
        (void *)GetProcAddress(LoadLibraryA("dxva2.dll"), "DXVAHD_CreateDevice");
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3D9Ex *d3d = NULL;
    IDirect3DDevice9Ex *dev = NULL;
    content_desc desc = { 0, 30, 1, 32, 32, 30, 1, 64, 64 };
    Obj *hd = NULL, *vp = NULL;
    devcaps caps;
    vpcaps vcaps;
    D3DFORMAT fmts[16];
    IDirect3DSurface9 *out = NULL, *in = NULL;
    background bg = { FALSE, { 0.0f, 1.0f, 0.0f, 1.0f } }, bg2;
    rect_state dst = { TRUE, { 16, 16, 48, 48 } };
    stream_data sd = {0}, two[2] = {{0}};
    HWND hwnd;
    HRESULT hr;

    hwnd = CreateWindowA("static", "dxvahd", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    if (!create9ex || FAILED(create9ex(D3D_SDK_VERSION, &d3d))) { printf("d3d=0\n"); return 1; }
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = hwnd; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    hr = IDirect3D9Ex_CreateDeviceEx(d3d, 0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL, &dev);
    printf("d3d=%d\n", SUCCEEDED(hr));
    if (FAILED(hr)) return 1;
    hr = createhd ? createhd(dev, &desc, 0, NULL, &hd) : E_FAIL;
    printf("CreateDevice=%#lx\n", hr);
    if (FAILED(hr) || !hd) return 1;
    memset(&caps, 0, sizeof(caps));
    hr = CALL(hd, 4, HRESULT, devcaps *)(hd, &caps);
    printf("DeviceCaps=%#lx %u %u %u %u\n", hr, caps.OutputFormatCount >= 1, caps.InputFormatCount >= 1, caps.VideoProcessorCount, caps.MaxInputStreams);
    hr = CALL(hd, 5, HRESULT, UINT, D3DFORMAT *)(hd, caps.OutputFormatCount, fmts);
    printf("OutputFormats=%#lx %d\n", hr, fmts[0] == D3DFMT_X8R8G8B8);
    hr = CALL(hd, 6, HRESULT, UINT, D3DFORMAT *)(hd, caps.InputFormatCount, fmts);
    printf("InputFormats=%#lx %d\n", hr, fmts[0] == MAKEFOURCC('N','V','1','2'));
    hr = CALL(hd, 7, HRESULT, UINT, vpcaps *)(hd, 1, &vcaps);
    printf("ProcessorCaps=%#lx\n", hr);
    hr = CALL(hd, 3, HRESULT, UINT, UINT, D3DFORMAT, D3DPOOL, DWORD, int, UINT, IDirect3DSurface9 **, HANDLE *)
        (hd, 64, 64, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, 0, 2 /* output */, 1, &out, NULL);
    printf("OutputSurface=%#lx\n", hr);
    hr = CALL(hd, 3, HRESULT, UINT, UINT, D3DFORMAT, D3DPOOL, DWORD, int, UINT, IDirect3DSurface9 **, HANDLE *)
        (hd, 32, 32, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, 0, 0 /* input */, 1, &in, NULL);
    printf("InputSurface=%#lx\n", hr);
    hr = CALL(hd, 3, HRESULT, UINT, UINT, D3DFORMAT, D3DPOOL, DWORD, int, UINT, IDirect3DSurface9 **, HANDLE *)
        (hd, 32, 32, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, 0, 0, 1, &in, NULL);
    printf("BadFormat=%d\n", hr == E_INVALIDARG);
    hr = CALL(hd, 10, HRESULT, const GUID *, Obj **)(hd, &vcaps.VPGuid, &vp);
    printf("Processor=%#lx\n", hr);
    if (!vp || !out || !in) return 1;

    hr = CALL(vp, 3, HRESULT, int, UINT, const void *)(vp, 1 /* BACKGROUND_COLOR */, sizeof(bg), &bg);
    memset(&bg2, 0, sizeof(bg2));
    CALL(vp, 4, HRESULT, int, UINT, void *)(vp, 1, sizeof(bg2), &bg2);
    printf("Background=%#lx %d\n", hr, bg2.Color[1] == 1.0f);
    hr = CALL(vp, 3, HRESULT, int, UINT, const void *)(vp, 1, 3, &bg);
    printf("BadSize=%d\n", hr == E_INVALIDARG);
    hr = CALL(vp, 5, HRESULT, UINT, int, UINT, const void *)(vp, 0, 5 /* DESTINATION_RECT */, sizeof(dst), &dst);
    printf("Destination=%#lx\n", hr);

    IDirect3DDevice9Ex_ColorFill(dev, in, NULL, D3DCOLOR_XRGB(255, 0, 0));
    IDirect3DDevice9Ex_ColorFill(dev, out, NULL, D3DCOLOR_XRGB(0, 0, 255));
    sd.Enable = TRUE; sd.pInputSurface = in;
    hr = CALL(vp, 7, HRESULT, IDirect3DSurface9 *, UINT, UINT, const stream_data *)(vp, out, 0, 1, &sd);
    printf("Blt=%#lx\n", hr);
    printf("Inside=%06lx\n", pixel(dev, out, 32, 32));
    printf("Outside=%06lx\n", pixel(dev, out, 4, 4));
    two[0] = sd; two[1] = sd;
    hr = CALL(vp, 7, HRESULT, IDirect3DSurface9 *, UINT, UINT, const stream_data *)(vp, out, 0, 2, two);
    printf("TwoStreams=%d\n", hr == E_INVALIDARG);

    CALL(vp, 2, ULONG)(vp);
    IDirect3DSurface9_Release(in);
    IDirect3DSurface9_Release(out);
    CALL(hd, 2, ULONG)(hd);
    IDirect3DDevice9Ex_Release(dev);
    IDirect3D9Ex_Release(d3d);
    printf("done=1\n");
    return 0;
}
