/* elevmin-gate.sh's elevated program (0500): a window on the Wine desktop
 * WinSta0\sg-elevated-3, where sg-elevated-run puts an elevated program's.
 * Prints "xwin=<id>", the X window the taskbar names it by (its own if
 * the driver gave it none), and stays for a minute. */
#include <windows.h>
#include <stdio.h>

int wmain(void)
{
    HDESK desk = CreateDesktopW(L"sg-elevated-3", NULL, NULL, 0, GENERIC_ALL, NULL);
    HWND hwnd;
    ULONG_PTR xwin;
    DWORD end;
    MSG msg;

    if (!desk || !SetThreadDesktop(desk)) { printf("desktop failed %lu\n", GetLastError()); return 1; }
    hwnd = CreateWindowExW(0, L"STATIC", L"Elevated Probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           100, 100, 300, 200, NULL, NULL, NULL, NULL);
    if (!hwnd) { printf("window failed %lu\n", GetLastError()); return 1; }
    if (!(xwin = (ULONG_PTR)GetPropW(hwnd, L"__wine_x11_whole_window")))
        SetPropW(hwnd, L"__wine_x11_whole_window", (HANDLE)(xwin = 777));
    printf("xwin=%Iu\n", xwin);
    fflush(stdout);
    for (end = GetTickCount() + 60000; GetTickCount() < end; Sleep(50))
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    return 0;
}
