/* A windowed DirectDraw primary keeps reaching the screen after an exclusive-mode round trip
 * (wine-sg 6549). Like SDL 1.2's directx driver (DOSBox Alt+Enter twice): blit to the primary
 * through a clipper; go exclusive fullscreen with a display mode change; restore the mode and go
 * back to DDSCL_NORMAL while the window is still screen-sized; shrink the window; blit again.
 * Output: key=value words; the gate checks shown=1. */
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>

static IDirectDraw7 *dd;

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

static COLORREF screen_px(HWND h, int x, int y)
{
    POINT p = {x, y};
    HDC dc = GetDC(0);
    COLORREF c;
    ClientToScreen(h, &p);
    c = GetPixel(dc, p.x, p.y);
    ReleaseDC(0, dc);
    return c;
}

static HRESULT blit(HWND h, DWORD color)
{
    DDSURFACEDESC2 sd = { sizeof(sd) };
    IDirectDrawSurface7 *prim, *back;
    IDirectDrawClipper *clip;
    DDBLTFX fx = { sizeof(fx) };
    POINT p = {0, 0};
    HRESULT hr;
    RECT r;

    sd.dwFlags = DDSD_CAPS;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (FAILED(hr = IDirectDraw7_CreateSurface(dd, &sd, &prim, NULL))) return hr;
    IDirectDraw7_CreateClipper(dd, 0, &clip, NULL);
    IDirectDrawClipper_SetHWnd(clip, 0, h);
    IDirectDrawSurface7_SetClipper(prim, clip);
    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    sd.dwWidth = 320;
    sd.dwHeight = 200;
    if (SUCCEEDED(hr = IDirectDraw7_CreateSurface(dd, &sd, &back, NULL)))
    {
        fx.dwFillColor = color;
        IDirectDrawSurface7_Blt(back, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
        GetClientRect(h, &r);
        ClientToScreen(h, &p);
        OffsetRect(&r, p.x, p.y);
        hr = IDirectDrawSurface7_Blt(prim, &r, back, NULL, DDBLT_WAIT, NULL);
        IDirectDrawSurface7_Release(back);
    }
    IDirectDrawClipper_Release(clip);
    IDirectDrawSurface7_Release(prim);
    return hr;
}

int main(int argc, char **argv)
{
    /* norestore (6113): back to DDSCL_NORMAL with no RestoreDisplayMode, as DOSBox does, the
       window then where a 640x480 front buffer does not reach */
    BOOL norestore = argc > 1 && !strcmp(argv[1], "norestore");
    int x = norestore ? 600 : 50, y = norestore ? 440 : 50;
    WNDCLASSW wc = {0};
    COLORREF c;
    HRESULT hr;
    RECT wr;
    HWND h;

    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = L"glresize";
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    h = CreateWindowW(L"glresize", L"glresize", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 400, 300, 0, 0, 0, 0);
    SetForegroundWindow(h);
    pump(500);
    if (FAILED(hr = DirectDrawCreateEx(NULL, (void **)&dd, &IID_IDirectDraw7, NULL)))
    {
        printf("ddraw=0 hr=%#lx\n", hr);
        return 1;
    }
    IDirectDraw7_SetCooperativeLevel(dd, h, DDSCL_NORMAL);
    hr = blit(h, 0xff0000);
    pump(500);
    c = screen_px(h, 100, 100);
    printf("windowed_blit=%d hr=%#lx px=%06lx\n", c == RGB(255, 0, 0), hr, c);

    hr = IDirectDraw7_SetCooperativeLevel(dd, h, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
    if (SUCCEEDED(hr)) hr = IDirectDraw7_SetDisplayMode(dd, 640, 480, 32, 0, 0);
    printf("fullscreen=%d hr=%#lx\n", SUCCEEDED(hr), hr);
    pump(500);
    if (!norestore) IDirectDraw7_RestoreDisplayMode(dd);
    IDirectDraw7_SetCooperativeLevel(dd, h, DDSCL_NORMAL);
    SetWindowLongW(h, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(h, HWND_NOTOPMOST, x, y, 400, 300, SWP_FRAMECHANGED);
    pump(500);
    GetWindowRect(h, &wr);
    printf("restored=%d rect=%ld,%ld-%ld,%ld\n", wr.right - wr.left == 400 && wr.bottom - wr.top == 300,
            wr.left, wr.top, wr.right, wr.bottom);

    hr = blit(h, 0x0000ff);
    pump(800);
    c = screen_px(h, 100, 100);
    printf("shown=%d hr=%#lx px=%06lx\n", c == RGB(0, 0, 255), hr, c);
    IDirectDraw7_Release(dd);
    DestroyWindow(h);
    printf("done=1\n");
    return 0;
}
