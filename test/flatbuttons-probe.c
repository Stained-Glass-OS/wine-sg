/* flatbuttons-probe: a message box with a default and a second button
 * (patches/sg/0454), for test/flatbuttons-gate.sh. It prints where its
 * buttons are (screen coordinates) and the colours they should have.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static DWORD WINAPI report(void *arg)
{
    HWND box = NULL, yes, no;
    RECT ry, rn, rb;
    int i;
    for (i = 0; i < 100 && !(box = FindWindowW(L"#32770", L"SgFlat")); i++) Sleep(100);
    Sleep(1500);
    if (!box) { printf("nobox\n"); fflush(stdout); return 0; }
    yes = GetDlgItem(box, IDYES); no = GetDlgItem(box, IDNO);
    GetWindowRect(yes, &ry); GetWindowRect(no, &rn); GetWindowRect(box, &rb);
    printf("yes %ld %ld %ld %ld\nno %ld %ld %ld %ld\nbox %ld %ld %ld %ld\n", ry.left, ry.top, ry.right, ry.bottom,
           rn.left, rn.top, rn.right, rn.bottom, rb.left, rb.top, rb.right, rb.bottom);
    printf("window %lu\nface %lu\nhighlight %lu\n", GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_HIGHLIGHT));
    fflush(stdout);
    return 0;
}

static LRESULT CALLBACK form_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(h, m, w, l);
}

/* "form": a drop-down list and an edit box (0455); prints their screen rects */
static int form(void)
{
    WNDCLASSW wc = {0};
    MSG msg;
    HWND h, cb, ed;
    RECT rc, re;
    wc.lpfnWndProc = form_proc; wc.lpszClassName = L"SgFlatForm"; wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    RegisterClassW(&wc);
    h = CreateWindowW(L"SgFlatForm", L"SgFlatForm", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 460, 260, 0, 0, 0, 0);
    cb = CreateWindowW(L"COMBOBOX", 0, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 20, 20, 300, 200, h, 0, 0, 0);
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)L"English (United States)"); SendMessageW(cb, CB_SETCURSEL, 0, 0);
    ed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"live", WS_CHILD | WS_VISIBLE, 20, 70, 300, 26, h, 0, 0, 0);
    UpdateWindow(h);
    GetWindowRect(cb, &rc); GetWindowRect(ed, &re);
    printf("combo %ld %ld %ld %ld\nedit %ld %ld %ld %ld\nwindow %lu\nshadow %lu\ntext %lu\n", rc.left, rc.top, rc.right, rc.bottom,
           re.left, re.top, re.right, re.bottom, GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNTEXT));
    fflush(stdout);
    while (GetMessageW(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "form")) return form();
    CloseHandle(CreateThread(NULL, 0, report, NULL, 0, NULL));
    MessageBoxW(NULL, L"Delete partition 2? Everything on it will be lost.", L"SgFlat", MB_YESNO | MB_ICONWARNING);
    return 0;
}
