/* Process and thread settings that are set and read back (patches/sg/1602).
 *
 *  - GetProcessMitigationPolicy writes the policy (the stub returned TRUE and
 *    left the caller's buffer as it was); SetProcessMitigationPolicy keeps
 *    what it is given, refuses to turn a policy off again, and refuses bad
 *    lengths and policies with ERROR_INVALID_PARAMETER;
 *  - DEP reads as on and permanent in a 64-bit process;
 *  - SetProcessInformation/GetProcessInformation: memory priority, power
 *    throttling (bad masks refused), protection level;
 *  - SetThreadInformation/GetThreadInformation: memory priority, power
 *    throttling, dynamic code policy, on this thread and on another thread.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { ULONG Version, ControlMask, StateMask; } throttling_state;
typedef struct { DWORD Flags; BOOLEAN Permanent; } dep_policy;

enum { pi_memory_priority = 0, pi_app_memory = 2, pi_power_throttling = 4, pi_protection_level = 7 };
enum { ti_memory_priority = 0, ti_absolute_cpu_priority = 1, ti_dynamic_code = 2, ti_power_throttling = 3 };
enum { pol_dep = 0, pol_aslr = 1, pol_dynamic_code = 2, pol_strict_handle = 3, pol_options_mask = 5,
       pol_extension_point = 6, pol_signature = 8, pol_font = 9, pol_image_load = 10 };

typedef BOOL (WINAPI *get_mitigation_t)(HANDLE, int, void *, SIZE_T);
typedef BOOL (WINAPI *set_mitigation_t)(int, void *, SIZE_T);
typedef BOOL (WINAPI *get_info_t)(HANDLE, int, void *, DWORD);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static DWORD WINAPI idle_thread(void *arg)
{
    WaitForSingleObject(arg, INFINITE);
    return 0;
}

int main(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    get_mitigation_t pGetProcessMitigationPolicy = (void *)GetProcAddress(k32, "GetProcessMitigationPolicy");
    set_mitigation_t pSetProcessMitigationPolicy = (void *)GetProcAddress(k32, "SetProcessMitigationPolicy");
    get_info_t pGetProcessInformation = (void *)GetProcAddress(k32, "GetProcessInformation");
    get_info_t pSetProcessInformation = (void *)GetProcAddress(k32, "SetProcessInformation");
    get_info_t pGetThreadInformation = (void *)GetProcAddress(k32, "GetThreadInformation");
    get_info_t pSetThreadInformation = (void *)GetProcAddress(k32, "SetThreadInformation");
    DWORD flags, mask[3], level, prio;
    throttling_state ts;
    dep_policy dep;
    HANDLE thread, stop;
    BOOL ok;

    if (!pGetThreadInformation) { printf("FAIL  GetThreadInformation is not exported\n"); failures++; }

    /* mitigation policies */
    flags = 0xdeadbeef;
    ok = pGetProcessMitigationPolicy(GetCurrentProcess(), pol_signature, &flags, sizeof(flags));
    printf("signature policy: ok %d flags %#lx\n", ok, flags);
    check(ok && flags == 0, "GetProcessMitigationPolicy writes the policy (none set yet)");

    flags = 0x1;  /* MicrosoftSignedOnly */
    ok = pSetProcessMitigationPolicy(pol_signature, &flags, sizeof(flags));
    check(ok, "SetProcessMitigationPolicy(signature) succeeds");
    flags = 0xdeadbeef;
    ok = pGetProcessMitigationPolicy(GetCurrentProcess(), pol_signature, &flags, sizeof(flags));
    check(ok && flags == 0x1, "... and the policy reads back");
    flags = 0;
    SetLastError(0xdeadbeef);
    ok = pSetProcessMitigationPolicy(pol_signature, &flags, sizeof(flags));
    printf("turning it off: ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(!ok && GetLastError() == ERROR_ACCESS_DENIED, "... and cannot be turned off again");

    flags = 0x1;  /* DisableExtensionPoints */
    ok = pSetProcessMitigationPolicy(pol_extension_point, &flags, sizeof(flags));
    flags = 0;
    ok = ok && pGetProcessMitigationPolicy(GetCurrentProcess(), pol_extension_point, &flags, sizeof(flags));
    check(ok && flags == 0x1, "extension point policy is kept");

    SetLastError(0xdeadbeef);
    ok = pGetProcessMitigationPolicy(GetCurrentProcess(), pol_signature, &flags, 2);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "a wrong length is refused (ERROR_INVALID_PARAMETER)");
    SetLastError(0xdeadbeef);
    ok = pGetProcessMitigationPolicy(GetCurrentProcess(), 1000, &flags, sizeof(flags));
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown policy is refused");

    memset(mask, 0xcc, sizeof(mask));
    ok = pGetProcessMitigationPolicy(GetCurrentProcess(), pol_options_mask, mask, 2 * sizeof(ULONG64));
    check(ok && mask[0] == 0 && mask[1] == 0, "the options mask is written");

    memset(&dep, 0xcc, sizeof(dep));
    ok = pGetProcessMitigationPolicy(GetCurrentProcess(), pol_dep, &dep, sizeof(dep));
    printf("DEP: ok %d flags %#lx permanent %d\n", ok, dep.Flags, dep.Permanent);
#ifdef _WIN64
    check(ok && (dep.Flags & 1) && dep.Permanent == 1, "DEP reads as on and permanent (64-bit)");
#else
    check(ok && dep.Flags != 0xcccccccc, "DEP policy is written (32-bit)");
#endif

    /* process information */
    prio = 1;
    ok = pSetProcessInformation(GetCurrentProcess(), pi_memory_priority, &prio, sizeof(prio));
    printf("SetProcessInformation(memory priority): ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(ok, "SetProcessInformation(ProcessMemoryPriority) succeeds");
    prio = 0xdead;
    ok = pGetProcessInformation(GetCurrentProcess(), pi_memory_priority, &prio, sizeof(prio));
    check(ok && prio == 1, "... and GetProcessInformation reads it back");
    prio = 9;
    SetLastError(0xdeadbeef);
    ok = pSetProcessInformation(GetCurrentProcess(), pi_memory_priority, &prio, sizeof(prio));
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "a memory priority above normal is refused");

    ts.Version = 1; ts.ControlMask = 1; ts.StateMask = 1;
    ok = pSetProcessInformation(GetCurrentProcess(), pi_power_throttling, &ts, sizeof(ts));
    check(ok, "SetProcessInformation(ProcessPowerThrottling) succeeds");
    memset(&ts, 0, sizeof(ts));
    ok = pGetProcessInformation(GetCurrentProcess(), pi_power_throttling, &ts, sizeof(ts));
    printf("process throttling: ok %d version %lu control %#lx state %#lx\n", ok, ts.Version, ts.ControlMask, ts.StateMask);
    check(ok && ts.Version == 1 && ts.ControlMask == 1 && ts.StateMask == 1, "... and reads back");
    ts.Version = 1; ts.ControlMask = 1; ts.StateMask = 2;
    SetLastError(0xdeadbeef);
    ok = pSetProcessInformation(GetCurrentProcess(), pi_power_throttling, &ts, sizeof(ts));
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "a state outside the control mask is refused");

    level = 0;
    ok = pGetProcessInformation(GetCurrentProcess(), pi_protection_level, &level, sizeof(level));
    check(ok && level == 0xfffffffe, "ProcessProtectionLevelInfo: PROTECTION_LEVEL_NONE");

    /* thread information, this thread */
    prio = 2;
    ok = pSetThreadInformation(GetCurrentThread(), ti_memory_priority, &prio, sizeof(prio));
    prio = 0xdead;
    ok = ok && pGetThreadInformation && pGetThreadInformation(GetCurrentThread(), ti_memory_priority, &prio, sizeof(prio));
    check(ok && prio == 2, "thread memory priority is kept");
    ts.Version = 1; ts.ControlMask = 1; ts.StateMask = 0;
    ok = pSetThreadInformation(GetCurrentThread(), ti_power_throttling, &ts, sizeof(ts));
    memset(&ts, 0xcc, sizeof(ts));
    ok = ok && pGetThreadInformation && pGetThreadInformation(GetCurrentThread(), ti_power_throttling, &ts, sizeof(ts));
    check(ok && ts.Version == 1 && ts.ControlMask == 1 && ts.StateMask == 0, "thread power throttling is kept");
    flags = 1;
    SetLastError(0xdeadbeef);
    ok = pSetThreadInformation(GetCurrentThread(), ti_dynamic_code, &flags, sizeof(flags));
    printf("SetThreadInformation(dynamic code): ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    flags = 0;
    ok = ok && pGetThreadInformation && pGetThreadInformation(GetCurrentThread(), ti_dynamic_code, &flags, sizeof(flags));
    check(ok && flags == 1, "thread dynamic code policy is kept");
    ok = pGetThreadInformation && pGetThreadInformation(GetCurrentThread(), ti_absolute_cpu_priority, &level, sizeof(level));
    printf("absolute priority %ld\n", (long)level);
    check(ok && level == 8, "ThreadAbsoluteCpuPriority: 8 for a normal thread of a normal process");

    /* another thread of this process */
    stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, idle_thread, stop, 0, NULL);
    prio = 0xdead;
    ok = pGetThreadInformation && pGetThreadInformation(thread, ti_memory_priority, &prio, sizeof(prio));
    check(ok && prio == 5, "a new thread's memory priority is normal");
    prio = 3;
    ok = pSetThreadInformation(thread, ti_memory_priority, &prio, sizeof(prio));
    prio = 0xdead;
    ok = ok && pGetThreadInformation && pGetThreadInformation(thread, ti_memory_priority, &prio, sizeof(prio));
    check(ok && prio == 3, "another thread's memory priority is kept");
    prio = 0xdead;
    if (pGetThreadInformation) pGetThreadInformation(GetCurrentThread(), ti_memory_priority, &prio, sizeof(prio));
    check(prio == 2, "... apart from this thread's");
    SetEvent(stop);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
