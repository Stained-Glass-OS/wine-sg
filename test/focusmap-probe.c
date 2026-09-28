/* focusmap-probe: a top-level window given the focus before it is shown, as
 * Firefox does (patches/sg/0449). What is typed goes into its title:
 * "typed:<keys>". For test/focusmap-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CHAR && w >= ' ')
    {
        WCHAR t[128];
        int n = GetWindowTextW(h, t, 120);
        t[n] = (WCHAR)w; t[n + 1] = 0;
        SetWindowTextW(h, t);
    }
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(h, m, w, l);
}

int main(void)
{
    WNDCLASSW wc = { 0 };
    MSG msg;
    HWND h;

    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"SgFocusMap";
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    RegisterClassW(&wc);
    h = CreateWindowW(L"SgFocusMap", L"typed:", WS_OVERLAPPEDWINDOW, 100, 100, 500, 200, NULL, NULL, NULL, NULL);
    /* the focus first, then the window on the screen */
    SetForegroundWindow(h);
    SetFocus(h);
    ShowWindow(h, SW_SHOW);
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
