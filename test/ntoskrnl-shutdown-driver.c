/* Probe driver for patches/sg/5086: shutdown notification.
 * - IoRegisterShutdownNotification / IoRegisterLastChanceShutdownNotification set
 *   DO_SHUTDOWN_REGISTERED and succeed (also when repeated);
 * - IoUnregisterShutdownNotification clears it;
 * - when services.exe sends the shutdown control (the host asks for it after the checks), the driver
 *   gets one IRP_MJ_SHUTDOWN per registered device, the last chance one after the other, none for
 *   the unregistered one; it appends a line per IRP to C:\\sgshutdown.txt for the gate. */
#include <stdarg.h>
#include <ntddk.h>
extern size_t __cdecl strlen(const char *);

static DEVICE_OBJECT *dev_normal, *dev_last, *dev_unreg;
static DEVICE_OBJECT *received[8];
static int nreceived;
static NTSTATUS reg_status[4];
static ULONG flags_before_unreg, flags_after_unreg;

static void note(const char *line)
{
    UNICODE_STRING path;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK iosb;
    HANDLE file;

    RtlInitUnicodeString(&path, L"\\??\\C:\\sgshutdown.txt");
    InitializeObjectAttributes(&attr, &path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    if (ZwCreateFile(&file, FILE_APPEND_DATA | SYNCHRONIZE, &attr, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN_IF,
                     FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0)) return;
    ZwWriteFile(file, NULL, NULL, NULL, &iosb, (void *)line, (ULONG)strlen(line), NULL, NULL);
    ZwClose(file);
}

static NTSTATUS NTAPI shutdown_dispatch(DEVICE_OBJECT *device, IRP *irp)
{
    if (nreceived < 8) received[nreceived++] = device;
    note(device == dev_normal ? "normal\n" : device == dev_last ? "last\n" : "other\n");
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

#define SG_DRIVER_ENTRY_HOOK(driver) driver->MajorFunction[IRP_MJ_SHUTDOWN] = shutdown_dispatch
#define SG_DRIVER_ENTRY_AFTER_HOOK(driver) register_all(driver)
static void register_all(DRIVER_OBJECT *driver);

#include "sg-driver.h"

static void register_all(DRIVER_OBJECT *driver)
{
    IoCreateDevice(driver, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev_last);
    IoCreateDevice(driver, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev_unreg);
    dev_normal = sg_device;
    /* the last chance one is registered first: it must still be sent last */
    reg_status[0] = IoRegisterLastChanceShutdownNotification(dev_last);
    reg_status[1] = IoRegisterShutdownNotification(dev_normal);
    reg_status[2] = IoRegisterShutdownNotification(dev_normal);
    IoRegisterShutdownNotification(dev_unreg);
    flags_before_unreg = dev_unreg->Flags;
    IoUnregisterShutdownNotification(dev_unreg);
    flags_after_unreg = dev_unreg->Flags;
}

static void run_probe(DRIVER_OBJECT *driver)
{
    check(!reg_status[0] && !reg_status[1] && !reg_status[2], "registrations succeed (also twice): %#lx %#lx %#lx", reg_status[0], reg_status[1], reg_status[2]);
    check((dev_normal->Flags & DO_SHUTDOWN_REGISTERED) && (dev_last->Flags & DO_SHUTDOWN_REGISTERED), "registered devices carry DO_SHUTDOWN_REGISTERED");
    check((flags_before_unreg & DO_SHUTDOWN_REGISTERED) && !(flags_after_unreg & DO_SHUTDOWN_REGISTERED), "unregistering clears it: %#lx -> %#lx", flags_before_unreg, flags_after_unreg);
    check(nreceived == 0, "no IRP_MJ_SHUTDOWN before the shutdown: %d", nreceived);
}
