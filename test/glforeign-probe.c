/* glforeign-probe.c (wine-sg 6099): Direct3D 11 (wined3d's OpenGL path) presenting into another
 * process's top-level window, as Chromium's GPU process does into the browser's window.
 *   glforeign-probe.exe host DIR framed|popup -- a white window (with a frame, or a bare popup whose
 *                                       client area is all of it) at -50,200 (400x300, partly off screen) with a
 *                                       child, a gray window over its corner (250,400 200x200);
 *                                       writes DIR\host.hwnd; on DIR\redraw redraws and erases
 *                                       itself and writes DIR\redrawn; stays up 60 s
 *   glforeign-probe.exe render DIR    -- a swap chain on that window: three red frames, then still;
 *                                       writes DIR\render.out (create=, presented=1) */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>

static char path[MAX_PATH];

static const char *file(const char *dir, const char *name)
{
    snprintf(path, sizeof(path), "%s\\%s", dir, name);
    return path;
}

static int host(const char *dir, BOOL framed)
{
    WNDCLASSW wc = {0};
    HWND top;
    FILE *f;
    MSG msg;
    DWORD start = GetTickCount();
    BOOL redrawn = FALSE;

    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SgGlForeignHost";
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    RegisterClassW(&wc);
    top = CreateWindowW(L"SgGlForeignHost", L"gl foreign host", framed ? WS_OVERLAPPEDWINDOW : WS_POPUP, -50, 200, 400, 300, NULL, NULL,
                        wc.hInstance, NULL);
    /* a window with children, as the browser's (Chrome Legacy Window) */
    CreateWindowExW(WS_EX_TRANSPARENT, L"Static", L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, top, NULL, NULL, NULL);
    ShowWindow(top, SW_SHOW);
    UpdateWindow(top);
    /* something over part of it when the drawable is made: its visible region is not its client area */
    CreateWindowW(L"Static", L"", WS_POPUP | WS_VISIBLE | SS_GRAYRECT, 250, 400, 200, 200, NULL, NULL, NULL, NULL);
    if (!(f = fopen(file(dir, "host.hwnd"), "w"))) return 2;
    fprintf(f, "%p\n", top);
    fclose(f);
    while (GetTickCount() - start < 60000)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!redrawn && GetFileAttributesA(file(dir, "redraw")) != INVALID_FILE_ATTRIBUTES)
        {
            /* the browser's own painting there: an expose, erasing its background */
            RedrawWindow(top, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
            GdiFlush();
            Sleep(500);
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
            if ((f = fopen(file(dir, "redrawn"), "w"))) fclose(f);
            redrawn = TRUE;
        }
        Sleep(50);
    }
    return 0;
}

static int render(const char *dir)
{
    DXGI_SWAP_CHAIN_DESC sd = {0};
    IDXGISwapChain *chain;
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11Texture2D *buffer;
    ID3D11RenderTargetView *view;
    float red[4] = {1, 0, 0, 1};
    void *p = NULL;
    HRESULT hr;
    FILE *f;
    int i;

    if (!(f = fopen(file(dir, "host.hwnd"), "r"))) return 2;
    fscanf(f, "%p", &p);
    fclose(f);
    sd.BufferDesc.Width = 400;
    sd.BufferDesc.Height = 300;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.OutputWindow = p;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd,
                                       &chain, &device, NULL, &context);
    if (!(f = fopen(file(dir, "render.out"), "w"))) return 2;
    fprintf(f, "create=%#lx\n", hr);
    fflush(f);
    if (FAILED(hr)) return 1;
    IDXGISwapChain_GetBuffer(chain, 0, &IID_ID3D11Texture2D, (void **)&buffer);
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)buffer, NULL, &view);
    for (i = 0; i < 3; i++)
    {
        ID3D11DeviceContext_OMSetRenderTargets(context, 1, &view, NULL);
        ID3D11DeviceContext_ClearRenderTargetView(context, view, red);
        IDXGISwapChain_Present(chain, 0, 0);
        Sleep(300);
    }
    fprintf(f, "presented=1\n");
    fclose(f);
    Sleep(40000);  /* a still page: no more frames */
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    if (!strcmp(argv[1], "host")) return host(argv[2], argc > 3 && !strcmp(argv[3], "framed"));
    if (!strcmp(argv[1], "render")) return render(argv[2]);
    return 2;
}
