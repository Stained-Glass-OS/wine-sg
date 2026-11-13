/* multimon-probe: what programs see of a desktop over several monitors (multimon-gate.sh)
 *
 *   multimon-probe.exe OUT   writes to OUT:
 *     monitors N virtual X Y W H
 *     monitor NAME L T R B primary|- HZ
 *     max MONITOR-INDEX L T R B     a window shown maximized on that monitor
 *     ready
 *   multimon-probe.exe OUT TRIGGER   then, once the file TRIGGER is there (the main display
 *     changed while it ran), writes to OUT.after:
 *     monitor ... as above, as this running program now sees them
 *     ready
 *   and shows a window "mmafter" at 10,10 for a few seconds; a window "mmbefore", also at
 *   10,10, is there from before the change
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
    if (argc >= 3)
    {
        char after[MAX_PATH];
        HWND w;
        MSG msg;
        DWORD start = GetTickCount();
        HWND before = CreateWindowA("multimon", "mmbefore", WS_POPUP | WS_VISIBLE, 10, 10, 150, 80, NULL, NULL,
                                    wc.hInstance, NULL);
        /* a running program, its message loop going, while the displays change */
        while (GetFileAttributesA(argv[2]) == INVALID_FILE_ATTRIBUTES && GetTickCount() - start < 90000)
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
            Sleep(100);
        }
        start = GetTickCount();
        while (GetTickCount() - start < 3000)
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
            Sleep(50);
        }
        snprintf(after, sizeof(after), "%s.after", argv[1]);
        if (!(out = fopen(after, "w"))) return 2;
        nmons = 0;
        EnumDisplayMonitors(NULL, NULL, each, 0);
        w = CreateWindowA("multimon", "mmafter", WS_POPUP | WS_VISIBLE, 10, 10, 200, 100, NULL, NULL, wc.hInstance, NULL);
        start = GetTickCount();
        while (GetTickCount() - start < 1500)
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
            Sleep(50);
        }
        fprintf(out, "ready\n");
        fclose(out);
        start = GetTickCount();
        while (GetTickCount() - start < 4000)
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
            Sleep(50);
        }
        DestroyWindow(w);
        DestroyWindow(before);
    }
    return 0;
}
