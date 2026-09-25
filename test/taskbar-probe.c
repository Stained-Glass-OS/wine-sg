/* The taskbar honours Settings (patches/sg/0164): what a program can see.
 *
 *   taskbar-probe window NAME X Y     a plain top-level window (this program's)
 *   taskbar-probe apply               WM_SETTINGCHANGE "TraySettings", as Settings sends
 *   taskbar-probe state               the bar, the work area, SHAppBarMessage's view
 *                                     and the bar's visible buttons
 *   taskbar-probe autohide 0|1        SHAppBarMessage(ABM_SETSTATE)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

#ifndef ABM_SETSTATE
#define ABM_SETSTATE 0x0000000a
#endif

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int do_window(const WCHAR *title, int x, int y)
{
    WNDCLASSW wc = {0};
    MSG msg;
    HWND hwnd;

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.lpszClassName = L"SgTaskbarProbe";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"SgTaskbarProbe", title, WS_OVERLAPPEDWINDOW, x, y, 300, 200, NULL, NULL, wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    while (GetMessageW(&msg, NULL, 0, 0)) DispatchMessageW(&msg);
    return 0;
}

static int do_state(void)
{
    APPBARDATA abd = { sizeof(abd) };
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL), child;
    RECT bar = {0}, work, rc;
    UINT state;
    int buttons = 0, windows = 0;

    GetWindowRect(tray, &bar);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    state = SHAppBarMessage(ABM_GETSTATE, &abd);
    SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    printf("bar=%ld,%ld,%ld,%ld topmost=%d\n", bar.left, bar.top, bar.right, bar.bottom,
           (GetWindowLongW(tray, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
    printf("work=%ld,%ld,%ld,%ld\n", work.left, work.top, work.right, work.bottom);
    printf("appbar=%u edge=%u rc=%ld,%ld,%ld,%ld\n", state, abd.uEdge,
           abd.rc.left, abd.rc.top, abd.rc.right, abd.rc.bottom);
    {
        APPBARDATA q = { sizeof(q) };
        q.uEdge = ABE_BOTTOM;
        printf("autohidebar=%d\n", SHAppBarMessage(ABM_GETAUTOHIDEBAR, &q) != 0);
    }
    /* the bar's buttons: Start (id 0), Task View (0x5654), search (0x5345), windows */
    for (child = GetWindow(tray, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        WCHAR cls[32];
        LONG_PTR id = GetWindowLongPtrW(child, GWLP_ID);
        GetClassNameW(child, cls, 32);
        if (lstrcmpiW(cls, L"Button") || !IsWindowVisible(child)) continue;
        GetWindowRect(child, &rc);
        buttons++;
        if (id == 0) printf("start=%ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom);
        else if (id == 0x5654) printf("taskview=%ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom);
        else if (id == 0x5345) printf("search=%ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom);
        else { windows++; printf("button=%ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom); }
    }
    printf("windows=%d\n", windows);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    DWORD_PTR res;

    if (argc >= 5 && !lstrcmpW(argv[1], L"window")) return do_window(argv[2], _wtoi(argv[3]), _wtoi(argv[4]));
    if (argc >= 2 && !lstrcmpW(argv[1], L"apply"))
    {
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"TraySettings", SMTO_ABORTIFHUNG, 5000, &res);
        return 0;
    }
    if (argc >= 2 && !lstrcmpW(argv[1], L"state")) return do_state();
    if (argc >= 3 && !lstrcmpW(argv[1], L"autohide"))
    {
        APPBARDATA abd = { sizeof(abd) };
        abd.lParam = _wtoi(argv[2]) ? ABS_AUTOHIDE | ABS_ALWAYSONTOP : ABS_ALWAYSONTOP;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        return 0;
    }
    fprintf(stderr, "usage: taskbar-probe window NAME X Y | apply | state | autohide 0|1\n");
    return 2;
}
