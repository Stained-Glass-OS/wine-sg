/* Driver side of the kernel driver probes: makes \Device\SgProbe
 * (\DosDevices\SgProbe); the host's IOCTL runs the probe's run_probe() and
 * returns its "ok"/"FAIL" lines and a "RESULT: PASS|FAIL" line.
 * A probe defines run_probe() and may define SG_DRIVER_ENTRY_HOOK to run
 * checks inside DriverEntry (they are reported with the rest). */
#include <stdarg.h>
#include <ntddk.h>

#define SG_PROBE_IOCTL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

int __cdecl _vsnprintf(char *buffer, size_t count, const char *format, va_list args);

static char sg_out[16384];
static size_t sg_pos;
static int failures;
static DEVICE_OBJECT *sg_device;

static void sg_printf(const char *fmt, ...)
{
    va_list args;
    int len;

    if (sg_pos >= sizeof(sg_out) - 1) return;
    va_start(args, fmt);
    len = _vsnprintf(sg_out + sg_pos, sizeof(sg_out) - 1 - sg_pos, fmt, args);
    va_end(args);
    if (len < 0) sg_pos = sizeof(sg_out) - 1;
    else sg_pos += len;
}

#define check(c, ...) do { if (c) sg_printf("ok   "); else { sg_printf("FAIL "); failures++; } sg_printf(__VA_ARGS__); sg_printf("\n"); } while (0)

static void run_probe(DRIVER_OBJECT *driver);

static NTSTATUS NTAPI sg_complete(DEVICE_OBJECT *device, IRP *irp)
{
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI sg_ioctl(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    ULONG len = stack->Parameters.DeviceIoControl.OutputBufferLength;
    NTSTATUS status = STATUS_NOT_SUPPORTED;

    irp->IoStatus.Information = 0;
    if (stack->Parameters.DeviceIoControl.IoControlCode == SG_PROBE_IOCTL)
    {
        run_probe(device->DriverObject);
        sg_printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
        if (len > sg_pos) len = sg_pos;
        RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, sg_out, len);
        irp->IoStatus.Information = len;
        status = STATUS_SUCCESS;
    }
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static void NTAPI sg_unload(DRIVER_OBJECT *driver)
{
    UNICODE_STRING link;

    RtlInitUnicodeString(&link, L"\\DosDevices\\SgProbe");
    IoDeleteSymbolicLink(&link);
    if (sg_device) IoDeleteDevice(sg_device);
}

NTSTATUS NTAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *registry)
{
    UNICODE_STRING name, link;
    NTSTATUS status;

    driver->MajorFunction[IRP_MJ_CREATE] = sg_complete;
    driver->MajorFunction[IRP_MJ_CLOSE] = sg_complete;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = sg_ioctl;
    driver->DriverUnload = sg_unload;

#ifdef SG_DRIVER_ENTRY_HOOK
    SG_DRIVER_ENTRY_HOOK(driver);
#endif
    RtlInitUnicodeString(&name, L"\\Device\\SgProbe");
    RtlInitUnicodeString(&link, L"\\DosDevices\\SgProbe");
    if ((status = IoCreateDevice(driver, 0, &name, FILE_DEVICE_UNKNOWN, 0, FALSE, &sg_device))) return status;
    return IoCreateSymbolicLink(&link, &name);
}
