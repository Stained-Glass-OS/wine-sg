/* crashconsole-gate.sh's probe.
 *   crashconsole-probe.exe crash   a windowed program that crashes (no console)
 *   crashconsole-probe.exe watch   for 30 s, the top-level windows shown: prints
 *                                  "dialog=1" once the crash box ("Program Error")
 *                                  is up and "console=N" for console windows
 *                                  (conhost's, or any window of conhost.exe) seen */
#include <windows.h>
#include <stdio.h>

static int dialog, console;

static BOOL CALLBACK each(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64], title[128], image[MAX_PATH];
    DWORD pid = 0, n = MAX_PATH;
    HANDLE proc;
    if (!IsWindowVisible(hwnd)) return TRUE;
    GetClassNameW(hwnd, cls, 64);
    GetWindowTextW(hwnd, title, 128);
    GetWindowThreadProcessId(hwnd, &pid);
    image[0] = 0;
    if ((proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)))
    {
        QueryFullProcessImageNameW(proc, 0, image, &n);
        CloseHandle(proc);
    }
    if (!wcscmp(title, L"Program Error")) dialog = 1;
    if (!wcscmp(cls, L"WineConsoleClass") || wcsstr(image, L"conhost.exe"))
    {
        console++;
        printf("console window: class %ls title [%ls] %ls\n", cls, title, image);
    }
    return TRUE;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, WCHAR *cmd, int show)
{
    int i;
    if (wcsstr(cmd, L"crash")) { *(volatile int *)0 = 1; return 0; }
    for (i = 0; i < 60; i++)
    {
        EnumWindows(each, 0);
        Sleep(500);
    }
    printf("dialog=%d console=%d\n", dialog, console);
    fflush(stdout);
    return 0;
}
