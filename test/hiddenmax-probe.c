/* hiddenmax-gate.sh's probe (patches/sg/1300): WM_SYSCOMMAND SC_MAXIMIZE to
 * a window that is hidden and already maximized (created with WS_MAXIMIZE,
 * as Chromium's and Edge's browser windows are, Widget::Init's Maximize())
 * changes nothing: the window stays hidden and is not activated until the
 * program shows it. Edge 154 crashed in its WM_ACTIVATE handler when Wine
 * showed and activated its window there, before its first tab existed.
 * Prints one line per check: "NAME VALUE". */
#include <windows.h>
#include <stdio.h>

static int activations;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ACTIVATE && LOWORD(wp) != WA_INACTIVE) activations++;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void pump(void)
{
    MSG m;
    DWORD end = GetTickCount() + 300;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&m, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(10);
    }
}

int main(void)
{
    WNDCLASSW wc = {0};
    HWND first, hidden, normal;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"sg_hiddenmax";
    RegisterClassW(&wc);

    first = CreateWindowW(L"sg_hiddenmax", L"first", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 300, 200, 0, 0, 0, 0);
    SetForegroundWindow(first);
    SetActiveWindow(first);
    pump();
    printf("first-active %d\n", GetActiveWindow() == first);

    /* the browser window: created maximized, not yet shown */
    hidden = CreateWindowW(L"sg_hiddenmax", L"hidden", WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, 10, 10, 400, 300, 0, 0, 0, 0);
    activations = 0;
    SendMessageW(hidden, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    pump();
    printf("hidden-visible %d\n", IsWindowVisible(hidden) ? 1 : 0);
    printf("hidden-activations %d\n", activations);
    printf("hidden-active %d\n", GetActiveWindow() == hidden);
    printf("hidden-zoomed %d\n", IsZoomed(hidden) ? 1 : 0);

    /* and when the program shows it, it is shown maximized and active */
    ShowWindow(hidden, SW_SHOWMAXIMIZED);
    pump();
    printf("shown-visible %d\n", IsWindowVisible(hidden) ? 1 : 0);
    printf("shown-active %d\n", GetActiveWindow() == hidden);
    printf("shown-zoomed %d\n", IsZoomed(hidden) ? 1 : 0);
    printf("shown-activations %d\n", activations);

    /* a visible window still maximizes on SC_MAXIMIZE */
    normal = CreateWindowW(L"sg_hiddenmax", L"normal", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 60, 60, 300, 200, 0, 0, 0, 0);
    pump();
    SendMessageW(normal, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    pump();
    printf("visible-zoomed %d\n", IsZoomed(normal) ? 1 : 0);
    fflush(stdout);
    DestroyWindow(normal);
    DestroyWindow(hidden);
    DestroyWindow(first);
    return 0;
}
