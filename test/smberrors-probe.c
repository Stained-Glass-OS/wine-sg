/* smberrors-probe: for smberrors-gate.sh.
 *   smberrors-probe.exe cmdbar  File Explorer's command bar, in screen
 *                               coordinates: "CMDBAR left top right bottom"
 *   smberrors-probe.exe tips    the visible tooltips ("TIP l t r b") and
 *                               menus ("MENU l t r b") on the desktop */
#include <windows.h>
#include <stdio.h>

static BOOL CALLBACK find_cmdbar(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    GetClassNameW(hwnd, cls, 64);
    if (!lstrcmpW(cls, L"SGExplorerCommandBar") && IsWindowVisible(hwnd)) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK top(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    RECT r;
    (void)lp;
    if (!IsWindowVisible(hwnd)) return TRUE;
    GetClassNameW(hwnd, cls, 64);
    GetWindowRect(hwnd, &r);
    if (!lstrcmpiW(cls, L"tooltips_class32")) printf("TIP %ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom);
    if (!lstrcmpW(cls, L"#32768")) printf("MENU %ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom);
    if (lp) EnumChildWindows(hwnd, find_cmdbar, lp);
    return TRUE;
}

int wmain(int argc, WCHAR **argv)
{
    if (argc > 1 && !lstrcmpW(argv[1], L"cmdbar"))
    {
        HWND bar = NULL;
        RECT r;
        int i;
        for (i = 0; i < 50 && !bar; i++)
        {
            EnumWindows(top, (LPARAM)&bar);
            if (!bar) Sleep(200);
        }
        if (!bar) { printf("NOCMDBAR\n"); return 1; }
        GetWindowRect(bar, &r);
        printf("CMDBAR %ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom);
        return 0;
    }
    EnumWindows(top, 0);
    return 0;
}
