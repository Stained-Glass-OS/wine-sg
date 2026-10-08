/* InitiateSystemShutdownEx, InitiateShutdown and AbortSystemShutdown
 * (patches/sg/1618). They said yes and did nothing. A shutdown is asked for
 * with a long delay and aborted before it comes; the gate's SG_POWERCTL
 * stand-in records any power request, and there must be none.
 *
 *  - without SeShutdownPrivilege enabled: ERROR_PRIVILEGE_NOT_HELD;
 *  - with it: the shutdown is pending; a second one is
 *    ERROR_SHUTDOWN_IN_PROGRESS;
 *  - AbortSystemShutdown aborts it; again: ERROR_NO_SHUTDOWN_IN_PROGRESS;
 *  - InitiateShutdownW (restart) likewise, aborted;
 *  - another computer: not supported.
 */
#include <windows.h>
#include <stdio.h>

typedef DWORD (WINAPI *initiate_t)(WCHAR *, WCHAR *, DWORD, DWORD, DWORD);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void privilege(BOOL enable)
{
    TOKEN_PRIVILEGES tp = { 1 };
    HANDLE token;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token);
    LookupPrivilegeValueW(NULL, L"SeShutdownPrivilege", &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    CloseHandle(token);
}

int main(void)
{
    initiate_t pInitiateShutdownW = (void *)GetProcAddress(GetModuleHandleA("advapi32.dll"), "InitiateShutdownW");
    BOOL ok;
    DWORD err;

    privilege(FALSE);
    SetLastError(0xdeadbeef);
    ok = InitiateSystemShutdownExW(NULL, L"probe", 600, FALSE, FALSE, SHTDN_REASON_FLAG_PLANNED);
    check(!ok && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "without SeShutdownPrivilege: ERROR_PRIVILEGE_NOT_HELD");

    privilege(TRUE);
    SetLastError(0xdeadbeef);
    ok = AbortSystemShutdownW(NULL);
    check(!ok && GetLastError() == ERROR_NO_SHUTDOWN_IN_PROGRESS, "nothing to abort yet");
    ok = InitiateSystemShutdownExW(NULL, L"probe \"shutdown\"", 600, TRUE, FALSE, SHTDN_REASON_FLAG_PLANNED);
    printf("initiate: ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(ok, "a shutdown in ten minutes is pending");
    SetLastError(0xdeadbeef);
    ok = InitiateSystemShutdownExW(NULL, NULL, 600, FALSE, TRUE, SHTDN_REASON_FLAG_PLANNED);
    check(!ok && GetLastError() == ERROR_SHUTDOWN_IN_PROGRESS, "a second one: ERROR_SHUTDOWN_IN_PROGRESS");
    ok = AbortSystemShutdownW(NULL);
    check(ok, "AbortSystemShutdownW aborts it");
    Sleep(500);
    SetLastError(0xdeadbeef);
    ok = AbortSystemShutdownW(NULL);
    check(!ok && GetLastError() == ERROR_NO_SHUTDOWN_IN_PROGRESS, "... and then there is none");

    err = pInitiateShutdownW(NULL, NULL, 600, 0x4 /* SHUTDOWN_RESTART */, SHTDN_REASON_FLAG_PLANNED);
    check(err == ERROR_SUCCESS, "InitiateShutdownW: a restart is pending");
    check(AbortSystemShutdownW(NULL), "... and aborted");
    Sleep(500);

    SetLastError(0xdeadbeef);
    ok = InitiateSystemShutdownExW((WCHAR *)L"\\\\someotherpc", NULL, 600, FALSE, FALSE, 0);
    check(!ok && GetLastError() == ERROR_NOT_SUPPORTED, "another computer: not supported");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
