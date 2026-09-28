/* btnmenu-probe button|menu|count: where Notepad's taskbar button is (its
 * centre, screen coordinates), whether a popup menu is on screen, and how
 * many Notepad windows there are. (patches/sg/0473) */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HWND notepad;
static int count;

static BOOL CALLBACK find_menu(HWND hwnd, LPARAM lp)
{
    WCHAR cls[16];
    if (IsWindowVisible(hwnd) && GetClassNameW(hwnd, cls, 16) && !lstrcmpW(cls, L"#32768")) *(BOOL *)lp = TRUE;
    return TRUE;
}

static BOOL CALLBACK find_button(HWND hwnd, LPARAM lp)
{
    if (IsWindowVisible(hwnd) && (HWND)GetWindowLongPtrW(hwnd, GWLP_ID) == notepad) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK count_notepads(HWND hwnd, LPARAM lp)
{
    WCHAR cls[16];
    (void)lp;
    if (IsWindowVisible(hwnd) && GetClassNameW(hwnd, cls, 16) && !lstrcmpW(cls, L"Notepad")) count++;
    return TRUE;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "menu"))
    {
        BOOL shown = FALSE;
        EnumWindows(find_menu, (LPARAM)&shown);
        printf("menu %s\n", shown ? "shown" : "none");
    }
    else if (argc > 1 && !strcmp(argv[1], "button"))
    {
        HWND tray = FindWindowW(L"Shell_TrayWnd", NULL), button = NULL;
        RECT rc;
        notepad = FindWindowW(L"Notepad", NULL);
        if (tray && notepad) EnumChildWindows(tray, find_button, (LPARAM)&button);
        if (button && GetWindowRect(button, &rc)) printf("button %ld %ld\n", (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2);
        else printf("button none\n");
    }
    else
    {
        EnumWindows(count_notepads, 0);
        printf("notepads %d\n", count);
    }
    return 0;
}
