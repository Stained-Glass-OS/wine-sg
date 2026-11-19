/* ddperf-probe: what an old 2D program does to a windowed DirectDraw primary surface, timed.
 * DDSCL_NORMAL on the program's own window, a primary with a clipper on it, and in turn:
 *   small  frames of 200 Blts of a 32x32 sprite to the primary (a sprite engine);
 *   ref    the same 200 Blts per frame to an offscreen surface (what the blits cost alone);
 *   gdi    frames of GetDC on the primary, 20 lines of TextOut, ReleaseDC (a GDI-on-primary UI);
 *   full   frames of one Blt of the whole window (a program that draws off screen and copies);
 *   final  a burst of small Blts, then a 120x120 magenta square at client (200,150), drawn
 *          once and never again while the program only waits for messages: it must show.
 * usage: ddperf-probe SECONDS-PER-PHASE (in its directory: phase.txt names the phase running,
 * "final" once the square is drawn, then "done"; probe.log "PHASE blits N frames F ms M") */
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdarg.h>

static HWND win;
static int seconds = 3;

static void plog(const char *fmt, ...)
{
    FILE *f = fopen("probe.log", "a");
    va_list ap;

    if (!f) return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

static void phase(const char *s)
{
    FILE *f = fopen("phase.txt", "w");

    if (f) { fputs(s, f); fclose(f); }
}

static void pump(void)
{
    MSG msg;

    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static IDirectDrawSurface7 *offscreen(IDirectDraw7 *ddraw, int w, int h, DWORD colour)
{
    DDSURFACEDESC2 desc = {sizeof(desc)};
    IDirectDrawSurface7 *s;
    DDBLTFX fx = {sizeof(fx)};

    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.dwWidth = w;
    desc.dwHeight = h;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    if (FAILED(IDirectDraw7_CreateSurface(ddraw, &desc, &s, NULL))) return NULL;
    fx.dwFillColor = colour;
    IDirectDrawSurface7_Blt(s, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    return s;
}

static RECT client_screen(void)
{
    POINT origin = {0, 0};
    RECT r;

    GetClientRect(win, &r);
    ClientToScreen(win, &origin);
    OffsetRect(&r, origin.x, origin.y);
    return r;
}

/* 200 sprites a frame, spread over the window, each frame moved a little */
static void small_frame(IDirectDrawSurface7 *dst, IDirectDrawSurface7 *sprite, RECT area, int frame, int count)
{
    int k;

    for (k = 0; k < count; k++)
    {
        int x = (k * 37 + frame * 3) % (area.right - area.left - 32);
        int y = (k * 53 + frame * 2) % (area.bottom - area.top - 32);
        RECT r = {area.left + x, area.top + y, area.left + x + 32, area.top + y + 32};

        IDirectDrawSurface7_Blt(dst, &r, sprite, NULL, DDBLT_WAIT, NULL);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show)
{
    IDirectDrawSurface7 *primary, *sprite, *back, *page;
    DDSURFACEDESC2 desc = {sizeof(desc)};
    IDirectDrawClipper *clipper;
    IDirectDraw7 *ddraw;
    DDBLTFX fx = {sizeof(fx)};
    DWORD start, now;
    WNDCLASSA wc = {0};
    RECT area, r;
    HRESULT hr;
    int frames, blits, i;

    if (atoi(cmdline) > 0) seconds = atoi(cmdline);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = instance;
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "ddperf";
    RegisterClassA(&wc);
    win = CreateWindowA("ddperf", "ddperf", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, instance, NULL);
    pump();

    if (FAILED(hr = DirectDrawCreateEx(NULL, (void **)&ddraw, &IID_IDirectDraw7, NULL))
            || FAILED(hr = IDirectDraw7_SetCooperativeLevel(ddraw, win, DDSCL_NORMAL)))
    {
        plog("DirectDraw %#lx\n", hr);
        phase("failed");
        return 1;
    }
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (FAILED(hr = IDirectDraw7_CreateSurface(ddraw, &desc, &primary, NULL))
            || FAILED(hr = IDirectDraw7_CreateClipper(ddraw, 0, &clipper, NULL)))
    {
        plog("primary %#lx\n", hr);
        phase("failed");
        return 1;
    }
    IDirectDrawClipper_SetHWnd(clipper, 0, win);
    IDirectDrawSurface7_SetClipper(primary, clipper);
    IDirectDrawClipper_Release(clipper);
    sprite = offscreen(ddraw, 32, 32, 0x00ffc000);
    back = offscreen(ddraw, 640, 480, 0x00202020);
    page = offscreen(ddraw, 640, 480, 0x00404080);
    if (!sprite || !back || !page)
    {
        plog("offscreen surfaces failed\n");
        phase("failed");
        return 1;
    }

    phase("small");
    area = client_screen();
    for (frames = blits = 0, start = GetTickCount(); GetTickCount() - start < (DWORD)seconds * 1000; frames++)
    {
        small_frame(primary, sprite, area, frames, 200);
        blits += 200;
        pump();
    }
    now = GetTickCount();
    plog("small blits %d frames %d ms %lu\nsmall start %lu\nsmall end %lu\n", blits, frames, now - start, start, now);

    phase("ref");
    SetRect(&r, 0, 0, 640, 480);
    for (frames = blits = 0, start = GetTickCount(); GetTickCount() - start < (DWORD)seconds * 1000; frames++)
    {
        small_frame(back, sprite, r, frames, 200);
        blits += 200;
        pump();
    }
    plog("ref blits %d frames %d ms %lu\n", blits, frames, GetTickCount() - start);

    phase("gdi");
    for (frames = blits = 0, start = GetTickCount(); GetTickCount() - start < (DWORD)seconds * 1000; frames++)
    {
        HDC dc;

        if (FAILED(hr = IDirectDrawSurface7_GetDC(primary, &dc)))
        {
            plog("GetDC %#lx\n", hr);
            break;
        }
        SetBkColor(dc, RGB(0, 0, 64 + frames % 128));
        SetTextColor(dc, RGB(255, 255, 255));
        for (i = 0; i < 20; i++)
        {
            char line[64];
            int n = sprintf(line, "line %d of frame %d", i, frames);

            TextOutA(dc, area.left + 20, area.top + 20 + i * 20, line, n);
        }
        IDirectDrawSurface7_ReleaseDC(primary, dc);
        blits++;
        pump();
    }
    plog("gdi blits %d frames %d ms %lu\n", blits, frames, GetTickCount() - start);

    phase("full");
    for (frames = 0, start = GetTickCount(); GetTickCount() - start < (DWORD)seconds * 1000; frames++)
    {
        IDirectDrawSurface7_Blt(primary, &area, (frames & 1) ? back : page, NULL, DDBLT_WAIT, NULL);
        pump();
    }
    plog("full blits %d frames %d ms %lu\n", frames, frames, GetTickCount() - start);

    /* the last picture: right after a burst (inside any coalescing window), then nothing */
    fx.dwFillColor = 0;
    IDirectDrawSurface7_Blt(primary, &area, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    small_frame(primary, sprite, area, 0, 50);
    SetRect(&r, area.left + 200, area.top + 150, area.left + 320, area.top + 270);
    fx.dwFillColor = 0x00ff00ff;
    IDirectDrawSurface7_Blt(primary, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    plog("final drawn\n");
    phase("final");
    for (now = GetTickCount(); GetTickCount() - now < 4000;)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
        pump();
    }
    phase("done");
    IDirectDrawSurface7_Release(page);
    IDirectDrawSurface7_Release(back);
    IDirectDrawSurface7_Release(sprite);
    IDirectDrawSurface7_Release(primary);
    IDirectDraw7_Release(ddraw);
    DestroyWindow(win);
    return 0;
}
