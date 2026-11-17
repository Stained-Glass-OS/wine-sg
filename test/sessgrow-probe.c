/* sessgrow-probe.c: a desktop made after the server's shared session mapping grew (wine-sg 7821), for
 * test/sessgrow-gate.sh.
 *   sessgrow-probe.exe [N [HOLD]]   keeps N desktops open (HOLD seconds more at the end) (each takes a shared object), then starts itself again:
 *   sessgrow-probe.exe child  a new process makes a desktop of its own and asks for the screen first thing,
 *                            as a consent prompt does
 * prints Before=CX MONITORS, After=CX MONITORS, Desktops=N */
#include <windows.h>
#include <stdio.h>

static int count;
static BOOL CALLBACK cb(HMONITOR m, HDC dc, LPRECT r, LPARAM lp) { count++; return TRUE; }

int main(int argc, char **argv)
{
    int n, i, made = 0;
    char name[64], cmd[MAX_PATH + 16], exe[MAX_PATH];
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    HDESK d;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "child"))
    {
        sprintf(name, "sessgrow-child-%lu", GetCurrentProcessId());
        d = CreateDesktopA(name, NULL, NULL, 0, GENERIC_ALL, NULL);
        SetThreadDesktop(d);
        count = 0;
        printf("%s=%d", argc > 2 ? argv[2] : "After", GetSystemMetrics(SM_CXSCREEN));
        EnumDisplayMonitors(NULL, NULL, cb, 0);
        printf(" %d\n", count);
        return 0;
    }
    n = argc > 1 ? atoi(argv[1]) : 1200;
    GetModuleFileNameA(NULL, exe, sizeof(exe));
    sprintf(cmd, "\"%s\" child Before", exe);
    if (CreateProcessA(exe, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    { WaitForSingleObject(pi.hProcess, 30000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    for (i = 0; i < n; i++)
    {
        sprintf(name, "sessgrow-%lu-%d", GetCurrentProcessId(), i);
        if (CreateDesktopA(name, NULL, NULL, 0, GENERIC_ALL, NULL)) made++;   /* (kept open) */
    }
    printf("Desktops=%d\n", made);
    sprintf(cmd, "\"%s\" child After", exe);
    if (CreateProcessA(exe, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    { WaitForSingleObject(pi.hProcess, 30000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    if (argc > 2) Sleep(atoi(argv[2]) * 1000);   /* (holding them, for a check from outside) */
    return 0;
}
