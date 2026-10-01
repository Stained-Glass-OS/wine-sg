// A popup that is a sheet of glass, drawn by another process with Vulkan
// (patches/sg/0615): Chromium's GPU process presents into the browser's
// bubbles. "host" makes the popup (DwmExtendFrameIntoClientArea -1) and
// starts "render HWND", which presents premultiplied half-transparent red
// with Direct3D 11 (DXVK: Vulkan) for a few seconds.
#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int render(HWND hwnd)
{
    ID3D11Device *dev; ID3D11DeviceContext *ctx;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx)))
        return 2;
    IDXGIDevice *dd; IDXGIAdapter *ad; IDXGIFactory2 *f;
    dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dd); dd->GetAdapter(&ad);
    ad->GetParent(__uuidof(IDXGIFactory2), (void **)&f);
    RECT cr; GetClientRect(hwnd, &cr);
    DXGI_SWAP_CHAIN_DESC1 d = {}; d.Width = cr.right; d.Height = cr.bottom; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2;
    d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    IDXGISwapChain1 *sc;
    if (FAILED(f->CreateSwapChainForHwnd(dev, hwnd, &d, nullptr, nullptr, &sc))) return 3;
    ID3D11Texture2D *bb; ID3D11RenderTargetView *rtv;
    sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb); dev->CreateRenderTargetView(bb, nullptr, &rtv);
    const float half_red[4] = { 0.5f, 0, 0, 0.5f };   /* premultiplied */
    for (int i = 0; i < 400; i++) { ctx->OMSetRenderTargets(1, &rtv, nullptr); ctx->ClearRenderTargetView(rtv, half_red); sc->Present(1, 0); Sleep(50); }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "host"))
    {
        WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "GlassVkHost";
        RegisterClassA(&wc);
        HWND top = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "GlassVkHost", "glassvk", WS_POPUP,
                                   200, 150, 300, 200, 0, 0, 0, 0);
        MARGINS m = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(top, &m);
        ShowWindow(top, SW_SHOWNOACTIVATE);
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "\"%s\" render %p", argv[0], (void *)top);
        STARTUPINFOA si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("glassvk=FAIL spawn\n"); return 1; }
        while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT)
        {
            MSG msg;
            while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        }
        DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
        printf("glassvk=done render=%lu\n", code);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "render"))
        return render((HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16));
    return 4;
}
