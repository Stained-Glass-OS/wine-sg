/* progman-gate.sh's probe: the desktop's windows as programs find them */
#include <windows.h>
#include <stdio.h>
int wmain(void)
{
    HWND pm = FindWindowW(L"Progman", NULL), view = NULL, list = NULL;
    WCHAR title[64] = L"";
    RECT r = { 0 };
    if (pm) { view = FindWindowExW(pm, NULL, L"SHELLDLL_DefView", NULL); GetWindowTextW(pm, title, 64); GetWindowRect(pm, &r); }
    if (view) list = FindWindowExW(view, NULL, L"SysListView32", NULL);
    printf("PROGMAN %s %ls\n", pm ? "yes" : "no", title);
    printf("CHAIN %s %s\n", view ? "view" : "-", list ? "list" : "-");
    printf("VISIBLE %d SIZE %ldx%ld\n", pm ? IsWindowVisible(pm) : 0, r.right - r.left, r.bottom - r.top);
    printf("WORKERW %ld\n", pm ? (long)SendMessageTimeoutW(pm, 0x052c, 0, 0, SMTO_NORMAL, 2000, NULL) : -1);
    return 0;
}
