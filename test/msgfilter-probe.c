/* ChangeWindowMessageFilter, the process-wide filter (patches/sg/1684),
 * run by test/msgfilter-gate.sh. This process (the prefix's token: high
 * integrity) lets WM_COPYDATA through UIPI for all its windows, but one
 * window disallows it for itself (ChangeWindowMessageFilterEx); a copy of
 * this program at medium integrity then sends to both and posts another
 * message. Removed from the filter, WM_COPYDATA is refused again. The
 * process-wide call was a stub that let nothing through. */
#include <windows.h>
#include <stdio.h>

static int failures, copydata_a, copydata_b;
static HWND win_a, win_b;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_COPYDATA)
    {
        if (hwnd == win_a) copydata_a++;
        else copydata_b++;
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* the medium integrity side: exit code bit 0 = copydata to A went,
 * bit 1 = copydata to B went, bit 2 = the post to A went */
static int sender(HWND a, HWND b)
{
    COPYDATASTRUCT cds = { 1, 4, (void *)"sg!" };
    DWORD_PTR res = 0;
    int code = 0;

    if (SendMessageTimeoutW(a, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 5000, &res) && res == 1) code |= 1;
    res = 0;
    if (SendMessageTimeoutW(b, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 5000, &res) && res == 1) code |= 2;
    if (PostMessageW(a, WM_USER + 5, 0, 0)) code |= 4;
    return code;
}

static int run_sender(void)
{
    struct { TOKEN_MANDATORY_LABEL label; BYTE sid[SECURITY_MAX_SID_SIZE]; } il;
    DWORD size = sizeof(il.sid), code = 99;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR cmd[MAX_PATH + 64], self[MAX_PATH];
    HANDLE token, medium;
    MSG msg;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    DuplicateTokenEx(token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &medium);
    CreateWellKnownSid(WinMediumLabelSid, NULL, il.sid, &size);
    il.label.Label.Sid = il.sid;
    il.label.Label.Attributes = SE_GROUP_INTEGRITY;
    if (!SetTokenInformation(medium, TokenIntegrityLevel, &il.label, sizeof(il.label) + size)) return -1;
    GetModuleFileNameW(NULL, self, MAX_PATH);
    _snwprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" sender %Iu %Iu", self, (ULONG_PTR)win_a, (ULONG_PTR)win_b);
    if (!CreateProcessAsUserW(medium, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;
    while (MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 30000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

int wmain(int argc, WCHAR **argv)
{
    CHANGEFILTERSTRUCT cfs = { sizeof(cfs) };
    WNDCLASSW wc = { 0 };
    int code;

    if (argc > 3 && !wcscmp(argv[1], L"sender"))
        return sender((HWND)(ULONG_PTR)_wcstoui64(argv[2], NULL, 10), (HWND)(ULONG_PTR)_wcstoui64(argv[3], NULL, 10));

    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"SgMsgFilter";
    RegisterClassW(&wc);
    win_a = CreateWindowW(L"SgMsgFilter", L"A", WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, NULL, NULL);
    win_b = CreateWindowW(L"SgMsgFilter", L"B", WS_OVERLAPPED, 0, 0, 50, 50, NULL, NULL, NULL, NULL);

    SetLastError(0xdeadbeef);
    check(!ChangeWindowMessageFilter(WM_COPYDATA, 7) && GetLastError() == ERROR_INVALID_PARAMETER,
          "a flag that is neither MSGFLT_ADD nor MSGFLT_REMOVE: ERROR_INVALID_PARAMETER");

    code = run_sender();
    check(code == 0, "before: a program below cannot send WM_COPYDATA or post");

    check(ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD), "ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD)");
    check(ChangeWindowMessageFilterEx(win_b, WM_COPYDATA, MSGFLT_DISALLOW, &cfs) && cfs.ExtStatus == MSGFLTINFO_NONE,
          "ChangeWindowMessageFilterEx(B, MSGFLT_DISALLOW)");
    check(ChangeWindowMessageFilterEx(win_b, WM_COPYDATA, MSGFLT_DISALLOW, &cfs) &&
          cfs.ExtStatus == MSGFLTINFO_ALREADYDISALLOWED_FORWND, "again: MSGFLTINFO_ALREADYDISALLOWED_FORWND");
    copydata_a = copydata_b = 0;
    code = run_sender();
    check(code >= 0 && (code & 1) && copydata_a == 1, "WM_COPYDATA from below reaches window A (the process's filter)");
    check(code >= 0 && !(code & 2) && copydata_b == 0, "but not window B, which disallowed it");
    check(code >= 0 && !(code & 4), "another message is still refused");

    check(ChangeWindowMessageFilterEx(win_b, WM_COPYDATA, MSGFLT_RESET, NULL), "MSGFLT_RESET on B");
    copydata_a = copydata_b = 0;
    code = run_sender();
    check(code >= 0 && (code & 3) == 3 && copydata_b == 1, "reset: B takes the process's filter, WM_COPYDATA reaches it");

    check(ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_REMOVE), "ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_REMOVE)");
    copydata_a = 0;
    code = run_sender();
    check(code == 0 && copydata_a == 0, "removed: refused again");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
