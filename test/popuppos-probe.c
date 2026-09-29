/* popuppos-gate.sh's probe (0511): CalculatePopupWindowPosition. Every
 * placement is printed relative to the primary monitor: l t r b are the
 * popup's left and top minus the monitor's, then the monitor's right and
 * bottom minus the popup's right and bottom. */
#include <windows.h>
#include <stdio.h>

#ifndef TPM_WORKAREA
#define TPM_WORKAREA 0x10000
#endif

typedef BOOL (WINAPI *calc_fn)(const POINT *, const SIZE *, UINT, RECT *, RECT *);
static calc_fn calc;
static RECT mon;

static void place(const char *name, LONG ax, LONG ay, LONG cx, LONG cy, UINT flags, const RECT *ex)
{
    POINT pt = { ax, ay };
    SIZE sz = { cx, cy };
    RECT exc, out = { -1, -1, -1, -1 };
    BOOL ok;

    if (ex) exc = *ex;
    ok = calc(&pt, &sz, flags, ex ? &exc : NULL, &out);
    printf("%s %d %ld %ld %ld %ld\n", name, ok, out.left - mon.left, out.top - mon.top,
           mon.right - out.right, mon.bottom - out.bottom);
}

int main(void)
{
    MONITORINFO mi = { sizeof(mi) };
    POINT origin = { 0, 0 };
    SIZE sz = { 10, 10 };
    RECT out, ex;
    LONG L, T, R, B;
    BOOL ok;

    calc = (calc_fn)GetProcAddress(GetModuleHandleA("user32.dll"), "CalculatePopupWindowPosition");
    if (!calc) { printf("export 0\n"); return 0; }
    printf("export 1\n");
    GetMonitorInfoA(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &mi);
    mon = mi.rcMonitor;
    L = mon.left; T = mon.top; R = mon.right; B = mon.bottom;

    place("plain", L + 100, T + 100, 50, 40, 0, NULL);
    place("rightbottom", L + 100, T + 100, 50, 40, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, NULL);
    place("center", L + 200, T + 200, 50, 40, TPM_CENTERALIGN | TPM_VCENTERALIGN, NULL);
    place("flipright", R - 10, T + 100, 50, 40, 0, NULL);
    place("flipbottom", L + 100, B - 10, 50, 40, 0, NULL);
    place("clamp", R - 10, B - 10, 5000, 40, 0, NULL);
    /* a menu under a button, and above it when there is no room below */
    SetRect(&ex, L + 100, T + 100, L + 180, T + 120);
    place("below", L + 100, T + 120, 50, 40, TPM_VERTICAL, &ex);
    SetRect(&ex, L + 100, B - 30, L + 180, B - 10);
    place("above", L + 100, B - 10, 50, 40, TPM_VERTICAL, &ex);
    /* a submenu beside its item, and on the left when there is no room */
    SetRect(&ex, L + 100, T + 100, L + 180, T + 120);
    place("beside", L + 100, T + 100, 50, 40, 0, &ex);
    SetRect(&ex, R - 90, T + 100, R - 10, T + 120);
    place("leftside", R - 90, T + 100, 50, 40, 0, &ex);
    /* an exclude rectangle the popup misses keeps it where it is */
    SetRect(&ex, L + 400, T + 400, L + 480, T + 420);
    place("clear", L + 100, T + 100, 50, 40, 0, &ex);
    place("rtl", L + 100, T + 100, 50, 40, TPM_LAYOUTRTL, NULL);

    SetLastError(0);
    ok = calc(NULL, &sz, 0, NULL, &out);
    printf("nullanchor %d %lu\n", ok, GetLastError());
    return 0;
}
