/* emufs-probe: a full-screen window of an emulated display mode (wine-sg 9556).
 * A window fills the monitor in green; then the mode becomes 640x480
 * (ChangeDisplaySettings CDS_FULLSCREEN, emulated: the monitor keeps its size),
 * the window is made the new screen's size and painted red with a blue middle.
 * test/emufs-gate.sh reads the screen while it shows: the picture must be the
 * mode's shape in the middle of the monitor, black beside it.
 *   emufs-probe.exe [W H HOLD_MS]   (prints READY when the picture is up) */
#include <windows.h>
#include <stdio.h>
static COLORREF fill = RGB(0, 255, 0);
static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_PAINT)
    {
        PAINTSTRUCT ps; RECT r; HBRUSH b; HDC dc = BeginPaint(h, &ps);
        GetClientRect(h, &r);
        b = CreateSolidBrush(fill); FillRect(dc, &r, b); DeleteObject(b);
        if (fill == RGB(255, 0, 0))
        {
            RECT q = {r.right / 4, r.bottom / 4, r.right * 3 / 4, r.bottom * 3 / 4};
            b = CreateSolidBrush(RGB(0, 0, 255)); FillRect(dc, &q, b); DeleteObject(b);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcA(h, m, w, l);
}
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        Sleep(10);
    }
}
int main(int argc, char **argv)
{
    int w = argc > 2 ? atoi(argv[1]) : 640, h = argc > 2 ? atoi(argv[2]) : 480, hold = argc > 3 ? atoi(argv[3]) : 4000;
    WNDCLASSA wc = {0}; DEVMODEA dm = {0}; HWND hw; LONG r; RECT rc;
    wc.lpfnWndProc = wp; wc.lpszClassName = "emufs"; wc.hCursor = LoadCursor(0, IDC_ARROW);
    RegisterClassA(&wc);
    hw = CreateWindowExA(WS_EX_TOPMOST, "emufs", "emufs", WS_POPUP | WS_VISIBLE, 0, 0,
                         GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0, 0, 0, 0);
    SetForegroundWindow(hw);
    pump(800);
    dm.dmSize = sizeof(dm); dm.dmPelsWidth = w; dm.dmPelsHeight = h; dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
    r = ChangeDisplaySettingsExA(NULL, &dm, NULL, CDS_FULLSCREEN, NULL);
    printf("CDS %dx%d %ld screen %dx%d\n", w, h, r, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    fill = RGB(255, 0, 0);
    SetWindowPos(hw, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW);
    InvalidateRect(hw, NULL, TRUE); UpdateWindow(hw);
    pump(500);
    GetWindowRect(hw, &rc);
    printf("READY window %ld,%ld-%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom);
    fflush(stdout);
    pump(hold);
    ChangeDisplaySettingsExA(NULL, NULL, NULL, 0, NULL);
    DestroyWindow(hw);
    return 0;
}
