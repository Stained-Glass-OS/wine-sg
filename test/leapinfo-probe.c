/* leapinfo-probe: NtQuerySystemInformation(SystemLeapSecondInformation) answers (Stained Glass OS wine-sg
 * 8298; test/leapinfo-gate.sh): no leap seconds (the host clock is POSIX time); it was a stub, and in 32-bit
 * processes an unsupported class (.NET 8 programs ask at start: NAPS2, ShareX).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    LONG (WINAPI *pQuery)( ULONG, void *, ULONG, ULONG * ) = (void *)GetProcAddress( GetModuleHandleA( "ntdll" ), "NtQuerySystemInformation" );
    struct { BOOLEAN Enabled; ULONG Flags; } leap;
    ULONG len = 0;
    LONG st;
    int fails = 0;

    memset( &leap, 0xcc, sizeof(leap) );
    st = pQuery( 206, &leap, sizeof(leap), &len );
    printf( "%s  SystemLeapSecondInformation: %#lx, enabled %u, flags %#lx, length %lu\n",
            !st && !leap.Enabled && !leap.Flags && len == sizeof(leap) ? "PASS" : "FAIL", st, leap.Enabled, leap.Flags, len );
    if (st || leap.Enabled || leap.Flags || len != sizeof(leap)) fails++;
    st = pQuery( 206, &leap, 4, &len );
    printf( "%s  a short buffer: %#lx\n", st == (LONG)0xc0000004 ? "PASS" : "FAIL", st );
    if (st != (LONG)0xc0000004) fails++;
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
