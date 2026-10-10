/* FileFsVolumeInformation reports a volume creation time (patches/sg/2246). */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

typedef struct { LARGE_INTEGER VolumeCreationTime; ULONG VolumeSerialNumber; ULONG VolumeLabelLength; BOOLEAN SupportsObjects; WCHAR VolumeLabel[1]; } FSVOL;

int main(void)
{
    LONG (WINAPI *q)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, ULONG) = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryVolumeInformationFile");
    HANDLE h = CreateFileA("C:\\", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    IO_STATUS_BLOCK io;
    BYTE buf[512];
    FSVOL *v = (FSVOL *)buf;
    FILETIME now;
    ULARGE_INTEGER n;
    LONG st;

    CHECK(h != INVALID_HANDLE_VALUE, "open C:\\ %lu", GetLastError());
    st = q(h, &io, buf, sizeof(buf), 1 /* FileFsVolumeInformation */);
    CHECK(!st, "query st %#lx", st);
    GetSystemTimeAsFileTime(&now);
    n.LowPart = now.dwLowDateTime; n.HighPart = now.dwHighDateTime;
    CHECK(v->VolumeCreationTime.QuadPart != 0, "creation time set");
    CHECK((ULONGLONG)v->VolumeCreationTime.QuadPart <= n.QuadPart + 10000000ull * 86400, "not in the future");
    CHECK(v->VolumeCreationTime.QuadPart > 116444736000000000ll, "after 1970 (%I64d)", v->VolumeCreationTime.QuadPart);
    CHECK(v->VolumeSerialNumber != 0, "serial");
    CloseHandle(h);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
