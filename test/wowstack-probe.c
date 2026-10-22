/* wowstack-probe: a 32-bit (WoW64) thread's stack overflow is EXCEPTION_STACK_OVERFLOW to the
 * program, on the main thread and on others, with room for its handlers (Stained Glass OS
 * wine-sg 6027). Before, the exception frame did not fit in the stack left, and the thread was
 * aborted: the process exited 1, or a secondary thread vanished and its process went on (or hung).
 *
 *   wowstack-probe.exe          the test: runs itself in each mode and checks the exit codes
 *   wowstack-probe.exe MODE     a child: 0 main thread + unhandled filter, 1 other thread + filter,
 *                               2 main thread + vectored handler with a 3000-byte frame,
 *                               3 other thread + vectored handler, 4 twice in a row on new threads,
 *                               5 other thread, no handler (the process ends, as on Windows),
 *                               6 other thread, a handler that overflows again (no stack left: the process ends)
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static volatile int sink;
static int failures;

static int __attribute__((noinline)) forever( int n )
{
    volatile char buf[1024];
    buf[0] = (char)n;
    return forever( n + 1 ) + buf[0];
}

static LONG WINAPI filter( EXCEPTION_POINTERS *ptrs )
{
    TerminateProcess( GetCurrentProcess(), ptrs->ExceptionRecord->ExceptionCode );
    return EXCEPTION_EXECUTE_HANDLER;
}

static LONG WINAPI veh( EXCEPTION_POINTERS *ptrs )
{
    volatile char big[3000];

    if (ptrs->ExceptionRecord->ExceptionCode != STATUS_STACK_OVERFLOW) return EXCEPTION_CONTINUE_SEARCH;
    memset( (char *)big, 1, sizeof(big) );
    sink = big[100];
    TerminateProcess( GetCurrentProcess(), 0x5a5a );
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG count;
static LONG WINAPI veh_count( EXCEPTION_POINTERS *ptrs )
{
    if (ptrs->ExceptionRecord->ExceptionCode != STATUS_STACK_OVERFLOW) return EXCEPTION_CONTINUE_SEARCH;
    /* each overflowing thread ends itself; the second one ends the process */
    if (InterlockedIncrement( &count ) == 2) TerminateProcess( GetCurrentProcess(), 0x2222 );
    ExitThread( 0 );
    return EXCEPTION_CONTINUE_SEARCH;
}

/* a handler that overflows again: no stack is left to dispatch that */
static LONG WINAPI veh_again( EXCEPTION_POINTERS *ptrs )
{
    if (ptrs->ExceptionRecord->ExceptionCode == STATUS_STACK_OVERFLOW) sink = forever( 0 );
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI thread_proc( void *arg )
{
    sink = forever( 0 );
    return 0;
}

static void run_thread(void)
{
    HANDLE t = CreateThread( NULL, 0, thread_proc, NULL, 0, NULL );
    WaitForSingleObject( t, 30000 );
    CloseHandle( t );
}

static void check( const char *self, int mode, DWORD want, const char *what )
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 32];
    DWORD code = 0xdeadbeef;

    sprintf( cmd, "\"%s\" %d", self, mode );
    if (CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
    {
        if (WaitForSingleObject( pi.hProcess, 30000 )) TerminateProcess( pi.hProcess, 0xdead );
        GetExitCodeProcess( pi.hProcess, &code );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
    if (code == want) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (exit %#lx, wanted %#lx)\n", what, code, want ); failures++; }
}

int main( int argc, char **argv )
{
    char self[MAX_PATH];

    if (argc > 1)
    {
        switch (atoi( argv[1] ))
        {
        case 0: SetUnhandledExceptionFilter( filter ); sink = forever( 0 ); break;
        case 1: SetUnhandledExceptionFilter( filter ); run_thread(); break;
        case 2: AddVectoredExceptionHandler( 1, veh ); sink = forever( 0 ); break;
        case 3: AddVectoredExceptionHandler( 1, veh ); run_thread(); break;
        case 4: AddVectoredExceptionHandler( 1, veh_count ); run_thread(); run_thread(); break;
        case 5: SetErrorMode( SEM_NOGPFAULTERRORBOX ); run_thread(); break;
        case 6: AddVectoredExceptionHandler( 1, veh_again ); run_thread(); break;
        }
        return 0x77; /* the overflow did not end the process */
    }
    GetModuleFileNameA( NULL, self, MAX_PATH );
    printf( "      %d-bit\n", (int)sizeof(void *) * 8 );
    check( self, 0, STATUS_STACK_OVERFLOW, "the main thread's endless recursion reaches the unhandled exception filter" );
    check( self, 1, STATUS_STACK_OVERFLOW, "another thread's reaches it too" );
    check( self, 2, 0x5a5a, "a vectored handler with a 3000-byte frame sees STATUS_STACK_OVERFLOW on the main thread" );
    check( self, 3, 0x5a5a, "and on another thread" );
    check( self, 4, 0x2222, "two threads overflow one after the other, each delivered" );
    check( self, 5, STATUS_STACK_OVERFLOW, "an unhandled overflow on another thread ends the process" );
    check( self, 6, STATUS_STACK_OVERFLOW, "an overflow with no stack left to dispatch it ends the process, not just the thread" );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
