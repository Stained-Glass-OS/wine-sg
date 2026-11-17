/* magctl-probe: a magnifier control (the "Magnifier" window class, MagSetWindowSource) -- for
 * magapi-gate.sh. A popup at 100,100 holds a 300x200 control showing 10,20 150x100 at 200%; then a
 * window comes over its right half; then its colours are inverted; then it is destroyed. */
#include <windows.h>
#include <stdio.h>

typedef struct { float transform[5][5]; } MAGCOLOREFFECT;
typedef struct { float v[3][3]; } MAGTRANSFORM;

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

int main(void)
{
    HMODULE m;
    BOOL (WINAPI *init)(void), (WINAPI *setsrc)(HWND, RECT), (WINAPI *getsrc)(HWND, RECT *);
    BOOL (WINAPI *setxf)(HWND, MAGTRANSFORM *), (WINAPI *seteff)(HWND, MAGCOLOREFFECT *);
    MAGTRANSFORM xf = {{ { 2, 0, 0 }, { 0, 2, 0 }, { 0, 0, 1 } }};
    MAGCOLOREFFECT inv = {{ { -1, 0, 0, 0, 0 }, { 0, -1, 0, 0, 0 }, { 0, 0, -1, 0, 0 }, { 0, 0, 0, 1, 0 }, { 1, 1, 1, 0, 1 } }};
    RECT src = { 10, 20, 160, 120 }, got = { 0 };
    HWND host, ctl, cover;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    m = LoadLibraryA("magnification.dll");
    init = (void *)GetProcAddress(m, "MagInitialize");
    setsrc = (void *)GetProcAddress(m, "MagSetWindowSource");
    getsrc = (void *)GetProcAddress(m, "MagGetWindowSource");
    setxf = (void *)GetProcAddress(m, "MagSetWindowTransform");
    seteff = (void *)GetProcAddress(m, "MagSetColorEffect");
    if (!init || !setsrc || !getsrc || !setxf || !seteff) { printf("missing exports\n"); return 1; }
    printf("init %d\n", init());
    host = CreateWindowExW(0, L"Static", L"host", WS_POPUP | WS_VISIBLE, 100, 100, 400, 300, NULL, NULL, NULL, NULL);
    ctl = CreateWindowExW(0, L"Magnifier", L"MagnifierWindow", WS_CHILD | WS_VISIBLE, 0, 0, 300, 200, host, NULL, NULL, NULL);
    printf("host %d control %d err %lu\n", host != NULL, ctl != NULL, GetLastError());
    printf("id %u\n", (unsigned int)(UINT_PTR)ctl);
    printf("transform %d\n", setxf(ctl, &xf));
    printf("source %d\n", setsrc(ctl, src));
    getsrc(ctl, &got);
    printf("getsource %ld %ld %ld %ld\n", got.left, got.top, got.right, got.bottom);
    fflush(stdout);
    pump(800);
    printf("phase cover\n"); fflush(stdout);
    cover = CreateWindowExW(WS_EX_TOPMOST, L"Static", L"cover", WS_POPUP | WS_VISIBLE, 250, 50, 400, 400, NULL, NULL, NULL, NULL);
    pump(800);
    printf("phase invert\n"); fflush(stdout);
    DestroyWindow(cover);
    seteff(ctl, &inv);
    pump(800);
    printf("phase destroy\n"); fflush(stdout);
    DestroyWindow(ctl);
    pump(500);
    printf("done\n");
    return 0;
}
