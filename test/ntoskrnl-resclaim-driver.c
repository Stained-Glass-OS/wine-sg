/* Probe driver for patches/sg/5088: IoReportResourceForDetection.
 * - a list of port / memory / interrupt / dma resources is claimed for the driver or, when a device
 *   list is given, for the device (the device list wins over the driver list);
 * - an overlapping claim of another owner is STATUS_CONFLICTING_ADDRESSES with ConflictDetected TRUE,
 *   and claims nothing; two shared claims do not conflict;
 * - a list with a count of zero gives the claim back; a new claim of the owner replaces its old one;
 * - a list that is too small or has an invalid descriptor type is STATUS_UNSUCCESSFUL. */
#include "sg-driver.h"

NTSYSAPI NTSTATUS NTAPI IoReportResourceForDetection(DRIVER_OBJECT *drv, CM_RESOURCE_LIST *drv_list, ULONG drv_size,
                                                     DEVICE_OBJECT *dev, CM_RESOURCE_LIST *dev_list, ULONG dev_size,
                                                     BOOLEAN *conflict);

#define BUF_SIZE 256

static CM_RESOURCE_LIST *make_list(void *buffer, int count)
{
    CM_RESOURCE_LIST *list = buffer;
    RtlZeroMemory(buffer, BUF_SIZE);
    list->Count = 1;
    list->List[0].InterfaceType = Isa;
    list->List[0].PartialResourceList.Version = 1;
    list->List[0].PartialResourceList.Revision = 1;
    list->List[0].PartialResourceList.Count = count;
    return list;
}

static void add_range(CM_RESOURCE_LIST *list, int i, UCHAR type, UCHAR share, ULONGLONG start, ULONG length)
{
    CM_PARTIAL_RESOURCE_DESCRIPTOR *d = &list->List[0].PartialResourceList.PartialDescriptors[i];
    d->Type = type;
    d->ShareDisposition = share;
    d->u.Port.Start.QuadPart = start;
    d->u.Port.Length = length;
}

static void add_irq(CM_RESOURCE_LIST *list, int i, UCHAR share, ULONG vector)
{
    CM_PARTIAL_RESOURCE_DESCRIPTOR *d = &list->List[0].PartialResourceList.PartialDescriptors[i];
    d->Type = CmResourceTypeInterrupt;
    d->ShareDisposition = share;
    d->u.Interrupt.Level = vector;
    d->u.Interrupt.Vector = vector;
    d->u.Interrupt.Affinity = 0xff;
}

static void add_dma(CM_RESOURCE_LIST *list, int i, UCHAR share, ULONG channel)
{
    CM_PARTIAL_RESOURCE_DESCRIPTOR *d = &list->List[0].PartialResourceList.PartialDescriptors[i];
    d->Type = CmResourceTypeDma;
    d->ShareDisposition = share;
    d->u.Dma.Channel = channel;
}

static NTSTATUS claim(DRIVER_OBJECT *drv, CM_RESOURCE_LIST *drv_list, DEVICE_OBJECT *dev, CM_RESOURCE_LIST *dev_list, BOOLEAN *conflict)
{
    *conflict = 0x77;
    return IoReportResourceForDetection(drv, drv_list, drv_list ? BUF_SIZE : 0, dev, dev_list, dev_list ? BUF_SIZE : 0, conflict);
}

static void run_probe(DRIVER_OBJECT *driver)
{
    static char b1[BUF_SIZE], b2[BUF_SIZE], b3[BUF_SIZE];
    DEVICE_OBJECT *dev2 = NULL;
    CM_RESOURCE_LIST *l1, *l2, *l3;
    BOOLEAN conflict;
    NTSTATUS status;

    IoCreateDevice(driver, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev2);

    /* the driver claims ports 0x300-0x30f and an interrupt */
    l1 = make_list(b1, 2);
    add_range(l1, 0, CmResourceTypePort, CmResourceShareDriverExclusive, 0x300, 0x10);
    add_irq(l1, 1, CmResourceShareDriverExclusive, 5);
    status = claim(driver, l1, NULL, NULL, &conflict);
    check(status == STATUS_SUCCESS && conflict == FALSE, "driver claim: %#lx conflict %d", status, conflict);

    /* a device overlapping the ports */
    l2 = make_list(b2, 1);
    add_range(l2, 0, CmResourceTypePort, CmResourceShareDriverExclusive, 0x308, 0x10);
    status = claim(driver, NULL, dev2, l2, &conflict);
    check(status == STATUS_CONFLICTING_ADDRESSES && conflict == TRUE, "overlapping ports: %#lx conflict %d", status, conflict);
    /* adjacent ports do not overlap */
    add_range(l2, 0, CmResourceTypePort, CmResourceShareDriverExclusive, 0x310, 0x10);
    status = claim(driver, NULL, dev2, l2, &conflict);
    check(status == STATUS_SUCCESS && conflict == FALSE, "adjacent ports: %#lx conflict %d", status, conflict);
    /* the same interrupt */
    l3 = make_list(b3, 2);
    add_range(l3, 0, CmResourceTypePort, CmResourceShareDriverExclusive, 0x500, 0x10);
    add_irq(l3, 1, CmResourceShareDriverExclusive, 5);
    status = claim(driver, NULL, dev2, l3, &conflict);
    check(status == STATUS_CONFLICTING_ADDRESSES && conflict == TRUE, "same interrupt: %#lx", status);
    /* the failed claim claimed nothing: the ports 0x500 are free for the driver */
    add_range(l3, 1, CmResourceTypePort, CmResourceShareDriverExclusive, 0x700, 0x10);
    status = claim(driver, NULL, dev2, l3, &conflict);
    check(status == STATUS_SUCCESS, "no conflict now (the device's claim was replaced): %#lx", status);

    /* a device list wins over the driver list: the driver list overlaps, the device list does not */
    l1 = make_list(b1, 1);
    add_range(l1, 0, CmResourceTypePort, CmResourceShareDriverExclusive, 0x700, 0x10);
    l2 = make_list(b2, 1);
    add_range(l2, 0, CmResourceTypePort, CmResourceShareDriverExclusive, 0x900, 0x10);
    status = claim(driver, l1, sg_device, l2, &conflict);
    check(status == STATUS_SUCCESS && conflict == FALSE, "the device list wins over the driver list: %#lx", status);

    /* shared against shared is fine, shared against exclusive is not */
    l1 = make_list(b1, 1);
    add_range(l1, 0, CmResourceTypeMemory, CmResourceShareShared, 0xa0000, 0x1000);
    status = claim(driver, NULL, sg_device, l1, &conflict);
    check(status == STATUS_SUCCESS, "memory claim shared by the probe device: %#lx", status);
    status = claim(driver, NULL, dev2, l1, &conflict);
    check(status == STATUS_SUCCESS && conflict == FALSE, "shared against shared: %#lx", status);
    add_range(l1, 0, CmResourceTypeMemory, CmResourceShareDriverExclusive, 0xa0800, 0x1000);
    status = claim(driver, NULL, dev2, l1, &conflict);
    check(status == STATUS_CONFLICTING_ADDRESSES && conflict == TRUE, "exclusive against shared: %#lx", status);

    /* dma */
    l2 = make_list(b2, 1);
    add_dma(l2, 0, CmResourceShareDriverExclusive, 3);
    status = claim(driver, NULL, sg_device, l2, &conflict);
    check(status == STATUS_SUCCESS, "dma 3 for the probe device: %#lx", status);
    status = claim(driver, NULL, dev2, l2, &conflict);
    check(status == STATUS_CONFLICTING_ADDRESSES, "dma 3 for the other device: %#lx", status);

    /* giving a claim back */
    l3 = make_list(b3, 0);
    status = claim(driver, NULL, sg_device, l3, &conflict);
    check(status == STATUS_SUCCESS && conflict == FALSE, "a count of zero releases: %#lx", status);
    status = claim(driver, NULL, dev2, l2, &conflict);
    check(status == STATUS_SUCCESS, "dma 3 is free after that: %#lx", status);

    /* invalid lists */
    l1 = make_list(b1, 1);
    add_range(l1, 0, 9, CmResourceShareDriverExclusive, 0x1000, 0x10);
    status = claim(driver, NULL, dev2, l1, &conflict);
    check(status == STATUS_UNSUCCESSFUL, "invalid descriptor type: %#lx", status);
    conflict = 0;
    status = IoReportResourceForDetection(driver, NULL, 0, dev2, l1, 8, &conflict);
    check(status == STATUS_UNSUCCESSFUL, "list too small: %#lx", status);

    /* clean up */
    l3 = make_list(b3, 0);
    claim(driver, NULL, dev2, l3, &conflict);
    claim(driver, l3, NULL, NULL, &conflict);
    if (dev2) IoDeleteDevice(dev2);
}
