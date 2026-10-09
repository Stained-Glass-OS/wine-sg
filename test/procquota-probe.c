/* Working set limits, pooled usage and critical processes (patches/sg/1701),
 * run by test/procquota-gate.sh 64- and 32-bit (the 32-bit one through
 * wow64): GetProcessWorkingSetSizeEx was a FIXME giving 32 MB and
 * SetProcessWorkingSetSizeEx did nothing; NtQueryInformationProcess
 * ProcessQuotaLimits was a FIXME taking only QUOTA_LIMITS and setting it
 * was "stub" STATUS_NOT_IMPLEMENTED; ProcessPooledUsageAndLimits and
 * ProcessBreakOnTermination were unimplemented classes. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

#define ProcessQuotaLimits_ 1
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#define STATUS_PRIVILEGE_NOT_HELD ((NTSTATUS)0xC0000061)
#define ProcessPooledUsageAndLimits_ 14
#define ProcessBreakOnTermination_ 29

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef struct
{
    SIZE_T PeakPagedPoolUsage, PagedPoolUsage, PagedPoolLimit;
    SIZE_T PeakNonPagedPoolUsage, NonPagedPoolUsage, NonPagedPoolLimit;
    SIZE_T PeakPagefileUsage, PagefileUsage, PagefileLimit;
} pooled_usage;

static NTSTATUS (WINAPI *pNtQueryInformationProcess)(HANDLE, ULONG, void *, ULONG, ULONG *);
static NTSTATUS (WINAPI *pNtSetInformationProcess)(HANDLE, ULONG, void *, ULONG);

static BOOL enable_debug_privilege(BOOL enable)
{
    TOKEN_PRIVILEGES tp = { 1 };
    HANDLE token;
    BOOL ret;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) return FALSE;
    LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
    ret = AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL) && GetLastError() == ERROR_SUCCESS;
    CloseHandle(token);
    return ret;
}

int main(int argc, char **argv)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    SIZE_T min = 0, max = 0;
    DWORD flags = 0;
    QUOTA_LIMITS limits;
    QUOTA_LIMITS_EX limits_ex;
    pooled_usage pooled;
    ULONG value, len;
    NTSTATUS status;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 16];

    if (argc > 1 && !strcmp(argv[1], "child"))
    {
        Sleep(10000);
        return 0;
    }

    pNtQueryInformationProcess = (void *)GetProcAddress(ntdll, "NtQueryInformationProcess");
    pNtSetInformationProcess = (void *)GetProcAddress(ntdll, "NtSetInformationProcess");

    check(GetProcessWorkingSetSize(GetCurrentProcess(), &min, &max) && min == 204800 && max == 1413120,
          "GetProcessWorkingSetSize: Windows' defaults (was 32 MB)");
    check(GetProcessWorkingSetSizeEx(GetCurrentProcess(), &min, &max, &flags)
          && flags == (QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE), "no hard limits");

    check(SetProcessWorkingSetSizeEx(GetCurrentProcess(), 4 << 20, 64 << 20, QUOTA_LIMITS_HARDWS_MIN_ENABLE),
          "SetProcessWorkingSetSizeEx");
    check(GetProcessWorkingSetSizeEx(GetCurrentProcess(), &min, &max, &flags) && min == 4 << 20 && max == 64 << 20
          && flags == (QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE), "kept (it did nothing)");
    SetLastError(0xdeadbeef);
    check(!SetProcessWorkingSetSizeEx(GetCurrentProcess(), 64 << 20, 4 << 20, 0) && GetLastError() == ERROR_INVALID_PARAMETER,
          "minimum over maximum: ERROR_INVALID_PARAMETER");
    if (GetLastError() != ERROR_INVALID_PARAMETER) printf("      error %lu\n", GetLastError());
    check(!SetProcessWorkingSetSizeEx(GetCurrentProcess(), 4 << 20, 64 << 20,
                                      QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MIN_DISABLE),
          "enable and disable: refused");
    check(SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1)
          && GetProcessWorkingSetSize(GetCurrentProcess(), &min, &max) && min == 4 << 20, "trimming leaves the limits");

    memset(&limits, 0xcc, sizeof(limits));
    status = pNtQueryInformationProcess(GetCurrentProcess(), ProcessQuotaLimits_, &limits, sizeof(limits), &len);
    check(!status && len == sizeof(limits) && limits.MaximumWorkingSetSize == 64 << 20
          && limits.PagefileLimit == (SIZE_T)-1, "ProcessQuotaLimits as QUOTA_LIMITS");
    status = pNtQueryInformationProcess(GetCurrentProcess(), ProcessQuotaLimits_, &limits_ex, sizeof(limits_ex), &len);
    check(!status && len == sizeof(limits_ex) && limits_ex.Flags == QUOTA_LIMITS_HARDWS_MIN_ENABLE,
          "and as QUOTA_LIMITS_EX, with its flags");
    status = pNtQueryInformationProcess(GetCurrentProcess(), ProcessQuotaLimits_, &limits, sizeof(limits) - 4, &len);
    check(status == STATUS_INFO_LENGTH_MISMATCH, "a wrong size: STATUS_INFO_LENGTH_MISMATCH");

    status = pNtQueryInformationProcess(GetCurrentProcess(), ProcessPooledUsageAndLimits_, &pooled, sizeof(pooled), &len);
    check(!status && len == sizeof(pooled) && pooled.PagefileUsage && pooled.PeakPagefileUsage >= pooled.PagefileUsage
          && pooled.PagefileLimit == (SIZE_T)-1, "ProcessPooledUsageAndLimits (was not implemented)");

    value = 0xdeadbeef;
    status = pNtQueryInformationProcess(GetCurrentProcess(), ProcessBreakOnTermination_, &value, sizeof(value), &len);
    check(!status && value == 0, "ProcessBreakOnTermination: not critical (was not implemented)");
    enable_debug_privilege(FALSE);
    value = 1;
    status = pNtSetInformationProcess(GetCurrentProcess(), ProcessBreakOnTermination_, &value, sizeof(value));
    check(status == STATUS_PRIVILEGE_NOT_HELD, "making it critical takes SeDebugPrivilege");
    if (enable_debug_privilege(TRUE))
    {
        status = pNtSetInformationProcess(GetCurrentProcess(), ProcessBreakOnTermination_, &value, sizeof(value));
        pNtQueryInformationProcess(GetCurrentProcess(), ProcessBreakOnTermination_, &value, sizeof(value), &len);
        check(!status && value == 1, "with it: critical");
        value = 0;
        pNtSetInformationProcess(GetCurrentProcess(), ProcessBreakOnTermination_, &value, sizeof(value));
        enable_debug_privilege(FALSE);
    }
    else printf("      (no SeDebugPrivilege to enable)\n");

    /* another process has its own */
    cmd[0] = '"';
    GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
    strcat(cmd, "\" child");
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        check(GetProcessWorkingSetSize(pi.hProcess, &min, &max) && min == 204800 && max == 1413120,
              "another process: its defaults");
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
