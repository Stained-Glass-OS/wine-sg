/* Probe driver for patches/sg/5087: error log entries.
 * - IoAllocateErrorLogEntry returns a packet, NULL for a size over ERROR_LOG_MAXIMUM_SIZE (152);
 * - IoFreeErrorLogEntry frees an unused packet;
 * - IoWriteErrorLogEntry posts the packet to the event log (the host reads it back: source =
 *   the driver's name, event id = ErrorCode, error type for an error status, the strings, the
 *   dump data) and frees it. */
#include "sg-driver.h"

#define MAX_SIZE 152

typedef struct
{
    UCHAR MajorFunctionCode, RetryCount;
    USHORT DumpDataSize, NumberOfStrings, StringOffset, EventCategory;
    NTSTATUS ErrorCode;
    ULONG UniqueErrorValue;
    NTSTATUS FinalStatus;
    ULONG SequenceNumber, IoControlCode;
    LARGE_INTEGER DeviceOffset;
    ULONG DumpData[1];
} SG_PACKET;

NTSYSAPI void *NTAPI IoAllocateErrorLogEntry(void *object, UCHAR size);
NTSYSAPI void NTAPI IoWriteErrorLogEntry(void *packet);
NTSYSAPI void NTAPI IoFreeErrorLogEntry(void *packet);

static void run_probe(DRIVER_OBJECT *driver)
{
    static const WCHAR strings[] = L"first\0second";
    SG_PACKET *p;
    UCHAR size;
    void *q;

    check(IoAllocateErrorLogEntry(driver, MAX_SIZE + 1) == NULL, "size 153: NULL");
    check(IoAllocateErrorLogEntry(driver, 255) == NULL, "size 255: NULL");
    q = IoAllocateErrorLogEntry(sg_device, MAX_SIZE);
    check(q != NULL, "size 152 on a device object: a packet");
    if (q) IoFreeErrorLogEntry(q);

    size = (UCHAR)(FIELD_OFFSET(SG_PACKET, DumpData) + 8 + sizeof(strings) + sizeof(WCHAR));
    p = IoAllocateErrorLogEntry(driver, size);
    check(p != NULL, "a packet for a driver object");
    if (!p) return;
    p->MajorFunctionCode = IRP_MJ_DEVICE_CONTROL;
    p->DumpDataSize = 8;
    p->NumberOfStrings = 2;
    p->StringOffset = (USHORT)(FIELD_OFFSET(SG_PACKET, DumpData) + 8);
    p->EventCategory = 7;
    p->ErrorCode = 0xC0001234;
    p->UniqueErrorValue = 0x42;
    p->DumpData[0] = 0x11223344;
    p->DumpData[1] = 0x55667788;
    RtlCopyMemory((char *)p + p->StringOffset, strings, sizeof(strings));
    ((WCHAR *)((char *)p + p->StringOffset))[sizeof(strings) / sizeof(WCHAR)] = 0;
    IoWriteErrorLogEntry(p);
    check(1, "entry written");
}
