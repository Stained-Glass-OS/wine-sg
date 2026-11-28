/* Probe driver for patches/sg/8018 (test/nocode4-gate.sh): ntoskrnl exports a driver bundled with
 * UltiMaker Cura imports, spec stubs before:
 * - MmLockPagableCodeSection / MmLockPagableDataSection with MmLockPagableSectionByHandle and
 *   MmUnlockPagableImageSection;
 * - IoBuildPartialMdl;
 * - IoOpenDeviceInterfaceRegistryKey: the "Device Parameters" key of an interface the probe's device
 *   registers (the device answers IRP_MN_QUERY_ID as ROOT\SGNOCODE4\0000), and of one not registered. */
#include "sg-driver.h"

static const GUID probe_class = {0x5d6a8a1e,0x2b8e,0x4d7c,{0x9a,0x51,0x3e,0x6f,0x10,0x22,0x33,0x20}};

static NTSTATUS NTAPI probe_pnp(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = irp->IoStatus.Status;

    if (stack->MinorFunction == IRP_MN_QUERY_ID &&
        (stack->Parameters.QueryId.IdType == BusQueryDeviceID || stack->Parameters.QueryId.IdType == BusQueryInstanceID))
    {
        const WCHAR *id = stack->Parameters.QueryId.IdType == BusQueryDeviceID ? L"ROOT\\SGNOCODE4" : L"0000";
        SIZE_T size = (wcslen(id) + 1) * sizeof(WCHAR);
        WCHAR *buf = ExAllocatePool(PagedPool, size);
        if (buf)
        {
            RtlCopyMemory(buf, id, size);
            irp->IoStatus.Information = (ULONG_PTR)buf;
            status = STATUS_SUCCESS;
        }
        else status = STATUS_NO_MEMORY;
    }
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static void run_probe(DRIVER_OBJECT *driver)
{
    char *pool;
    MDL *mdl, *part;
    PFN_NUMBER *pfns, *ppfns;
    void *handle, *va;
    UNICODE_STRING name;
    HANDLE key = (HANDLE)0xdead;
    NTSTATUS status;
    PDRIVER_DISPATCH old_pnp;

    handle = MmLockPagableCodeSection((void *)run_probe);
    check(handle != NULL, "MmLockPagableCodeSection gives a handle %p", handle);
    MmUnlockPagableImageSection(handle);
    MmLockPagableSectionByHandle(handle);
    MmUnlockPagableImageSection(handle);
    handle = MmLockPagableDataSection(&failures);
    check(handle != NULL, "MmLockPagableDataSection gives a handle %p", handle);
    MmUnlockPagableImageSection(handle);

    /* a partial mdl of a nonpaged buffer, 6000 bytes from 4196 bytes in */
    pool = ExAllocatePool(NonPagedPool, 4 * 4096);
    mdl = IoAllocateMdl(pool, 4 * 4096, FALSE, FALSE, NULL);
    MmBuildMdlForNonPagedPool(mdl);
    va = pool + 4096 + 100;
    part = IoAllocateMdl(va, 6000, FALSE, FALSE, NULL);
    IoBuildPartialMdl(mdl, part, va, 6000);
    check(part->StartVa == PAGE_ALIGN(va) && part->ByteOffset == BYTE_OFFSET(va) && part->ByteCount == 6000,
          "partial mdl: start %p offset %lu count %lu", part->StartVa, part->ByteOffset, part->ByteCount);
    check((part->MdlFlags & (MDL_PARTIAL | MDL_SOURCE_IS_NONPAGED_POOL)) == (MDL_PARTIAL | MDL_SOURCE_IS_NONPAGED_POOL),
          "partial, nonpaged pool: flags %#x", part->MdlFlags);
    check(part->MappedSystemVa == va, "its system address %p (buffer %p)", part->MappedSystemVa, va);
    check(MmGetSystemAddressForMdlSafe(part, NormalPagePriority) == va, "MmGetSystemAddressForMdlSafe of it");
    pfns = MmGetMdlPfnArray(mdl);
    ppfns = MmGetMdlPfnArray(part);
    {
        ULONG first = ((ULONG_PTR)PAGE_ALIGN(va) - (ULONG_PTR)mdl->StartVa) >> PAGE_SHIFT, i, n = ADDRESS_AND_SIZE_TO_SPAN_PAGES(va, 6000), same = 1;
        for (i = 0; i < n; i++) if (ppfns[i] != pfns[first + i]) same = 0;
        check(same && n >= 2, "its %lu pages are the source's, from page %lu", n, first);
        va = pool + 3 * 4096;
        first = ((ULONG_PTR)PAGE_ALIGN(va) - (ULONG_PTR)mdl->StartVa) >> PAGE_SHIFT;
        IoBuildPartialMdl(mdl, part, va, 0);
        check(part->ByteCount == 4096 && ppfns[0] == pfns[first], "length 0: to the end of the source (%lu)", part->ByteCount);
    }
    IoFreeMdl(part);
    IoFreeMdl(mdl);
    ExFreePool(pool);

    old_pnp = driver->MajorFunction[IRP_MJ_PNP];
    driver->MajorFunction[IRP_MJ_PNP] = probe_pnp;
    status = IoRegisterDeviceInterface(sg_device, &probe_class, NULL, &name);
    check(!status, "IoRegisterDeviceInterface %#lx", status);
    if (!status)
    {
        UNICODE_STRING value = RTL_CONSTANT_STRING(L"SgValue");
        struct { KEY_VALUE_PARTIAL_INFORMATION info; ULONG more[4]; } buf;
        ULONG data = 0x8018, len = 0;

        status = IoOpenDeviceInterfaceRegistryKey(&name, KEY_ALL_ACCESS, &key);
        check(!status && key, "IoOpenDeviceInterfaceRegistryKey of %wZ: %#lx", &name, status);
        if (!status)
        {
            status = ZwSetValueKey(key, &value, 0, REG_DWORD, &data, sizeof(data));
            check(!status, "a value in its Device Parameters: %#lx", status);
            ZwClose(key);
        }
        key = NULL;
        status = IoOpenDeviceInterfaceRegistryKey(&name, KEY_READ, &key);
        if (!status)
        {
            status = ZwQueryValueKey(key, &value, KeyValuePartialInformation, &buf, sizeof(buf), &len);
            ZwClose(key);
        }
        check(!status && buf.info.Type == REG_DWORD && *(ULONG *)buf.info.Data == 0x8018,
              "opened again, the value is there: %#lx", status);
        RtlFreeUnicodeString(&name);
    }
    driver->MajorFunction[IRP_MJ_PNP] = old_pnp;

    key = (HANDLE)0xdead;
    RtlInitUnicodeString(&name, L"\\??\\ROOT#SGNOCODE4#0001#{5d6a8a1e-2b8e-4d7c-9a51-3e6f10223320}");
    status = IoOpenDeviceInterfaceRegistryKey(&name, KEY_READ, &key);
    check(status == STATUS_OBJECT_NAME_NOT_FOUND && !key, "IoOpenDeviceInterfaceRegistryKey of no interface: %#lx", status);
}
