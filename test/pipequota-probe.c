/* pipequota-probe: a pipe end reports how much it can write
 * (patches/sg/0083), for test/pipequota-gate.sh.
 *
 * A pipe with a 200-byte outbound and 100-byte inbound quota, as in Wine's
 * own ntdll pipe test: each end's WriteQuotaAvailable is what the other end
 * can buffer (server 200, client 100), drops by what is written and not yet
 * read, and comes back when it is read.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

typedef struct { ULONG NamedPipeType, NamedPipeConfiguration, MaximumInstances, CurrentInstances, InboundQuota,
                 ReadDataAvailable, OutboundQuota, WriteQuotaAvailable, NamedPipeState, NamedPipeEnd; } PIPE_LOCAL;
NTSTATUS WINAPI NtQueryInformationFile( HANDLE, IO_STATUS_BLOCK *, void *, ULONG, FILE_INFORMATION_CLASS );

static ULONG quota( HANDLE h )
{
    IO_STATUS_BLOCK io;
    PIPE_LOCAL info = { 0 };
    if (NtQueryInformationFile( h, &io, &info, sizeof(info), 24 /* FilePipeLocalInformation */ )) return 0xdead;
    return info.WriteQuotaAvailable;
}

int main( void )
{
    char buf[64] = "0123456789012345678901234567890";
    DWORD n;
    HANDLE s = CreateNamedPipeW( L"\\\\.\\pipe\\sg-pipequota", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT,
                                 1, 200, 100, 0, NULL );
    HANDLE c = CreateFileW( L"\\\\.\\pipe\\sg-pipequota", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );

    printf( "empty server=%lu client=%lu\n", quota( s ), quota( c ) );
    WriteFile( s, buf, 30, &n, NULL );
    WriteFile( c, buf, 10, &n, NULL );
    printf( "written server=%lu client=%lu\n", quota( s ), quota( c ) );
    ReadFile( c, buf, 30, &n, NULL );
    ReadFile( s, buf, 10, &n, NULL );
    printf( "read server=%lu client=%lu\n", quota( s ), quota( c ) );
    return 0;
}
