/* objprops-probe: SHObjectProperties shows a properties sheet (wine-sg 1175)
 * for test/objprops-gate.sh.
 *
 *   objprops-probe PATH [PATH2]
 *       SHObjectProperties(SHOP_FILEPATH) for PATH (with PATH2: both, as a
 *       double-NUL-terminated list), then looks for the sheet for up to 10 s
 *       and prints:
 *         returned=0|1     what SHObjectProperties said, and at once (ms=)
 *         sheet=TITLE      the sheet's title, or sheet=none
 *         text=...         the texts of its General page's labels, one a line
 *       then closes it. --hold (last): it stays six seconds first.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static HWND found;
static const WCHAR *want;

static BOOL CALLBACK top(HWND hwnd, LPARAM lp)
{
    WCHAR title[512], cls[64];
    DWORD pid;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
    GetClassNameW(hwnd, cls, 64);
    GetWindowTextW(hwnd, title, 512);
    if (!lstrcmpW(cls, L"#32770") && wcsstr(title, L"Properties")) { found = hwnd; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK child(HWND hwnd, LPARAM lp)
{
    WCHAR text[512];

    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextW(hwnd, text, 512) && text[0]) wprintf(L"text=%ls\n", text);
    return TRUE;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR list[2 * MAX_PATH + 2] = { 0 };
    DWORD start, i;
    BOOL ret, hold = FALSE;

    if (argc > 1 && !lstrcmpW(argv[argc - 1], L"--hold")) { hold = TRUE; argc--; }
    if (argc < 2) return 2;
    CoInitialize(NULL);
    lstrcpyW(list, argv[1]);
    if (argc > 2) lstrcpyW(list + lstrlenW(argv[1]) + 1, argv[2]);
    start = GetTickCount();
    ret = SHObjectProperties(NULL, SHOP_FILEPATH, list, NULL);
    wprintf(L"returned=%d ms=%lu\n", ret ? 1 : 0, GetTickCount() - start);
    for (i = 0; i < 100 && !found; i++)
    {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(100);
        EnumWindows(top, 0);
    }
    if (!found)
    {
        wprintf(L"sheet=none\n");
        return 1;
    }
    {
        WCHAR title[512];
        GetWindowTextW(found, title, 512);
        wprintf(L"sheet=%ls\n", title);
    }
    Sleep(hold ? 6000 : 500);
    EnumChildWindows(found, child, 0);
    fflush(stdout);
    PostMessageW(found, WM_CLOSE, 0, 0);
    Sleep(500);
    return 0;
}
