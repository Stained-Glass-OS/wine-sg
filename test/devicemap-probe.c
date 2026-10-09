/* The remaining NtQueryInformationProcess information classes
 * (patches/sg/2203): ProcessDeviceMap and ProcessLUIDDeviceMapsEnabled were
 * "Unimplemented information class" FIXMEs (STATUS_INVALID_INFO_CLASS,
 * wrongly -- both are valid query classes on real Windows);
 * ProcessWorkingSetWatch and ProcessHandleTracing answered
 * STATUS_INVALID_INFO_CLASS instead of STATUS_UNSUCCESSFUL (not yet
 * enabled); ProcessBasePriority and friends are genuinely set-only, where
 * STATUS_INVALID_INFO_CLASS was already right -- only the FIXME noise on
 * every normal probe was a problem, fixed by downgrading it to a TRACE. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define ProcessWorkingSetWatch_ 15
#define ProcessDeviceMap_ 23
#define ProcessForegroundInformation_ 25
#define ProcessLUIDDeviceMapsEnabled_ 28
#define ProcessHandleTracing_ 32
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001)
#define STATUS_INVALID_INFO_CLASS ((NTSTATUS)0xC0000003)

typedef struct
{
    union
    {
        struct { HANDLE DirectoryHandle; } Set;
        struct { ULONG DriveMap; UCHAR DriveType[32]; } Query;
    };
} PROCESS_DEVICEMAP_INFORMATION_;

static NTSTATUS (WINAPI *pNtQueryInformationProcess)(HANDLE, ULONG, void *, ULONG, ULONG *);

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    PROCESS_DEVICEMAP_INFORMATION_ map;
    ULONG enabled = 0xdeadbeef, len;
    NTSTATUS status;
    WCHAR sysdir[MAX_PATH];
    char drive;

    pNtQueryInformationProcess = (void *)GetProcAddress( ntdll, "NtQueryInformationProcess" );
    if (!pNtQueryInformationProcess) { printf("FAIL  NtQueryInformationProcess missing\n"); return 1; }

    GetSystemDirectoryW( sysdir, MAX_PATH );
    drive = (char)sysdir[0];

    memset( &map, 0xcc, sizeof(map) );
    status = pNtQueryInformationProcess( GetCurrentProcess(), ProcessDeviceMap_, &map, sizeof(map), &len );
    check( status == 0, "ProcessDeviceMap succeeds (was STATUS_INVALID_INFO_CLASS)" );
    check( (map.Query.DriveMap & (1u << (drive - 'A'))) != 0, "the boot drive's bit is set in DriveMap" );

    status = pNtQueryInformationProcess( GetCurrentProcess(), ProcessDeviceMap_, &map, sizeof(map) - 1, &len );
    check( status == STATUS_INFO_LENGTH_MISMATCH, "a wrong-size ProcessDeviceMap buffer is rejected" );

    status = pNtQueryInformationProcess( GetCurrentProcess(), ProcessLUIDDeviceMapsEnabled_, &enabled, sizeof(enabled), &len );
    check( status == 0 && enabled == 0, "ProcessLUIDDeviceMapsEnabled answers disabled (was STATUS_INVALID_INFO_CLASS)" );

    status = pNtQueryInformationProcess( GetCurrentProcess(), ProcessWorkingSetWatch_, NULL, 0, &len );
    check( status == STATUS_UNSUCCESSFUL, "ProcessWorkingSetWatch (never enabled) is STATUS_UNSUCCESSFUL" );

    status = pNtQueryInformationProcess( GetCurrentProcess(), ProcessHandleTracing_, NULL, 0, &len );
    check( status == STATUS_UNSUCCESSFUL, "ProcessHandleTracing (never enabled) is STATUS_UNSUCCESSFUL" );

    /* a handle with only the limited query right is refused (these two need
     * PROCESS_QUERY_INFORMATION), a full query handle to ourselves is not */
    {
        HANDLE limited = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId() );
        HANDLE full = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId() );
        NTSTATUS a = pNtQueryInformationProcess( limited, ProcessWorkingSetWatch_, NULL, 0, &len );
        NTSTATUS b = pNtQueryInformationProcess( limited, ProcessHandleTracing_, NULL, 0, &len );
        NTSTATUS c = pNtQueryInformationProcess( full, ProcessWorkingSetWatch_, NULL, 0, &len );
        check( a == (NTSTATUS)0xC0000022 && b == (NTSTATUS)0xC0000022,
               "a PROCESS_QUERY_LIMITED_INFORMATION handle is STATUS_ACCESS_DENIED for them" );
        check( c == STATUS_UNSUCCESSFUL, "a PROCESS_QUERY_INFORMATION handle gets STATUS_UNSUCCESSFUL" );
        CloseHandle( limited );
        CloseHandle( full );
    }

    /* genuinely set-only: still STATUS_INVALID_INFO_CLASS, just quieter now */
    status = pNtQueryInformationProcess( GetCurrentProcess(), ProcessForegroundInformation_, NULL, 0, &len );
    check( status == STATUS_INVALID_INFO_CLASS, "a set-only class is still STATUS_INVALID_INFO_CLASS to query" );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures ? 1 : 0;
}
