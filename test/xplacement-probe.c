/* Another process's window placement (patches/sg/1660), run by
 * test/xplacement-gate.sh: the probe starts itself as a child that makes a
 * window, moves it, then maximizes (and later minimizes) it; the parent
 * reads it with GetWindowPlacement. The normal rectangle must be where the
 * window was before it was maximized, and the minimized and maximized
 * positions and WPF_RESTORETOMAXIMIZED the window's own. It was "not fully
 * supported": the current rectangle came back as the normal one and the
 * positions as -1. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int child(void)
{
    HWND hwnd = CreateWindowA("STATIC", "SG Placement Child", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 100, 100,
                              NULL, NULL, NULL, NULL);
    WINDOWPLACEMENT wp = { sizeof(wp) };
    MSG msg;
    DWORD start = GetTickCount();

    SetWindowPos(hwnd, NULL, 123, 77, 333, 222, SWP_NOZORDER);
    wp.showCmd = SW_SHOWMAXIMIZED;
    wp.ptMinPosition.x = 11; wp.ptMinPosition.y = 22;
    wp.ptMaxPosition.x = -1; wp.ptMaxPosition.y = -1;
    SetRect(&wp.rcNormalPosition, 123, 77, 456, 299);
    wp.flags = WPF_SETMINPOSITION;
    SetWindowPlacement(hwnd, &wp);
    while (GetTickCount() - start < 30000)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        if (GetPropA(hwnd, "quit")) break;
        if (GetPropA(hwnd, "minimize")) { RemovePropA(hwnd, "minimize"); ShowWindow(hwnd, SW_MINIMIZE); }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
    }
    return 0;
}

int main(int argc, char **argv)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WINDOWPLACEMENT wp;
    char cmd[MAX_PATH + 16], self[MAX_PATH];
    HWND hwnd = NULL;
    RECT current;
    int i;

    if (argc > 1 && !strcmp(argv[1], "child")) return child();
    GetModuleFileNameA(NULL, self, sizeof(self));
    sprintf(cmd, "\"%s\" child", self);
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    for (i = 0; i < 100 && !(hwnd = FindWindowA(NULL, "SG Placement Child")); i++) Sleep(100);
    for (i = 0; i < 50 && !IsZoomed(hwnd); i++) Sleep(100);
    check(hwnd && IsZoomed(hwnd), "the other process's window is maximized");

    memset(&wp, 0xcc, sizeof(wp));
    wp.length = sizeof(wp);
    check(GetWindowPlacement(hwnd, &wp), "GetWindowPlacement of another process's window");
    GetWindowRect(hwnd, &current);
    printf("  showCmd %u flags %u min (%ld,%ld) max (%ld,%ld) normal (%ld,%ld)-(%ld,%ld), now (%ld,%ld)-(%ld,%ld)\n",
           wp.showCmd, wp.flags, wp.ptMinPosition.x, wp.ptMinPosition.y, wp.ptMaxPosition.x, wp.ptMaxPosition.y,
           wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right, wp.rcNormalPosition.bottom,
           current.left, current.top, current.right, current.bottom);
    check(wp.showCmd == SW_SHOWMAXIMIZED, "showCmd: maximized");
    check(wp.rcNormalPosition.left == 123 && wp.rcNormalPosition.top == 77 &&
          wp.rcNormalPosition.right == 456 && wp.rcNormalPosition.bottom == 299,
          "the normal rectangle is the one before maximizing, not the current one");
    check(wp.ptMinPosition.x == 11 && wp.ptMinPosition.y == 22, "the minimized position the window has");

    SetPropA(hwnd, "minimize", (HANDLE)1);
    PostMessageA(hwnd, WM_NULL, 0, 0);
    for (i = 0; i < 50 && !IsIconic(hwnd); i++) Sleep(100);
    memset(&wp, 0xcc, sizeof(wp));
    wp.length = sizeof(wp);
    check(GetWindowPlacement(hwnd, &wp) && wp.showCmd == SW_SHOWMINIMIZED && (wp.flags & WPF_RESTORETOMAXIMIZED),
          "minimized from maximized: SW_SHOWMINIMIZED and WPF_RESTORETOMAXIMIZED");
    check(wp.rcNormalPosition.left == 123 && wp.rcNormalPosition.right == 456, "... the normal rectangle kept");

    SetPropA(hwnd, "quit", (HANDLE)1);
    PostMessageA(hwnd, WM_NULL, 0, 0);
    WaitForSingleObject(pi.hProcess, 5000);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
