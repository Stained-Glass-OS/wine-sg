/* Probe and stand-in for test/a11y-gate.sh (patches/sg/0181, 0182).
 *
 *   probe run CMDLINE        CreateProcess(NULL, CMDLINE), wait for the launcher
 *   probe appbar EDGE SIZE   register an appbar on EDGE (top|left) SIZE pixels
 *                            deep, print the work area, remove it, print again
 *   probe appbar-leak        register a 80-pixel left appbar and exit without
 *                            removing it
 *   probe workarea           the work area
 *
 * As a stand-in (its file name starts mag- or osk-): appends "cmdline=..." to
 * C:\standin.log, opens a window of Magnifier's class (SgMagnifier) or the
 * On-Screen Keyboard's (OSKMainClass), and logs "command N" for WM_COMMAND
 * and "close" for WM_CLOSE, which ends it.
 *
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

static void log_line(const WCHAR *fmt, ...)
{
    WCHAR w[1024];
    char u[3072];
    FILE *f;
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(w, ARRAYSIZE(w) - 1, fmt, ap);
    va_end(ap);
    w[ARRAYSIZE(w) - 1] = 0;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof(u), NULL, NULL);
    if (!(f = _wfopen(L"C:\\standin.log", L"ab"))) return;   /* binary: no CR */
    fprintf(f, "%s\n", u);
    fclose(f);
}

static LRESULT CALLBACK standin_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND: log_line(L"command %u", (unsigned)LOWORD(wp)); return 0;
    case WM_CLOSE: log_line(L"close"); DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void print_workarea(void)
{
    RECT r;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &r, 0);
    printf("%ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom);
    fflush(stdout);
}

static HWND appbar_window(void)
{
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SgA11yProbeBar";
    RegisterClassW(&wc);
    return CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"bar", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
}

static void appbar_set(HWND hwnd, UINT edge, int size)
{
    APPBARDATA abd = { sizeof(abd) };
    abd.hWnd = hwnd;
    abd.uCallbackMessage = WM_APP;
    SHAppBarMessage(ABM_NEW, &abd);
    abd.uEdge = edge;
    if (edge == ABE_TOP) SetRect(&abd.rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), size);
    else SetRect(&abd.rc, 0, 0, size, GetSystemMetrics(SM_CYSCREEN));
    SHAppBarMessage(ABM_QUERYPOS, &abd);
    SHAppBarMessage(ABM_SETPOS, &abd);
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR self[MAX_PATH], *name;
    GetModuleFileNameW(NULL, self, MAX_PATH);
    name = wcsrchr(self, '\\') ? wcsrchr(self, '\\') + 1 : self;
    if (!_wcsnicmp(name, L"mag-", 4) || !_wcsnicmp(name, L"osk-", 4))
    {
        WNDCLASSW wc = { 0 };
        MSG msg;
        wc.lpfnWndProc = standin_proc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = !_wcsnicmp(name, L"mag-", 4) ? L"SgMagnifier" : L"OSKMainClass";
        RegisterClassW(&wc);
        CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, name, WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
        log_line(L"cmdline=%ls", GetCommandLineW());
        while (GetMessageW(&msg, NULL, 0, 0) > 0) DispatchMessageW(&msg);
        return 0;
    }
    if (argc >= 3 && !wcscmp(argv[1], L"run"))
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(NULL, argv[2], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            printf("CreateProcess failed %lu\n", GetLastError());
            return 1;
        }
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return 0;
    }
    if (argc >= 4 && !wcscmp(argv[1], L"appbar"))
    {
        HWND bar = appbar_window();
        APPBARDATA abd = { sizeof(abd) };
        appbar_set(bar, !wcscmp(argv[2], L"top") ? ABE_TOP : ABE_LEFT, _wtoi(argv[3]));
        Sleep(300);
        print_workarea();
        abd.hWnd = bar;
        SHAppBarMessage(ABM_REMOVE, &abd);
        Sleep(300);
        print_workarea();
        return 0;
    }
    if (argc >= 2 && !wcscmp(argv[1], L"appbar-leak"))
    {
        appbar_set(appbar_window(), ABE_LEFT, 80);
        Sleep(300);
        print_workarea();
        return 0;   /* the window goes with the process; the appbar is never removed */
    }
    if (argc >= 2 && !wcscmp(argv[1], L"workarea"))
    {
        print_workarea();
        return 0;
    }
    return 2;
}
