/* GetSystemFileCacheSize / SetSystemFileCacheSize, NtQuerySystemInformation(
 * SystemFileCacheInformation) and Get/SetThreadDpiHostingBehavior
 * (patches/sg/2208).  The first three were stubs (ERROR_CALL_NOT_IMPLEMENTED,
 * all zeros) and the DPI pair returned a constant.
 *
 *   probe.exe [OTHER.exe]   run the checks, then have OTHER.exe (a 32- or 64-bit
 *                           process) read the limits this process just set
 *   probe.exe --expect MIN MAX FLAGS   exit 0 if the limits read back as given */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef DPI_HOSTING_BEHAVIOR (WINAPI *dpi_get_t)(void);
typedef DPI_HOSTING_BEHAVIOR (WINAPI *dpi_set_t)(DPI_HOSTING_BEHAVIOR);
typedef BOOL (WINAPI *cache_get_t)(SIZE_T *, SIZE_T *, DWORD *);
typedef BOOL (WINAPI *cache_set_t)(SIZE_T, SIZE_T, DWORD);
typedef LONG (WINAPI *nqsi_t)(ULONG, void *, ULONG, ULONG *);

typedef struct
{
    SIZE_T CurrentSize, PeakSize;
    ULONG PageFaultCount;
    SIZE_T MinimumWorkingSet, MaximumWorkingSet, CurrentSizeIncludingTransitionInPages, PeakSizeIncludingTransitionInPages;
    ULONG TransitionRePurposeCount, Flags;
} CACHEINFO;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static cache_get_t pGet;
static cache_set_t pSet;
static dpi_get_t pGetDpi;
static dpi_set_t pSetDpi;

static int expect_mode(SIZE_T min, SIZE_T max, DWORD flags)
{
    SIZE_T a = 0, b = 0;
    DWORD f = 0xdead;
    return pGet(&a, &b, &f) && a == min && b == max && f == flags ? 0 : 1;
}

static int run_other(const char *exe, SIZE_T min, SIZE_T max, DWORD flags)
{
    char cmd[MAX_PATH + 96];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 99;

    snprintf(cmd, sizeof(cmd), "\"%s\" --expect %lu %lu %lu", exe, (unsigned long)min, (unsigned long)max, (unsigned long)flags);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 98;
    if (WaitForSingleObject(pi.hProcess, 30000) != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 96);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

static DWORD WINAPI other_thread(void *arg)
{
    int *r = arg;
    r[0] = pGetDpi();                       /* a new thread starts at default */
    r[1] = pSetDpi(DPI_HOSTING_BEHAVIOR_MIXED);
    r[2] = pGetDpi();
    return 0;
}

static unsigned long long meminfo_kb(const char *key)
{
    FILE *f = fopen("Z:\\proc\\meminfo", "r");
    char line[128];
    unsigned long long v = 0;
    size_t n = strlen(key);

    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, key, n) && line[n] == ':') { v = strtoull(line + n + 1, NULL, 10); break; }
    fclose(f);
    return v;
}

int main(int argc, char **argv)
{
    HMODULE kb = GetModuleHandleA("kernelbase.dll"), u32 = LoadLibraryA("user32.dll");
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    nqsi_t pNtQuery = (nqsi_t)GetProcAddress(nt, "NtQuerySystemInformation");
    SIZE_T min = 1, max = 2, min2, max2;
    DWORD flags = 0, f2;
    CACHEINFO ci;
    ULONG len;
    SYSTEM_INFO si;
    MEMORYSTATUSEX ms = { sizeof(ms) };
    unsigned long long cached_kb;
    HANDLE th;
    int r[3], i;
    DWORD err;

    pGet = (cache_get_t)GetProcAddress(kb, "GetSystemFileCacheSize");
    pSet = (cache_set_t)GetProcAddress(kb, "SetSystemFileCacheSize");
    pGetDpi = (dpi_get_t)GetProcAddress(u32, "GetThreadDpiHostingBehavior");
    pSetDpi = (dpi_set_t)GetProcAddress(u32, "SetThreadDpiHostingBehavior");
    if (!pGet || !pSet || !pGetDpi || !pSetDpi || !pNtQuery) { printf("FAIL  entry points\n"); return 1; }

    if (argc >= 2 && !strcmp(argv[1], "--expect"))   /* never spawns anything */
    {
        if (argc < 5) return 97;
        return expect_mode(strtoul(argv[2], NULL, 10), strtoul(argv[3], NULL, 10), strtoul(argv[4], NULL, 10));
    }

    GetSystemInfo(&si);
    GlobalMemoryStatusEx(&ms);

    /* without the privilege enabled the calls are refused */
    SetLastError(0);
    check(!pSet(1 << 20, 8 << 20, 0) && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "without SeIncreaseQuotaPrivilege enabled: ERROR_PRIVILEGE_NOT_HELD");
    SetLastError(0);
    check(!pSet((SIZE_T)-1, (SIZE_T)-1, 0) && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "...also for the flush request");
    {
        HANDLE tok;
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tok) ||
            !LookupPrivilegeValueA(NULL, SE_INCREASE_QUOTA_NAME, &tp.Privileges[0].Luid) ||
            !AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL) || GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        { printf("FAIL  cannot enable SeIncreaseQuotaPrivilege\n"); return 1; }
        CloseHandle(tok);
    }


    /* --- the query --- */
    SetLastError(0);
    check(!pGet(NULL, &max, &flags) && GetLastError() == ERROR_INVALID_PARAMETER, "a NULL minimum is ERROR_INVALID_PARAMETER");
    check(!pGet(&min, NULL, &flags) && !pGet(&min, &max, NULL), "...and so is a NULL maximum or flags");
    check(pGet(&min, &max, &flags), "GetSystemFileCacheSize succeeds");
    check(min >= 0x100000 / 2 && min <= max, "the minimum is a real size and not above the maximum");
    check(max >= 64 * 1024 * 1024 / 2 && (unsigned long long)max <= ms.ullTotalPhys + si.dwPageSize,
          "the maximum is the order of physical memory");
    check(min % si.dwPageSize == 0 && max % si.dwPageSize == 0, "both are whole pages");
    check(flags == (0x02 | 0x08), "by default no hard limit is enabled (MAX_HARD_DISABLE | MIN_HARD_DISABLE)");

    memset(&ci, 0xcc, sizeof(ci));
    check(!pNtQuery(21, &ci, sizeof(ci), &len) && len == sizeof(ci), "SystemFileCacheInformation succeeds");
    cached_kb = meminfo_kb("Cached");
    {
        SIZE_T want = (SIZE_T)(cached_kb * 1024), d = ci.CurrentSize - want, e = want - ci.CurrentSize;
        /* a 32-bit process sees the size modulo 4 GB; the cache moves a little between the two reads */
        check(cached_kb && ci.CurrentSize > 0 && (d < e ? d : e) < (SIZE_T)256 * 1024 * 1024,
              "its current size is the kernel page cache (/proc/meminfo Cached)");
    }
    check(ci.PeakSize + 256 * 1024 * 1024 >= ci.CurrentSize || sizeof(SIZE_T) == 4, "...the peak is not below the current size");
    check((ULONGLONG)ci.MinimumWorkingSet * si.dwPageSize == min &&
          ((ULONGLONG)ci.MaximumWorkingSet * si.dwPageSize == max || max == (SIZE_T)-1 - (si.dwPageSize - 1)) && ci.Flags == flags,
          "...the working set limits (in pages) are what GetSystemFileCacheSize reports");
    check(ci.CurrentSizeIncludingTransitionInPages > 0, "...and the size in pages is set");

    /* --- Set: validation --- */
    SetLastError(0);
    check(!pSet(1 << 20, 1 << 10, 0) && GetLastError() == ERROR_INVALID_PARAMETER, "a minimum above the maximum is ERROR_INVALID_PARAMETER");
    SetLastError(0);
    check(!pSet(1 << 20, 8 << 20, 0x10) && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown flag is ERROR_INVALID_PARAMETER");
    SetLastError(0);
    check(!pSet(1 << 20, 8 << 20, 0x01 | 0x02) && GetLastError() == ERROR_INVALID_PARAMETER, "MAX_HARD_ENABLE with MAX_HARD_DISABLE is rejected");
    SetLastError(0);
    check(!pSet(1 << 20, 8 << 20, 0x04 | 0x08) && GetLastError() == ERROR_INVALID_PARAMETER, "MIN_HARD_ENABLE with MIN_HARD_DISABLE is rejected");
    check(pGet(&min2, &max2, &f2) && min2 == min && max2 == max && f2 == flags, "...and none of those changed the limits");
    check(pSet((SIZE_T)-1, (SIZE_T)-1, 0), "(-1, -1) asks for a flush and succeeds");
    check(pGet(&min2, &max2, &f2) && min2 == min && max2 == max && f2 == flags, "...without changing the limits");

    /* --- Set: effect, seen from here and from another process --- */
    check(pSet(2 << 20, 64 << 20, 0x01 | 0x04), "SetSystemFileCacheSize sets hard limits");
    check(pGet(&min2, &max2, &f2) && min2 == (2 << 20) && max2 == (64 << 20) && f2 == 5, "GetSystemFileCacheSize reads them back");
    for (i = 1; i < argc; i++)
        check(run_other(argv[i], 2 << 20, 64 << 20, 5) == 0, "[other process] reads the same limits");
    check(run_other(argv[0], 2 << 20, 64 << 20, 5) == 0, "[other process, same arch] reads the same limits");
    check(run_other(argv[0], 1, 2, 3) == 1, "(the second process does detect different limits)");
    check(pSet(4 << 20, 128 << 20, 0x02 | 0x08) && pGet(&min2, &max2, &f2) && min2 == (4 << 20) && max2 == (128 << 20) && f2 == 0x0a,
          "the limits can be changed again");

    /* --- thread DPI hosting behavior --- */
    check(pGetDpi() == DPI_HOSTING_BEHAVIOR_DEFAULT, "a thread starts with the default DPI hosting behavior");
    check(pSetDpi(DPI_HOSTING_BEHAVIOR_MIXED) == DPI_HOSTING_BEHAVIOR_DEFAULT, "setting MIXED returns the previous (default)");
    check(pGetDpi() == DPI_HOSTING_BEHAVIOR_MIXED, "...and Get reads MIXED back");
    SetLastError(0);
    check(pSetDpi((DPI_HOSTING_BEHAVIOR)5) == DPI_HOSTING_BEHAVIOR_INVALID && GetLastError() == ERROR_INVALID_PARAMETER,
          "an unknown value returns DPI_HOSTING_BEHAVIOR_INVALID with ERROR_INVALID_PARAMETER");
    check(pSetDpi(DPI_HOSTING_BEHAVIOR_INVALID) == DPI_HOSTING_BEHAVIOR_INVALID && pGetDpi() == DPI_HOSTING_BEHAVIOR_MIXED,
          "...INVALID itself too, and the setting is untouched");
    memset(r, 0xff, sizeof(r));
    th = CreateThread(NULL, 0, other_thread, r, 0, NULL);
    WaitForSingleObject(th, 30000);
    CloseHandle(th);
    check(r[0] == DPI_HOSTING_BEHAVIOR_DEFAULT && r[1] == DPI_HOSTING_BEHAVIOR_DEFAULT && r[2] == DPI_HOSTING_BEHAVIOR_MIXED,
          "another thread has its own setting");
    check(pGetDpi() == DPI_HOSTING_BEHAVIOR_MIXED, "...which did not touch this thread's");
    check(pSetDpi(DPI_HOSTING_BEHAVIOR_DEFAULT) == DPI_HOSTING_BEHAVIOR_MIXED && pGetDpi() == DPI_HOSTING_BEHAVIOR_DEFAULT,
          "setting back to default returns MIXED");
    (void)err;

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
