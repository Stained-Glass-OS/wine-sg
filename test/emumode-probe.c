/* emumode-probe: a program's display mode change in the shell's desktop over
 * the host's monitors (wine-sg 9555), run by test/emumode-gate.sh in a
 * Stained Glass session. Lists the modes, asks for W x H (CDS_FULLSCREEN), and
 * reports the mode the program then finds -- at once and after the display
 * change has gone round (another look at the devices) -- then puts it back.
 *   emumode-probe.exe OUTFILE [W H HOLD_MS] */
#include <windows.h>
#include <stdio.h>
static void report(FILE *f, const char *tag)
{
    DEVMODEA dm = {0};
    MONITORINFO mi = {sizeof(mi)};
    POINT p = {0, 0};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &dm);
    GetMonitorInfoA(MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY), &mi);
    fprintf(f, "%s mode %lux%lu@%lu screen %dx%d monitor %ldx%ld\n", tag, dm.dmPelsWidth, dm.dmPelsHeight,
            dm.dmDisplayFrequency, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
    fflush(f);
}
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(10);
    }
}
int main(int argc, char **argv)
{
    FILE *f = argc > 1 ? fopen(argv[1], "w") : stdout;
    int w = argc > 3 ? atoi(argv[2]) : 640, h = argc > 3 ? atoi(argv[3]) : 480, hold = argc > 4 ? atoi(argv[4]) : 3000;
    DEVMODEA dm; int i, listed = 0, count = 0; LONG r;
    HWND hw = CreateWindowA("STATIC", "emumode", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, 0, 0, 0, 0);

    if (!f) return 1;
    report(f, "START");
    for (i = 0;; i++)
    {
        memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsA(NULL, i, &dm)) break;
        count++;
        if ((int)dm.dmPelsWidth == w && (int)dm.dmPelsHeight == h) listed = 1;
    }
    fprintf(f, "MODES %d listed %d\n", count, listed);
    memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = w; dm.dmPelsHeight = h; dm.dmDisplayFrequency = 60;
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    r = ChangeDisplaySettingsExA(NULL, &dm, NULL, CDS_FULLSCREEN, NULL);
    fprintf(f, "CDS %dx%d@60 %ld\n", w, h, r);
    report(f, "AFTER");
    SetWindowPos(hw, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0);
    pump(hold);
    report(f, "LATER");
    fprintf(f, "HELD\n"); fflush(f);
    pump(hold);
    r = ChangeDisplaySettingsExA(NULL, NULL, NULL, 0, NULL);
    fprintf(f, "RESTORE %ld\n", r);
    pump(500);
    report(f, "RESTORED");
    fprintf(f, "DONE\n");
    fclose(f);
    return 0;
}
