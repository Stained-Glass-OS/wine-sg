/* Probe driver for patches/sg/5081:
 * - SeLocateProcessImageName returns the current process's image name in one allocation
 *   (a UNICODE_STRING followed by the text), STATUS_SUCCESS, freed with ExFreePool;
 * - IoGetRelatedDeviceObject returns the highest device attached to the file's device. */
#include "sg-driver.h"

NTSYSAPI NTSTATUS NTAPI SeLocateProcessImageName(PEPROCESS process, UNICODE_STRING **name);

static void run_probe(DRIVER_OBJECT *driver)
{
    UNICODE_STRING *name = (UNICODE_STRING *)0x1234;
    DEVICE_OBJECT *upper = NULL, *top;
    NTSTATUS status;
    USHORT n;

    status = SeLocateProcessImageName(PsGetCurrentProcess(), &name);
    check(status == STATUS_SUCCESS && name && name != (UNICODE_STRING *)0x1234, "SeLocateProcessImageName: %#lx %p", status, name);
    if (!status && name)
    {
        n = name->Length / sizeof(WCHAR);
        check(n > 4 && name->Buffer && !_wcsnicmp(name->Buffer + n - 4, L".exe", 4), "image name ends in .exe: %.*ws", n, name->Buffer);
        check(name->Buffer > (WCHAR *)name && (char *)name->Buffer >= (char *)(name + 1), "text follows the UNICODE_STRING in one block");
        check(name->MaximumLength >= name->Length, "MaximumLength %u covers Length %u", name->MaximumLength, name->Length);
        ExFreePool(name);
    }
    name = (UNICODE_STRING *)0x1234;
    status = SeLocateProcessImageName(NULL, &name);
    check(status != STATUS_SUCCESS && (!name || name == (UNICODE_STRING *)0x1234), "no process: %#lx", status);

    check(sg_file && sg_file->DeviceObject == sg_device, "the ioctl's file is on our device");
    check(IoGetRelatedDeviceObject(sg_file) == sg_device, "no attached device: the device itself");
    status = IoCreateDevice(driver, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &upper);
    check(!status, "IoCreateDevice for the upper device: %#lx", status);
    if (!status)
    {
        top = IoAttachDeviceToDeviceStack(upper, sg_device);
        check(top == sg_device, "attached above the probe device");
        check(IoGetRelatedDeviceObject(sg_file) == upper, "the highest attached device");
        IoDetachDevice(sg_device);
        IoDeleteDevice(upper);
        check(IoGetRelatedDeviceObject(sg_file) == sg_device, "after detaching, the device itself");
    }
}
