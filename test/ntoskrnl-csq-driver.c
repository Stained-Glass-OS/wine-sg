/* Probe driver for patches/sg/5083: cancel-safe IRP queues.
 * - IoCsqInitialize / IoCsqInitializeEx fill the IO_CSQ (Type IO_TYPE_CSQ / IO_TYPE_CSQ_EX);
 * - IoCsqInsertIrp locks the queue, inserts, marks the irp pending and sets a cancel routine, and
 *   keeps the queue (or the IO_CSQ_IRP_CONTEXT) in DriverContext[3];
 * - IoCsqRemoveNextIrp passes PeekContext to the peek routine, removes the irp it returns;
 * - IoCsqRemoveIrp removes the irp of a context, NULL when it was canceled;
 * - canceling a queued irp removes it from the queue and calls CsqCompleteCanceledIrp;
 * - an irp already canceled is not inserted and is completed as canceled;
 * - IoCsqInsertIrpEx passes InsertContext to CsqInsertIrpEx and returns its status. */
#include "sg-driver.h"
#include <csq.h>

static LIST_ENTRY queue;
static KSPIN_LOCK lock;
static int inserts, removes, peeks, completes, acquires, releases;
static PVOID last_peek_context, last_insert_context;
static PIRP last_completed;
static NTSTATUS ex_status = STATUS_PENDING;

static void NTAPI csq_insert(IO_CSQ *csq, IRP *irp) { inserts++; InsertTailList(&queue, &irp->Tail.Overlay.ListEntry); }
static NTSTATUS NTAPI csq_insert_ex(IO_CSQ *csq, IRP *irp, void *context)
{
    last_insert_context = context;
    inserts++;
    if (NT_SUCCESS(ex_status)) InsertTailList(&queue, &irp->Tail.Overlay.ListEntry);
    return ex_status;
}
static void NTAPI csq_remove(IO_CSQ *csq, IRP *irp) { removes++; RemoveEntryList(&irp->Tail.Overlay.ListEntry); }
static IRP *NTAPI csq_peek(IO_CSQ *csq, IRP *irp, void *context)
{
    LIST_ENTRY *e = irp ? irp->Tail.Overlay.ListEntry.Flink : queue.Flink;
    peeks++;
    last_peek_context = context;
    return e == &queue ? NULL : CONTAINING_RECORD(e, IRP, Tail.Overlay.ListEntry);
}
static void NTAPI csq_acquire(IO_CSQ *csq, KIRQL *irql) { acquires++; KeAcquireSpinLock(&lock, irql); }
static void NTAPI csq_release(IO_CSQ *csq, KIRQL irql) { releases++; KeReleaseSpinLock(&lock, irql); }
static void NTAPI csq_complete(IO_CSQ *csq, IRP *irp) { completes++; last_completed = irp; }

static IRP *new_irp(void)
{
    IRP *irp = IoAllocateIrp(2, FALSE);
    if (irp) IoSetNextIrpStackLocation(irp);
    return irp;
}

static int queue_count(void)
{
    LIST_ENTRY *e;
    int n = 0;
    for (e = queue.Flink; e != &queue; e = e->Flink) n++;
    return n;
}

static void run_probe(DRIVER_OBJECT *driver)
{
    IO_CSQ csq, csq_ex;
    IO_CSQ_IRP_CONTEXT ctx1, ctx2;
    IRP *a = new_irp(), *b = new_irp(), *c = new_irp(), *d = new_irp(), *r;
    NTSTATUS status;
    int n;

    InitializeListHead(&queue);
    KeInitializeSpinLock(&lock);

    memset(&csq, 0xcc, sizeof(csq));
    status = IoCsqInitialize(&csq, csq_insert, csq_remove, csq_peek, csq_acquire, csq_release, csq_complete);
    check(!status && csq.Type == IO_TYPE_CSQ && csq.CsqInsertIrp == csq_insert && csq.CsqRemoveIrp == csq_remove &&
          csq.CsqPeekNextIrp == csq_peek && csq.CsqAcquireLock == csq_acquire && csq.CsqReleaseLock == csq_release &&
          csq.CsqCompleteCanceledIrp == csq_complete && !csq.ReservePointer, "IoCsqInitialize fills the IO_CSQ: %#lx type %lu", status, csq.Type);

    /* insert */
    IoCsqInsertIrp(&csq, a, &ctx1);
    check(inserts == 1 && acquires == 1 && releases == 1 && queue_count() == 1, "insert: callbacks %d %d %d, %d queued", inserts, acquires, releases, queue_count());
    check(ctx1.Type == IO_TYPE_CSQ_IRP_CONTEXT && ctx1.Irp == a && ctx1.Csq == &csq && a->Tail.Overlay.DriverContext[3] == &ctx1, "context filled, DriverContext[3] set");
    check(a->CancelRoutine != NULL, "a cancel routine is set");
    check(IoGetCurrentIrpStackLocation(a)->Control & SL_PENDING_RETURNED, "marked pending");
    IoCsqInsertIrp(&csq, b, NULL);
    check(b->Tail.Overlay.DriverContext[3] == &csq && queue_count() == 2, "insert without context keeps the queue in DriverContext[3]");
    IoCsqInsertIrp(&csq, c, &ctx2);

    /* remove next, with a peek context */
    r = IoCsqRemoveNextIrp(&csq, (void *)0x1234);
    check(r == a && last_peek_context == (void *)0x1234 && removes == 1 && !a->CancelRoutine && queue_count() == 2, "RemoveNextIrp returns the first irp, peek context passed");

    /* cancel a queued irp */
    IoCancelIrp(b);
    check(completes == 1 && last_completed == b && queue_count() == 1, "canceling a queued irp removes it and completes it as canceled: %d %d", completes, queue_count());

    /* remove by context */
    r = IoCsqRemoveIrp(&csq, &ctx2);
    check(r == c && queue_count() == 0 && !c->CancelRoutine, "RemoveIrp removes the irp of the context");

    /* canceled before it is removed: the cancel routine ran, RemoveIrp finds nothing */
    IoCsqInsertIrp(&csq, c, &ctx2);
    IoCancelIrp(c);
    n = completes;
    check(n == 2 && IoCsqRemoveIrp(&csq, &ctx2) == NULL, "RemoveIrp of a canceled irp is NULL");

    /* already canceled when inserted */
    inserts = 0;
    a->Cancel = TRUE;
    IoCsqInsertIrp(&csq, a, &ctx1);
    check(inserts == 0 && queue_count() == 0 && completes == 3 && last_completed == a, "an irp canceled before the insert is not inserted, completed as canceled");
    a->Cancel = FALSE;

    /* remove next from an empty queue */
    check(IoCsqRemoveNextIrp(&csq, NULL) == NULL, "empty queue: NULL");

    /* IoCsqInsertIrpEx on a plain queue always succeeds */
    status = IoCsqInsertIrpEx(&csq, a, &ctx1, (void *)7);
    check(status == STATUS_SUCCESS && queue_count() == 1, "InsertIrpEx on a plain queue: %#lx", status);
    IoCsqRemoveIrp(&csq, &ctx1);

    /* Ex queue */
    status = IoCsqInitializeEx(&csq_ex, csq_insert_ex, csq_remove, csq_peek, csq_acquire, csq_release, csq_complete);
    check(!status && csq_ex.Type == IO_TYPE_CSQ_EX, "IoCsqInitializeEx: type %lu", csq_ex.Type);
    ex_status = STATUS_PENDING;
    status = IoCsqInsertIrpEx(&csq_ex, a, &ctx1, (void *)0x77);
    check(status == STATUS_PENDING && last_insert_context == (void *)0x77 && queue_count() == 1 && a->CancelRoutine, "InsertIrpEx: CsqInsertIrpEx's status %#lx and InsertContext passed", status);
    IoCsqRemoveIrp(&csq_ex, &ctx1);
    ex_status = STATUS_INSUFFICIENT_RESOURCES;
    status = IoCsqInsertIrpEx(&csq_ex, d, &ctx2, NULL);
    check(status == STATUS_INSUFFICIENT_RESOURCES && queue_count() == 0 && !d->CancelRoutine, "a failed insert: status returned, irp not queued, no cancel routine");

    IoFreeIrp(a); IoFreeIrp(b); IoFreeIrp(c); IoFreeIrp(d);
}
