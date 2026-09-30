// A process renders into another process's child window through Vulkan
// (patches/sg/0586): Chromium's GPU process draws into the browser's child
// window. "host" makes a window with a child and, after the render process
// has presented, prints the child's centre pixel; "render HWND" presents red
// into that child with Direct3D 11 (DXVK: Vulkan).
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cstdlib>

static int render(HWND child);
static DWORD WINAPI self_render(void *arg) { return render((HWND)arg); }

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "host"))
    {
        WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "XprocHost";
        wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        RegisterClassA(&wc);
        HWND top = CreateWindowA("XprocHost", "xproc", WS_POPUP | WS_VISIBLE, 100, 100, 300, 300, 0, 0, 0, 0);
        HWND child = CreateWindowA("XprocHost", "", WS_CHILD | WS_VISIBLE, 50, 50, 200, 200, top, 0, 0, 0);
        char cmd[512];
        /* "host top": render into the top-level itself (Chromium draws there
         * when it has no DirectComposition); otherwise into its child */
        HWND target = (argc > 2 && strstr(argv[2], "top")) ? top : child;
        snprintf(cmd, sizeof(cmd), "\"%s\" render %p", argv[0], (void *)target);
        STARTUPINFOA si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
        if (argc > 2 && !strncmp(argv[2], "self", 4))   /* the control: the same process renders */
            pi.hProcess = CreateThread(NULL, 0, self_render, target, 0, NULL);
        else if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("xproc=FAIL spawn\n"); return 1; }
        DWORD t0 = GetTickCount();
        COLORREF c = 0; bool red = false;
        while (GetTickCount() - t0 < 8000)
        {
            MSG msg;
            while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
            HDC dc = GetDC(NULL); c = GetPixel(dc, 250, 250); ReleaseDC(NULL, dc);
            if (GetRValue(c) > 200 && GetGValue(c) < 60 && GetBValue(c) < 60) { red = true; break; }
            if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) break;
        }
        DWORD code = 0; WaitForSingleObject(pi.hProcess, 5000);
        if (!GetExitCodeProcess(pi.hProcess, &code)) GetExitCodeThread(pi.hProcess, &code);
        printf(red ? "xproc=ok %06lx render=%lu\n" : "xproc=FAIL pixel %06lx render=%lu\n", c, code);
        return red ? 0 : 1;
    }
    if (argc > 2 && !strcmp(argv[1], "render"))
        return render((HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16));
    return 4;
}

static int render(HWND child)
{
    {
        ID3D11Device *dev; ID3D11DeviceContext *ctx;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx)))
            return 2;
        IDXGIDevice *dd; IDXGIAdapter *ad; IDXGIFactory2 *f;
        dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dd); dd->GetAdapter(&ad);
        ad->GetParent(__uuidof(IDXGIFactory2), (void **)&f);
        RECT cr; GetClientRect(child, &cr);
        DXGI_SWAP_CHAIN_DESC1 d = {}; d.Width = cr.right; d.Height = cr.bottom; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2;
        d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        IDXGISwapChain1 *sc;
        if (FAILED(f->CreateSwapChainForHwnd(dev, child, &d, nullptr, nullptr, &sc))) return 3;
        ID3D11Texture2D *bb; ID3D11RenderTargetView *rtv;
        sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb); dev->CreateRenderTargetView(bb, nullptr, &rtv);
        const float red[4] = { 1, 0, 0, 1 };
        for (int i = 0; i < 60; i++) { ctx->OMSetRenderTargets(1, &rtv, nullptr); ctx->ClearRenderTargetView(rtv, red); sc->Present(1, 0); Sleep(50); }
        return 0;
    }
}
