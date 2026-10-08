/* CPU time and cycle counts of threads and processes (patches/sg/1600).
 *
 *  - GetProcessTimes of another process gives that process's time, not the
 *    caller's: a child spins ~600 ms while the parent sleeps;
 *  - the same through a PROCESS_QUERY_LIMITED_INFORMATION handle;
 *  - QueryProcessCycleTime of the child grows with its CPU time;
 *  - QueryThreadCycleTime works (it failed with ERROR_CALL_NOT_IMPLEMENTED)
 *    for the current thread and for another thread, and its cycles track
 *    the thread's CPU time at a plausible clock rate;
 *  - QueryIdleProcessorCycleTime is not all zero.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void spin(DWORD ms)
{
    DWORD start = GetTickCount();
    volatile unsigned int x = 0;
    while (GetTickCount() - start < ms) x++;
}

static ULONG64 ft64(FILETIME ft)
{
    return ((ULONG64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static ULONG64 thread_cpu(HANDLE thread)
{
    FILETIME c, e, k, u;
    GetThreadTimes(thread, &c, &e, &k, &u);
    return ft64(k) + ft64(u);
}

static DWORD WINAPI spinner(void *arg)
{
    spin(400);
    SetEvent(arg);
    Sleep(INFINITE);
    return 0;
}

int main(int argc, char **argv)
{
    char cmd[MAX_PATH + 16];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    FILETIME c, e, k, u;
    ULONG64 child_cpu, own_cpu, cycles, cycles2, t1, t2;
    HANDLE limited, done, thread;
    ULONG size;
    ULONG64 idle[256];
    BOOL ok;
    unsigned int i, nonzero;

    if (argc > 1 && !strcmp(argv[1], "child"))
    {
        HANDLE ev = OpenEventA(EVENT_MODIFY_STATE, FALSE, "sg-cputime-child");
        spin(600);
        SetEvent(ev);
        Sleep(30000);
        return 0;
    }

    /* the child process */
    done = CreateEventA(NULL, TRUE, FALSE, "sg-cputime-child");
    snprintf(cmd, sizeof(cmd), "\"%s\" child", argv[0]);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("FAIL  could not start the child (%lu)\n", GetLastError());
        printf("RESULT: FAIL\n");
        return 1;
    }
    WaitForSingleObject(done, 20000);
    Sleep(100);

    ok = GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    own_cpu = ft64(k) + ft64(u);
    ok = ok && GetProcessTimes(pi.hProcess, &c, &e, &k, &u);
    child_cpu = ft64(k) + ft64(u);
    printf("own cpu %llu ms, child cpu %llu ms\n", own_cpu / 10000, child_cpu / 10000);
    check(ok && child_cpu >= 4000000, "GetProcessTimes of another process gives its CPU time");
    check(ok && child_cpu != own_cpu, "... not the caller's");

    limited = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    ok = limited && GetProcessTimes(limited, &c, &e, &k, &u);
    printf("limited handle: child cpu %llu ms\n", (ft64(k) + ft64(u)) / 10000);
    check(ok && ft64(k) + ft64(u) >= 4000000, "... also through a PROCESS_QUERY_LIMITED_INFORMATION handle");

    cycles = 0;
    ok = QueryProcessCycleTime(pi.hProcess, &cycles);
    printf("child cycles %llu\n", cycles);
    check(ok && cycles >= 200000000ull, "QueryProcessCycleTime of the child counts its cycles");
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    cycles = 0;
    ok = QueryProcessCycleTime(GetCurrentProcess(), &cycles);
    check(ok, "QueryProcessCycleTime of the current process succeeds");

    /* the current thread */
    cycles = 0;
    SetLastError(0xdeadbeef);
    ok = QueryThreadCycleTime(GetCurrentThread(), &cycles);
    printf("QueryThreadCycleTime: ok %d cycles %llu err %lu\n", ok, cycles, ok ? 0 : GetLastError());
    check(ok, "QueryThreadCycleTime of the current thread succeeds");
    t1 = thread_cpu(GetCurrentThread());
    spin(500);
    cycles2 = 0;
    QueryThreadCycleTime(GetCurrentThread(), &cycles2);
    t2 = thread_cpu(GetCurrentThread());
    printf("500 ms spin: %llu cycles over %llu ms CPU\n", cycles2 - cycles, (t2 - t1) / 10000);
    check(cycles2 > cycles, "the thread's cycle count grows as it runs");
    if (t2 > t1)
    {
        double hz = (double)(cycles2 - cycles) / ((double)(t2 - t1) / 1e7);
        printf("implied clock %.0f MHz\n", hz / 1e6);
        check(hz > 3e8 && hz < 1e10, "its cycles track CPU time at a plausible clock rate");
    }
    else check(0, "the thread's CPU time did not grow");

    /* another thread */
    done = CreateEventA(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, spinner, done, 0, NULL);
    WaitForSingleObject(done, 10000);
    Sleep(50);
    cycles = 0;
    ok = QueryThreadCycleTime(thread, &cycles);
    printf("other thread cycles %llu\n", cycles);
    check(ok && cycles >= 100000000ull, "QueryThreadCycleTime of another thread counts its cycles");
    TerminateThread(thread, 0);
    CloseHandle(thread);

    /* idle cycles */
    size = sizeof(idle);
    memset(idle, 0, sizeof(idle));
    ok = QueryIdleProcessorCycleTime(&size, idle);
    for (i = nonzero = 0; ok && i < size / sizeof(ULONG64); i++) if (idle[i]) nonzero++;
    printf("idle cycles: ok %d, %u of %lu processors non-zero\n", ok, nonzero, size / (ULONG)sizeof(ULONG64));
    check(ok && nonzero > 0, "QueryIdleProcessorCycleTime counts idle cycles");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
