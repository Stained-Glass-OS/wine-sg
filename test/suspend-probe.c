/* SetSuspendState (patches/sg/2414), run by test/suspend-gate.sh against a
 * stand-in logind on a private bus: prints "ret=N err=N ms=N" for one call.
 *
 *   suspend-probe.exe suspend      SetSuspendState(FALSE, FALSE, FALSE)
 *   suspend-probe.exe hibernate    SetSuspendState(TRUE, FALSE, FALSE)
 *   suspend-probe.exe nopriv       the same, from a thread without the shutdown privilege */
#include <windows.h>
#include <powrprof.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    BOOLEAN ret;
    DWORD t0, err;
    int hib = argc > 1 && !strcmp(argv[1], "hibernate");
    HANDLE proc, restricted, imp = NULL;

    if (argc > 1 && !strcmp(argv[1], "nopriv"))
    {
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &proc) &&
            CreateRestrictedToken(proc, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &restricted) &&
            DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp))
            SetThreadToken(NULL, imp);
        else
        {
            printf("no restricted token\n");
            return 1;
        }
    }
    t0 = GetTickCount();
    SetLastError(0);
    ret = SetSuspendState(hib, FALSE, FALSE);
    err = GetLastError();
    printf("ret=%d err=%lu ms=%lu\n", ret, ret ? 0 : err, GetTickCount() - t0);
    fflush(stdout);
    return 0;
}
