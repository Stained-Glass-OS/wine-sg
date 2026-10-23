// xproc-still-probe: a still frame of another process's swap chain stays when the window repaints
// (Stained Glass OS wine-sg 6046). "host" makes a top-level (white background) with a small child
// window, and a render process presents red into the top-level a few times, then stops presenting
// (a still page). The host then repaints itself (RedrawWindow, erase and all) as an expose, a focus
// change or a compositor starting would. Phases are written to C:\phase.txt for the gate, which
// reads the screen's pixels.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cstdlib>

static void phase(const char *text)
{
    FILE *f = fopen("C:\\phase.txt", "w");
    if (f) { fputs(text, f); fclose(f); }
}

static void pump(DWORD ms)
{
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < ms)
    {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(20);
    }
}

static int render(HWND target)
{
    ID3D11Device *dev; ID3D11DeviceContext *ctx;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx)))
        return 2;
    IDXGIDevice *dd; IDXGIAdapter *ad; IDXGIFactory2 *f;
    dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dd); dd->GetAdapter(&ad);
    ad->GetParent(__uuidof(IDXGIFactory2), (void **)&f);
    RECT cr; GetClientRect(target, &cr);
    DXGI_SWAP_CHAIN_DESC1 d = {}; d.Width = cr.right; d.Height = cr.bottom; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2;
    d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1 *sc;
    if (FAILED(f->CreateSwapChainForHwnd(dev, target, &d, nullptr, nullptr, &sc))) return 3;
    ID3D11Texture2D *bb; ID3D11RenderTargetView *rtv;
    sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb); dev->CreateRenderTargetView(bb, nullptr, &rtv);
    const float red[4] = { 1, 0, 0, 1 };
    for (int i = 0; i < 60; i++) { ctx->OMSetRenderTargets(1, &rtv, nullptr); ctx->ClearRenderTargetView(rtv, red); sc->Present(1, 0); Sleep(50); }
    Sleep(30000);   /* a still page: nothing more is presented */
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "render"))
        return render((HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16));
    if (argc < 2 || strcmp(argv[1], "host")) return 4;

    WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "XprocStill";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    RegisterClassA(&wc);
    HWND top = CreateWindowA("XprocStill", "xproc-still", WS_POPUP | WS_VISIBLE, 100, 100, 300, 300, 0, 0, 0, 0);
    /* a child window, as a browser's top-level has (Chromium's hidden ones) */
    CreateWindowA("XprocStill", "", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, top, 0, 0, 0);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "\"%s\" render %p", argv[0], (void *)top);
    STARTUPINFOA si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("still=FAIL spawn\n"); return 1; }
    pump(7000);
    phase("presented");
    pump(3000);
    RedrawWindow(top, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
    pump(500);
    phase("repainted");
    pump(5000);
    TerminateProcess(pi.hProcess, 0);
    printf("still=done\n");
    return 0;
}
