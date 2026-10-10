/* Small file-system and URL answers (patches/sg/2234): FileFsControlInformation
 * of a volume without quotas, FileFsLabelInformation is not a query class,
 * FILE_SKIP_SET_USER_EVENT_ON_FAST_IO is accepted, UrlIs(URLIS_HASQUERY). */
#include <windows.h>
#include <winternl.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef struct
{
    LARGE_INTEGER FreeSpaceStartFiltering, FreeSpaceThreshold, FreeSpaceStopFiltering;
    LARGE_INTEGER DefaultQuotaThreshold, DefaultQuotaLimit;
    ULONG FileSystemControlFlags;
} FS_CONTROL;

#define STATUS_INVALID_INFO_CLASS ((NTSTATUS)0xC0000003)
#define STATUS_BUFFER_TOO_SMALL   ((NTSTATUS)0xC0000023)

int main(void)
{
    NTSTATUS (WINAPI *pQueryVol)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int);
    NTSTATUS (WINAPI *pSetInfo)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int);
    HMODULE nt = GetModuleHandleA( "ntdll.dll" );
    IO_STATUS_BLOCK io;
    FS_CONTROL ctl;
    char buf[64];
    NTSTATUS status;
    HANDLE h;
    struct { ULONG Flags; } comp;

    pQueryVol = (void *)GetProcAddress( nt, "NtQueryVolumeInformationFile" );
    pSetInfo = (void *)GetProcAddress( nt, "NtSetInformationFile" );

    h = CreateFileA( "C:\\windows", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    check( h != INVALID_HANDLE_VALUE, "open C:\\windows" );

    memset( &ctl, 0xcc, sizeof(ctl) );
    status = pQueryVol( h, &io, &ctl, sizeof(ctl), 6 /* FileFsControlInformation */ );
    check( !status && io.Information == sizeof(ctl), "FileFsControlInformation succeeds" );
    check( ctl.DefaultQuotaThreshold.QuadPart == -1 && ctl.DefaultQuotaLimit.QuadPart == -1 && ctl.FileSystemControlFlags == 0 &&
           !ctl.FreeSpaceStartFiltering.QuadPart && !ctl.FreeSpaceThreshold.QuadPart && !ctl.FreeSpaceStopFiltering.QuadPart,
           "no quotas: thresholds -1, nothing tracked" );
    status = pQueryVol( h, &io, &ctl, sizeof(ctl) - 1, 6 );
    check( status == STATUS_BUFFER_TOO_SMALL, "a short buffer: STATUS_BUFFER_TOO_SMALL" );
    status = pQueryVol( h, &io, buf, sizeof(buf), 2 /* FileFsLabelInformation */ );
    check( status == STATUS_INVALID_INFO_CLASS, "FileFsLabelInformation is not a query class" );

    CloseHandle( h );
    h = CreateFileA( "C:\\windows\\system.ini", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL );
    comp.Flags = 4; /* FILE_SKIP_SET_USER_EVENT_ON_FAST_IO */
    status = pSetInfo( h, &io, &comp, sizeof(comp), 41 /* FileIoCompletionNotificationInformation */ );
    check( !status, "FILE_SKIP_SET_USER_EVENT_ON_FAST_IO is accepted" );
    CloseHandle( h );

    check( UrlIsA( "http://x.org/a?b=1", URLIS_HASQUERY ), "UrlIsA HASQUERY with a query" );
    check( !UrlIsA( "http://x.org/a", URLIS_HASQUERY ), "UrlIsA HASQUERY without" );
    check( UrlIsW( L"http://x.org/a?b=1", URLIS_HASQUERY ), "UrlIsW HASQUERY with a query" );
    check( !UrlIsW( L"http://x.org/a", URLIS_HASQUERY ), "UrlIsW HASQUERY without" );
    check( !UrlIsW( NULL, URLIS_HASQUERY ), "NULL" );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
