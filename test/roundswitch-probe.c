/* roundswitch-probe (roundswitch-gate.sh, patches/sg/9067): a flyout (a popup asking
 * DWMWCP_ROUND) shown in the Rounded style, hidden, the style turned off (Classic), resized
 * while hidden and shown again: all of its new size is drawn, its corners square. Reads the
 * screen (the shell desktop's) and writes C:\out.txt. */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_ERASEBKGND) { RECT r; GetClientRect(h, &r); HBRUSH b = CreateSolidBrush(RGB(255,0,0)); FillRect((HDC)w, &r, b); DeleteObject(b); return 1; }
    return DefWindowProcW(h, m, w, l);
}
static void pump(int ms) { DWORD t = GetTickCount(); MSG msg; while (GetTickCount() - t < (DWORD)ms) { while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg); Sleep(20); } }
static void setstyle(DWORD v) { HKEY k; RegCreateKeyW(HKEY_CURRENT_USER, L"Software\\Stained Glass\\Style", &k); RegSetValueExW(k, L"Rounded", 0, REG_DWORD, (BYTE*)&v, 4); RegCloseKey(k); }
static int red(int x, int y) { HDC dc = GetDC(NULL); COLORREF c = GetPixel(dc, x, y); ReleaseDC(NULL, dc); return c == RGB(255,0,0); }
static FILE *o;
int wmain(void)
{
    WNDCLASSW wc = { 0, proc, 0, 0, GetModuleHandleW(0), 0, LoadCursorW(0, (LPCWSTR)IDC_ARROW), 0, 0, L"roundsw" };
    DWORD pref = 2; /* DWMWCP_ROUND */
    o = _wfopen(L"C:\\out.txt", L"w");
    RegisterClassW(&wc);
    setstyle(1); Sleep(600);
    HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"roundsw", L"fly", WS_POPUP, 600, 300, 200, 150, 0, 0, wc.hInstance, 0);
    DwmSetWindowAttribute(h, 33, &pref, sizeof(pref));
    ShowWindow(h, SW_SHOWNOACTIVATE); UpdateWindow(h); pump(500);
    SetWindowPos(h, 0, 600, 300, 201, 150, SWP_NOZORDER | SWP_NOACTIVATE); pump(300);
    SetWindowPos(h, 0, 600, 300, 200, 150, SWP_NOZORDER | SWP_NOACTIVATE); pump(500);
    fwprintf(o, L"rounded: corner %d mid %d\n", red(600, 300), red(700, 370));
    ShowWindow(h, SW_HIDE); pump(200);
    setstyle(0); Sleep(600);
    SetWindowPos(h, 0, 600, 300, 400, 300, SWP_NOZORDER | SWP_NOACTIVATE); pump(200);
    ShowWindow(h, SW_SHOWNOACTIVATE); UpdateWindow(h); pump(800);
    fwprintf(o, L"classic: corner %d oldarea %d newarea %d newcorner %d\n", red(600, 300), red(700, 370), red(950, 550), red(999, 599));
    { HDC dc = GetDC(NULL); fwprintf(o, L"color950,550=%06lx 650,350=%06lx\n", GetPixel(dc, 950, 550), GetPixel(dc, 650, 350)); ReleaseDC(NULL, dc); }
    { RECT r; GetWindowRect(h, &r); fwprintf(o, L"rect %ld %ld %ld %ld vis %d\n", r.left, r.top, r.right, r.bottom, IsWindowVisible(h)); }
    { HRGN r = CreateRectRgn(0,0,0,0); RECT b = {0}; int t = GetWindowRgn(h, r); GetRgnBox(r, &b);
      fwprintf(o, L"winrgn type %d box %ld %ld %ld %ld\n", t, b.left, b.top, b.right, b.bottom);
      HDC dc = GetDC(h); t = GetRandomRgn(dc, r, SYSRGN); GetRgnBox(r, &b); ReleaseDC(h, dc);
      fwprintf(o, L"sysrgn %d box %ld %ld %ld %ld\n", t, b.left, b.top, b.right, b.bottom); DeleteObject(r); }
    InvalidateRect(h, NULL, TRUE); UpdateWindow(h); pump(800);
    fwprintf(o, L"after repaint: newarea %d newcorner %d corner %d\n", red(950, 550), red(999, 599), red(600, 300));
    fclose(o);
    DestroyWindow(h);
    return 0;
}
