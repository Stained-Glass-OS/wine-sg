/* flatbuttons-probe: a message box with a default and a second button
 * (patches/sg/0454), for test/flatbuttons-gate.sh. It prints where its
 * buttons are (screen coordinates) and the colours they should have.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

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

int main(void)
{
    CloseHandle(CreateThread(NULL, 0, report, NULL, 0, NULL));
    MessageBoxW(NULL, L"Delete partition 2? Everything on it will be lost.", L"SgFlat", MB_YESNO | MB_ICONWARNING);
    return 0;
}
