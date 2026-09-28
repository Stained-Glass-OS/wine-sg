/* traymenu-probe menu|notepad: whether a popup menu is on screen, and
 * whether Notepad's window is minimized */
#include <windows.h>
#include <stdio.h>

static BOOL CALLBACK find_menu(HWND hwnd, LPARAM lp)
{
    WCHAR cls[16];
    if (IsWindowVisible(hwnd) && GetClassNameW(hwnd, cls, 16) && !lstrcmpW(cls, L"#32768")) *(BOOL *)lp = TRUE;
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
    else
    {
        HWND np = FindWindowW(L"Notepad", NULL);
        printf("notepad %s\n", !np ? "none" : IsIconic(np) ? "minimized" : "shown");
    }
    return 0;
}
