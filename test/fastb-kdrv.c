/* kernel-mode half of the fastb-kernel gate: irql, spin locks, probes, contiguous memory, MDL pages,
 * MmCopyVirtualMemory and KeQueryPriorityThread, run inside a driver loaded as a service */
#include <stdarg.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ddk/ntddk.h"
#include "ddk/ntifs.h"
#include "ddk/wdm.h"
#include "wine/exception.h"
#include "fastb-kdrv.h"


#ifdef _WIN64
#define KERNEL_ADDRESS ((void *)0xffff800000000123ull)
#else
#define KERNEL_ADDRESS ((void *)0xc0000123u)
#endif

static const char readonly_const[4096] = "read only";   /* a page that cannot be written */

static DRIVER_OBJECT *driver_obj;
static DEVICE_OBJECT *device;
static ULONG exc_code;

static LONG filter(EXCEPTION_POINTERS *ep)
{
    exc_code = ep->ExceptionRecord->ExceptionCode;
    return EXCEPTION_EXECUTE_HANDLER;
}


static ULONG try_probe_read(void *a, SIZE_T len, ULONG align)
{
    exc_code = 0;
    __TRY { ProbeForRead(a, len, align); } __EXCEPT(filter) { } __ENDTRY
    return exc_code;
}

static ULONG try_probe_write(void *a, SIZE_T len, ULONG align)
{
    exc_code = 0;
    __TRY { ProbeForWrite(a, len, align); } __EXCEPT(filter) { } __ENDTRY
    return exc_code;
}

#define CHECK(name, cond) do { res->ok[R_##name] = !!(cond); res->done++; } while (0)
#define CCHECK(name, cond) do { res->ok[C_##name] = !!(cond); } while (0)
#define CHECK_CODE(name, c, expected) do { res->code[R_##name] = (c); CHECK(name, (c) == (expected)); } while (0)

static void run_irql(struct self_results *res)
{
    KIRQL old, lock_old;
    KSPIN_LOCK lock;
    KLOCK_QUEUE_HANDLE queue;

    CHECK(irql_raise_from_passive, KeGetCurrentIrql() == PASSIVE_LEVEL && KfRaiseIrql(DISPATCH_LEVEL) == PASSIVE_LEVEL);
    CHECK(irql_raise_returns_dispatch, KfRaiseIrql(HIGH_LEVEL) == DISPATCH_LEVEL);
    KeLowerIrql(DISPATCH_LEVEL);
    CHECK(irql_lower_to_dispatch, KeGetCurrentIrql() == DISPATCH_LEVEL);
    KeLowerIrql(PASSIVE_LEVEL);
    CHECK(irql_lower_to_passive, KeGetCurrentIrql() == PASSIVE_LEVEL);

    old = KfRaiseIrql(APC_LEVEL);
    CHECK(irql_get_after_raise, old == PASSIVE_LEVEL && KeGetCurrentIrql() == APC_LEVEL);
    KeLowerIrql(old);
    old = KeRaiseIrqlToDpcLevel();
    CHECK(irql_raise_dpc_level_returns_old, old == PASSIVE_LEVEL && KeGetCurrentIrql() == DISPATCH_LEVEL);
    KeLowerIrql(old);
    old = KeRaiseIrqlToSynchLevel();
    CHECK(irql_raise_synch_level, old == PASSIVE_LEVEL && KeGetCurrentIrql() == SYNCH_LEVEL);
    KeLowerIrql(old);

    KeInitializeSpinLock(&lock);
    KeAcquireSpinLock(&lock, &lock_old);
    CHECK(spin_acquire_old_irql, lock_old == PASSIVE_LEVEL);
    CHECK(spin_held_at_dispatch, KeGetCurrentIrql() == DISPATCH_LEVEL);
    KeReleaseSpinLock(&lock, lock_old);
    CHECK(spin_release_restores, KeGetCurrentIrql() == PASSIVE_LEVEL);

    KfRaiseIrql(APC_LEVEL);
    KeAcquireSpinLock(&lock, &lock_old);
    CHECK(spin_nested_old_apc, lock_old == APC_LEVEL);
    KeReleaseSpinLock(&lock, lock_old);
    CHECK(spin_nested_restores_apc, KeGetCurrentIrql() == APC_LEVEL);
    KeLowerIrql(PASSIVE_LEVEL);

    KfRaiseIrql(APC_LEVEL);
    KeAcquireInStackQueuedSpinLock(&lock, &queue);
    CHECK(queued_spin_old_irql, queue.OldIrql == APC_LEVEL);
    CHECK(queued_spin_held_at_dispatch, KeGetCurrentIrql() == DISPATCH_LEVEL);
    KeReleaseInStackQueuedSpinLock(&queue);
    CHECK(queued_spin_release_restores, KeGetCurrentIrql() == APC_LEVEL);
    KeLowerIrql(PASSIVE_LEVEL);
}

typedef NTSTATUS (WINAPI *alloc_vm_t)(HANDLE, PVOID *, ULONG_PTR, SIZE_T *, ULONG, ULONG);
typedef NTSTATUS (WINAPI *protect_vm_t)(HANDLE, PVOID *, SIZE_T *, ULONG, ULONG *);
typedef NTSTATUS (WINAPI *free_vm_t)(HANDLE, PVOID *, SIZE_T *, ULONG);

/* two pages, the second one read-only (the driver lives in the service host, not in the caller) */
static void *make_two_pages(void)
{
    UNICODE_STRING n1 = RTL_CONSTANT_STRING(L"ZwAllocateVirtualMemory"), n2 = RTL_CONSTANT_STRING(L"ZwProtectVirtualMemory");
    alloc_vm_t alloc = MmGetSystemRoutineAddress(&n1);
    protect_vm_t protect = MmGetSystemRoutineAddress(&n2);
    void *pages = NULL, *second;
    SIZE_T size = 0x2000, psize = 0x1000;
    ULONG old;

    if (!alloc || !protect) return NULL;
    if (alloc(NtCurrentProcess(), &pages, 0, &size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) return NULL;
    second = (char *)pages + 0x1000;
    if (protect(NtCurrentProcess(), &second, &psize, PAGE_READONLY, &old)) return NULL;
    return pages;
}

static void run_probes(struct self_results *res)
{
    char *buf = ExAllocatePool(NonPagedPool, 0x300);
    void *pages = make_two_pages();
    ULONG c;

    CHECK(probe_read_valid, try_probe_read(buf, 16, 4) == 0);
    CHECK(probe_write_valid, try_probe_write(buf, 16, 4) == 0);
    CHECK_CODE(probe_read_misaligned, try_probe_read(buf + 1, 16, 4), STATUS_DATATYPE_MISALIGNMENT);
    CHECK_CODE(probe_write_misaligned, try_probe_write(buf + 2, 16, 4), STATUS_DATATYPE_MISALIGNMENT);
    CHECK(probe_align1_odd_ok, try_probe_read(buf + 1, 16, 1) == 0 && try_probe_write(buf + 3, 5, 1) == 0);
    CHECK_CODE(probe_read_kernel_range, try_probe_read(KERNEL_ADDRESS, 4, 1), STATUS_ACCESS_VIOLATION);
    CHECK_CODE(probe_write_kernel_range, try_probe_write(KERNEL_ADDRESS, 4, 1), STATUS_ACCESS_VIOLATION);
    CHECK_CODE(probe_read_wraparound, try_probe_read((void *)(~(ULONG_PTR)0 - 4), 16, 1), STATUS_ACCESS_VIOLATION);
    CHECK(probe_zero_length_kernel_ok, try_probe_read(KERNEL_ADDRESS, 0, 1) == 0 && try_probe_write(KERNEL_ADDRESS, 0, 1) == 0);
    CHECK_CODE(probe_write_readonly_const, try_probe_write((void *)readonly_const, 16, 1), STATUS_ACCESS_VIOLATION);
    CHECK(probe_read_readonly_const_ok, try_probe_read((void *)readonly_const, 16, 1) == 0);

    /* two pages from the caller, the second one read-only */
    if (pages)
    {
        CHECK_CODE(probe_write_second_page_readonly, try_probe_write((char *)pages + 0xff0, 0x20, 1), STATUS_ACCESS_VIOLATION);
        CHECK(probe_read_second_page_readonly_ok, try_probe_read((char *)pages + 0xff0, 0x20, 1) == 0);
        /* a write probe keeps what is there */
        memset(pages, 0x5a, 0x100);
        c = try_probe_write(pages, 0x100, 1);
        CHECK(probe_write_keeps_data, c == 0 && ((unsigned char *)pages)[0] == 0x5a && ((unsigned char *)pages)[0xff] == 0x5a);
        /* the last byte of the writable page is probed too */
        CHECK(probe_write_last_byte_page, try_probe_write((char *)pages + 0xfff, 1, 1) == 0 &&
                                          try_probe_write((char *)pages + 0xfff, 2, 1) == STATUS_ACCESS_VIOLATION);
    }
    ExFreePool(buf);
}

static void run_memory(struct self_results *res)
{
    PHYSICAL_ADDRESS low = {{0}}, high, boundary = {{0}}, phys;
    char *p;
    PMDL mdl;
    PFN_NUMBER *pfns;
    ULONG i;
    BOOLEAN ident;
    void *mapped;

    high.QuadPart = 0xffffffffull;
    p = MmAllocateContiguousMemory(0x2000, high);
    CHECK(contig_alloc_ok, p != NULL);
    CHECK(contig_page_aligned, p && !((ULONG_PTR)p & 0xfff));
    CHECK(contig_below_4gb, p && (ULONG_PTR)p + 0x1fff <= 0xffffffffull);
    if (p) { memset(p, 0xa5, 0x2000); }
    CHECK(contig_writable, p && p[0x1fff] == (char)0xa5);
    phys = MmGetPhysicalAddress(p);
    CHECK(contig_physical_identity, p && phys.QuadPart == (LONGLONG)(ULONG_PTR)p);
    if (p) MmFreeContiguousMemory(p);

    high.QuadPart = 0xffff;
    CHECK(contig_impossible_range_null, MmAllocateContiguousMemory(0x1000, high) == NULL);
    high.QuadPart = ~0ull;
    p = MmAllocateContiguousMemorySpecifyCache(0x1000, low, high, boundary, MmNonCached);
    CHECK(contig_cache_noncached_ok, p != NULL);
    if (p) MmFreeContiguousMemorySpecifyCache(p, 0x1000, MmNonCached);
    CHECK(contig_bad_cache_type_null, MmAllocateContiguousMemorySpecifyCache(0x1000, low, high, boundary, 99) == NULL);
    boundary.QuadPart = 0x1000;
    CHECK(contig_boundary_crossing_null, MmAllocateContiguousMemorySpecifyCache(0x2000, low, high, boundary, MmCached) == NULL);
    p = MmAllocateContiguousMemorySpecifyCache(0x1000, low, high, boundary, MmCached);
    CHECK(contig_boundary_ok, p != NULL);
    if (p) MmFreeContiguousMemorySpecifyCache(p, 0x1000, MmCached);
    boundary.QuadPart = 0;
    low.QuadPart = 0x800000000000ull;   /* above the whole user address space */
    CHECK(contig_low_bound_null, MmAllocateContiguousMemorySpecifyCache(0x1000, low, high, boundary, MmCached) == NULL);
    low.QuadPart = 0;
    CHECK(contig_zero_size_null, MmAllocateContiguousMemory(0, high) == NULL);

    mdl = MmAllocatePagesForMdl(low, high, boundary, 0x3000);
    CHECK(mdl_pages_alloc, mdl != NULL);
    CHECK(mdl_pages_bytecount, mdl && mdl->ByteCount == 0x3000);
    ident = mdl != NULL;
    if (mdl)
    {
        pfns = MmGetMdlPfnArray(mdl);
        for (i = 0; i < 3; i++) if (pfns[i] != ((ULONG_PTR)mdl->StartVa >> 12) + i) ident = FALSE;
    }
    CHECK(mdl_pages_pfn_identity, ident);
    mapped = mdl ? MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmCached, NULL, FALSE, NormalPagePriority) : NULL;
    CHECK(mdl_pages_map_returns_startva, mapped && mapped == mdl->StartVa);
    if (mapped) { memset(mapped, 0x33, 0x3000); }
    CHECK(mdl_pages_writable, mapped && ((char *)mapped)[0x2fff] == 0x33);
    if (mapped) MmUnmapLockedPages(mapped, mdl);
    CHECK(mdl_pages_unaligned_size_null, MmAllocatePagesForMdl(low, high, boundary, 0x1234) == NULL);
    if (mdl)
    {
        MmFreePagesFromMdl(mdl);
        CHECK(mdl_pages_free, mdl->ByteCount == 0);
        IoFreeMdl(mdl);
    }

    /* an MDL over non-paged pool describes it with the same numbers */
    p = ExAllocatePool(NonPagedPool, 0x1000);
    mdl = IoAllocateMdl(p, 0x1000, FALSE, FALSE, NULL);
    MmBuildMdlForNonPagedPool(mdl);
    pfns = MmGetMdlPfnArray(mdl);
    CHECK(mdl_nonpaged_pfn, pfns[0] == ((ULONG_PTR)p >> 12) && (mdl->MdlFlags & MDL_SOURCE_IS_NONPAGED_POOL));
    IoFreeMdl(mdl);
    ExFreePool(p);
}

static void run_copy(struct copy_input *in, struct copy_results *res)
{
    /* the driver's own process, where its memory is */
    PEPROCESS local_process = NULL;
    unsigned char *buf = ExAllocatePool(NonPagedPool, 0x400), *buf2 = buf + 0x200;
    PEPROCESS other = NULL;
    SIZE_T copied;
    NTSTATUS status;
    int i, good;

    PsLookupProcessByProcessId(NtCurrentTeb()->ClientId.UniqueProcess, &local_process);
    PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)in->pid, &other);

    memset(buf, 0, 0x200);
    copied = 0xdead;
    status = MmCopyVirtualMemory(other, (void *)(ULONG_PTR)in->src, local_process, buf, 256, KernelMode, &copied);
    CCHECK(copy_from_other_status, status == STATUS_SUCCESS);
    CCHECK(copy_from_other_count, copied == 256);
    for (i = 0, good = 1; i < 256; i++) if (buf[i] != (unsigned char)(i * 7 + 1)) good = 0;
    CCHECK(copy_from_other_data, good);

    for (i = 0; i < 256; i++) buf2[i] = (unsigned char)(i ^ 0x5a);
    copied = 0xdead;
    status = MmCopyVirtualMemory(local_process, buf2, other, (void *)(ULONG_PTR)in->dst, 256, KernelMode, &copied);
    CCHECK(copy_to_other_status, status == STATUS_SUCCESS);
    CCHECK(copy_to_other_count, copied == 256);

    copied = 0xdead;
    status = MmCopyVirtualMemory(other, (void *)(ULONG_PTR)in->src, local_process, buf, 0, KernelMode, &copied);
    CCHECK(copy_zero_length, status == STATUS_SUCCESS && copied == 0);

    copied = 0xdead;
    status = MmCopyVirtualMemory(other, (void *)0x10, local_process, buf, 16, KernelMode, &copied);
    CCHECK(copy_unmapped_partial, status == STATUS_PARTIAL_COPY);
    CCHECK(copy_unmapped_count, copied == 0);

    memset(buf2, 0, 64);
    for (i = 0; i < 64; i++) buf[i] = (unsigned char)(i + 100);
    copied = 0;
    status = MmCopyVirtualMemory(local_process, buf, local_process, buf2, 64, KernelMode, &copied);
    CCHECK(copy_same_process, status == STATUS_SUCCESS && copied == 64);
    for (i = 0, good = 1; i < 64; i++) if (buf2[i] != (unsigned char)(i + 100)) good = 0;
    CCHECK(copy_same_process_data, good);

    copied = 0xdead;
    status = MmCopyVirtualMemory(local_process, KERNEL_ADDRESS, local_process, buf2, 16, UserMode, &copied);
    CCHECK(copy_usermode_kernel_address, status == STATUS_ACCESS_VIOLATION);
    CCHECK(copy_usermode_kernel_count, copied == 0);

    res->done = NUM_COPY_CHECKS;
    if (other) ObDereferenceObject(other);
    if (local_process) ObDereferenceObject(local_process);
    ExFreePool(buf);
}

static NTSTATUS WINAPI dispatch(DEVICE_OBJECT *dev, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR info = 0;

    if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL)
    {
        ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
        ULONG outlen = stack->Parameters.DeviceIoControl.OutputBufferLength;
        void *buffer = irp->AssociatedIrp.SystemBuffer;

        if (code == FASTB_IOCTL_SELF && outlen >= sizeof(struct self_results))
        {
            struct self_results *res = buffer;
            memset(res, 0, sizeof(*res));
            run_irql(res);
            run_probes(res);
            run_memory(res);
            info = sizeof(*res);
        }
        else if (code == FASTB_IOCTL_COPY && outlen >= sizeof(struct copy_results) &&
                 stack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(struct copy_input))
        {
            struct copy_input in = *(struct copy_input *)buffer;
            struct copy_results *res = buffer;
            memset(res, 0, sizeof(*res));
            run_copy(&in, res);
            info = sizeof(*res);
        }
        else if (code == FASTB_IOCTL_PRIO && outlen >= sizeof(ULONG))
        {
            *(ULONG *)buffer = KeQueryPriorityThread(KeGetCurrentThread());
            info = sizeof(ULONG);
        }
        else status = STATUS_INVALID_PARAMETER;
    }
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = info;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static void WINAPI unload(DRIVER_OBJECT *driver)
{
    UNICODE_STRING link = RTL_CONSTANT_STRING(L"\\??\\FastBKernel");
    IoDeleteSymbolicLink(&link);
    IoDeleteDevice(device);
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Device\\FastBKernel");
    UNICODE_STRING link = RTL_CONSTANT_STRING(L"\\??\\FastBKernel");
    NTSTATUS status;

    driver_obj = driver;
    if ((status = IoCreateDevice(driver, 0, &name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device)))
        return status;
    if ((status = IoCreateSymbolicLink(&link, &name))) return status;
    driver->MajorFunction[IRP_MJ_CREATE] = dispatch;
    driver->MajorFunction[IRP_MJ_CLOSE] = dispatch;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;
    driver->DriverUnload = unload;
    return STATUS_SUCCESS;
}
