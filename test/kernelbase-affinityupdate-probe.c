/* Probe for patches/sg/5064: SetProcessAffinityUpdateMode / QueryProcessAffinityUpdateMode.
 * - the mode starts disabled; enabling and then disabling it succeed and Query follows;
 * - after it has been disabled by a call it cannot be enabled again (documented);
 * - flags other than PROCESS_AFFINITY_ENABLE_AUTO_UPDATE and a handle that is not
 *   GetCurrentProcess() are refused;
 * - Query takes a real handle to the current process too, and a NULL flags pointer.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef BOOL (WINAPI *set_t)(HANDLE, DWORD);
typedef BOOL (WINAPI *query_t)(HANDLE, DWORD *);

int main(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    set_t pSet = (set_t)GetProcAddress(k32, "SetProcessAffinityUpdateMode");
    query_t pQuery = (query_t)GetProcAddress(k32, "QueryProcessAffinityUpdateMode");
    /* in order: a call, whether it succeeds, the mode Query then reports */
    static const struct { int handle; DWORD flags; BOOL ret; DWORD mode; } steps[] =
    {
        {0, 2, FALSE, 0},       /* unknown flag */
        {1, 1, FALSE, 0},       /* not the pseudo handle */
        {0, 1, TRUE, 1},
        {0, 1, TRUE, 1},
        {0, 0, TRUE, 0},
        {0, 1, FALSE, 0},       /* no way back after a disable */
        {0, 0, TRUE, 0},
    };
    HANDLE real;
    DWORD mode;
    unsigned int i;
    BOOL ret;

    check(pSet && pQuery, "both exported from kernel32");
    if (!pSet || !pQuery) goto done;
    DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &real, 0, FALSE, DUPLICATE_SAME_ACCESS);

    mode = 0xdead;
    ret = pQuery(GetCurrentProcess(), &mode);
    check(ret && mode == 0, "starts disabled: %d %lu", ret, mode);
    for (i = 0; i < ARRAYSIZE(steps); i++)
    {
        SetLastError(0xdeadbeef);
        ret = pSet(steps[i].handle ? real : GetCurrentProcess(), steps[i].flags);
        check(ret == steps[i].ret && (ret || GetLastError() == ERROR_INVALID_PARAMETER),
              "step %u: Set(%s, %lu) %d error %lu", i, steps[i].handle ? "real handle" : "GetCurrentProcess()",
              steps[i].flags, ret, ret ? 0 : GetLastError());
        mode = 0xdead;
        ret = pQuery(steps[i].handle ? real : GetCurrentProcess(), &mode);
        check(ret && mode == steps[i].mode, "step %u: Query %d mode %lu", i, ret, mode);
    }
    ret = pQuery(GetCurrentProcess(), NULL);
    check(ret, "Query with no flags pointer %d", ret);
    CloseHandle(real);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
