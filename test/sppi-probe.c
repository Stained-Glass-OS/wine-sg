/* SystemProcessorPerformanceInformation reports interrupt and DPC time from
 * the kernel's irq/softirq counters (patches/sg/2221); it used to leave them 0.
 * Compares the sum over processors with /proc/stat read before and after. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef LONG NTSTATUS;
typedef struct
{
    LARGE_INTEGER IdleTime, KernelTime, UserTime, DpcTime, InterruptTime;
    ULONG InterruptCount;
} SPPI;
static NTSTATUS (WINAPI *pQuery)(ULONG, void *, ULONG, ULONG *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* irq and softirq ticks of the combined cpu line, in 100 ns units (10 ms ticks assumed 100 Hz) */
static void proc_stat(unsigned long long *irq, unsigned long long *softirq, unsigned long long *idle)
{
    FILE *f = fopen("Z:\\proc\\stat", "r");
    char line[512];
    unsigned long long v[10] = {0};

    *irq = *softirq = *idle = 0;
    if (!f) return;
    if (fgets(line, sizeof(line), f))
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]);
    fclose(f);
    *idle = v[3];
    *irq = v[5];
    *softirq = v[6];
}

int main(void)
{
    SYSTEM_INFO si;
    SPPI *p;
    ULONG len = 0, i;
    unsigned long long b_irq, b_soft, b_idle, a_irq, a_soft, a_idle, irq = 0, dpc = 0, kernel = 0, idle = 0;
    NTSTATUS st;

    pQuery = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    GetSystemInfo(&si);
    p = calloc(si.dwNumberOfProcessors + 4, sizeof(*p));
    proc_stat(&b_irq, &b_soft, &b_idle);
    st = pQuery(8, p, si.dwNumberOfProcessors * sizeof(*p), &len);
    proc_stat(&a_irq, &a_soft, &a_idle);
    check(st == 0 && len == si.dwNumberOfProcessors * sizeof(*p), "the query succeeds for every processor");
    for (i = 0; i < si.dwNumberOfProcessors; i++)
    {
        irq += p[i].InterruptTime.QuadPart;
        dpc += p[i].DpcTime.QuadPart;
        kernel += p[i].KernelTime.QuadPart;
        idle += p[i].IdleTime.QuadPart;
        if (p[i].KernelTime.QuadPart < p[i].IdleTime.QuadPart) check(0, "kernel time includes idle time");
    }
    check(kernel >= idle, "kernel time includes idle time (summed)");
    if (b_irq || a_irq)
        check(irq / 100000 + si.dwNumberOfProcessors >= b_irq && irq / 100000 <= a_irq + si.dwNumberOfProcessors + 1,
              "the interrupt time sums to the kernel's irq ticks");
    else
        check(irq == 0, "(no irq ticks on this host)");
    check(dpc / 100000 + si.dwNumberOfProcessors >= b_soft && dpc / 100000 <= a_soft + si.dwNumberOfProcessors + 1, "the DPC time sums to the kernel's softirq ticks");
    check(dpc > 0 || b_soft == 0, "...and is not zero when the host has softirq time");
    free(p);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
