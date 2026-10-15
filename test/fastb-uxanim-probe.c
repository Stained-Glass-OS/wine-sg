/* uxtheme: buffered animations, HitTestThemeBackground, IsCompositionActive */
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int render_true, render_false;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (BufferedPaintRenderAnimation(hwnd, dc)) render_true++;
        else
        {
            RECT r;
            GetClientRect(hwnd, &r);
            FillRect(dc, &r, GetStockObject(WHITE_BRUSH));
            render_false++;
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pump(DWORD ms, HWND hwnd, int *min_r, int *max_r, int *min_b, int *max_b)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;

    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        if (hwnd && min_r)
        {
            HDC dc = GetDC(hwnd);
            COLORREF c = GetPixel(dc, 20, 20);
            ReleaseDC(hwnd, dc);
            if ((int)GetRValue(c) < *min_r) *min_r = GetRValue(c);
            if ((int)GetRValue(c) > *max_r) *max_r = GetRValue(c);
            if ((int)GetBValue(c) < *min_b) *min_b = GetBValue(c);
            if ((int)GetBValue(c) > *max_b) *max_b = GetBValue(c);
        }
        Sleep(5);
    }
}

/* run the message loop and count the thread timer messages (the animation timer) */
static int count_timers(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    int n = 0;

    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_TIMER && !msg.hwnd) n++;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(5);
    }
    return n;
}

static ULONGLONG cpu_ms(void)
{
    FILETIME c, e, k, u;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    return (((ULONGLONG)k.dwHighDateTime << 32 | k.dwLowDateTime) + ((ULONGLONG)u.dwHighDateTime << 32 | u.dwLowDateTime)) / 10000;
}

static COLORREF pixel(HWND hwnd, int x, int y)
{
    HDC dc = GetDC(hwnd);
    COLORREF c = GetPixel(dc, x, y);
    ReleaseDC(hwnd, dc);
    return c;
}

static void fill(HDC dc, const RECT *r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

int main(void)
{
    WNDCLASSA wc = { 0 };
    HWND hwnd;
    HDC dc, from, to;
    HANIMATIONBUFFER anim;
    BP_ANIMATIONPARAMS ap = { sizeof(ap) };
    RECT rc = { 0, 0, 100, 100 };
    int min_r, max_r, min_b, max_b;
    BOOL enabled = FALSE;
    HRESULT hr;
    HTHEME theme;
    WORD code;
    POINT pt;
    HRGN rgn;
    RECT box = { 10, 10, 110, 90 };

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "SgUxAnim";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("SgUxAnim", "anim", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 300, NULL, NULL, wc.hInstance, NULL);
    pump(300, NULL, NULL, NULL, NULL, NULL);

    DwmIsCompositionEnabled(&enabled);
    SetLastError(0xdeadbeef);
    CHECK(IsCompositionActive() == enabled, "IsCompositionActive %d vs DwmIsCompositionEnabled %d", IsCompositionActive(), enabled);
    CHECK(GetLastError() == ERROR_SUCCESS, "last error %lu", GetLastError());

    /* argument checks */
    ap.style = BPAS_LINEAR;
    ap.dwDuration = 400;
    dc = GetDC(hwnd);
    CHECK(BeginBufferedAnimation(NULL, dc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to) == NULL, "NULL window");
    CHECK(BeginBufferedAnimation(hwnd, NULL, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to) == NULL, "NULL dc");
    CHECK(BeginBufferedAnimation(hwnd, dc, NULL, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to) == NULL, "NULL rect");
    CHECK(BeginBufferedAnimation(hwnd, dc, &rc, BPBF_COMPATIBLEBITMAP, NULL, NULL, &from, &to) == NULL, "NULL params");
    ap.cbSize = 4;
    CHECK(BeginBufferedAnimation(hwnd, dc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to) == NULL, "short params");
    ap.cbSize = sizeof(ap);
    CHECK(EndBufferedAnimation((HANIMATIONBUFFER)0x1234, TRUE) == E_INVALIDARG, "End of nothing");
    CHECK(!BufferedPaintRenderAnimation(hwnd, dc), "render with nothing running");

    /* no update: nothing is drawn, the buffer is gone */
    anim = BeginBufferedAnimation(hwnd, dc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to);
    CHECK(anim && from && to && from != to, "begin %p %p %p", anim, from, to);
    if (anim)
    {
        fill(from, &rc, RGB(255, 0, 0));
        fill(to, &rc, RGB(0, 0, 255));
        CHECK(EndBufferedAnimation(anim, FALSE) == S_OK, "end without update");
        CHECK(EndBufferedAnimation(anim, FALSE) == E_INVALIDARG, "end twice");
        CHECK(pixel(hwnd, 20, 20) == RGB(255, 255, 255), "target changed without an update: %06lx", pixel(hwnd, 20, 20));
    }

    /* a zero duration is a plain paint of the final state */
    ap.dwDuration = 0;
    anim = BeginBufferedAnimation(hwnd, dc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to);
    CHECK(anim && from == NULL && to, "zero duration: from %p to %p", from, to);
    if (anim)
    {
        fill(to, &rc, RGB(0, 255, 0));
        CHECK(EndBufferedAnimation(anim, TRUE) == S_OK, "end");
        CHECK(pixel(hwnd, 20, 20) == RGB(0, 255, 0), "final state drawn at once: %06lx", pixel(hwnd, 20, 20));
    }

    /* a linear animation from red to blue */
    ap.dwDuration = 600;
    anim = BeginBufferedAnimation(hwnd, dc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to);
    CHECK(anim && from && to, "begin animation");
    if (anim)
    {
        fill(from, &rc, RGB(255, 0, 0));
        fill(to, &rc, RGB(0, 0, 255));
        CHECK(EndBufferedAnimation(anim, TRUE) == S_OK, "end animation");
        CHECK(pixel(hwnd, 20, 20) == RGB(255, 0, 0), "first frame is the initial state: %06lx", pixel(hwnd, 20, 20));
        render_true = render_false = 0;
        min_r = min_b = 255;
        max_r = max_b = 0;
        pump(1000, hwnd, &min_r, &max_r, &min_b, &max_b);
        CHECK(max_r == 255 && min_r == 0 && max_b == 255 && min_b < 40, "red %d..%d blue %d..%d", min_r, max_r, min_b, max_b);
        CHECK(render_true >= 5, "BufferedPaintRenderAnimation painted %d frames", render_true);
        CHECK(pixel(hwnd, 20, 20) == RGB(0, 0, 255), "ends in the final state: %06lx", pixel(hwnd, 20, 20));
        {
            PAINTSTRUCT ps;
            HDC pdc = BeginPaint(hwnd, &ps);
            CHECK(!BufferedPaintRenderAnimation(hwnd, pdc), "render after the end");
            EndPaint(hwnd, &ps);
        }
    }

    /* stopping */
    ap.dwDuration = 5000;
    anim = BeginBufferedAnimation(hwnd, dc, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to);
    if (anim)
    {
        fill(from, &rc, RGB(255, 0, 0));
        fill(to, &rc, RGB(0, 0, 255));
        EndBufferedAnimation(anim, TRUE);
        pump(200, NULL, NULL, NULL, NULL, NULL);
        render_true = 0;
        CHECK(BufferedPaintStopAllAnimations(hwnd) == S_OK, "stop");
        CHECK(BufferedPaintStopAllAnimations(NULL) == E_INVALIDARG, "stop NULL");
        pump(300, NULL, NULL, NULL, NULL, NULL);
        CHECK(render_true == 0, "frames painted after stopping: %d", render_true);
        CHECK(!BufferedPaintRenderAnimation(hwnd, dc), "render after stopping");
    }
    ReleaseDC(hwnd, dc);

    /* the window goes away while its animation runs: the timer must end with it
     * (a leak was 15 ms timer messages for ever), and the idle process stays idle */
    {
        HWND h2 = CreateWindowA("SgUxAnim", "gone", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, NULL, NULL, wc.hInstance, NULL);
        HDC dc2 = GetDC(h2);
        RECT r2 = { 0, 0, 50, 50 };
        int ticks;
        ULONGLONG cpu;

        count_timers(100);
        ap.dwDuration = 60000;
        anim = BeginBufferedAnimation(h2, dc2, &r2, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to);
        CHECK(anim != NULL, "begin on the doomed window");
        if (anim)
        {
            fill(from, &r2, RGB(255, 0, 0));
            fill(to, &r2, RGB(0, 0, 255));
            EndBufferedAnimation(anim, TRUE);
            ticks = count_timers(200);
            CHECK(ticks >= 3, "the animation timer runs while the window lives: %d", ticks);
            ReleaseDC(h2, dc2);
            DestroyWindow(h2);
            count_timers(100);
            cpu = cpu_ms();
            ticks = count_timers(1500);
            CHECK(ticks <= 2, "animation timer still ticking after the window was destroyed: %d messages in 1.5 s", ticks);
            cpu = cpu_ms() - cpu;
            CHECK(cpu < 150, "busy after the window was destroyed: %I64u ms of CPU in 1.5 s", cpu);
        }
        else { ReleaseDC(h2, dc2); DestroyWindow(h2); }
    }

    /* BufferedPaintUnInit ends the thread's animations */
    {
        HDC dc3 = GetDC(hwnd);
        int ticks;

        BufferedPaintInit();
        ap.dwDuration = 60000;
        anim = BeginBufferedAnimation(hwnd, dc3, &rc, BPBF_COMPATIBLEBITMAP, NULL, &ap, &from, &to);
        CHECK(anim != NULL, "begin before UnInit");
        if (anim)
        {
            fill(from, &rc, RGB(255, 0, 0));
            fill(to, &rc, RGB(0, 0, 255));
            EndBufferedAnimation(anim, TRUE);
            ticks = count_timers(150);
            CHECK(ticks >= 3, "timer runs before UnInit: %d", ticks);
            CHECK(BufferedPaintUnInit() == S_OK, "UnInit");
            count_timers(100);
            ticks = count_timers(500);
            CHECK(ticks <= 1, "animation timer still ticking after BufferedPaintUnInit: %d", ticks);
            CHECK(EndBufferedAnimation(anim, TRUE) == E_INVALIDARG, "animation handle survived UnInit");
        }
        ReleaseDC(hwnd, dc3);
    }

    /* hit testing */
    theme = OpenThemeData(hwnd, L"Window");
    CHECK(theme != NULL, "no Window theme data");
    pt.x = 50; pt.y = 50;
    hr = HitTestThemeBackground(NULL, NULL, 1, 1, 0, &box, NULL, pt, &code);
    CHECK(hr == E_HANDLE, "null theme %#lx", hr);
    if (theme)
    {
        hr = HitTestThemeBackground(theme, NULL, 1, 1, 0, &box, NULL, pt, NULL);
        CHECK(hr == E_POINTER, "null result %#lx", hr);
        hr = HitTestThemeBackground(theme, NULL, 1, 1, 0, NULL, NULL, pt, &code);
        CHECK(hr == E_POINTER, "null rect %#lx", hr);
        code = 0xffff;
        hr = HitTestThemeBackground(theme, NULL, 1, 1, 0, &box, NULL, pt, &code);
        CHECK(hr == S_OK && code == HTCLIENT, "inside %#lx %u", hr, code);
        pt.x = 5;
        hr = HitTestThemeBackground(theme, NULL, 1, 1, 0, &box, NULL, pt, &code);
        CHECK(hr == S_OK && code == HTNOWHERE, "outside %#lx %u", hr, code);
        pt.x = 110;
        hr = HitTestThemeBackground(theme, NULL, 1, 1, 0, &box, NULL, pt, &code);
        CHECK(hr == S_OK && code == HTNOWHERE, "right edge is outside %#lx %u", hr, code);
        rgn = CreateEllipticRgn(10, 10, 110, 90);
        pt.x = 11; pt.y = 11;
        hr = HitTestThemeBackground(theme, NULL, 1, 1, 0, &box, rgn, pt, &code);
        CHECK(hr == S_OK && code == HTNOWHERE, "in the rectangle, outside the region %#lx %u", hr, code);
        pt.x = 60; pt.y = 50;
        hr = HitTestThemeBackground(theme, NULL, 1, 1, 0, &box, rgn, pt, &code);
        CHECK(hr == S_OK && code == HTCLIENT, "inside the region %#lx %u", hr, code);
        DeleteObject(rgn);
        {
            DWORD o = HTTB_RESIZINGBORDER | HTTB_SYSTEMSIZINGMARGINS;
            int mx = GetSystemMetrics(SM_CXSIZEFRAME), my = GetSystemMetrics(SM_CYSIZEFRAME);
            struct { int x, y; WORD want; } t[] = {
                { 10, 50, HTLEFT }, { 109, 50, HTRIGHT }, { 60, 10, HTTOP }, { 60, 89, HTBOTTOM },
                { 10, 10, HTTOPLEFT }, { 109, 10, HTTOPRIGHT }, { 10, 89, HTBOTTOMLEFT }, { 109, 89, HTBOTTOMRIGHT },
                { 60, 50, HTCLIENT }, { 10 + mx, 50, HTCLIENT }, { 60, 10 + my, HTCLIENT },
            };
            unsigned i;
            for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
            {
                pt.x = t[i].x; pt.y = t[i].y;
                hr = HitTestThemeBackground(theme, NULL, 1, 1, o, &box, NULL, pt, &code);
                CHECK(hr == S_OK && code == t[i].want, "sizing border at %d,%d: %#lx %u (want %u)", t[i].x, t[i].y, hr, code, t[i].want);
            }
            pt.x = 10; pt.y = 50;
            hr = HitTestThemeBackground(theme, NULL, 1, 1, HTTB_RESIZINGBORDER_RIGHT | HTTB_SYSTEMSIZINGMARGINS, &box, NULL, pt, &code);
            CHECK(hr == S_OK && code == HTCLIENT, "only the asked-for borders: %u", code);
            pt.x = 60; pt.y = 10;
            hr = HitTestThemeBackground(theme, NULL, 1, 1, HTTB_CAPTION | HTTB_SYSTEMSIZINGMARGINS, &box, NULL, pt, &code);
            CHECK(hr == S_OK && code == HTCAPTION, "caption: %u", code);
        }
        CloseThemeData(theme);
    }

    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
