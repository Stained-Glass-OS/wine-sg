/* ddprimary-probe: what Wine quartz's VMR-7 video renderer does with every frame.
 * DirectDraw at DDSCL_NORMAL with no window, a primary surface with a clipper on a child
 * (video) window, a YUY2 flipping surface the size of a 960x540 film written with Lock,
 * and one Blt per frame to the child's screen rectangle, from a streaming thread, 25 times
 * a second -- while the top-level's own thread repaints a seek bar with GDI.
 * Each frame is one flat grey, its luma stepping by 7 per frame, so no two frames within
 * 31 in a row look alike: whoever reads the screen can tell a standing picture.
 * usage: ddprimary-probe SECONDS (in its directory: phase.txt "running", then "done";
 * probe.log "frames N ms M", or what failed) */
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdarg.h>

static HWND top, child;
static volatile LONG finished;
static int seconds = 12, owner = 1, ui = 200;

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

static LRESULT CALLBACK top_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static int pos;

    if (msg == WM_TIMER)
    {
        HDC dc = GetDC(hwnd);
        RECT r;

        SetRect(&r, 40, 420, 600, 440);
        FillRect(dc, &r, GetStockObject(GRAY_BRUSH));
        r.right = 40 + (pos++ % 56) * 10;
        FillRect(dc, &r, GetStockObject(WHITE_BRUSH));
        ReleaseDC(hwnd, dc);
        return 0;
    }
    if (msg == WM_APP)
    {
        SetWindowLongA(child, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
        SetParent(child, hwnd);
        SetWindowPos(child, HWND_TOP, 40, 40, 320, 240, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static DWORD WINAPI render(void *arg)
{
    DDSURFACEDESC2 desc = {sizeof(desc)};
    IDirectDrawSurface7 *primary, *frame;
    IDirectDrawClipper *clipper;
    IDirectDraw7 *ddraw;
    DWORD start, next;
    HRESULT hr;
    int i;

    if (FAILED(hr = DirectDrawCreateEx(NULL, (void **)&ddraw, &IID_IDirectDraw7, NULL)))
    {
        plog("DirectDrawCreateEx %#lx\n", hr);
        goto fail;
    }
    if (FAILED(hr = IDirectDraw7_SetCooperativeLevel(ddraw, NULL, DDSCL_NORMAL)))
    {
        plog("SetCooperativeLevel %#lx\n", hr);
        goto fail;
    }
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (FAILED(hr = IDirectDraw7_CreateSurface(ddraw, &desc, &primary, NULL)))
    {
        plog("primary surface %#lx\n", hr);
        goto fail;
    }
    if (FAILED(hr = IDirectDraw7_CreateClipper(ddraw, 0, &clipper, NULL)))
    {
        plog("CreateClipper %#lx\n", hr);
        goto fail;
    }
    IDirectDrawClipper_SetHWnd(clipper, 0, child);
    IDirectDrawSurface7_SetClipper(primary, clipper);
    IDirectDrawClipper_Release(clipper);

    /* as the VMR-7's surface allocator makes it */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_BACKBUFFERCOUNT;
    desc.dwWidth = 960;
    desc.dwHeight = 540;
    desc.dwBackBufferCount = 1;
    desc.ddsCaps.dwCaps = DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_OFFSCREENPLAIN;
    desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
    desc.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    desc.ddpfPixelFormat.dwFourCC = MAKEFOURCC('Y','U','Y','2');
    if (FAILED(hr = IDirectDraw7_CreateSurface(ddraw, &desc, &frame, NULL)))
    {
        plog("YUY2 surface %#lx\n", hr);
        goto fail;
    }

    if (owner)
    {
        /* the VMR's window is a hidden pop-up of its own when it shows the first frame (the
         * graph pauses before it runs); the program then makes it a child of its own window */
        POINT origin = {0, 0};
        RECT rect;

        GetClientRect(child, &rect);
        ClientToScreen(child, &origin);
        OffsetRect(&rect, origin.x, origin.y);
        IDirectDrawSurface7_Blt(primary, &rect, frame, NULL, DDBLT_WAIT, NULL);
        SendMessageA(top, WM_APP, 0, 0);
    }

    phase("running");
    start = next = GetTickCount();
    for (i = 0; GetTickCount() - start < (DWORD)seconds * 1000; i++)
    {
        DDSURFACEDESC2 lock = {sizeof(lock)};
        POINT origin = {0, 0};
        DWORD luma = 16 + (i * 7) % 220, pair = luma | (128 << 8) | (luma << 16) | (128u << 24);
        RECT rect;
        int x, y;

        if (SUCCEEDED(hr = IDirectDrawSurface7_Lock(frame, NULL, &lock, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL)))
        {
            for (y = 0; y < 540; ++y)
                for (x = 0; x < 480; ++x)
                    ((DWORD *)((BYTE *)lock.lpSurface + y * lock.lPitch))[x] = pair;
            IDirectDrawSurface7_Unlock(frame, NULL);
        }
        else plog("frame %d Lock %#lx\n", i, hr);
        GetClientRect(child, &rect);
        ClientToScreen(child, &origin);
        OffsetRect(&rect, origin.x, origin.y);
        if (FAILED(hr = IDirectDrawSurface7_Blt(primary, &rect, frame, NULL, DDBLT_WAIT, NULL)))
            plog("frame %d Blt %#lx\n", i, hr);
        next += 40;
        while ((int)(next - GetTickCount()) > 0) Sleep(2);
    }
    plog("frames %d ms %lu\n", i, GetTickCount() - start);
    phase("done");
    IDirectDrawSurface7_Release(frame);
    IDirectDrawSurface7_Release(primary);
    IDirectDraw7_Release(ddraw);
    finished = 1;
    return 0;

fail:
    phase("failed");
    finished = 1;
    return 1;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show)
{
    WNDCLASSA wc = {0};
    HANDLE thread;
    MSG msg;

    if (atoi(cmdline) > 0) seconds = atoi(cmdline);
    wc.lpfnWndProc = top_proc;
    wc.hInstance = instance;
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "ddprimary_top";
    RegisterClassA(&wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.lpszClassName = "ddprimary_video";
    RegisterClassA(&wc);
    top = CreateWindowA("ddprimary_top", "ddprimary", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 640, 480, NULL, NULL, instance, NULL);
    if (strstr(cmdline, "child")) owner = 0;
    if (owner)
        child = CreateWindowA("ddprimary_video", "video", WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                40, 40, 320, 240, NULL, NULL, instance, NULL);
    else
        child = CreateWindowA("ddprimary_video", "video", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                40, 40, 320, 240, top, NULL, instance, NULL);
    if (strstr(cmdline, "ui=")) ui = atoi(strstr(cmdline, "ui=") + 3);
    SetTimer(top, 1, ui, NULL);
    thread = CreateThread(NULL, 0, render, NULL, 0, NULL);
    while (!finished)
    {
        MsgWaitForMultipleObjects(1, &thread, FALSE, 50, QS_ALLINPUT);
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    WaitForSingleObject(thread, INFINITE);
    DestroyWindow(top);
    return 0;
}
