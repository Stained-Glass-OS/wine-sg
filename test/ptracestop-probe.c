/* wineserver's ptrace wait (6259): ReadProcessMemory makes the server PTRACE_ATTACH the target's
 * first thread and wait (3 s watchdog) for its stop.  A thread that cannot stop meanwhile (here: in
 * a vfork, waiting for a child that sleeps 6 s) kept the attach's SIGSTOP pending after the server
 * gave up and detached, and the whole process then stopped for good (state T; SYSTEM's processes
 * on qa5).
 *   probe target DIR : prints its Unix pid, then vforks a child that sleeps 6 s; prints "returned"
 *   probe reader DIR : waits for the target, reads its memory once, prints the result */
#include <windows.h>
#include <stdio.h>

static long raw_getpid(void)
{
    long ret;
    __asm__ volatile( "mov $39, %%eax\n\tsyscall" : "=a"(ret) : : "rcx", "r11", "memory" );
    return ret;
}

/* vfork; the child sleeps 3 s and exits, all in raw syscalls on the parent's stack */
static long raw_vfork_sleep(void)
{
    static struct { long s, ns; } ts = { 6, 0 };
    long ret;
    __asm__ volatile(
        "mov $58, %%eax\n\t"        /* vfork */
        "syscall\n\t"
        "test %%rax, %%rax\n\t"
        "jnz 1f\n\t"
        "mov $35, %%eax\n\t"        /* nanosleep( &ts, NULL ) */
        "lea %1, %%rdi\n\t"
        "xor %%esi, %%esi\n\t"
        "syscall\n\t"
        "mov $60, %%eax\n\t"        /* exit( 0 ) */
        "xor %%edi, %%edi\n\t"
        "syscall\n\t"
        "1:\n\t"
        : "=a"(ret) : "m"(ts) : "rcx", "r11", "rdi", "rsi", "memory" );
    return ret;
}

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    FILE *f;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 3) return 2;
    snprintf(path, sizeof(path), "%s\\ready.txt", argv[2]);
    if (!strcmp(argv[1], "target"))
    {
        printf("unixpid %ld\n", raw_getpid());
        {
            char tmp[MAX_PATH];
            snprintf(tmp, sizeof(tmp), "%s.tmp", path);
            if (!(f = fopen(tmp, "w"))) return 3;
            fprintf(f, "%lu %lu\n", GetCurrentProcessId(), GetCurrentThreadId());
            fclose(f);
            MoveFileA(tmp, path);   /* whole, or not there */
        }
        Sleep(500);                 /* the reader is about to read */
        printf("vfork %ld\n", raw_vfork_sleep() > 0 ? 1L : 0L);
        printf("returned\n");
        snprintf(path, sizeof(path), "%s\\returned.txt", argv[2]);
        if ((f = fopen(path, "w"))) fclose(f);
        {
            /* the reader looks at this thread again meanwhile */
            volatile unsigned long spins = 0;
            DWORD end = GetTickCount() + 6000;
            while ((int)(end - GetTickCount()) > 0) { spins++; Sleep(10); }
            printf("alive %lu\n", spins);
        }
        return 0;
    }
    else
    {
        DWORD pid = 0, start, took;
        HANDLE process;
        char buf[64];
        SIZE_T done = 0;
        BOOL ok;
        int i;

        for (i = 0; i < 200 && !(f = fopen(path, "r")); i++) Sleep(50);
        DWORD tid = 0;
        if (!f || fscanf(f, "%lu %lu", &pid, &tid) != 2) { printf("no target\n"); return 1; }
        fclose(f);
        Sleep(900);                 /* the target is in its vfork now */
        process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!process) { printf("open %lu failed %lu\n", pid, GetLastError()); return 1; }
        start = GetTickCount();
        ok = process && ReadProcessMemory(process, (void *)GetModuleHandleA(NULL), buf, sizeof(buf), &done);
        took = GetTickCount() - start;
        printf("read %d %lu ms (error %lu)\n", ok, took, GetLastError());
        /* when the target is out of its vfork: its thread can be suspended, looked at and read */
        snprintf(path, sizeof(path), "%s\\returned.txt", argv[2]);
        for (i = 0; i < 300 && GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES; i++) Sleep(50);
        Sleep(300);
        {
            HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, tid);
            CONTEXT ctx = { 0 };
            DWORD sus = th ? SuspendThread(th) : (DWORD)-2;
            BOOL got;
            ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
            got = th && GetThreadContext(th, &ctx);
            printf("suspend %ld context %d rip %d\n", (long)sus, got, ctx.Rip != 0);
            if (th) ResumeThread(th);
            ok = ReadProcessMemory(process, (void *)GetModuleHandleA(NULL), buf, sizeof(buf), &done);
            printf("read again %d (error %lu)\n", ok, ok ? 0 : GetLastError());
        }
        return 0;
    }
}
