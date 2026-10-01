/* A composition swap chain with premultiplied alpha, through DirectComposition,
 * on a popup, cleared fully transparent and presented (patches/sg/0605).
 * Prints: GLASS <prop set 0|1> <X window id hex>, then stays up until the
 * gate is done ("done" file in the folder given). C++: mingw's dcomp.h is.
 * And FRAME <frame changes inside Commit> <after>: the change that makes the
 * window glass must come later, not into the window procedure during Commit
 * (0626: Qt 6 answered it mid-frame and left its windows blank). */
#include <windows.h>
#include <stdio.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>

static BOOL in_commit;
static int frame_inside, frame_after;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCALCSIZE) { if (in_commit) frame_inside++; else frame_after++; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = {};
    ID3D11Device *dev; ID3D11DeviceContext *ctx;
    IDXGIDevice *dxgidev; IDXGIAdapter *adapter; IDXGIFactory2 *factory;
    IDXGISwapChain1 *chain; ID3D11Texture2D *buf; ID3D11RenderTargetView *rtv;
    IDCompositionDevice *dcomp; IDCompositionTarget *target; IDCompositionVisual *visual;
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    static const float clear[4] = { 0, 0, 0, 0 };
    char done[MAX_PATH];
    MSG msg;
    HRESULT hr;

    snprintf(done, sizeof(done), "%s\\done", argc > 1 ? argv[1] : ".");
    wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"DcompAlpha";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST, L"DcompAlpha", L"dcompalpha", WS_POPUP | WS_VISIBLE,
                                100, 100, 200, 100, NULL, NULL, wc.hInstance, NULL);
    if (FAILED(hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                      D3D11_SDK_VERSION, &dev, NULL, &ctx))) { printf("NODEVICE %08lx\n", hr); return 1; }
    dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgidev);
    dxgidev->GetAdapter(&adapter);
    adapter->GetParent(__uuidof(IDXGIFactory2), (void **)&factory);
    desc.Width = 200; desc.Height = 100; desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (FAILED(hr = factory->CreateSwapChainForComposition(dev, &desc, NULL, &chain))) { printf("NOCHAIN %08lx\n", hr); return 1; }
    if (FAILED(hr = DCompositionCreateDevice(dxgidev, __uuidof(IDCompositionDevice), (void **)&dcomp)))
    { printf("NODCOMP %08lx\n", hr); return 1; }
    dcomp->CreateTargetForHwnd(hwnd, TRUE, &target);
    dcomp->CreateVisual(&visual);
    visual->SetContent(chain);
    target->SetRoot(visual);
    frame_after = 0;
    in_commit = TRUE;
    dcomp->Commit();
    in_commit = FALSE;
    chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&buf);
    dev->CreateRenderTargetView(buf, NULL, &rtv);
    for (int i = 0; i < 20; i++)
    {
        ctx->ClearRenderTargetView(rtv, clear);
        chain->Present(0, 0);
        for (DWORD start = GetTickCount(); GetTickCount() - start < 50; Sleep(5))
            while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    }
    printf("FRAME %d %d\n", frame_inside, frame_after);
    printf("GLASS %d %lx\n", GetPropW(hwnd, L"__wine_dwm_glass") != NULL,
           (unsigned long)(ULONG_PTR)GetPropA(hwnd, "__wine_x11_whole_window"));
    fflush(stdout);
    for (DWORD start = GetTickCount(); GetFileAttributesA(done) == INVALID_FILE_ATTRIBUTES && GetTickCount() - start < 60000; Sleep(20))
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    return 0;
}
