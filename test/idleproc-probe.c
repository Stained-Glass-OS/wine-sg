/* idleproc gate probe (wine-sg 8402): the System Idle Process and processes' cycle times in
 * SystemProcessInformation, and cycle-based CPU usage computed from them as System Informer does.
 * Prints KEY=1 for each check that holds (KEY=0 and why otherwise), then done=1. */
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <stdio.h>

struct sample { DWORD pid[1024]; ULONG64 cycles[1024]; ULONG64 create[1024]; int count; };

/* the winternl.h of mingw names the fields differently: read them at Windows' offsets */
typedef struct
{
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
} SPI;

static BYTE *query(void)
{
    ULONG size = 1 << 20, len;
    BYTE *buf;
    NTSTATUS st;

    for (;;)
    {
        if (!(buf = malloc(size))) return NULL;
        st = NtQuerySystemInformation(SystemProcessInformation, buf, size, &len);
        if (st != STATUS_INFO_LENGTH_MISMATCH) break;
        free(buf);
        size *= 2;
    }
    if (st) { free(buf); return NULL; }
    return buf;
}

static void take(struct sample *s)
{
    BYTE *buf = query();
    SPI *p;

    s->count = 0;
    if (!buf) return;
    for (p = (SPI *)buf; s->count < 1024; p = (SPI *)((BYTE *)p + p->NextEntryOffset))
    {
        s->pid[s->count] = (DWORD)(ULONG_PTR)p->UniqueProcessId;
        s->cycles[s->count] = p->CycleTime;
        s->create[s->count] = p->CreateTime.QuadPart;
        s->count++;
        if (!p->NextEntryOffset) break;
    }
    free(buf);
}

static volatile LONG stop;

static DWORD WINAPI spin(void *arg)
{
    volatile ULONG64 x = 0;
    while (!stop) x++;
    return 0;
}

int main(void)
{
    BYTE *buf;
    SPI *first, *p, *self = NULL;
    DWORD me = GetCurrentProcessId(), pids[4096], got = 0, i;
    ULONG64 own_cycles = 0, total = 0, idle = 0, own = 0;
    struct sample a, b;
    PROCESSENTRY32W pe = { sizeof(pe) };
    HANDLE snap, thread;
    BOOL found;
    int j;

    /* some CPU time of its own first: /proc's process times tick in 10 ms steps */
    {
        DWORD start = GetTickCount();
        volatile ULONG64 x = 0;
        while (GetTickCount() - start < 300) x++;
    }
    buf = query();
    first = (SPI *)buf;
    if (!buf) { printf("query=0\ndone=1\n"); return 0; }
    printf("first: pid %lu threads %lu kernel %llu cycles %llu name %u\n", (DWORD)(ULONG_PTR)first->UniqueProcessId,
           first->NumberOfThreads, first->KernelTime.QuadPart, first->CycleTime, first->ImageName.Length);
    printf("idle=%d\n", first->UniqueProcessId == 0 && !first->ImageName.Length);
    printf("idlethreads=%d\n", first->NumberOfThreads == GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    printf("idletimes=%d\n", first->KernelTime.QuadPart > 0 && first->CycleTime > 0);
    for (p = first;; p = (SPI *)((BYTE *)p + p->NextEntryOffset))
    {
        if ((DWORD)(ULONG_PTR)p->UniqueProcessId == me) self = p;
        if (!p->NextEntryOffset) break;
    }
    /* the process's own entry: its CPU time as cycles, QueryProcessCycleTime's measure */
    QueryProcessCycleTime(GetCurrentProcess(), &own_cycles);
    printf("own: %llu, QueryProcessCycleTime %llu\n", self ? self->CycleTime : 0, own_cycles);
    printf("owncycles=%d\n", self && self->CycleTime > 0 && own_cycles >= self->CycleTime / 2);
    free(buf);

    /* EnumProcesses and toolhelp have process 0 too */
    found = FALSE;
    if (EnumProcesses(pids, sizeof(pids), &got))
        for (i = 0; i < got / sizeof(DWORD); i++) if (!pids[i]) found = TRUE;
    printf("enumprocesses=%d\n", found);
    found = FALSE;
    if ((snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) != INVALID_HANDLE_VALUE)
    {
        if (Process32FirstW(snap, &pe)) do if (!pe.th32ProcessID) found = TRUE; while (Process32NextW(snap, &pe));
        CloseHandle(snap);
    }
    printf("toolhelp=%d\n", found);

    /* System Informer's cycle-based usage over a second with one thread busy: the sum of every
     * process's cycle delta, the idle process's included, and each one's share of it */
    take(&a);
    thread = CreateThread(NULL, 0, spin, NULL, 0, NULL);
    Sleep(1000);
    take(&b);
    stop = 1;
    WaitForSingleObject(thread, INFINITE);
    for (j = 0; j < b.count; j++)
    {
        int k;
        ULONG64 delta = b.cycles[j];

        for (k = 0; k < a.count; k++)
            if (a.pid[k] == b.pid[j] && a.create[k] == b.create[j]) { delta = b.cycles[j] - a.cycles[k]; break; }
        total += delta;
        if (!b.pid[j]) idle = delta;
        if (b.pid[j] == me) own = delta;
    }
    printf("usage: total %llu idle %llu own %llu -> cpu %.2f%% own %.2f%%\n", total, idle, own,
           total ? 100.0 - 100.0 * idle / total : -1.0, total ? 100.0 * own / total : -1.0);
    printf("usage=%d\n", total > 0 && idle <= total && own > 0 && idle > 0);
    printf("done=1\n");
    return 0;
}
