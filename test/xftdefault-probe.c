/* xftdefault-gate.sh's probe: a window's DC selects fonts many times, as
 * File Explorer drawing a folder's items does */
#include <windows.h>
int main(void)
{
    HWND w = CreateWindowExW(0, L"STATIC", L"xft", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    int i;
    for (i = 0; i < 300; i++)
    {
        HDC dc = GetDC(w);
        HFONT f = CreateFontW(-12 - (i % 5), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Tahoma");
        HGDIOBJ old = SelectObject(dc, f);
        TextOutW(dc, 2, 2, L"x", 1);
        SelectObject(dc, old);
        DeleteObject(f);
        ReleaseDC(w, dc);
    }
    DestroyWindow(w);
    return 0;
}
