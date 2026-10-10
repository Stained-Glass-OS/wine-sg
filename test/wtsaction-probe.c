/* wtsapi32's session actions (patches/sg/2412), run by test/wtsaction-gate.sh:
 * WTSSendMessage shows a message box in the caller's own session and honours the
 * wait flag and the timeout; WTSLogoffSession, WTSDisconnectSession and
 * WTSSendMessage on any other session need an administrator; WTSShutdownSystem
 * needs the shutdown privilege and asks the system to shut down, reboot or power off.
 *
 *   wtsaction-probe.exe                 checks that leave the session alone
 *   wtsaction-probe.exe logoff          signs the session out (the process is ended)
 *   wtsaction-probe.exe shutdown N      WTSShutdownSystem with flag N (the process is ended) */
#include <windows.h>
#include <wtsapi32.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef WTS_WSD_LOGOFF
#define WTS_WSD_LOGOFF 1
#define WTS_WSD_SHUTDOWN 2
#define WTS_WSD_REBOOT 4
#define WTS_WSD_POWEROFF 8
#define WTS_WSD_FASTREBOOT 16
#endif
#ifndef IDTIMEOUT
#define IDTIMEOUT 32000
#define IDASYNC 32001
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* answers the message box titled `title` by sending it the command `button` */
struct clicker { const WCHAR *title; int button; int found; WCHAR text[128]; WCHAR shown_title[128]; };

static DWORD WINAPI click_thread(void *arg)
{
    struct clicker *c = arg;
    int i;
    for (i = 0; i < 100; i++)
    {
        HWND w = FindWindowW(L"#32770", c->title);
        if (w)
        {
            HWND st = FindWindowExW(w, NULL, L"Static", NULL);
            while (st && !GetWindowTextW(st, c->text, 128)) st = FindWindowExW(w, st, L"Static", NULL);
            GetWindowTextW(w, c->shown_title, 128);
            c->found = 1;
            SendMessageW(w, WM_COMMAND, c->button, 0);
            return 0;
        }
        Sleep(100);
    }
    return 0;
}

static HANDLE start_click(struct clicker *c, const WCHAR *title, int button)
{
    memset(c, 0, sizeof(*c));
    c->title = title; c->button = button;
    return CreateThread(NULL, 0, click_thread, c, 0, NULL);
}

static int box_exists(const WCHAR *title) { return FindWindowW(L"#32770", title) != NULL; }

static HANDLE restricted_thread_token(void)
{
    HANDLE proc, restricted, imp = NULL;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins;
    SID_AND_ATTRIBUTES sa;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &proc)) return NULL;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);
    sa.Sid = admins; sa.Attributes = 0;
    if (CreateRestrictedToken(proc, DISABLE_MAX_PRIVILEGE, 1, &sa, 0, NULL, 0, NULL, &restricted))
    {
        DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp);
        CloseHandle(restricted);
    }
    CloseHandle(proc);
    FreeSid(admins);
    return imp;
}

static void own_session_tests(void)
{
    DWORD me, resp, t0, ms;
    struct clicker c;
    HANDLE th;
    BOOL ret;
    WCHAR title[] = L"SgWtsBox1";
    char atitle[] = "SgWtsBoxA";
    WCHAR atitlew[] = L"SgWtsBoxA";

    ProcessIdToSessionId(GetCurrentProcessId(), &me);

    /* the message appears in the caller's session, with the title and text, and the button pressed comes back */
    th = start_click(&c, title, IDNO);
    resp = 0;
    ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, title, sizeof(title) - sizeof(WCHAR),
                          (WCHAR *)L"Hello from WTS", 14 * sizeof(WCHAR), MB_YESNO, 0, &resp, TRUE);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    check(ret && c.found, "WTSSendMessageW(current session) shows the box");
    check(resp == IDNO, "the answer is the button pressed (IDNO)");
    check(!wcscmp(c.text, L"Hello from WTS"), "the box shows the message text, cut to its length");
    check(!wcscmp(c.shown_title, L"SgWtsBox1"), "and the title");

    /* the session's own number works the same as WTS_CURRENT_SESSION */
    th = start_click(&c, title, IDOK);
    resp = 0;
    ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, me, title, sizeof(title) - sizeof(WCHAR), (WCHAR *)L"x", sizeof(WCHAR),
                          MB_OK, 0, &resp, TRUE);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    check(ret && c.found && resp == IDOK, "the session's own number is the same as WTS_CURRENT_SESSION");

    /* ANSI */
    th = start_click(&c, atitlew, IDOK);
    resp = 0;
    ret = WTSSendMessageA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, atitle, 9, (char *)"ansi text!!", 9, MB_OK, 0, &resp, TRUE);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    check(ret && c.found && resp == IDOK && !wcscmp(c.text, L"ansi text"), "WTSSendMessageA, text cut to its length");

    /* the timeout (seconds), answered as IDTIMEOUT */
    resp = 0;
    t0 = GetTickCount();
    ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, (WCHAR *)L"SgWtsTimeout", 12 * sizeof(WCHAR),
                          (WCHAR *)L"wait", 4 * sizeof(WCHAR), MB_OK, 1, &resp, TRUE);
    ms = GetTickCount() - t0;
    check(ret && resp == IDTIMEOUT, "an unanswered box ends after the timeout with IDTIMEOUT");
    check(ms >= 900 && ms < 8000, "after about the time asked for");
    check(!box_exists(L"SgWtsTimeout"), "and the box is gone");

    /* no waiting: back at once, IDASYNC, the box stays up until answered */
    resp = 0;
    t0 = GetTickCount();
    ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, (WCHAR *)L"SgWtsAsync", 10 * sizeof(WCHAR),
                          (WCHAR *)L"async", 5 * sizeof(WCHAR), MB_OK, 0, &resp, FALSE);
    ms = GetTickCount() - t0;
    check(ret && resp == IDASYNC && ms < 2000, "bWait FALSE returns at once with IDASYNC");
    Sleep(500);
    check(box_exists(L"SgWtsAsync"), "and the box is up");
    th = start_click(&c, L"SgWtsAsync", IDOK);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    Sleep(500);
    check(c.found && !box_exists(L"SgWtsAsync"), "and answering it ends it");

    /* a timeout on an async box also ends it */
    ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, (WCHAR *)L"SgWtsAsyncT", 11 * sizeof(WCHAR),
                          (WCHAR *)L"async", 5 * sizeof(WCHAR), MB_OK, 1, &resp, FALSE);
    Sleep(2500);
    check(ret && !box_exists(L"SgWtsAsyncT"), "an async box ends at its timeout too");

    /* bad arguments */
    SetLastError(0);
    ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, title, 4, (WCHAR *)L"x", 2, MB_OK, 0, NULL, TRUE);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no response pointer: ERROR_INVALID_PARAMETER");
    SetLastError(0);
    ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, title, 4, NULL, 0, MB_OK, 0, &resp, TRUE);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no message: ERROR_INVALID_PARAMETER");
    SetLastError(0);
    ret = WTSSendMessageW((HANDLE)0x1234, WTS_CURRENT_SESSION, title, 4, (WCHAR *)L"x", 2, MB_OK, 0, &resp, TRUE);
    check(!ret && GetLastError() == RPC_S_SERVER_UNAVAILABLE, "a server we cannot reach: RPC_S_SERVER_UNAVAILABLE");
    SetLastError(0);
    ret = WTSLogoffSession((HANDLE)0x1234, WTS_CURRENT_SESSION, FALSE);
    check(!ret && GetLastError() == RPC_S_SERVER_UNAVAILABLE, "WTSLogoffSession on such a server: RPC_S_SERVER_UNAVAILABLE");
    SetLastError(0);
    ret = WTSDisconnectSession((HANDLE)0x1234, WTS_CURRENT_SESSION, FALSE);
    check(!ret && GetLastError() == RPC_S_SERVER_UNAVAILABLE, "WTSDisconnectSession on such a server: RPC_S_SERVER_UNAVAILABLE");
    SetLastError(0);
    ret = WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, WTS_ANY_SESSION, FALSE);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "WTS_ANY_SESSION is no session to log off: ERROR_INVALID_PARAMETER");
}

static void other_session_tests(void)
{
    DWORD me, resp;
    BOOL ret;
    HANDLE imp;
    DWORD others[2] = { 0, 77 }, i;

    ProcessIdToSessionId(GetCurrentProcessId(), &me);
    if (me == 77) others[1] = 78;
    if (me == 0) others[0] = 76;

    /* as an administrator: the services session is not to be touched, others are not there */
    for (i = 0; i < 2; i++)
    {
        DWORD want = others[i] == 0 ? ERROR_ACCESS_DENIED : ERROR_NOT_FOUND;
        char what[96];

        SetLastError(0);
        ret = WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, others[i], FALSE);
        sprintf(what, "administrator: WTSLogoffSession(%lu) fails with %lu", others[i], want);
        check(!ret && GetLastError() == want, what);
        SetLastError(0);
        ret = WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, others[i], FALSE);
        sprintf(what, "administrator: WTSDisconnectSession(%lu) fails with %lu", others[i], want);
        check(!ret && GetLastError() == want, what);
        SetLastError(0);
        ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, others[i], (WCHAR *)L"t", 2, (WCHAR *)L"m", 2, MB_OK, 0, &resp, TRUE);
        sprintf(what, "administrator: WTSSendMessage(%lu) fails with %lu", others[i], want);
        check(!ret && GetLastError() == want, what);
    }

    /* without administrator rights, anything but one's own session is refused */
    imp = restricted_thread_token();
    check(imp != NULL && SetThreadToken(NULL, imp), "a thread without the administrators group");
    if (imp)
    {
        for (i = 0; i < 2; i++)
        {
            char what[96];

            SetLastError(0);
            ret = WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, others[i], FALSE);
            sprintf(what, "non-administrator: WTSLogoffSession(%lu) is ERROR_ACCESS_DENIED", others[i]);
            check(!ret && GetLastError() == ERROR_ACCESS_DENIED, what);
            SetLastError(0);
            ret = WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, others[i], FALSE);
            sprintf(what, "non-administrator: WTSDisconnectSession(%lu) is ERROR_ACCESS_DENIED", others[i]);
            check(!ret && GetLastError() == ERROR_ACCESS_DENIED, what);
            SetLastError(0);
            ret = WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, others[i], (WCHAR *)L"t", 2, (WCHAR *)L"m", 2, MB_OK, 0, &resp, TRUE);
            sprintf(what, "non-administrator: WTSSendMessage(%lu) is ERROR_ACCESS_DENIED", others[i]);
            check(!ret && GetLastError() == ERROR_ACCESS_DENIED, what);
        }
        /* the privilege, too */
        SetLastError(0);
        ret = WTSShutdownSystem(WTS_CURRENT_SERVER_HANDLE, WTS_WSD_POWEROFF);
        check(!ret && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "without the shutdown privilege: ERROR_PRIVILEGE_NOT_HELD");
        SetThreadToken(NULL, NULL);
        CloseHandle(imp);
    }

    SetLastError(0);
    ret = WTSShutdownSystem(WTS_CURRENT_SERVER_HANDLE, 0);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "WTSShutdownSystem(0): ERROR_INVALID_PARAMETER");
    SetLastError(0);
    ret = WTSShutdownSystem(WTS_CURRENT_SERVER_HANDLE, 0x100);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown flag: ERROR_INVALID_PARAMETER");
    SetLastError(0);
    ret = WTSShutdownSystem((HANDLE)0x1234, WTS_WSD_POWEROFF);
    check(!ret && GetLastError() == RPC_S_SERVER_UNAVAILABLE, "a server we cannot reach: RPC_S_SERVER_UNAVAILABLE");
}

static int end_seen, end_logoff;
static LRESULT CALLBACK end_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_QUERYENDSESSION)
    {
        end_seen = 1;
        end_logoff = (lp & ENDSESSION_LOGOFF) != 0;
        PostQuitMessage(0);
        return TRUE;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

/* the session ending is seen as the end-of-session query reaching a window of ours */
static void wait_for_end(const char *what, int want_logoff)
{
    MSG msg;
    DWORD t0 = GetTickCount();

    while (!end_seen && GetTickCount() - t0 < 30000)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(50);
    }
    check(end_seen, what);
    if (end_seen && !want_logoff) check(!end_logoff, "it is a shutdown query");
}

static void make_end_window(void)
{
    WNDCLASSW wc = { 0 };

    wc.lpfnWndProc = end_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SgWtsEnd";
    RegisterClassW(&wc);
    ShowWindow(CreateWindowW(L"SgWtsEnd", L"end", WS_OVERLAPPEDWINDOW, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL), SW_SHOWNOACTIVATE);
}

int main(int argc, char **argv)
{
    BOOL ret;

    if (argc > 1 && !strcmp(argv[1], "logoff"))
    {
        make_end_window();
        ret = WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE);
        check(ret, "WTSLogoffSession(own session) is accepted");
        wait_for_end("the session is asked to end", 1);
        printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
        return failures != 0;
    }
    if (argc > 2 && !strcmp(argv[1], "shutdown"))
    {
        int flag = atoi(argv[2]);

        make_end_window();
        ret = WTSShutdownSystem(WTS_CURRENT_SERVER_HANDLE, flag);
        check(ret, "WTSShutdownSystem is accepted");
        wait_for_end("the session is asked to end", flag == WTS_WSD_LOGOFF);
        printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
        return failures != 0;
    }

    own_session_tests();
    other_session_tests();
    {
        /* disconnecting one's own session is what locking it is */
        BOOL lock = LockWorkStation();
        SetLastError(0);
        ret = WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE);
        check(ret == lock, "WTSDisconnectSession(own session) does what LockWorkStation does");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
