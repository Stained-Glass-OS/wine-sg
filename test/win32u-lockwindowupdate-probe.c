/* Probe for patches/sg/5050: while LockWindowUpdate holds a window, DCs of it
 * and its children draw nothing (GetPixel gives CLR_INVALID) unless they were
 * taken with DCX_LOCKWINDOWUPDATE; drawing meanwhile is lost; after the
 * unlock the DC works again (user32 win tests, run on Windows). One window at
 * a time can be locked. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#ifndef DCX_USESTYLE
#define DCX_USESTYLE 0x00010000
#endif
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void pump(void)
{
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
}

int main(void)
{
    HWND parent = CreateWindowA("static", "sg", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    HWND child = CreateWindowA("static", 0, WS_CHILD | WS_VISIBLE, 0, 0, 40, 40, parent, 0, 0, 0);
    HWND other = CreateWindowA("static", "other", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 300, 0, 100, 100, 0, 0, 0, 0);
    struct { const char *what; HWND lock, draw; BOOL allow; BOOL valid; } tests[] =
    {
        {"child locked, child drawn", child, child, FALSE, FALSE},
        {"child locked, child drawn with DCX_LOCKWINDOWUPDATE", child, child, TRUE, TRUE},
        {"child locked, parent drawn", child, parent, FALSE, TRUE},
        {"parent locked, child drawn", parent, child, FALSE, FALSE},
        {"parent locked, parent drawn", parent, parent, FALSE, FALSE},
        {"parent locked, parent drawn with DCX_LOCKWINDOWUPDATE", parent, parent, TRUE, TRUE},
    };
    const COLORREF c1 = 0x111100, c2 = 0x222200;
    POINT p = {10, 10};
    unsigned int i;

    UpdateWindow(parent);
    pump();
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        HDC hdc = GetDCEx(tests[i].draw, 0, DCX_USESTYLE | (tests[i].allow ? DCX_LOCKWINDOWUPDATE : 0));
        COLORREF c;
        BOOL ret;

        SetPixel(hdc, p.x, p.y, c1);
        ret = LockWindowUpdate(tests[i].lock);
        check(ret, "%s: locked", tests[i].what);
        c = GetPixel(hdc, p.x, p.y);
        check(c == (tests[i].valid ? c1 : CLR_INVALID), "%s: while locked %08lx", tests[i].what, c);
        SetPixel(hdc, p.x, p.y, c2);
        if (!i) check(!LockWindowUpdate(other), "a second window cannot be locked");
        LockWindowUpdate(0);
        c = GetPixel(hdc, p.x, p.y);
        check(c == (tests[i].valid ? c2 : c1), "%s: after the unlock %08lx", tests[i].what, c);
        ReleaseDC(tests[i].draw, hdc);
    }
    DestroyWindow(other);
    DestroyWindow(parent);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
