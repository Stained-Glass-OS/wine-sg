/* Probe for patches/sg/5600: SystemProcessorPerformanceInformation without a
 * host /proc/stat. Times are real: elapsed time per CPU in 100 ns units
 * (idle + user + the kernel time without its idle part), monotonic, kernel
 * time includes idle time, busy time of this process shows up as a lower
 * bound. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { LARGE_INTEGER IdleTime, KernelTime, UserTime, Reserved1[2]; ULONG Reserved2; } SPPI;

static NTSTATUS (WINAPI *pNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS, void *, ULONG, ULONG *);

int main(void)
{
    SYSTEM_INFO si;
    SPPI a[64], b[64], c[64];
    ULONG len, i, n;
    NTSTATUS st;
    ULONGLONG t0, dt_elapsed, busy = 0;
    DWORD start;
    volatile unsigned long long x = 0;

    pNtQuerySystemInformation = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    GetSystemInfo(&si);
    n = si.dwNumberOfProcessors;
    if (n > 64) n = 64;
    memset(a, 0xcc, sizeof(a));
    st = pNtQuerySystemInformation(8, a, n * sizeof(SPPI), &len);
    check(!st && len == n * sizeof(SPPI), "query: %#lx len %lu for %lu cpus", st, len, n);
    for (i = 0; i < n; i++)
    {
        check(a[i].KernelTime.QuadPart >= a[i].IdleTime.QuadPart, "cpu %lu: kernel includes idle", i);
        check(a[i].IdleTime.QuadPart + a[i].UserTime.QuadPart > 10000000, "cpu %lu: elapsed time is real (%lld)", i,
              a[i].KernelTime.QuadPart + a[i].UserTime.QuadPart);
    }
    t0 = GetTickCount64();
    Sleep(500);
    start = GetTickCount();
    while (GetTickCount() - start < 500) x++;
    st = pNtQuerySystemInformation(8, b, n * sizeof(SPPI), &len);
    check(!st, "second query %#lx", st);
    dt_elapsed = GetTickCount64() - t0;
    for (i = 0; i < n; i++)
    {
        ULONGLONG e1 = a[i].KernelTime.QuadPart + a[i].UserTime.QuadPart, e2 = b[i].KernelTime.QuadPart + b[i].UserTime.QuadPart;
        check(b[i].IdleTime.QuadPart >= a[i].IdleTime.QuadPart && b[i].UserTime.QuadPart >= a[i].UserTime.QuadPart &&
              b[i].KernelTime.QuadPart >= a[i].KernelTime.QuadPart, "cpu %lu: monotonic", i);
        check(e2 - e1 >= dt_elapsed * 9000 && e2 - e1 <= dt_elapsed * 15000, "cpu %lu: elapsed grew by %llu ms for %llu ms", i,
              (e2 - e1) / 10000, dt_elapsed);
        busy += (b[i].UserTime.QuadPart - a[i].UserTime.QuadPart) + (b[i].KernelTime.QuadPart - b[i].IdleTime.QuadPart)
                - (a[i].KernelTime.QuadPart - a[i].IdleTime.QuadPart);
    }
    check(busy >= 3500000, "the busy half second shows as busy time (%llu ms)", busy / 10000);
    st = pNtQuerySystemInformation(8, c, n * sizeof(SPPI), &len);
    for (i = 0; i < n; i++)
        check(c[i].IdleTime.QuadPart >= b[i].IdleTime.QuadPart && c[i].UserTime.QuadPart >= b[i].UserTime.QuadPart &&
              c[i].KernelTime.QuadPart >= b[i].KernelTime.QuadPart, "cpu %lu: monotonic again", i);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
