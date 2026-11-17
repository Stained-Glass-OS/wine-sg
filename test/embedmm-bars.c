/* embedmm-bars.c: for embedmm-gate.sh (7826) -- the taskbars against the full-screen frames:
 * for each bar (Shell_TrayWnd, Shell_SecondaryTrayWnd): "BAR CLASS L T R B topmost=0|1 below=0|1",
 * below=1 when a visible window covering the bar's display is above it in the z-order; and the
 * foreground window's class and rect. */
#include <windows.h>
#include <stdio.h>

static HWND order[512];
static int count;
static BOOL CALLBACK collect(HWND h, LPARAM lp) { if (count < 512) order[count++] = h; return TRUE; }

static void report(HWND bar, const char *cls)
{
    MONITORINFO mi = { sizeof(mi) };
    RECT r, w;
    int i, below = 0;
    GetWindowRect(bar, &r);
    GetMonitorInfoW(MonitorFromWindow(bar, MONITOR_DEFAULTTONEAREST), &mi);
    for (i = 0; i < count && order[i] != bar; i++)
    {
        if (!IsWindowVisible(order[i]) || !GetWindowRect(order[i], &w)) continue;
        if (w.left <= mi.rcMonitor.left && w.top <= mi.rcMonitor.top && w.right >= mi.rcMonitor.right &&
            w.bottom >= mi.rcMonitor.bottom) below = 1;
    }
    printf("BAR %s %ld %ld %ld %ld topmost=%d below=%d\n", cls, r.left, r.top, r.right, r.bottom,
           !!(GetWindowLongW(bar, GWL_EXSTYLE) & WS_EX_TOPMOST), below);
}

int main(void)
{
    HWND h = NULL, fg = GetForegroundWindow();
    char cls[64] = "";
    RECT r = {0};
    EnumWindows(collect, 0);
    while ((h = FindWindowExA(NULL, h, "Shell_TrayWnd", NULL))) report(h, "Shell_TrayWnd");
    while ((h = FindWindowExA(NULL, h, "Shell_SecondaryTrayWnd", NULL))) report(h, "Shell_SecondaryTrayWnd");
    if (fg) { GetClassNameA(fg, cls, sizeof(cls)); GetWindowRect(fg, &r); }
    printf("FOREGROUND %s %ld %ld %ld %ld\n", cls, r.left, r.top, r.right, r.bottom);
    return 0;
}
