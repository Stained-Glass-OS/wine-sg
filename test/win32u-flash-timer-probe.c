/* Probe for patches/sg/5215: FlashWindowEx flashes on its own timer.  uCount flashes (each an
 * inversion of the caption, WM_NCACTIVATE, and its return) at the dwTimeout rate, then stops
 * with the caption as it was; FLASHW_TIMER goes on until FLASHW_STOP; FLASHW_TIMERNOFG until the
 * window is in front; a single flash (FlashWindow) stays the one inversion.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int ncact_count, ncact_last = -1;
static int ncact_log[64];

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCACTIVATE)
    {
        if (ncact_count < 64) ncact_log[ncact_count] = wp != 0;
        ncact_count++;
        ncact_last = wp != 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
}

static void reset(void) { ncact_count = 0; ncact_last = -1; }

int main(void)
{
    WNDCLASSA wc = {0};
    HWND hwnd, other;
    FLASHWINFO fi = {sizeof(fi)};
    BOOL ret;
    int i, alternates;

    setvbuf(stdout, NULL, _IONBF, 0);
    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "SgFlash";
    RegisterClassA(&wc);
    other = CreateWindowA("SgFlash", "other", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    hwnd = CreateWindowA("SgFlash", "flash", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    SetForegroundWindow(other);
    pump(300);
    check(GetForegroundWindow() == other, "the other window is in front");

    /* three flashes at 100 ms: six inversions, alternating, then it is over */
    reset();
    fi.hwnd = hwnd; fi.dwFlags = FLASHW_CAPTION; fi.uCount = 3; fi.dwTimeout = 100;
    ret = FlashWindowEx(&fi);
    check(ncact_count >= 1, "the first inversion is at once (%d)", ncact_count);
    pump(1500);
    alternates = 1;
    for (i = 1; i < ncact_count && i < 64; i++) if (ncact_log[i] == ncact_log[i - 1]) alternates = 0;
    check(ncact_count == 6 && alternates, "3 flashes: %d NCACTIVATE messages, alternating %d", ncact_count, alternates);
    check(ncact_last == 0, "ends as the caption was (inactive): last %d", ncact_last);
    (void)ret;

    /* nothing more afterwards */
    i = ncact_count;
    pump(500);
    check(ncact_count == i, "stopped: %d more messages", ncact_count - i);

    /* FLASHW_TIMER: goes on until FLASHW_STOP, which restores */
    reset();
    fi.dwFlags = FLASHW_CAPTION | FLASHW_TIMER; fi.uCount = 2; fi.dwTimeout = 100;
    FlashWindowEx(&fi);
    pump(1500);
    check(ncact_count >= 12, "FLASHW_TIMER keeps flashing past its count: %d messages", ncact_count);
    fi.dwFlags = FLASHW_STOP;
    FlashWindowEx(&fi);
    pump(100);
    i = ncact_count;
    check(ncact_last == 0, "FLASHW_STOP restores the caption: last %d", ncact_last);
    pump(600);
    check(ncact_count == i, "FLASHW_STOP stops it: %d more", ncact_count - i);

    /* FLASHW_TIMERNOFG: until the window is in front */
    reset();
    fi.dwFlags = FLASHW_CAPTION | FLASHW_TIMERNOFG; fi.uCount = 0; fi.dwTimeout = 100;
    FlashWindowEx(&fi);
    pump(700);
    check(ncact_count >= 5, "TIMERNOFG flashing: %d messages", ncact_count);
    SetForegroundWindow(hwnd);
    pump(500);
    i = ncact_count;
    pump(600);
    check(GetForegroundWindow() == hwnd && ncact_count == i, "stops when in front: %d more", ncact_count - i);
    check(ncact_last == 1, "and the caption is active: last %d", ncact_last);

    /* a single flash is the one inversion, no timer */
    SetForegroundWindow(other);
    pump(300);
    reset();
    fi.dwFlags = FLASHW_ALL; fi.uCount = 1; fi.dwTimeout = 100;
    FlashWindowEx(&fi);
    pump(700);
    check(ncact_count == 1, "uCount 1: one inversion only (%d)", ncact_count);
    fi.dwFlags = FLASHW_STOP;
    FlashWindowEx(&fi);

    /* a new call replaces the running one; destroying the window ends it */
    reset();
    fi.dwFlags = FLASHW_CAPTION | FLASHW_TIMER; fi.uCount = 0; fi.dwTimeout = 100;
    FlashWindowEx(&fi);
    pump(300);
    DestroyWindow(hwnd);
    pump(500);
    check(IsWindow(hwnd) == FALSE, "destroyed while flashing");

    DestroyWindow(other);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
