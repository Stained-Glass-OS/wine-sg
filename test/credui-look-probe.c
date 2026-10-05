/*
 * credui-look-probe: opens CredUIPromptForCredentialsW's dialog in a thread
 * and prints what it shows (test/credui-look-gate.sh), then cancels it.
 *
 *   TITLE=..  HEADER=..  MESSAGE=..  CUEUSER=..  CUEPASS=..
 *   HEADERBIGGER=0|1  BACKGROUND=window|other  RET=<error>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <commctrl.h>
#include <wincred.h>
#include <stdio.h>

static WCHAR user[CREDUI_MAX_USERNAME_LENGTH + 1], pass[CREDUI_MAX_PASSWORD_LENGTH + 1];
static DWORD ret = 0xffffffff;

static DWORD WINAPI prompt(void *arg)
{
    CREDUI_INFOW info = { sizeof(info) };
    BOOL save = FALSE;

    ret = CredUIPromptForCredentialsW(&info, L"fileserver", NULL, 0, user, ARRAYSIZE(user),
                                      pass, ARRAYSIZE(pass), &save,
                                      CREDUI_FLAGS_ALWAYS_SHOW_UI | CREDUI_FLAGS_GENERIC_CREDENTIALS |
                                      CREDUI_FLAGS_DO_NOT_PERSIST);
    return 0;
}

static HWND found;

static BOOL CALLBACK find(HWND hwnd, LPARAM lp)
{
    WCHAR cls[64];
    DWORD pid;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && GetClassNameW(hwnd, cls, ARRAYSIZE(cls)) && !wcscmp(cls, L"#32770")
        && IsWindowVisible(hwnd))
    {
        found = hwnd;
        return FALSE;
    }
    return TRUE;
}

static LONG font_height(HWND hwnd)
{
    LOGFONTW lf;
    HFONT f = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);

    if (!f || !GetObjectW(f, sizeof(lf), &lf)) return 0;
    return lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
}

int main(void)
{
    WCHAR buf[256];
    HANDLE thread;
    HWND edit;
    HDC hdc;
    HBRUSH br;
    LOGBRUSH lb;
    int i;

    thread = CreateThread(NULL, 0, prompt, NULL, 0, NULL);
    for (i = 0; i < 200 && !found; i++)
    {
        Sleep(50);
        EnumWindows(find, 0);
    }
    if (!found)
    {
        printf("NODIALOG\n");
        return 1;
    }
    Sleep(500);
    GetWindowTextW(found, buf, ARRAYSIZE(buf));
    printf("TITLE=%ls\n", buf);
    buf[0] = 0; GetDlgItemTextW(found, 105, buf, ARRAYSIZE(buf));
    printf("HEADER=%ls\n", buf);
    buf[0] = 0; GetDlgItemTextW(found, 103, buf, ARRAYSIZE(buf));
    printf("MESSAGE=%ls\n", buf);
    edit = (HWND)SendDlgItemMessageW(found, 101, CBEM_GETEDITCONTROL, 0, 0);
    buf[0] = 0; SendMessageW(edit, EM_GETCUEBANNER, (WPARAM)buf, ARRAYSIZE(buf));
    printf("CUEUSER=%ls\n", buf);
    buf[0] = 0; SendDlgItemMessageW(found, 102, EM_GETCUEBANNER, (WPARAM)buf, ARRAYSIZE(buf));
    printf("CUEPASS=%ls\n", buf);
    printf("HEADERBIGGER=%d\n", font_height(GetDlgItem(found, 105)) > font_height(found));
    hdc = GetDC(found);
    br = (HBRUSH)SendMessageW(found, WM_CTLCOLORDLG, (WPARAM)hdc, (LPARAM)found);
    printf("BACKGROUND=%s\n", br && GetObjectW(br, sizeof(lb), &lb) && lb.lbColor == GetSysColor(COLOR_WINDOW)
           ? "window" : "other");
    ReleaseDC(found, hdc);
    fflush(stdout);
    if (getenv("PROBE_HOLD")) Sleep(atoi(getenv("PROBE_HOLD")));
    PostMessageW(found, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), (LPARAM)GetDlgItem(found, IDCANCEL));
    WaitForSingleObject(thread, 10000);
    printf("RET=%lu\n", ret);
    return 0;
}
