/* rounded-gate.sh's hands (0483): windows to look at in the Rounded style.
 *   win TITLE X Y W H [plain|rgn|donot|small|max]   show one and wait
 *   resize TITLE W H                                 size it
 *   pref TITLE                                       print its DWMWA_WINDOW_CORNER_PREFERENCE */
#include <windows.h>
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
        w = CreateWindowW(L"STATIC", argv[2], WS_OVERLAPPEDWINDOW, x, y, cx, cy, 0, 0, 0, 0);
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
    if (argc >= 3 && !lstrcmpW(argv[1], L"pref") && (w = FindWindowW(L"STATIC", argv[2])))
    {
        DWORD pref = 99;
        HRESULT hr = DwmGetWindowAttribute(w, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        printf("pref=%lu hr=%#lx\n", pref, hr);
        return 0;
    }
    return 1;
}
