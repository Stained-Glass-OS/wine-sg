/* NtWriteVirtualMemory (patches/sg/2229): it writes only to committed,
 * writable (read-write, write-copy, executable forms), unguarded pages and
 * answers STATUS_PARTIAL_COPY with the count of the bytes before the first
 * page it may not write.  The server's write goes through ptrace and used to
 * change read-only memory and report success.  Tried on this process and
 * on a suspended child. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define STATUS_PARTIAL_COPY ((NTSTATUS)0x8000000D)
static NTSTATUS (WINAPI *pNtWriteVirtualMemory)(HANDLE, void *, const void *, SIZE_T, SIZE_T *);

static void run(HANDLE process, const char *who, BOOL local)
{
    char name[160];
    char *src = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 0x100000 );
    char *base;
    SIZE_T written;
    NTSTATUS status;
    DWORD old;
    BOOL b;
    char probe[16];
    SIZE_T got;
    #define NAME(s) (sprintf( name, "[%s] %s", who, s ), name)

    memset( src, 'S', 0x100000 - 0x10000 );
    base = VirtualAllocEx( process, NULL, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
    check( base != NULL, NAME("allocate") );
    if (!base) return;

    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base, src, 0x10000, &written );
    check( !status && written == 0x10000, NAME("read-write: success, all bytes") );
    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base, src, 0, &written );
    check( !status && written == 0, NAME("zero bytes: success") );

    memset( src + 0x100000 - 0x10000, 0, 0x10000 );
    pNtWriteVirtualMemory( process, base, src + 0x100000 - 0x10000, 0x10000, &written );   /* zeroes */
    VirtualProtectEx( process, base + 0x2000, 0x2000, PAGE_READONLY, &old );
    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base, src, 0x10000, &written );
    check( status == STATUS_PARTIAL_COPY && written == 0x2000, NAME("read-write then read-only: partial copy, 0x2000 bytes") );
    ReadProcessMemory( process, base + 0x1fff, probe, 2, &got );
    check( got == 2 && probe[0] == 'S' && probe[1] == 0, NAME("the byte before the read-only page is written, the page is not") );

    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base + 0x2000, src, 0x100, &written );
    check( status == STATUS_PARTIAL_COPY && written == 0, NAME("read-only: partial copy, nothing written") );
    SetLastError( 0 );
    b = WriteProcessMemory( process, base + 0x2000, src, 0x100, &written );
    check( !b && GetLastError() == ERROR_NOACCESS, NAME("WriteProcessMemory to read-only fails with ERROR_NOACCESS") );

    VirtualProtectEx( process, base + 0x4000, 0x1000, PAGE_EXECUTE_READ, &old );
    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base + 0x4000, src, 0x100, &written );
    check( status == STATUS_PARTIAL_COPY && written == 0, NAME("execute-read: partial copy") );
    b = WriteProcessMemory( process, base + 0x4000, src, 0x100, &written );
    check( b && written == 0x100, NAME("WriteProcessMemory to execute-read succeeds") );

    VirtualProtectEx( process, base + 0x5000, 0x1000, PAGE_EXECUTE_READWRITE, &old );
    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base + 0x5000, src, 0x1000, &written );
    check( !status && written == 0x1000, NAME("execute-read-write: success") );

    VirtualProtectEx( process, base + 0x6000, 0x1000, PAGE_NOACCESS, &old );
    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base + 0x6000, src, 0x10, &written );
    check( status == STATUS_PARTIAL_COPY && written == 0, NAME("no access: partial copy") );

    VirtualProtectEx( process, base + 0x7000, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old );
    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base + 0x7000, src, 0x10, &written );
    check( status == STATUS_PARTIAL_COPY && written == 0, NAME("guard page: partial copy") );

    VirtualFreeEx( process, base + 0x8000, 0x1000, MEM_DECOMMIT );
    VirtualFreeEx( process, base + 0xa000, 0x1000, MEM_DECOMMIT );
    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base + 0x8000, src, 0x10, &written );
    check( status == STATUS_PARTIAL_COPY && written == 0, NAME("reserved, not committed: partial copy") );

    written = 0xdeadbeef;
    status = pNtWriteVirtualMemory( process, base + 0x9000, src, 0x2000, &written );
    check( status == STATUS_PARTIAL_COPY && written == 0x1000, NAME("committed then reserved: partial copy, 0x1000 bytes") );

    VirtualFreeEx( process, base, 0, MEM_RELEASE );
    base = VirtualAllocEx( process, NULL, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
    written = 0;
    status = pNtWriteVirtualMemory( process, base, src, 0x100000, &written );
    check( !status && written == 0x100000, NAME("1 MB in one region: success") );
    VirtualFreeEx( process, base, 0, MEM_RELEASE );
    (void)local;
    HeapFree( GetProcessHeap(), 0, src );
}

int main(int argc, char **argv)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 16];

    if (argc > 1 && !strcmp( argv[1], "idle" )) return 0;
    pNtWriteVirtualMemory = (void *)GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "NtWriteVirtualMemory" );

    run( GetCurrentProcess(), "this process", TRUE );

    sprintf( cmd, "\"%s\" idle", argv[0] );
    if (CreateProcessA( NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi ))
    {
        run( pi.hProcess, "child", FALSE );
        TerminateProcess( pi.hProcess, 0 );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
    else check( 0, "start a suspended child" );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
