/* DrawAnimatedRects probe: needs a display. Prints "ok ..." lines and RESULT: PASS|FAIL */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok %s\n", #c); else { fails++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define W 640
#define H 480
static BYTE *base, *snap;
static volatile LONG stop, changed_snaps;
static HBITMAP dib;
static HDC memdc;

static void grab(BYTE *out)
{
    HDC scr = GetDC(NULL);
    BitBlt(memdc, 0, 0, W, H, scr, 0, 0, SRCCOPY);
    ReleaseDC(NULL, scr);
    GdiFlush();
    memcpy(out, snap, W * H * 4);
}

static DWORD WINAPI sampler(void *arg)
{
    BYTE *buf = malloc(W * H * 4);
    while (!stop)
    {
        grab(buf);
        if (memcmp(buf, base, W * H * 4)) InterlockedIncrement(&changed_snaps);
    }
    free(buf);
    return 0;
}

static DWORD cpu_ms(void)
{
    FILETIME c, e, k, u;
    ULARGE_INTEGER kk, uu;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    kk.LowPart = k.dwLowDateTime; kk.HighPart = k.dwHighDateTime;
    uu.LowPart = u.dwLowDateTime; uu.HighPart = u.dwHighDateTime;
    return (DWORD)((kk.QuadPart + uu.QuadPart) / 10000);
}

static DWORD run(HWND hwnd, INT ani, const RECT *a, const RECT *b, BOOL *ret, LONG *drawn)
{
    HANDLE th;
    DWORD t0, el;
    stop = 0; changed_snaps = 0;
    grab(base);
    th = CreateThread(NULL, 0, sampler, NULL, 0, NULL);
    Sleep(50);
    t0 = GetTickCount();
    *ret = DrawAnimatedRects(hwnd, ani, a, b);
    el = GetTickCount() - t0;
    stop = 1;
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    *drawn = changed_snaps;
    return el;
}

static void set_min_animate(int on)
{
    ANIMATIONINFO ai = { sizeof(ai), on };
    BOOL r = SystemParametersInfoW(SPI_SETANIMATION, sizeof(ai), &ai, SPIF_UPDATEINIFILE);
    CHECK(r, "SPI_SETANIMATION failed %lu", GetLastError());
}

static int min_animate(void)
{
    ANIMATIONINFO ai = { sizeof(ai), 99 };
    BOOL r = SystemParametersInfoW(SPI_GETANIMATION, sizeof(ai), &ai, 0);
    CHECK(r, "SPI_GETANIMATION failed");
    return ai.iMinAnimate;
}

int main(void)
{
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB } };
    HDC scr = GetDC(NULL);
    RECT a = { 100, 100, 200, 130 }, b = { 300, 250, 600, 450 };
    BOOL ret;
    LONG drawn;
    DWORD el, c0, c1;
    HKEY key;
    char buf[16] = "";
    DWORD size = sizeof(buf);
    HWND hwnd;
    MSG msg;
    int timers = 0;
    BYTE *after;
    void *bits;

    dib = CreateDIBSection(scr, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    memdc = CreateCompatibleDC(scr);
    SelectObject(memdc, dib);
    snap = bits;
    ReleaseDC(NULL, scr);
    base = malloc(W * H * 4);
    after = malloc(W * H * 4);
    CHECK(GetSystemMetrics(SM_CXSCREEN) >= W && GetSystemMetrics(SM_CYSCREEN) >= H, "screen too small");

    CHECK(min_animate() == 0, "default MinAnimate not 0");
    set_min_animate(1);
    CHECK(min_animate() == 1, "MinAnimate not 1 after set");
    CHECK(!RegOpenKeyExA(HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", 0, KEY_READ, &key), "no key");
    if (!RegQueryValueExA(key, "MinAnimate", NULL, NULL, (BYTE *)buf, &size)) CHECK(!strcmp(buf, "1"), "value %s", buf);
    else CHECK(0, "MinAnimate value missing");
    RegCloseKey(key);

    el = run(NULL, 3, &a, &b, &ret, &drawn);
    CHECK(ret, "returned FALSE");
    CHECK(el >= 150 && el <= 900, "took %lu ms", el);
    CHECK(drawn >= 3, "sampler saw only %ld changed snapshots", drawn);
    grab(after);
    CHECK(!memcmp(after, base, W * H * 4), "frame left on screen");

    hwnd = CreateWindowA("static", "t", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    CHECK(DrawAnimatedRects(hwnd, 3, &a, &b), "with hwnd failed");
    c0 = cpu_ms();
    {
        DWORD t0 = GetTickCount();
        while (GetTickCount() - t0 < 1500)
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { if (msg.message == WM_TIMER) timers++; DispatchMessageA(&msg); }
            Sleep(20);
        }
    }
    c1 = cpu_ms();
    CHECK(timers == 0, "%d WM_TIMER after the animation", timers);
    CHECK(c1 - c0 < 150, "idle burned %lu ms CPU in 1.5 s", c1 - c0);
    DestroyWindow(hwnd);
    grab(after);
    CHECK(!memcmp(after, base, W * H * 4), "frame left on screen (hwnd)");

    el = run(NULL, 1, &a, &b, &ret, &drawn);
    CHECK(ret && el < 100 && drawn == 0, "IDANI_OPEN: ret %d el %lu drawn %ld", ret, el, drawn);
    SetLastError(0);
    CHECK(!DrawAnimatedRects(NULL, 3, NULL, &b) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL from: err %lu", GetLastError());
    SetLastError(0);
    CHECK(!DrawAnimatedRects((HWND)0xdeadbee0, 3, &a, &b) && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "bad hwnd: err %lu", GetLastError());

    {
        BOOL off = FALSE;
        CHECK(SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, (void *)(INT_PTR)off, 0), "set client-area failed");
        el = run(NULL, 3, &a, &b, &ret, &drawn);
        CHECK(ret && el < 100 && drawn == 0, "client-area off: ret %d el %lu drawn %ld", ret, el, drawn);
        SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, (void *)(INT_PTR)TRUE, 0);
    }

    set_min_animate(0);
    CHECK(min_animate() == 0, "not 0 after set 0");
    el = run(NULL, 3, &a, &b, &ret, &drawn);
    CHECK(ret && el < 100 && drawn == 0, "MinAnimate off: ret %d el %lu drawn %ld", ret, el, drawn);

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
