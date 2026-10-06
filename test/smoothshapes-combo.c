/* smoothshapes-combo: a window with two drop-down lists (user32's own combo
 * boxes, no common-controls 6 manifest), for test/smoothshapes-gate.sh. */
#include <windows.h>
static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l) { if (m == WM_DESTROY) PostQuitMessage(0); return DefWindowProcW(h, m, w, l); }
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE p, LPWSTR c, int n)
{
    WNDCLASSW wc = { 0 }; HWND w, cb; MSG msg; int i;
    wc.lpfnWndProc = wp; wc.hInstance = hi; wc.lpszClassName = L"P"; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    w = CreateWindowW(L"P", L"Combo probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 300, 200, 0, 0, hi, 0);
    for (i = 0; i < 2; i++) {
        cb = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 20, 20 + i * 50, 200, 200, w, (HMENU)(INT_PTR)(10 + i), hi, 0);
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)L"One"); SendMessageW(cb, CB_SETCURSEL, 0, 0);
    }
    while (GetMessageW(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
