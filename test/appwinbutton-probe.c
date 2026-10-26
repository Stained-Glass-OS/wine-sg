/* appwinbutton-probe.c (wine-sg 7008): an owned window with WS_EX_APPWINDOW has a taskbar button,
 * as on Windows -- Delphi and Lazarus programs' main forms are owned by a hidden application window.
 *   appwinbutton-probe.exe OUTFILE
 * A hidden owner owns "SgAppWin owned-app" (WS_EX_APPWINDOW: a button) and "SgAppWin owned"
 * (no WS_EX_APPWINDOW: no button); "SgAppWin top" is an ordinary top-level window (a button).
 * After 3 s it writes the taskbar's visible buttons' windows to OUTFILE. */
#include <windows.h>
#include <stdio.h>

static FILE *out;
static HWND owner_wnd, app_wnd, plain_wnd, top_wnd;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static BOOL CALLBACK button(HWND hwnd, LPARAM lp)
{
    HWND target = (HWND)GetWindowLongPtrW(hwnd, GWLP_ID);
    RECT rc;

    if (!IsWindowVisible(hwnd) || !target || !IsWindow(target)) return TRUE;
    GetWindowRect(hwnd, &rc);
    if (rc.right <= rc.left) return TRUE;
    if (target == app_wnd) fprintf(out, "button=owned-app\n");
    else if (target == plain_wnd) fprintf(out, "button=owned\n");
    else if (target == top_wnd) fprintf(out, "button=top\n");
    else if (target == owner_wnd) fprintf(out, "button=owner\n");
    return TRUE;
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = {0};
    MSG msg;
    DWORD start;

    if (argc < 2 || !(out = fopen(argv[1], "w"))) return 2;
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.lpszClassName = L"SgAppWin";
    RegisterClassW(&wc);
    /* the hidden application window, as Delphi's TApplication and Lazarus' LCL make it */
    owner_wnd = CreateWindowW(L"SgAppWin", L"SgAppWin owner", WS_POPUP | WS_CAPTION, 0, 0, 0, 0,
                              NULL, NULL, wc.hInstance, NULL);
    app_wnd = CreateWindowExW(WS_EX_APPWINDOW, L"SgAppWin", L"SgAppWin owned-app", WS_OVERLAPPEDWINDOW,
                              100, 100, 300, 200, owner_wnd, NULL, wc.hInstance, NULL);
    plain_wnd = CreateWindowExW(0, L"SgAppWin", L"SgAppWin owned", WS_OVERLAPPEDWINDOW,
                                450, 100, 300, 200, owner_wnd, NULL, wc.hInstance, NULL);
    top_wnd = CreateWindowW(L"SgAppWin", L"SgAppWin top", WS_OVERLAPPEDWINDOW, 100, 350, 300, 200,
                            NULL, NULL, wc.hInstance, NULL);
    ShowWindow(app_wnd, SW_SHOWNORMAL);
    ShowWindow(plain_wnd, SW_SHOWNORMAL);
    ShowWindow(top_wnd, SW_SHOWNORMAL);
    fprintf(out, "owned=%d\n", GetWindow(app_wnd, GW_OWNER) == owner_wnd && !IsWindowVisible(owner_wnd));

    start = GetTickCount();
    while (GetTickCount() - start < 3000)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(50);
    }
    EnumChildWindows(FindWindowW(L"Shell_TrayWnd", NULL), button, 0);
    fprintf(out, "done=1\n");
    fclose(out);
    return 0;
}
