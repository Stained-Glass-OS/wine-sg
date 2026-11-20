/* ddclip gate probe (wine-sg 8408): a clipped Blt to a DirectDraw primary surface for a window
 * partly off the screen, as the VMR-7 video renderer blits each frame. DDSCL_NORMAL, a primary
 * with a clipper on the window, a red 320x200 off-screen surface blitted to the client area's
 * screen rectangle: once for a window wholly on the screen, once for a window hanging 120 pixels
 * past the right edge. Prints KEY=1 per check and where to look:
 *   pixel=X,Y   a screen point inside the off-screen window's visible part
 *   done=1 */
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>

static int verbose = 1;

static HRESULT blit_to(IDirectDrawSurface7 *primary, IDirectDrawSurface7 *src, HWND w)
{
    POINT pt = { 0, 0 };
    RECT r;

    GetClientRect(w, &r);
    ClientToScreen(w, &pt);
    OffsetRect(&r, pt.x, pt.y);
    if (verbose) printf("client: %ld,%ld-%ld,%ld\n", r.left, r.top, r.right, r.bottom);
    return IDirectDrawSurface7_Blt(primary, &r, src, NULL, DDBLT_WAIT, NULL);
}

static void pump(int n, IDirectDrawSurface7 *primary, IDirectDrawSurface7 *src, HWND w)
{
    MSG msg;
    int i;

    for (i = 0; i < n; i++)
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        if (src) blit_to(primary, src, w);
        Sleep(25);
    }
}

static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) { ValidateRect(w, NULL); return 0; }
    return DefWindowProcA(w, m, wp, lp);
}

int main(void)
{
    WNDCLASSA wc = { 0 };
    IDirectDraw7 *dd;
    IDirectDrawSurface7 *primary, *src;
    IDirectDrawClipper *clipper;
    DDSURFACEDESC2 desc;
    DDBLTFX fx;
    HWND w;
    HRESULT hr;
    int cx = GetSystemMetrics(SM_CXSCREEN);

    wc.lpfnWndProc = proc;
    wc.lpszClassName = "ddclip";
    RegisterClassA(&wc);
    w = CreateWindowExA(0, "ddclip", "ddclip", WS_POPUP | WS_VISIBLE, 20, 300, 320, 200, 0, 0, 0, 0);
    pump(20, NULL, NULL, NULL);

    if (FAILED(DirectDrawCreateEx(NULL, (void **)&dd, &IID_IDirectDraw7, NULL)) ||
        FAILED(IDirectDraw7_SetCooperativeLevel(dd, NULL, DDSCL_NORMAL)))
    {
        printf("ddraw=0\ndone=1\n");
        return 0;
    }
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &desc, &primary, NULL))) { printf("ddraw=0\ndone=1\n"); return 0; }
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.dwWidth = 320;
    desc.dwHeight = 200;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &desc, &src, NULL)) ||
        FAILED(IDirectDraw7_CreateClipper(dd, 0, &clipper, NULL)))
    {
        printf("ddraw=0\ndone=1\n");
        return 0;
    }
    IDirectDrawClipper_SetHWnd(clipper, 0, w);
    IDirectDrawSurface7_SetClipper(primary, clipper);
    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = 0xff0000;
    IDirectDrawSurface7_Blt(src, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    printf("ddraw=1\nscreen: %d\n", cx);

    hr = blit_to(primary, src, w);
    printf("on: hr %08lx\non=%d\n", hr, hr == DD_OK);
    /* now hang it 120 pixels past the right edge */
    SetWindowPos(w, NULL, cx - 200, 300, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    pump(10, NULL, NULL, NULL);
    hr = blit_to(primary, src, w);
    printf("off: hr %08lx\noff=%d\n", hr, hr == DD_OK);
    printf("pixel=%d,%d\n", cx - 100, 400);
    fflush(stdout);
    verbose = 0;
    /* keep blitting, as a video does, while the gate reads the screen */
    pump(120, primary, src, w);
    printf("done=1\n");
    return 0;
}
