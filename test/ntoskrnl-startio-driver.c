/* Probe driver for patches/sg/5082: the device queue and StartIo.
 * - IoStartPacket calls StartIo at once when the device is idle, else queues the irp (in key
 *   order when a key is given); IoStartNextPacket / IoStartNextPacketByKey start the next one;
 * - an empty queue leaves the device idle (CurrentIrp NULL, queue not busy);
 * - a cancel routine given to IoStartPacket is set while the irp waits, cleared when it starts,
 *   and runs when a queued irp is canceled;
 * - with the deferred attribute, IoStartNextPacket called inside StartIo waits until it returns;
 * - KeInsertByKeyDeviceQueue / KeRemoveByKeyDeviceQueue / KeRemoveEntryDeviceQueue. */
#include <stdarg.h>
#include <ntddk.h>

static IRP *started[16];
static int nstarted, depth, max_depth, chain;

static void NTAPI start_io(DEVICE_OBJECT *device, IRP *irp)
{
    if (nstarted < 16) started[nstarted++] = irp;
    depth++;
    if (depth > max_depth) max_depth = depth;
    if (chain) IoStartNextPacket(device, FALSE);
    depth--;
}

static int cancels;
static void NTAPI cancel_routine(DEVICE_OBJECT *device, IRP *irp)
{
    cancels++;
    IoReleaseCancelSpinLock(irp->CancelIrql);
}

#define SG_DRIVER_ENTRY_HOOK(driver) driver->DriverStartIo = start_io

#include "sg-driver.h"

static IRP *new_irp(void)
{
    return IoAllocateIrp(1, FALSE);
}

static void run_probe(DRIVER_OBJECT *driver)
{
    DEVICE_OBJECT *dev = sg_device;
    IRP *irp[8];
    ULONG key;
    int i;
    KDEVICE_QUEUE q;
    KDEVICE_QUEUE_ENTRY e[4];
    KDEVICE_QUEUE_ENTRY *r;

    for (i = 0; i < 8; i++) irp[i] = new_irp();

    /* idle device: started at once */
    IoStartPacket(dev, irp[0], NULL, NULL);
    check(nstarted == 1 && started[0] == irp[0] && dev->CurrentIrp == irp[0], "idle device starts the irp: %d", nstarted);
    check(dev->DeviceQueue.Busy, "queue is busy while an irp is processed");

    /* busy device: queued, in key order for equal-or-lower keys first */
    key = 5; IoStartPacket(dev, irp[1], &key, NULL);
    key = 3; IoStartPacket(dev, irp[2], &key, NULL);
    key = 5; IoStartPacket(dev, irp[3], &key, NULL);
    IoStartPacket(dev, irp[4], NULL, NULL);
    check(nstarted == 1, "busy device only queues: %d started", nstarted);
    check(irp[1]->Tail.Overlay.DeviceQueueEntry.Inserted && irp[1]->Tail.Overlay.DeviceQueueEntry.SortKey == 5, "queued irp has its key");

    IoStartNextPacket(dev, FALSE);
    check(nstarted == 2 && started[1] == irp[2] && dev->CurrentIrp == irp[2], "lowest key first (3)");
    IoStartNextPacketByKey(dev, FALSE, 5);
    check(nstarted == 3 && started[2] == irp[1], "by key 5: the first entry with key >= 5, in insertion order");
    IoStartNextPacketByKey(dev, FALSE, 9);
    check(nstarted == 4 && started[3] == irp[3], "no key >= 9: the first entry (the other key-5 irp)");
    IoStartNextPacket(dev, FALSE);
    check(nstarted == 5 && started[4] == irp[4], "the irp queued without a key goes last");
    IoStartNextPacket(dev, FALSE);
    check(nstarted == 5 && dev->CurrentIrp == NULL && !dev->DeviceQueue.Busy, "empty queue: device idle");

    /* idle again */
    IoStartPacket(dev, irp[5], NULL, cancel_routine);
    check(nstarted == 6 && started[5] == irp[5] && !irp[5]->CancelRoutine, "started irp has no cancel routine");
    IoStartPacket(dev, irp[6], NULL, cancel_routine);
    check(nstarted == 6 && irp[6]->CancelRoutine == cancel_routine, "queued irp keeps its cancel routine");
    IoCancelIrp(irp[6]);
    check(cancels == 1, "canceling a queued irp runs the cancel routine");
    check(KeRemoveEntryDeviceQueue(&dev->DeviceQueue, &irp[6]->Tail.Overlay.DeviceQueueEntry), "the cancel routine's owner removes it");
    IoStartPacket(dev, irp[7], NULL, cancel_routine);
    IoStartNextPacket(dev, TRUE);
    check(nstarted == 7 && started[6] == irp[7] && !irp[7]->CancelRoutine, "cancelable start clears the cancel routine");
    IoStartNextPacket(dev, FALSE);
    check(!dev->DeviceQueue.Busy, "idle");

    /* deferred StartIo */
    for (i = 0; i < 4; i++) IoInitializeIrp(irp[i], IoSizeOfIrp(1), 1);
    nstarted = 0; max_depth = 0; chain = 0;
    IoStartPacket(dev, irp[0], NULL, NULL);
    IoStartPacket(dev, irp[1], NULL, NULL);
    IoStartPacket(dev, irp[2], NULL, NULL);
    IoStartPacket(dev, irp[3], NULL, NULL);
    check(nstarted == 1, "one started, three queued");
    chain = 1;
    IoStartNextPacket(dev, FALSE);   /* starts irp[1]; each StartIo asks for the next */
    check(nstarted == 4 && max_depth == 3 && !dev->DeviceQueue.Busy, "not deferred: StartIo nests (depth %d, started %d)", max_depth, nstarted);

    IoSetStartIoAttributes(dev, TRUE, FALSE);
    for (i = 0; i < 4; i++) IoInitializeIrp(irp[i], IoSizeOfIrp(1), 1);
    nstarted = 0; max_depth = 0; chain = 0;
    IoStartPacket(dev, irp[0], NULL, NULL);
    IoStartPacket(dev, irp[1], NULL, NULL);
    IoStartPacket(dev, irp[2], NULL, NULL);
    IoStartPacket(dev, irp[3], NULL, NULL);
    chain = 1;
    IoStartNextPacket(dev, FALSE);
    check(nstarted == 4 && max_depth == 1 && !dev->DeviceQueue.Busy, "deferred: StartIo never nests (depth %d, started %d)", max_depth, nstarted);
    check(started[1] == irp[1] && started[2] == irp[2] && started[3] == irp[3], "in queue order");
    IoSetStartIoAttributes(dev, FALSE, FALSE);
    chain = 0;

    /* the queue object */
    KeInitializeDeviceQueue(&q);
    check(!KeInsertByKeyDeviceQueue(&q, &e[0], 10) && q.Busy, "first insert: not queued, queue becomes busy");
    check(KeInsertByKeyDeviceQueue(&q, &e[1], 20) && KeInsertByKeyDeviceQueue(&q, &e[2], 10) && KeInsertByKeyDeviceQueue(&q, &e[3], 15), "later inserts queue");
    check(q.DeviceListHead.Flink == &e[2].DeviceListEntry && e[2].DeviceListEntry.Flink == &e[3].DeviceListEntry && e[3].DeviceListEntry.Flink == &e[1].DeviceListEntry, "order 10, 15, 20");
    check(KeRemoveEntryDeviceQueue(&q, &e[3]) && !e[3].Inserted && !KeRemoveEntryDeviceQueue(&q, &e[3]), "remove an entry once");
    r = KeRemoveByKeyDeviceQueue(&q, 12);
    check(r == &e[1], "first entry with key >= 12: 20");
    r = KeRemoveByKeyDeviceQueue(&q, 99);
    check(r == &e[2], "no larger key: the first entry");
    check(KeRemoveByKeyDeviceQueue(&q, 0) == NULL && !q.Busy, "empty: NULL and idle");
}
