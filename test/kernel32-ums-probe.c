/* Probe for patches/sg/5010: user-mode scheduling is not supported ("As of
 * Windows 11 ... all calls fail with the error ERROR_NOT_SUPPORTED"). 64-bit
 * kernel32 exports the functions; 32-bit kernel32 does not.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef ULONG_PTR (WINAPI *f1)(void *);
typedef ULONG_PTR (WINAPI *f0)(void);
typedef ULONG_PTR (WINAPI *f2)(void *, void *);
typedef ULONG_PTR (WINAPI *f3)(void *, ULONG_PTR, void *);
typedef ULONG_PTR (WINAPI *f4)(void *, ULONG_PTR, void *, ULONG_PTR);
typedef ULONG_PTR (WINAPI *f5)(void *, ULONG_PTR, void *, ULONG_PTR, void *);

int main(void)
{
    static const struct { const char *name; int args; } funcs[] =
    {
        {"CreateUmsCompletionList", 1}, {"CreateUmsThreadContext", 1}, {"DeleteUmsCompletionList", 1},
        {"DeleteUmsThreadContext", 1}, {"DequeueUmsCompletionListItems", 3}, {"EnterUmsSchedulingMode", 1},
        {"ExecuteUmsThread", 1}, {"GetCurrentUmsThread", 0}, {"GetNextUmsListItem", 1},
        {"GetUmsCompletionListEvent", 2}, {"QueryUmsThreadInformation", 5}, {"SetUmsThreadInformation", 4},
        {"UmsThreadYield", 1},
    };
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    BYTE buf[64];
    void *out = (void *)0xdeadbeef;
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(funcs); i++)
    {
        FARPROC f = GetProcAddress(k32, funcs[i].name);
        ULONG_PTR ret = 1;
        DWORD err;

        if (sizeof(void *) == 4)
        {
            check(!f, "%s is not exported by 32-bit kernel32", funcs[i].name);
            continue;
        }
        if (!f) { check(0, "%s is exported", funcs[i].name); continue; }
        memset(buf, 0, sizeof(buf));
        SetLastError(0xdeadbeef);
        switch (funcs[i].args)
        {
        case 0: ret = ((f0)f)(); break;
        case 1: ret = ((f1)f)(&out); break;
        case 2: ret = ((f2)f)(buf, &out); break;
        case 3: ret = ((f3)f)(buf, 0, &out); break;
        case 4: ret = ((f4)f)(buf, 1, buf, sizeof(buf)); break;
        case 5: ret = ((f5)f)(buf, 1, buf, sizeof(buf), &out); break;
        }
        err = GetLastError();
        check(!ret && err == ERROR_NOT_SUPPORTED, "%s: %#lx %lu", funcs[i].name, (unsigned long)ret, err);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
