/* quotafile-probe: ntdll NtQueryQuotaInformationFile / NtSetQuotaInformationFile, which were not exported
 * (Stained Glass OS wine-sg 8282; test/quotafile-gate.sh): a volume without quotas answers
 * STATUS_INVALID_DEVICE_REQUEST, as documented; a bad handle is refused.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <winternl.h>
#ifndef STATUS_INVALID_DEVICE_REQUEST
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xC0000010)
#endif
#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008)
#endif
#include <stdio.h>

int main( void )
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    NTSTATUS (WINAPI *pQuery)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, BOOLEAN, void *, ULONG, PSID, BOOLEAN) =
        (void *)GetProcAddress( ntdll, "NtQueryQuotaInformationFile" );
    NTSTATUS (WINAPI *pSet)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG) = (void *)GetProcAddress( ntdll, "NtSetQuotaInformationFile" );
    IO_STATUS_BLOCK io;
    BYTE buf[256];
    HANDLE vol;
    NTSTATUS st;
    int fails = 0;

    if (!pQuery || !pSet || !GetProcAddress( ntdll, "ZwQueryQuotaInformationFile" )) { printf( "FAIL  exports\nRESULT: FAIL\n" ); return 1; }
    vol = CreateFileA( "C:\\", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    st = pQuery( vol, &io, buf, sizeof(buf), FALSE, NULL, 0, NULL, TRUE );
    printf( "%s  query on C:\\ (no quotas): STATUS_INVALID_DEVICE_REQUEST (%#lx)\n", st == STATUS_INVALID_DEVICE_REQUEST ? "PASS" : "FAIL", st );
    if (st != STATUS_INVALID_DEVICE_REQUEST) fails++;
    st = pSet( vol, &io, buf, 0 );
    printf( "%s  set: STATUS_INVALID_DEVICE_REQUEST (%#lx)\n", st == STATUS_INVALID_DEVICE_REQUEST ? "PASS" : "FAIL", st );
    if (st != STATUS_INVALID_DEVICE_REQUEST) fails++;
    st = pQuery( (HANDLE)0xdead0, &io, buf, sizeof(buf), FALSE, NULL, 0, NULL, TRUE );
    printf( "%s  a bad handle: STATUS_INVALID_HANDLE (%#lx)\n", st == STATUS_INVALID_HANDLE ? "PASS" : "FAIL", st );
    if (st != STATUS_INVALID_HANDLE) fails++;
    CloseHandle( vol );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
