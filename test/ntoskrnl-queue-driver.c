/* Probe driver for patches/sg/5084: KQUEUE.
 * - KeInitializeQueue: queue object type, empty, MaximumCount;
 * - KeInsertQueue / KeInsertHeadQueue return the previous number of entries; KeRemoveQueue returns
 *   them in order (head inserts first), STATUS_TIMEOUT as the "pointer" when empty after the
 *   timeout, KeReadStateQueue the number of entries;
 * - a thread waiting in KeRemoveQueue gets an entry inserted later;
 * - KeRundownQueue returns the first entry, wakes waiters with STATUS_ABANDONED, later removals
 *   fail with it too. */
#include "sg-driver.h"
#include <ntifs.h>

#define QUEUE_OBJECT 4

static KQUEUE q;
static LIST_ENTRY e[8];
static KEVENT waiting_done;
static PLIST_ENTRY waiter_result;
static volatile LONG waiter_started;

static void NTAPI waiter(void *context)
{
    InterlockedExchange(&waiter_started, 1);
    waiter_result = KeRemoveQueue(&q, KernelMode, NULL);
    KeSetEvent(&waiting_done, IO_NO_INCREMENT, FALSE);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

static void start_waiter(void)
{
    HANDLE thread;
    LARGE_INTEGER delay;

    waiter_started = 0;
    waiter_result = (PLIST_ENTRY)(ULONG_PTR)0x1234;
    KeClearEvent(&waiting_done);
    if (PsCreateSystemThread(&thread, THREAD_ALL_ACCESS, NULL, NULL, NULL, waiter, NULL)) return;
    ZwClose(thread);
    while (!waiter_started) { delay.QuadPart = -10000; KeDelayExecutionThread(KernelMode, FALSE, &delay); }
    delay.QuadPart = -500000;  /* 50 ms: the thread is in KeRemoveQueue now */
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

static int wait_waiter(void)
{
    LARGE_INTEGER timeout;
    timeout.QuadPart = -30000000;
    return KeWaitForSingleObject(&waiting_done, Executive, KernelMode, FALSE, &timeout) == STATUS_SUCCESS;
}

static void run_probe(DRIVER_OBJECT *driver)
{
    LARGE_INTEGER zero = {{0}}, t1, t2, wait;
    PLIST_ENTRY r;
    LONG s;

    KeInitializeEvent(&waiting_done, NotificationEvent, FALSE);

    memset(&q, 0xcc, sizeof(q));
    KeInitializeQueue(&q, 3);
    check(q.Header.Type == QUEUE_OBJECT && q.Header.SignalState == 0 && q.MaximumCount == 3 && IsListEmpty(&q.EntryListHead), "KeInitializeQueue: type %u state %ld max %lu", q.Header.Type, q.Header.SignalState, q.MaximumCount);
    KeInitializeQueue(&q, 0);
    check(q.MaximumCount >= 1 && q.MaximumCount == KeQueryActiveProcessorCount(NULL), "default MaximumCount is the processor count: %lu", q.MaximumCount);

    s = KeInsertQueue(&q, &e[0]);
    check(s == 0, "first insert returns 0: %ld", s);
    s = KeInsertQueue(&q, &e[1]);
    check(s == 1, "second insert returns 1: %ld", s);
    s = KeInsertHeadQueue(&q, &e[2]);
    check(s == 2 && KeReadStateQueue(&q) == 3, "head insert returns 2, state is 3: %ld %ld", s, KeReadStateQueue(&q));

    r = KeRemoveQueue(&q, KernelMode, &zero);
    check(r == &e[2], "head entry first");
    r = KeRemoveQueue(&q, KernelMode, &zero);
    check(r == &e[0], "then the first tail entry");
    r = KeRemoveQueue(&q, KernelMode, &zero);
    check(r == &e[1] && KeReadStateQueue(&q) == 0, "then the second");

    r = KeRemoveQueue(&q, KernelMode, &zero);
    check(r == (PLIST_ENTRY)(ULONG_PTR)STATUS_TIMEOUT, "empty with a zero timeout: STATUS_TIMEOUT");
    wait.QuadPart = -1000000;  /* 100 ms */
    KeQuerySystemTime(&t1);
    r = KeRemoveQueue(&q, KernelMode, &wait);
    KeQuerySystemTime(&t2);
    check(r == (PLIST_ENTRY)(ULONG_PTR)STATUS_TIMEOUT && t2.QuadPart - t1.QuadPart >= 900000, "empty: STATUS_TIMEOUT after the interval (%I64d x100ns)", t2.QuadPart - t1.QuadPart);

    /* a waiting thread gets an entry inserted later */
    start_waiter();
    s = KeInsertQueue(&q, &e[3]);
    check(s == 0 && wait_waiter() && waiter_result == &e[3] && KeReadStateQueue(&q) == 0, "waiter gets the entry; insert returns 0, queue stays empty: %ld", s);

    /* rundown */
    KeInsertQueue(&q, &e[4]);
    KeInsertQueue(&q, &e[5]);
    r = KeRundownQueue(&q);
    check(r == &e[4], "rundown returns the first entry");
    r = KeRemoveQueue(&q, KernelMode, &zero);
    check(r == (PLIST_ENTRY)(ULONG_PTR)STATUS_ABANDONED, "removal after rundown: STATUS_ABANDONED");

    KeInitializeQueue(&q, 1);
    check(KeRundownQueue(&q) == NULL, "rundown of an empty queue returns NULL");
    KeInitializeQueue(&q, 1);
    start_waiter();
    r = KeRundownQueue(&q);
    check(r == NULL && wait_waiter() && waiter_result == (PLIST_ENTRY)(ULONG_PTR)STATUS_ABANDONED, "rundown wakes a waiter with STATUS_ABANDONED");
}
