/* stacklimit-probe: a thread's TEB StackLimit is the edge of the stack in use, as on Windows
 * (Stained Glass OS wine-sg 6025): Crashpad (Chromium, Electron, Dropbox) dumps [StackLimit,
 * StackBase) of every thread. The limit follows the stack down (alloca probes, recursion), a
 * kernel read into the guard page grows it, and a runaway recursion is still STATUS_STACK_OVERFLOW.
 *   stacklimit-probe.exe            the test
 *   stacklimit-probe.exe overflow   a child that recurses without end */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <malloc.h>

static int failures;

static void check( int ok, const char *what, long long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#llx)\n", what, got ); failures++; }
}

static char *stack_limit(void) { return (char *)((NT_TIB *)NtCurrentTeb())->StackLimit; }
static char *stack_base(void) { return (char *)((NT_TIB *)NtCurrentTeb())->StackBase; }

static volatile int sink;

static int __attribute__((noinline)) recurse( int n )
{
    volatile char buf[512];
    buf[0] = (char)n;
    buf[511] = 1;
    if (n <= 0) return buf[0];
    return recurse( n - 1 ) + buf[511];
}

static int __attribute__((noinline)) forever( int n )
{
    volatile char buf[1024];
    buf[0] = (char)n;
    return forever( n + 1 ) + buf[0];
}

static DWORD WINAPI thread_proc( void *arg )
{
    char here;
    char *limit = stack_limit(), *base = stack_base();
    char *probe;
    MEMORY_BASIC_INFORMATION mbi;
    char *guard;
    HANDLE file;
    DWORD got = 0;
    char path[MAX_PATH], tmp[MAX_PATH];

    printf( "      stack %p-%p, limit %p, here %p\n", (void *)((NT_TIB *)NtCurrentTeb())->StackLimit,
            base, limit, &here );
    check( limit < &here && &here - limit <= 0x10000, "a new thread's StackLimit is within 64 KB below its stack pointer",
           &here - limit );
    check( base - limit < 0x40000, "and not the whole reserve", base - limit );

    /* a probed allocation of 256 KB moves the limit under it */
    probe = alloca( 0x40000 );
    probe[0] = 1;
    sink = probe[0];
    check( stack_limit() <= probe && probe - stack_limit() <= 0x2000, "alloca's probes move the limit down",
           probe - stack_limit() );

    /* recursion moves it too */
    limit = stack_limit();
    sink = recurse( 600 );
    check( stack_limit() < limit, "recursion moves it further", limit - stack_limit() );

    /* the guard page under the limit, filled by the kernel (ReadFile) */
    limit = stack_limit();
    guard = limit - 4096;
    VirtualQuery( guard, &mbi, sizeof(mbi) );
    check( mbi.Protect & PAGE_GUARD, "a guard page under the limit", mbi.Protect );
    GetTempPathA( MAX_PATH, tmp );
    GetTempFileNameA( tmp, "sgs", 0, path );
    file = CreateFileA( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL );
    WriteFile( file, "stained glass stack", 19, &got, NULL );
    SetFilePointer( file, 0, NULL, FILE_BEGIN );
    got = 0;
    check( ReadFile( file, guard + 64, 19, &got, NULL ) && got == 19 && !memcmp( guard + 64, "stained glass stack", 19 ),
           "ReadFile into the guard page", GetLastError() );
    check( stack_limit() <= guard, "and the limit moved under it", stack_limit() - guard );
    CloseHandle( file );
    return 0;
}

/* the process ends with the exception's code, without a debugger */
static LONG WINAPI filter( EXCEPTION_POINTERS *ptrs )
{
    TerminateProcess( GetCurrentProcess(), ptrs->ExceptionRecord->ExceptionCode );
    return EXCEPTION_EXECUTE_HANDLER;
}

int main( int argc, char **argv )
{
    HANDLE thread;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 32], self[MAX_PATH];
    DWORD code = 0;

    if (argc > 1)
    {
        SetUnhandledExceptionFilter( filter );
        sink = forever( 0 );
        return 0;
    }

    thread = CreateThread( NULL, 0, thread_proc, NULL, 0, NULL );
    WaitForSingleObject( thread, 30000 );
    CloseHandle( thread );
    /* a thread with a reserve of its own */
    thread = CreateThread( NULL, 8 << 20, thread_proc, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL );
    WaitForSingleObject( thread, 30000 );
    CloseHandle( thread );

    GetModuleFileNameA( NULL, self, MAX_PATH );
    sprintf( cmd, "\"%s\" overflow", self );
    if (CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
    {
        WaitForSingleObject( pi.hProcess, 60000 );
        GetExitCodeProcess( pi.hProcess, &code );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
    check( code == STATUS_STACK_OVERFLOW, "endless recursion ends in STATUS_STACK_OVERFLOW", code );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
