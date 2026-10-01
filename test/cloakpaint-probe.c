/* A window cloaked from its creation is still painted (patches/sg/0629), as
 * Brave relies on: it cloaks its new windows in WM_NCCREATE (against a white
 * flash) and uncloaks them in WM_NCPAINT. Shows such a window, paints it
 * green, and prints after a second:
 *   cloaked=<hr of cloaking in WM_NCCREATE> ncpaint=<n> paint=<n>
 *   uncloak=<hr of uncloaking in WM_NCPAINT> now=<DWMWA_CLOAKED>
 * then stays up for the gate's screenshot until the "done" file appears in
 * the folder given. */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>

static int ncpaint, paint;
static HRESULT cloak_hr = E_FAIL, uncloak_hr = E_FAIL;
static BOOL cloaked;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_NCCREATE:
    {
        BOOL cloak = TRUE;
        cloaked = SUCCEEDED(cloak_hr = DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak)));
        break;
    }
    case WM_NCCALCSIZE:
        if (wp) return 0;   /* all of it is client area, as Chromium's windows */
        break;
    case WM_NCPAINT:
        ncpaint++;
        if (cloaked)
        {
            BOOL cloak = FALSE;
            cloaked = !SUCCEEDED(uncloak_hr = DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak)));
        }
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        HBRUSH green = CreateSolidBrush(RGB(0, 255, 0));
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, green);
        DeleteObject(green);
        EndPaint(hwnd, &ps);
        paint++;
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = {0};
    char done[MAX_PATH];
    DWORD now = 9, start;
    MSG msg;
    HWND hwnd;

    snprintf(done, sizeof(done), "%s\\done", argc > 1 ? argv[1] : ".");
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"CloakPaint";
    RegisterClassW(&wc);
    hwnd = CreateWindowExW(0, L"CloakPaint", L"cloakpaint", WS_OVERLAPPEDWINDOW, 200, 150, 400, 300,
                           NULL, NULL, wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    for (start = GetTickCount(); GetTickCount() - start < 1000; Sleep(10))
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &now, sizeof(now));
    printf("cloaked=%08lx ncpaint=%d paint=%d uncloak=%08lx now=%lu\n", cloak_hr, ncpaint, paint, uncloak_hr, now);
    fflush(stdout);
    for (start = GetTickCount(); GetFileAttributesA(done) == INVALID_FILE_ATTRIBUTES && GetTickCount() - start < 60000;
         Sleep(20))
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    return 0;
}
