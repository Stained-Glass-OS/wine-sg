/* test/htaapp-gate.sh: reads the styles of the HTML application's window (wine-sg 9608) */
#include <windows.h>
#include <stdio.h>
int main(void)
{
    HWND hwnd = NULL;
    LONG style, exstyle;
    int i;
    for (i = 0; i < 100 && !hwnd; i++) { hwnd = FindWindowW(NULL, L"htatest"); if (!hwnd) Sleep(100); }
    if (!hwnd) { printf("nowindow\n"); return 1; }
    style = GetWindowLongW(hwnd, GWL_STYLE);
    exstyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    printf("maxbox=%d minbox=%d thick=%d caption=%d sysmenu=%d dlgframe=%d\n", !!(style & WS_MAXIMIZEBOX),
           !!(style & WS_MINIMIZEBOX), !!(style & WS_THICKFRAME), (style & WS_CAPTION) == WS_CAPTION,
           !!(style & WS_SYSMENU), !!(exstyle & WS_EX_DLGMODALFRAME));
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return 0;
}
