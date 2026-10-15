/* Probe for patches/sg/5023: ThreadIsIoPending (GetThreadIOPendingFlag) needs
 * THREAD_QUERY_INFORMATION: a THREAD_QUERY_LIMITED_INFORMATION handle gets
 * STATUS_ACCESS_DENIED (kernel32 thread test, runs on Windows).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define ThreadIsIoPending_ 16

int main(void)
{
    static const struct { const char *what; DWORD access; BOOL pseudo; ULONG len; NTSTATUS want; } tests[] =
    {
        {"limited handle", THREAD_QUERY_LIMITED_INFORMATION, FALSE, sizeof(BOOL), 0xc0000022 /* STATUS_ACCESS_DENIED */},
        {"query handle", THREAD_QUERY_INFORMATION, FALSE, sizeof(BOOL), 0},
        {"all access", THREAD_ALL_ACCESS, FALSE, sizeof(BOOL), 0},
        {"pseudo handle", 0, TRUE, sizeof(BOOL), 0},
        {"wrong length", THREAD_QUERY_INFORMATION, FALSE, sizeof(BOOL) + 1, 0xc0000004 /* STATUS_INFO_LENGTH_MISMATCH */},
    };
    NTSTATUS (WINAPI *pNtQueryInformationThread)(HANDLE, int, void *, ULONG, ULONG *) =
        (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        HANDLE h = tests[i].pseudo ? GetCurrentThread() : OpenThread(tests[i].access, FALSE, GetCurrentThreadId());
        BYTE buf[8];
        BOOL flag = 0x55;
        NTSTATUS st;
        DWORD err;

        memset(buf, 0x55, sizeof(buf));
        st = pNtQueryInformationThread(h, ThreadIsIoPending_, buf, tests[i].len, NULL);
        check(st == tests[i].want && (st || !*(BOOL *)buf), "%s: %#lx (want %#lx)", tests[i].what, st, tests[i].want);
        if (tests[i].len == sizeof(BOOL))
        {
            SetLastError(0xdeadbeef);
            st = GetThreadIOPendingFlag(h, &flag);
            err = GetLastError();
            if (tests[i].want) check(!st && err == ERROR_ACCESS_DENIED, "%s, GetThreadIOPendingFlag: %ld %lu", tests[i].what, st, err);
            else check(st && !flag, "%s, GetThreadIOPendingFlag: %ld flag %d", tests[i].what, st, flag);
        }
        if (!tests[i].pseudo) CloseHandle(h);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
