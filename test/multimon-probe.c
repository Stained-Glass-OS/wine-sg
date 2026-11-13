/* multimon-probe: what programs see of a desktop over several monitors (multimon-gate.sh)
 *
 *   multimon-probe.exe OUT   writes to OUT:
 *     monitors N virtual X Y W H
 *     monitor NAME L T R B primary|- HZ
 *     max MONITOR-INDEX L T R B     a window shown maximized on that monitor
 *     ready
 */
#include <windows.h>
#include <stdio.h>

static FILE *out;
static RECT mons[8];
static int nmons;

static BOOL CALLBACK each(HMONITOR m, HDC dc, LPRECT r, LPARAM p)
{
    MONITORINFOEXW mi = { sizeof(mi) };
    DEVMODEW dm = { .dmSize = sizeof(dm) };
    GetMonitorInfoW(m, (MONITORINFO *)&mi);
    EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm);
    fprintf(out, "monitor %ls %ld %ld %ld %ld %s %lu\n", mi.szDevice, mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right, mi.rcMonitor.bottom, (mi.dwFlags & MONITORINFOF_PRIMARY) ? "primary" : "-",
            dm.dmDisplayFrequency);
    if (nmons < 8) mons[nmons++] = mi.rcWork;
    return TRUE;
}

int main(int argc, char **argv)
{
    WNDCLASSA wc = { .lpfnWndProc = DefWindowProcA, .lpszClassName = "multimon" };
    int i;

    if (argc < 2 || !(out = fopen(argv[1], "w"))) return 2;
    fprintf(out, "monitors %d virtual %d %d %d %d\n", GetSystemMetrics(SM_CMONITORS),
            GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
            GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
    EnumDisplayMonitors(NULL, NULL, each, 0);
    wc.hInstance = GetModuleHandleA(NULL);
    RegisterClassA(&wc);
    for (i = 0; i < nmons; i++)
    {
        /* a window on that monitor, maximized: it fills that monitor's work area, not all of them */
        RECT r;
        HWND w = CreateWindowA("multimon", "multimon", WS_OVERLAPPEDWINDOW, mons[i].left + 50, mons[i].top + 50, 300, 200,
                               NULL, NULL, wc.hInstance, NULL);
        ShowWindow(w, SW_MAXIMIZE);
        Sleep(300);
        GetWindowRect(w, &r);
        /* the client area: a maximized window's frame hangs over the work area's edges, as on Windows */
        {
            RECT c; POINT tl, br;
            GetClientRect(w, &c);
            tl.x = c.left; tl.y = c.top; br.x = c.right; br.y = c.bottom;
            ClientToScreen(w, &tl); ClientToScreen(w, &br);
            fprintf(out, "max %d %ld %ld %ld %ld\n", i, tl.x, tl.y, br.x, br.y);
        }
        DestroyWindow(w);
    }
    fprintf(out, "ready\n");
    fclose(out);
    return 0;
}
