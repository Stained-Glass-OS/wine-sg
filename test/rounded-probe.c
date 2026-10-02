/* rounded-gate.sh's hands (0483): windows to look at in the Rounded style.
 *   win TITLE X Y W H [plain|rgn|donot|small|max|popup|popupround]   show one and wait
 *   resize TITLE W H                                 size it
 *   pref TITLE                                       print its DWMWA_WINDOW_CORNER_PREFERENCE
 *   rgn TITLE                                        print what GetWindowRgn says of it
 *   menu X Y                                         a popup menu there, until killed
 *   tip X Y                                          a tooltip window there, until killed
 *   minimize TITLE                                   minimize it (0751: where it goes) */
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, WCHAR **argv)
{
    HWND w;
    MSG msg;
    if (argc >= 7 && !lstrcmpW(argv[1], L"win"))
    {
        int x = _wtoi(argv[3]), y = _wtoi(argv[4]), cx = _wtoi(argv[5]), cy = _wtoi(argv[6]);
        const WCHAR *how = argc > 7 ? argv[7] : L"plain";
        if (!lstrcmpW(how, L"popup") || !lstrcmpW(how, L"popupround"))
            w = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", argv[2], WS_POPUP | WS_BORDER, x, y, cx, cy, 0, 0, 0, 0);
        else
            w = CreateWindowW(L"STATIC", argv[2], WS_OVERLAPPEDWINDOW, x, y, cx, cy, 0, 0, 0, 0);
        if (!lstrcmpW(how, L"popupround"))
        {
            DWORD pref = DWMWCP_ROUND;
            DwmSetWindowAttribute(w, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        }
        if (!lstrcmpW(how, L"rgn")) SetWindowRgn(w, CreateRectRgn(0, 0, cx, cy), FALSE);
        if (!lstrcmpW(how, L"donot") || !lstrcmpW(how, L"small"))
        {
            DWORD pref = !lstrcmpW(how, L"donot") ? DWMWCP_DONOTROUND : DWMWCP_ROUNDSMALL;
            DwmSetWindowAttribute(w, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        }
        ShowWindow(w, !lstrcmpW(how, L"max") ? SW_SHOWMAXIMIZED : SW_SHOW);
        UpdateWindow(w);
        while (GetMessageW(&msg, 0, 0, 0)) DispatchMessageW(&msg);
        return 0;
    }
    if (argc >= 5 && !lstrcmpW(argv[1], L"resize") && (w = FindWindowW(L"STATIC", argv[2])))
    {
        SetWindowPos(w, 0, 0, 0, _wtoi(argv[3]), _wtoi(argv[4]), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    if (argc >= 3 && !lstrcmpW(argv[1], L"minimize") && (w = FindWindowW(L"STATIC", argv[2])))
    {
        ShowWindow(w, SW_MINIMIZE);
        return 0;
    }
    if (argc >= 3 && !lstrcmpW(argv[1], L"pref") && (w = FindWindowW(L"STATIC", argv[2])))
    {
        DWORD pref = 99;
        HRESULT hr = DwmGetWindowAttribute(w, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        printf("pref=%lu hr=%#lx\n", pref, hr);
        return 0;
    }
    if (argc >= 3 && !lstrcmpW(argv[1], L"rgn") && (w = FindWindowW(L"STATIC", argv[2])))
    {
        HRGN r = CreateRectRgn(0, 0, 0, 0);
        printf("rgn=%d\n", GetWindowRgn(w, r));
        return 0;
    }
    if (argc >= 4 && !lstrcmpW(argv[1], L"menu"))
    {
        HMENU m = CreatePopupMenu();
        int i;
        w = CreateWindowW(L"STATIC", L"MenuOwner", WS_POPUP, 0, 0, 1, 1, 0, 0, 0, 0);
        for (i = 0; i < 6; i++) AppendMenuW(m, MF_STRING, 100 + i, L"A menu item of some width");
        SetForegroundWindow(w);
        TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN, _wtoi(argv[2]), _wtoi(argv[3]), 0, w, NULL);
        return 0;
    }
    if (argc >= 4 && !lstrcmpW(argv[1], L"tip"))
    {
        InitCommonControls();
        w = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, L"A tooltip", WS_POPUP, _wtoi(argv[2]), _wtoi(argv[3]),
                            160, 40, 0, 0, 0, 0);
        ShowWindow(w, SW_SHOWNOACTIVATE);
        SetWindowPos(w, HWND_TOPMOST, _wtoi(argv[2]), _wtoi(argv[3]), 160, 40, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        while (GetMessageW(&msg, 0, 0, 0)) DispatchMessageW(&msg);
        return 0;
    }
    return 1;
}
