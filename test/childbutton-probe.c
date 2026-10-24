/* childbutton-probe.c (wine-sg 6098): a window created top-level and then put into another window
 * (as comdlg32's file dialog puts its custom-controls pane into the dialog) has no taskbar button.
 *   childbutton-probe.exe OUTFILE
 * Shows "SgChildBtn main" and "SgChildBtn other" (top-level: a button each) and makes "SgChildBtn pane"
 * a visible child of main; after 3 s it writes the taskbar's visible buttons' windows to OUTFILE. */
#include <windows.h>
#include <stdio.h>

static FILE *out;
static HWND main_wnd, other_wnd, pane_wnd;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static BOOL CALLBACK button(HWND hwnd, LPARAM lp)
{
    HWND target = (HWND)GetWindowLongPtrW(hwnd, GWLP_ID);
    WCHAR title[64] = L"";
    RECT rc;

    if (!IsWindowVisible(hwnd) || !target || !IsWindow(target)) return TRUE;
    GetWindowRect(hwnd, &rc);
    if (rc.right <= rc.left) return TRUE;
    if (target == main_wnd) fprintf(out, "button=main\n");
    else if (target == other_wnd) fprintf(out, "button=other\n");
    else if (target == pane_wnd) fprintf(out, "button=pane\n");
    else
    {
        GetWindowTextW(target, title, ARRAYSIZE(title));
        fprintf(out, "button=foreign %ls\n", title);
    }
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
    wc.lpszClassName = L"SgChildBtn";
    RegisterClassW(&wc);
    main_wnd = CreateWindowW(L"SgChildBtn", L"SgChildBtn main", WS_OVERLAPPEDWINDOW, 100, 100, 400, 300,
                             NULL, NULL, wc.hInstance, NULL);
    other_wnd = CreateWindowW(L"SgChildBtn", L"SgChildBtn other", WS_OVERLAPPEDWINDOW, 550, 100, 300, 200,
                              NULL, NULL, wc.hInstance, NULL);
    /* as comdlg32's itemdlg.c: created top-level, then a child of the dialog */
    pane_wnd = CreateWindowExW(0, L"SgChildBtn", NULL, WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 0, 0,
                               NULL, 0, wc.hInstance, NULL);
    SetWindowLongW(pane_wnd, GWL_STYLE, WS_TABSTOP);
    ShowWindow(main_wnd, SW_SHOWNORMAL);
    ShowWindow(other_wnd, SW_SHOWNORMAL);
    SetWindowLongW(pane_wnd, GWL_STYLE, (GetWindowLongW(pane_wnd, GWL_STYLE) & ~WS_POPUP) | WS_CHILD);
    SetParent(pane_wnd, main_wnd);
    SetWindowPos(pane_wnd, NULL, 10, 10, 200, 100, SWP_NOZORDER);
    ShowWindow(pane_wnd, SW_SHOW);
    fprintf(out, "pane_child=%d\n", GetAncestor(pane_wnd, GA_PARENT) == main_wnd && IsWindowVisible(pane_wnd));

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
