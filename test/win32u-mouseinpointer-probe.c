/* Probe for patches/sg/5046: EnableMouseInPointer is set once per process
 * (user32 input tests on Windows): switching it off succeeds, again too, and
 * a later switch the other way fails with ERROR_ACCESS_DENIED; a process that
 * asked for it on cannot switch it off afterwards. IsMouseInPointerEnabled
 * answers without an error (it set ERROR_CALL_NOT_IMPLEMENTED). Switching it
 * on is not supported here (no WM_POINTER for mouse input) and is not checked.
 * Runs itself as a child for the second process.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static BOOL (WINAPI *pEnableMouseInPointer)(BOOL);
static BOOL (WINAPI *pIsMouseInPointerEnabled)(void);

int main(int argc, char **argv)
{
    HMODULE user32 = LoadLibraryA("user32.dll");
    BOOL ret;

    pEnableMouseInPointer = (void *)GetProcAddress(user32, "EnableMouseInPointer");
    pIsMouseInPointerEnabled = (void *)GetProcAddress(user32, "IsMouseInPointerEnabled");
    if (!pEnableMouseInPointer || !pIsMouseInPointerEnabled) { printf("FAIL no exports\nRESULT: FAIL\n"); return 1; }

    if (argc > 1 && !strcmp(argv[1], "on-first"))
    {
        pEnableMouseInPointer(TRUE);
        SetLastError(0xdeadbeef);
        ret = pEnableMouseInPointer(FALSE);
        check(!ret && GetLastError() == ERROR_ACCESS_DENIED, "child: off after asking for on: %d %lu", ret, GetLastError());
        printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
        return !!failures;
    }

    SetLastError(0xdeadbeef);
    ret = pIsMouseInPointerEnabled();
    check(!ret && GetLastError() == 0xdeadbeef, "not enabled at first, no error: %d %lu", ret, GetLastError());
    ret = pEnableMouseInPointer(FALSE);
    check(ret, "switch off: %d %lu", ret, ret ? 0 : GetLastError());
    ret = pEnableMouseInPointer(FALSE);
    check(ret, "switch off again: %d", ret);
    SetLastError(0xdeadbeef);
    ret = pEnableMouseInPointer(TRUE);
    check(!ret && GetLastError() == ERROR_ACCESS_DENIED, "switch on after off: %d %lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = pIsMouseInPointerEnabled();
    check(!ret && GetLastError() == 0xdeadbeef, "still not enabled, no error: %d %lu", ret, GetLastError());

    {
        char cmd[MAX_PATH + 32];
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        DWORD code = 99;

        sprintf(cmd, "\"%s\" on-first", argv[0]);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        check(code == 0, "the child process passed (%lu)", code);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
