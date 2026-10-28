/* clockcal-probe: a stand-in for sg-shell's calendar (SgCalendarHost): counts the
 * SgCalendarToggle messages the taskbar's clock sends, into C:\clockcal.count */
#include <windows.h>
#include <stdio.h>
static UINT toggle;
static int count;
static void save(void)
{
    FILE *f = fopen("C:\\clockcal.count", "w");
    if (f) { fprintf(f, "toggles=%d\n", count); fclose(f); }
}
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == toggle) { count++; save(); return 0; }
    return DefWindowProcW(h, m, w, l);
}
int main(void)
{
    WNDCLASSW wc = { 0 };
    MSG msg;
    toggle = RegisterWindowMessageW(L"SgCalendarToggle");
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"SgCalendarHost";
    RegisterClassW(&wc);
    CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"Calendar", WS_POPUP, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    save();
    while (GetMessageW(&msg, NULL, 0, 0)) DispatchMessageW(&msg);
    return 0;
}
