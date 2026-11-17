/* agentgone-probe: SuspendThread + GetThreadContext on a busy thread of its own process, the way a
 * garbage collector stops a thread. Prints "ctx=OK", or "ctx=FAIL <error>", and the milliseconds it took.
 * In a shared prefix the machine's server (another user) reaches this thread through the user's
 * sg-procagent (wine-sg 0021); without one it must fail, not wait forever (7843). */
#include <windows.h>
#include <stdio.h>
static volatile LONG stop;
static DWORD WINAPI spin(void *arg) { volatile unsigned long n = 0; while (!stop) n++; return 0; }
int main(void)
{
    HANDLE t = CreateThread(NULL, 0, spin, NULL, 0, NULL);
    CONTEXT ctx;
    DWORD start, sus;
    BOOL ok;
    Sleep(200);
    start = GetTickCount();
    sus = SuspendThread(t);
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    ok = GetThreadContext(t, &ctx);
    if (ok) printf("ctx=OK\n"); else printf("ctx=FAIL %lu\n", GetLastError());
    printf("suspend=%ld\n", (long)sus);
    ResumeThread(t);
    printf("ms=%lu\n", GetTickCount() - start);
    stop = 1;
    WaitForSingleObject(t, 5000);
    fflush(stdout);
    return 0;
}
