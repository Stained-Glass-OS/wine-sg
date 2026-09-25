/* stackgrow-probe: a stack a program manages itself grows (patches/sg/0081),
 * for test/stackgrow-gate.sh.
 *
 * The Cygwin/MSYS runtime gives its threads stacks it allocates itself:
 * reserved PAGE_NOACCESS, the top pages committed read-write, a few
 * PAGE_GUARD pages below. The probe reproduces that on a thread's own stack
 * -- decommit everything below the top, commit a read-write guard region
 * under it, and recurse 200 KB deep. On Windows the stack grows into
 * read-write pages; before the patch Wine grew it into committed pages with
 * no access, and the recursion died of an access violation.
 *
 *   stackgrow-probe          run the test in a child; print grew=1|0 and what
 *                            the child printed (a child killed by the fault
 *                            reports exit code 0 all the same, so its output
 *                            is the evidence)
 *   stackgrow-probe child    the test itself
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int __attribute__((noinline)) recurse( int depth )
{
    volatile char frame[8192];
    frame[0] = (char)depth;
    frame[sizeof(frame) - 1] = (char)depth;
    return depth ? recurse( depth - 1 ) + frame[0] - frame[0] : frame[sizeof(frame) - 1];
}

/* run func on another stack (x86_64, Windows ABI): rcx = func, rdx = stack top */
extern int call_on_stack( int (*func)(void), void *top );
__asm__( ".globl call_on_stack\n"
         "call_on_stack:\n\t"
         "pushq %rbp\n\t"
         "movq %rsp, %rbp\n\t"
         "movq %rdx, %rsp\n\t"
         "subq $0x28, %rsp\n\t"
         "call *%rcx\n\t"
         "movq %rbp, %rsp\n\t"
         "popq %rbp\n\t"
         "ret" );

static int deep(void) { return recurse( 25 ); }   /* 25 x 8 KB, through the guard region and beyond */

static DWORD WINAPI thread( void *arg )
{
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    void **dealloc = (void **)((char *)NtCurrentTeb() + 0x1478);   /* TEB DeallocationStack (x64) */
    void *old_base = tib->StackBase, *old_limit = tib->StackLimit, *old_dealloc = *dealloc;
    const SIZE_T size = 1 << 20;
    char *stack, *top;

    /* the Cygwin/MSYS layout: reserved no-access, top 16 KB read-write, three
     * read-write guard pages below */
    if (!(stack = VirtualAlloc( NULL, size, MEM_RESERVE, PAGE_NOACCESS ))) return 2;
    top = stack + size;
    if (!VirtualAlloc( top - 0x4000, 0x4000, MEM_COMMIT, PAGE_READWRITE )) return 2;
    if (!VirtualAlloc( top - 0x7000, 0x3000, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD )) return 2;
    tib->StackBase = top;
    tib->StackLimit = top - 0x4000;
    *dealloc = stack;
    call_on_stack( deep, top - 64 );
    {
        /* what it grew into: read-write, committed, as on Windows */
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery( top - 0x20000, &mbi, sizeof(mbi) );
        printf( "grown state=%#lx protect=%#lx\n", mbi.State, mbi.Protect );
        fflush( stdout );
    }
    tib->StackBase = old_base;
    tib->StackLimit = old_limit;
    *dealloc = old_dealloc;
    return 0;
}

int main( int argc, char **argv )
{
    if (argc == 2 && !strcmp( argv[1], "child" ))
    {
        HANDLE h = CreateThread( NULL, 1 << 20, thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL );
        DWORD code = 3;
        WaitForSingleObject( h, INFINITE );
        GetExitCodeThread( h, &code );
        return code;
    }
    else
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        char cmd[MAX_PATH + 16], self[MAX_PATH], out[256] = "";
        HANDLE rd, wr;
        DWORD got = 0;

        CreatePipe( &rd, &wr, &sa, 0 );
        SetHandleInformation( rd, HANDLE_FLAG_INHERIT, 0 );
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = wr;
        GetModuleFileNameA( NULL, self, MAX_PATH );
        snprintf( cmd, sizeof(cmd), "\"%s\" child", self );
        if (!CreateProcessA( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi )) return 1;
        CloseHandle( wr );
        WaitForSingleObject( pi.hProcess, 60000 );
        ReadFile( rd, out, sizeof(out) - 1, &got, NULL );
        printf( "grew=%d %s", strstr( out, "grown state=0x1000 protect=0x4" ) != NULL, out[0] ? out : "(child printed nothing)\n" );
        return 0;
    }
}
