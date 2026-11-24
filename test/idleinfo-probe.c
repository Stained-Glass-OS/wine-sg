/* idleinfo-probe (patches/sg/9751, test/idleinfo-gate.sh): NtQuerySystemInformation's
 * SystemProcessorIdleInformation (42), which HWMonitor asks for on every refresh. Prints:
 *   CPUS <n>                     processors (SYSTEM_INFO)
 *   FULL <status> <returned>     a buffer for every processor
 *   IDLE <ok|bad i>              every processor's IdleTime is non-zero and close to
 *                                SystemProcessorPerformanceInformation's (within 2 s)
 *   CSTATE <total 100ns>         processor 0's C1 + C2 + C3 time
 *   SHORT <status> <returned>    a buffer one byte short of it */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    ULONGLONG IdleTime, C1Time, C2Time, C3Time;
    ULONG C1Transitions, C2Transitions, C3Transitions, Padding;
} IDLE_INFO;

typedef struct {
    LARGE_INTEGER IdleTime, KernelTime, UserTime, Reserved1[2];
    ULONG Reserved2;
} PERF_INFO;

typedef NTSTATUS (WINAPI *QSI)(ULONG, void *, ULONG, ULONG *);

int main(void)
{
    QSI qsi = (QSI)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    SYSTEM_INFO si;
    IDLE_INFO *idle;
    PERF_INFO *perf;
    ULONG ret = 0, i, n;
    NTSTATUS st;
    int bad = -1;

    GetSystemInfo(&si);
    n = si.dwNumberOfProcessors;
    printf("CPUS %lu\n", n);
    idle = calloc(n, sizeof(*idle));
    perf = calloc(n, sizeof(*perf));
    st = qsi(42, idle, n * sizeof(*idle), &ret);
    printf("FULL %08lx %lu\n", (unsigned long)st, ret);
    qsi(8, perf, n * sizeof(*perf), NULL);
    for (i = 0; i < n; i++)
    {
        LONGLONG d = (LONGLONG)idle[i].IdleTime - perf[i].IdleTime.QuadPart;
        if (!idle[i].IdleTime || d > 20000000 || d < -20000000) { bad = i; break; }
    }
    if (bad < 0) printf("IDLE ok\n"); else printf("IDLE bad %d\n", bad);
    printf("CSTATE %llu\n", idle[0].C1Time + idle[0].C2Time + idle[0].C3Time);
    ret = 0;
    st = qsi(42, idle, n * sizeof(*idle) - 1, &ret);
    printf("SHORT %08lx %lu\n", (unsigned long)st, ret);
    return 0;
}
