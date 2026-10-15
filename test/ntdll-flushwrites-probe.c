/* FlushProcessWriteBuffers (NtFlushProcessWriteBuffers) is a process-wide memory barrier.  The probe
 * calls it, also from several threads at once, and the gate checks the wine trace that the
 * system call that does the barrier ran and succeeded (the old stub did nothing). */
#include <windows.h>
#include <stdio.h>

static volatile LONG stop, calls;

static DWORD WINAPI hammer(void *arg)
{
    int i;
    for (i = 0; i < 500; ++i)
    {
        FlushProcessWriteBuffers();
        InterlockedIncrement((LONG *)&calls);
    }
    return 0;
}

int main(void)
{
    HANDLE threads[4];
    int i, ok = 1;

    FlushProcessWriteBuffers();  /* the first call sets things up and must not fail */
    printf("PASS  the call returns\n");

    for (i = 0; i < 4; ++i) threads[i] = CreateThread(NULL, 0, hammer, NULL, 0, NULL);
    if (WaitForMultipleObjects(4, threads, TRUE, 60000) != WAIT_OBJECT_0) ok = 0;
    printf("%s  four threads, 2000 calls in all (done: %ld)\n", ok && calls == 2000 ? "PASS" : "FAIL", (long)calls);
    printf("RESULT: %s\n", ok && calls == 2000 ? "PASS" : "FAIL");
    return !(ok && calls == 2000);
}
