/* wine-sg 9709: InitializeSynchronizationBarrier / EnterSynchronizationBarrier / DeleteSynchronizationBarrier
 * (kernel32, kernelbase; GnuCash imports them statically and did not load). Four threads, many phases: no
 * thread leaves a phase before all have arrived, exactly one Enter per phase returns TRUE. */
#include <windows.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

#define THREADS 4
#define PHASES 2000
static SYNCHRONIZATION_BARRIER barrier;
static volatile LONG arrived, trues, early;
static DWORD flags_of[THREADS] = { 0, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY, SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY, 0 };

static DWORD WINAPI worker(void *arg)
{
    DWORD flags = flags_of[(INT_PTR)arg];
    int i;
    for (i = 0; i < PHASES; i++)
    {
        InterlockedIncrement(&arrived);
        if (EnterSynchronizationBarrier(&barrier, flags)) InterlockedIncrement(&trues);
        if (arrived < (i + 1) * THREADS) InterlockedIncrement(&early);
        /* the next phase must not start before everyone has checked this one */
        if (EnterSynchronizationBarrier(&barrier, flags)) InterlockedIncrement(&trues);
    }
    return 0;
}

int main(void)
{
    HANDLE th[THREADS];
    int i;
    BOOL ret;

    ret = InitializeSynchronizationBarrier(&barrier, THREADS, -1);
    CHECK(ret, "InitializeSynchronizationBarrier(4 threads, default spin)");
    for (i = 0; i < THREADS; i++) th[i] = CreateThread(NULL, 0, worker, (void *)(INT_PTR)i, 0, NULL);
    CHECK(WaitForMultipleObjects(THREADS, th, TRUE, 60000) == WAIT_OBJECT_0, "all threads went through %d phases", PHASES * 2);
    CHECK(!early, "no thread left a phase before all four arrived (%ld early)", early);
    CHECK(trues == PHASES * 2, "exactly one TRUE per phase (%ld for %d phases)", trues, PHASES * 2);
    CHECK(DeleteSynchronizationBarrier(&barrier), "DeleteSynchronizationBarrier");

    ret = InitializeSynchronizationBarrier(&barrier, 1, 0);
    CHECK(ret && EnterSynchronizationBarrier(&barrier, 0) && EnterSynchronizationBarrier(&barrier, SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE),
          "a one-thread barrier: every Enter is the last");
    CHECK(DeleteSynchronizationBarrier(&barrier), "and deletes");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
