/* glhiddenframe-probe.c (wine-sg 6096): a Direct3D 11 swap chain on a child window (wined3d's OpenGL
 * path renders it offscreen and copies it into the top-level) presents one red frame and nothing after,
 * as Electron does for its first frame before it shows its window (ready-to-show).
 *   glhiddenframe-probe.exe hidden OUTFILE  -- present while the top-level is hidden, then show it
 *   glhiddenframe-probe.exe shown OUTFILE   -- show the top-level, then present (the control)
 * The window stays up 20 s at 300,200 (400x300); OUTFILE gets device= and present= lines. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    BOOL hidden = argc > 1 && !strcmp(argv[1], "hidden");
    WNDCLASSW wc = {0};
    DXGI_SWAP_CHAIN_DESC sd = {0};
    IDXGISwapChain *chain;
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11Texture2D *buffer;
    ID3D11RenderTargetView *view;
    float red[4] = {1, 0, 0, 1};
    HWND top, child;
    HRESULT hr;
    FILE *out;
    MSG msg;

    if (argc < 3 || !(out = fopen(argv[2], "w"))) return 2;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SgHiddenFrameTop";
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    wc.lpszClassName = L"SgHiddenFrameChild";
    wc.hbrBackground = NULL;  /* a swap chain's window paints nothing itself, as Chromium's */
    RegisterClassW(&wc);
    top = CreateWindowW(L"SgHiddenFrameTop", L"hidden frame probe", WS_POPUP | WS_CLIPCHILDREN, 300, 200, 400, 300,
                        NULL, NULL, wc.hInstance, NULL);
    child = CreateWindowW(L"SgHiddenFrameChild", L"", WS_CHILD | WS_VISIBLE, 0, 0, 400, 300, top, NULL,
                          wc.hInstance, NULL);

    sd.BufferDesc.Width = 400;
    sd.BufferDesc.Height = 300;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.OutputWindow = child;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd,
                                       &chain, &device, NULL, &context);
    fprintf(out, "device=%#lx\n", hr);
    fflush(out);
    if (FAILED(hr)) return 1;
    IDXGISwapChain_GetBuffer(chain, 0, &IID_ID3D11Texture2D, (void **)&buffer);
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)buffer, NULL, &view);

    if (!hidden)
    {
        ShowWindow(top, SW_SHOW);
        UpdateWindow(top);
        Sleep(500);
    }
    ID3D11DeviceContext_OMSetRenderTargets(context, 1, &view, NULL);
    ID3D11DeviceContext_ClearRenderTargetView(context, view, red);
    fprintf(out, "present=%#lx\n", IDXGISwapChain_Present(chain, 0, 0));
    fclose(out);
    if (hidden)
    {
        Sleep(300);
        ShowWindow(top, SW_SHOW);
    }

    SetTimer(top, 1, 20000, NULL);
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_TIMER) break;
        DispatchMessageW(&msg);
    }
    return 0;
}
