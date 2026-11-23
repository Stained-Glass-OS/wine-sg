/* wine-sg 9706: Address Windowing Extensions answer as Windows does for an account without
 * SeLockMemoryPrivilege (every SG account): AllocateUserPhysicalPages fails with ERROR_PRIVILEGE_NOT_HELD
 * (what database servers test before using ordinary memory), mapping/freeing pages never handed out is
 * ERROR_INVALID_PARAMETER; MapUserPhysicalPagesScatter is exported. */
#include <windows.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
typedef BOOL (WINAPI *scatter_t)(PVOID *, ULONG_PTR, PULONG_PTR);

int main(void)
{
    ULONG_PTR n = 16, pfns[16] = {0};
    HANDLE token;
    TOKEN_PRIVILEGES tp;
    BOOL ret;
    scatter_t scatter = (scatter_t)GetProcAddress(GetModuleHandleA("kernel32"), "MapUserPhysicalPagesScatter");
    void *addrs[1];
    char *mem = VirtualAlloc(NULL, 65536, MEM_COMMIT, PAGE_READWRITE);

    /* asking for the privilege does not give it: the token has none to enable */
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token);
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueA(NULL, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    CHECK(GetLastError() == ERROR_NOT_ALL_ASSIGNED, "the token holds no SeLockMemoryPrivilege (AdjustTokenPrivileges: %lu)", GetLastError());

    SetLastError(0xdead);
    ret = AllocateUserPhysicalPages(GetCurrentProcess(), &n, pfns);
    CHECK(!ret && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "AllocateUserPhysicalPages: ERROR_PRIVILEGE_NOT_HELD (ret %d, error %lu)", ret, GetLastError());
    SetLastError(0xdead);
    ret = AllocateUserPhysicalPages(GetCurrentProcess(), NULL, pfns);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "a NULL page count: ERROR_INVALID_PARAMETER (%lu)", GetLastError());
    SetLastError(0xdead);
    ret = MapUserPhysicalPages(mem, 1, pfns);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "MapUserPhysicalPages on ordinary memory: ERROR_INVALID_PARAMETER (%lu)", GetLastError());
    n = 1; pfns[0] = 12345;
    SetLastError(0xdead);
    ret = FreeUserPhysicalPages(GetCurrentProcess(), &n, pfns);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER && n == 0, "FreeUserPhysicalPages of pages never given: ERROR_INVALID_PARAMETER, 0 freed (%lu, %Iu)", GetLastError(), n);
    CHECK(scatter != NULL, "MapUserPhysicalPagesScatter is exported");
    if (scatter)
    {
        addrs[0] = mem;
        SetLastError(0xdead);
        ret = scatter(addrs, 1, pfns);
        CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "MapUserPhysicalPagesScatter: ERROR_INVALID_PARAMETER (%lu)", GetLastError());
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
