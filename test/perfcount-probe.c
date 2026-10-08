/* System performance counters and classic trace registrations
 * (patches/sg/1630).
 *
 *  - NtQuerySystemInformation(SystemPerformanceInformation) gave idle time
 *    and memory only: disk transfers, page faults, paging, pool usage and
 *    context switches were zeros (performance monitors and Task-Manager-like
 *    tools read them);
 *  - RegisterTraceGuidsW gave every registration and trace class the same
 *    handle, 0xdeadbeef, and took a missing callback or handle pointer.
 */
#include <windows.h>
#include <winternl.h>
#include <evntrace.h>
#include <stdio.h>

typedef NTSTATUS (WINAPI *query_t)(SYSTEM_INFORMATION_CLASS, void *, ULONG, ULONG *);

/* the documented layout, enough of it */
typedef struct
{
    LARGE_INTEGER IdleTime, ReadTransferCount, WriteTransferCount, OtherTransferCount;
    ULONG ReadOperationCount, WriteOperationCount, OtherOperationCount, AvailablePages, TotalCommittedPages,
          TotalCommitLimit, PeakCommitment, PageFaults, WriteCopyFaults, TransitionFaults, Reserved1,
          DemandZeroFaults, PagesRead, PageReadIos, Reserved2[2], PagefilePagesWritten, PagefilePageWriteIos,
          MappedFilePagesWritten, MappedFilePageWriteIos, PagedPoolUsage, NonPagedPoolUsage;
    ULONG rest[64];
} perf_info;

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static ULONG WINAPI control_cb(WMIDPREQUESTCODE code, void *ctx, ULONG *size, void *buf) { return 0; }

static ULONG context_switches(const perf_info *p)
{
    /* ContextSwitches is the fourth ULONG from the end of the documented structure */
    return ((const ULONG *)p)[(0x138 - 16) / 4];
}

int main(void)
{
    query_t pNtQuerySystemInformation = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    static GUID control = { 0x5a1e5a1e, 0x1234, 0x4321, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    static GUID cls = { 0x5a1e5a1f, 0x1234, 0x4321, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    TRACE_GUID_REGISTRATION reg[1] = {{ &cls, NULL }};
    TRACEHANDLE h1 = 0, h2 = 0;
    perf_info a, b;
    ULONG len, i;
    NTSTATUS status;
    BOOL have_disks;
    char *mem;
    HANDLE f;

    memset(&a, 0, sizeof(a));
    status = pNtQuerySystemInformation(2, &a, 0x138, &len);
    printf("SystemPerformanceInformation: %#lx len %lu\n", status, len);
    check(!status && len == 0x138, "SystemPerformanceInformation is 0x138 bytes");
    mem = VirtualAlloc(NULL, 16 << 20, MEM_COMMIT, PAGE_READWRITE);
    for (i = 0; i < (16 << 20); i += 4096) mem[i] = 1;
    Sleep(200);
    memset(&b, 0, sizeof(b));
    pNtQuerySystemInformation(2, &b, 0x138, &len);
    printf("reads %lu writes %lu read bytes %llu page faults %lu -> %lu context switches %lu -> %lu paged pool %lu peak %lu committed %lu\n",
           b.ReadOperationCount, b.WriteOperationCount, (unsigned long long)b.ReadTransferCount.QuadPart,
           a.PageFaults, b.PageFaults, context_switches(&a), context_switches(&b), b.PagedPoolUsage,
           b.PeakCommitment, b.TotalCommittedPages);
    check(b.PageFaults > a.PageFaults, "page faults are counted (and grow when memory is touched)");
    check(context_switches(&b) > context_switches(&a), "context switches are counted");
    check(b.PeakCommitment >= b.TotalCommittedPages && b.PeakCommitment, "the peak commitment is kept");
    check(b.PagedPoolUsage || b.NonPagedPoolUsage, "pool usage is given");
    f = CreateFileA("Z:\\proc\\diskstats", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    have_disks = f != INVALID_HANDLE_VALUE;
    if (have_disks) CloseHandle(f);
    if (have_disks) check(b.ReadOperationCount && b.ReadTransferCount.QuadPart, "disk reads are counted");

    /* trace registrations */
    check(RegisterTraceGuidsW(control_cb, NULL, &control, 1, reg, NULL, NULL, &h1) == ERROR_SUCCESS,
          "RegisterTraceGuids");
    check(RegisterTraceGuidsW(control_cb, NULL, &control, 0, NULL, NULL, NULL, &h2) == ERROR_SUCCESS,
          "... again");
    printf("handles %llx %llx class %p\n", (unsigned long long)h1, (unsigned long long)h2, reg[0].RegHandle);
    check(h1 && h2 && h1 != h2 && h1 != 0xdeadbeef, "each registration has a handle of its own");
    check(reg[0].RegHandle && reg[0].RegHandle != (HANDLE)0xdeadbeef, "... and each trace class");
    check(RegisterTraceGuidsW(NULL, NULL, &control, 0, NULL, NULL, NULL, &h2) == ERROR_INVALID_PARAMETER,
          "no callback: ERROR_INVALID_PARAMETER");
    check(UnregisterTraceGuids(h1) == ERROR_SUCCESS, "UnregisterTraceGuids");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
