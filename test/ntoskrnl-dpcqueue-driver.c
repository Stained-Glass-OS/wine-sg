/* Probe driver for patches/sg/5075: the DPC queue.
 * - KeInsertQueueDpc queues a DPC and its routine runs with the DPC, its context and both
 *   arguments; a DPC already in the queue is not queued again (FALSE);
 * - DPCs run in the order they were queued, a HighImportance one first;
 * - KeRemoveQueueDpc takes a queued DPC out (TRUE), and FALSE when it is not queued;
 * - KeFlushQueuedDpcs returns once every queued DPC has run;
 * - KeSetTargetProcessorDpcEx takes a processor of group 0 and refuses others
 *   (STATUS_INVALID_PARAMETER); KeSetTargetProcessorDpc records the number.
 * (The first DPC, once running, waits for the probe to queue the others: they stay queued.) */
#include "sg-driver.h"


static KEVENT go, started;
static int order[8], norder;
static void *args[8][3];

static void NTAPI dpc_routine(KDPC *dpc, void *ctx, void *a1, void *a2)
{
    int id = (int)(ULONG_PTR)ctx;
    if (id == 1 && !norder)
    {
        KeSetEvent(&started, 0, FALSE);
        KeWaitForSingleObject(&go, Executive, KernelMode, FALSE, NULL);
    }
    if (norder < 8)
    {
        args[norder][0] = dpc->DeferredContext; args[norder][1] = a1; args[norder][2] = a2;
        order[norder++] = id;
    }
}

static void run_probe(DRIVER_OBJECT *driver)
{
    static const int expect[] = {1, 4, 3};
    KDPC d1, d2, d3, d4;
    PROCESSOR_NUMBER pn;
    BOOLEAN ret;
    NTSTATUS st;
    int i;

    KeInitializeEvent(&go, NotificationEvent, FALSE);
    KeInitializeEvent(&started, NotificationEvent, FALSE);
    KeInitializeDpc(&d1, dpc_routine, (void *)1);
    KeInitializeDpc(&d2, dpc_routine, (void *)2);
    KeInitializeDpc(&d3, dpc_routine, (void *)3);
    KeInitializeDpc(&d4, dpc_routine, (void *)4);
    KeSetImportanceDpc(&d4, HighImportance);

    ret = KeInsertQueueDpc(&d1, (void *)0x11, (void *)0x12);
    check(ret, "d1 queued: %d", ret);
    KeWaitForSingleObject(&started, Executive, KernelMode, FALSE, NULL);   /* d1 runs, the rest stay queued */
    ret = KeInsertQueueDpc(&d2, NULL, NULL);
    check(ret, "d2 queued: %d", ret);
    ret = KeInsertQueueDpc(&d2, NULL, NULL);
    check(!ret, "d2 already in the queue: %d", ret);
    ret = KeRemoveQueueDpc(&d2);
    check(ret, "d2 taken out: %d", ret);
    ret = KeRemoveQueueDpc(&d2);
    check(!ret, "d2 no longer queued: %d", ret);
    ret = KeInsertQueueDpc(&d3, (void *)0x31, (void *)0x32);
    ret = ret && KeInsertQueueDpc(&d4, (void *)0x41, (void *)0x42);
    check(ret, "d3 and d4 queued");
    KeSetEvent(&go, 0, FALSE);
    KeFlushQueuedDpcs();
    check(norder == 3, "three ran by the time KeFlushQueuedDpcs returned: %d", norder);
    for (i = 0; i < 3 && i < norder; i++)
        check(order[i] == expect[i] && args[i][0] == (void *)(ULONG_PTR)expect[i] &&
              args[i][1] == (void *)(ULONG_PTR)(expect[i] * 16 + 1) && args[i][2] == (void *)(ULONG_PTR)(expect[i] * 16 + 2),
              "run %d: d%d with %p %p", i, order[i], args[i][1], args[i][2]);
    ret = KeInsertQueueDpc(&d1, NULL, NULL);
    check(ret, "d1 can be queued again once it ran: %d", ret);
    KeFlushQueuedDpcs();

    pn.Group = 0; pn.Number = 0; pn.Reserved = 0;
    st = KeSetTargetProcessorDpcEx(&d3, &pn);
    check(st == STATUS_SUCCESS && d3.Number == 0, "target processor 0: %#lx", st);
    pn.Number = (UCHAR)KeQueryActiveProcessorCountEx(0) - 1;
    st = KeSetTargetProcessorDpcEx(&d3, &pn);
    check(st == STATUS_SUCCESS && d3.Number == pn.Number, "the last processor %u: %#lx number %u", pn.Number, st, d3.Number);
    pn.Number = (UCHAR)KeQueryActiveProcessorCountEx(0);
    st = KeSetTargetProcessorDpcEx(&d3, &pn);
    check(st == STATUS_INVALID_PARAMETER, "a processor that is not there: %#lx", st);
    pn.Group = 1; pn.Number = 0;
    st = KeSetTargetProcessorDpcEx(&d3, &pn);
    check(st == STATUS_INVALID_PARAMETER, "group 1: %#lx", st);
    KeSetTargetProcessorDpc(&d4, 1);
    check(d4.Number == 1, "KeSetTargetProcessorDpc records the number: %u", d4.Number);
}
