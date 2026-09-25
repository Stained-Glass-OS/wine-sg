/* Dark mode (patches/sg/0160-0162): the gate's hands and eyes.
 *
 *   darkmode-probe window NAME X Y [dark]
 *       a 360x260 top-level window at X,Y titled NAME: a menu bar, a push
 *       button, an edit box and a check box (common controls 6, themed), on
 *       the class background COLOR_WINDOW. With "dark" it asks for a dark
 *       title bar (DwmSetWindowAttribute DWMWA_USE_IMMERSIVE_DARK_MODE).
 *       Whenever the system colours or the theme change it writes what it
 *       sees to C:\dark-NAME.txt: its own GetSysColor(COLOR_WINDOW) and
 *       (COLOR_MENUBAR), the colour scheme uxtheme has, and the push
 *       button's theme text colour.
 *   darkmode-probe mode apps|system light|dark
 *       what Settings does: HKCU\...\Themes\Personalize AppsUseLightTheme /
 *       SystemUsesLightTheme, then WM_SETTINGCHANGE "ImmersiveColorSet".
 *   darkmode-probe state
 *       a fresh process's view: GetSysColor(COLOR_WINDOW), the scheme,
 *       DwmGetWindowAttribute of the "Dark" window.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <stdio.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static WCHAR name[64];
static HWND button;

static void report(void)
{
    WCHAR path[MAX_PATH], theme[MAX_PATH], color[64], size[64];
    COLORREF text = 0;
    HTHEME th;
    FILE *f;

    swprintf(path, MAX_PATH, L"C:\\dark-%ls.txt", name);
    if (!(f = _wfopen(path, L"w"))) return;
    color[0] = 0;
    GetCurrentThemeName(theme, MAX_PATH, color, 64, size, 64);
    if ((th = OpenThemeData(button, L"Button")))
    {
        GetThemeColor(th, BP_PUSHBUTTON, PBS_NORMAL, TMT_TEXTCOLOR, &text);
        CloseThemeData(th);
    }
    fwprintf(f, L"window=%06lx menubar=%06lx scheme=%ls buttontext=%06lx\n",
             (unsigned long)GetSysColor(COLOR_WINDOW), (unsigned long)GetSysColor(COLOR_MENUBAR),
             color, (unsigned long)text);
    fclose(f);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
        /* after the controls have seen it too */
        SetTimer(hwnd, 1, 300, NULL);
        break;
    case WM_TIMER:
        KillTimer(hwnd, 1);
        report();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int do_window(const WCHAR *title, int x, int y, BOOL dark)
{
    WNDCLASSW wc = {0};
    HMENU bar = CreateMenu(), file = CreatePopupMenu();
    HWND hwnd;
    MSG msg;
    BOOL on = TRUE;

    InitCommonControls();
    lstrcpynW(name, title, 64);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SgDarkProbe";
    RegisterClassW(&wc);
    AppendMenuW(file, MF_STRING, 1, L"&Open");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"&File");
    AppendMenuW(bar, MF_STRING, 2, L"&Edit");
    AppendMenuW(bar, MF_STRING, 3, L"&View");
    hwnd = CreateWindowExW(0, L"SgDarkProbe", title, WS_OVERLAPPEDWINDOW, x, y, 360, 260,
                           NULL, bar, wc.hInstance, NULL);
    if (dark) DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &on, sizeof(on));
    /* the controls sit in the right half; the left half is bare background */
    button = CreateWindowW(L"BUTTON", L"Button", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           200, 20, 120, 34, hwnd, (HMENU)10, wc.hInstance, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE,
                    200, 70, 120, 26, hwnd, (HMENU)11, wc.hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Check", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  200, 110, 120, 24, hwnd, (HMENU)12, wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    report();
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

static int do_mode(const WCHAR *which, const WCHAR *mode)
{
    DWORD light = !lstrcmpW(mode, L"light"), res;
    HKEY key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL))
        return 1;
    RegSetValueExW(key, !lstrcmpW(which, L"apps") ? L"AppsUseLightTheme" : L"SystemUsesLightTheme",
                   0, REG_DWORD, (BYTE *)&light, sizeof(light));
    RegCloseKey(key);
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet",
                        SMTO_ABORTIFHUNG, 5000, (DWORD_PTR *)&res);
    printf("mode %ls %ls\n", which, mode);
    return 0;
}

static int do_state(void)
{
    WCHAR theme[MAX_PATH], color[64] = L"", size[64];
    HWND dark = FindWindowW(L"SgDarkProbe", L"Dark");
    BOOL on = FALSE;
    HRESULT hr = E_FAIL;

    GetCurrentThemeName(theme, MAX_PATH, color, 64, size, 64);
    if (dark) hr = DwmGetWindowAttribute(dark, DWMWA_USE_IMMERSIVE_DARK_MODE, &on, sizeof(on));
    printf("window=%06lx scheme=%ls darkattr=%d hr=%#lx\n", (unsigned long)GetSysColor(COLOR_WINDOW),
           color, on, (unsigned long)hr);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    if (argc >= 5 && !lstrcmpW(argv[1], L"window"))
        return do_window(argv[2], _wtoi(argv[3]), _wtoi(argv[4]), argc > 5 && !lstrcmpW(argv[5], L"dark"));
    if (argc >= 4 && !lstrcmpW(argv[1], L"mode")) return do_mode(argv[2], argv[3]);
    if (argc >= 2 && !lstrcmpW(argv[1], L"state")) return do_state();
    fprintf(stderr, "usage: darkmode-probe window NAME X Y [dark] | mode apps|system light|dark | state\n");
    return 2;
}
