/* Probe driver for patches/sg/5079: MDL locking, mapping and protection.
 * - MmBuildMdlForNonPagedPool marks the MDL as non-paged pool and describes the buffer's pages;
 * - MmProbeAndLockPages locks pages that permit the operation and raises STATUS_ACCESS_VIOLATION
 *   for those that do not (read-only for a write, no access, not committed);
 * - MmMapLockedPagesSpecifyCache maps a locked MDL, MmUnmapLockedPages releases that;
 * - MmUnlockPages unlocks (and releases the mapping); MmProtectMdlSystemAddress changes the
 *   protection of a mapped MDL, STATUS_NOT_MAPPED_VIEW otherwise. */
#include "sg-driver.h"

#include "sg-ntdll.h"

typedef struct { PVOID BaseAddress, AllocationBase; ULONG AllocationProtect; USHORT PartitionId; SIZE_T RegionSize; ULONG State, Protect, Type; } SG_MBI;
typedef PVOID (NTAPI *add_veh_t)(ULONG first, PVOID handler);
typedef ULONG (NTAPI *remove_veh_t)(PVOID handle);
typedef NTSTATUS (NTAPI *protect_t)(HANDLE, PVOID *, SIZE_T *, ULONG, ULONG *);
typedef NTSTATUS (NTAPI *query_t)(HANDLE, PVOID, int, PVOID, SIZE_T, SIZE_T *);
static protect_t pNtProtectVirtualMemory;
static query_t pNtQueryVirtualMemory;
NTSYSAPI NTSTATUS NTAPI NtAllocateVirtualMemory(HANDLE, PVOID *, ULONG_PTR, SIZE_T *, ULONG, ULONG);
NTSYSAPI NTSTATUS NTAPI NtFreeVirtualMemory(HANDLE, PVOID *, SIZE_T *, ULONG);
NTSYSAPI void NTAPI RtlCaptureContext(CONTEXT *context);
static void (NTAPI *pRtlRestoreContext)(CONTEXT *context, EXCEPTION_RECORD *rec);

static CONTEXT catch_ctx;
static volatile LONG catch_armed;
static volatile NTSTATUS caught;

static LONG NTAPI catch_handler(EXCEPTION_POINTERS *e)
{
    if (!catch_armed) return EXCEPTION_CONTINUE_SEARCH;
    catch_armed = 0;
    caught = e->ExceptionRecord->ExceptionCode;
    pRtlRestoreContext(&catch_ctx, NULL);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* lock with the exception caught: returns the exception code or 0 */
static NTSTATUS try_lock(MDL *mdl, KPROCESSOR_MODE mode, LOCK_OPERATION op)
{
    static volatile LONG phase;

    caught = 0;
    phase = 0;
    RtlCaptureContext(&catch_ctx);
    if (!phase)
    {
        phase = 1;
        catch_armed = 1;
        MmProbeAndLockPages(mdl, mode, op);
        catch_armed = 0;
    }
    return caught;
}

static void *alloc_pages(ULONG pages, ULONG protect)
{
    void *base = NULL;
    SIZE_T size = pages * 4096;

    if (NtAllocateVirtualMemory((HANDLE)-1, &base, 0, &size, MEM_RESERVE | MEM_COMMIT, protect)) return NULL;
    return base;
}

static ULONG region_protect(void *addr)
{
    SG_MBI info;
    SIZE_T len;

    if (pNtQueryVirtualMemory((HANDLE)-1, addr, 0, &info, sizeof(info), &len)) return 0xdead;
    return info.Protect;
}

static void run_probe(DRIVER_OBJECT *driver)
{
    PFN_NUMBER *pfns;
    add_veh_t add_veh = sg_ntdll_proc("RtlAddVectoredExceptionHandler");
    remove_veh_t remove_veh = sg_ntdll_proc("RtlRemoveVectoredExceptionHandler");
    PVOID veh;
    char *pool, *mem, *p;
    MDL *mdl;
    NTSTATUS status;
    void *map;
    ULONG i;
    int distinct;

    pRtlRestoreContext = sg_ntdll_proc("RtlRestoreContext");
    pNtProtectVirtualMemory = sg_ntdll_proc("NtProtectVirtualMemory");
    pNtQueryVirtualMemory = sg_ntdll_proc("NtQueryVirtualMemory");
    check(add_veh && remove_veh && pNtProtectVirtualMemory && pNtQueryVirtualMemory && pRtlRestoreContext, "ntdll routines found");
    if (!add_veh || !remove_veh || !pNtProtectVirtualMemory || !pNtQueryVirtualMemory || !pRtlRestoreContext) return;
    veh = add_veh(1, catch_handler);

    /* non-paged pool */
    pool = ExAllocatePool(NonPagedPool, 4 * 4096);
    mdl = IoAllocateMdl(pool + 100, 8000, FALSE, FALSE, NULL);
    check(mdl && !(mdl->MdlFlags & (MDL_SOURCE_IS_NONPAGED_POOL | MDL_PAGES_LOCKED | MDL_MAPPED_TO_SYSTEM_VA)), "fresh mdl has no flags");
    MmBuildMdlForNonPagedPool(mdl);
    check((mdl->MdlFlags & MDL_SOURCE_IS_NONPAGED_POOL) && !(mdl->MdlFlags & MDL_PAGES_LOCKED), "non-paged pool flag: %#x", mdl->MdlFlags);
    check(mdl->MappedSystemVa == pool + 100, "mapped system va %p (buffer %p)", mdl->MappedSystemVa, pool + 100);
    check(MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority) == pool + 100, "system address of the mdl");
    pfns = MmGetMdlPfnArray(mdl);
    distinct = 1;
    for (i = 0; i < ADDRESS_AND_SIZE_TO_SPAN_PAGES(pool + 100, 8000); i++)
    {
        if (!pfns[i]) distinct = 0;
        if (i && pfns[i] == pfns[i - 1]) distinct = 0;
    }
    check(distinct, "page frame numbers are set and distinct");
    check(MmProtectMdlSystemAddress(mdl, 0x123) == STATUS_INVALID_PAGE_PROTECTION, "invalid protection");
    IoFreeMdl(mdl);
    ExFreePool(pool);

    /* lock, map, protect, unlock */
    mem = alloc_pages(3, PAGE_READWRITE);
    check(mem != NULL, "allocated three pages");
    mdl = IoAllocateMdl(mem + 16, 9000, FALSE, FALSE, NULL);
    check(MmProtectMdlSystemAddress(mdl, PAGE_READONLY) == STATUS_NOT_MAPPED_VIEW, "protect before mapping: STATUS_NOT_MAPPED_VIEW");
    check(MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmCached, NULL, FALSE, NormalPagePriority) == NULL, "mapping an unlocked mdl fails");
    status = try_lock(mdl, KernelMode, IoWriteAccess);
    check(!status, "lock for write: exception %#lx", status);
    check((mdl->MdlFlags & (MDL_PAGES_LOCKED | MDL_WRITE_OPERATION)) == (MDL_PAGES_LOCKED | MDL_WRITE_OPERATION), "locked, write operation: %#x", mdl->MdlFlags);
    pfns = MmGetMdlPfnArray(mdl);
    check(pfns[0] && pfns[1] && pfns[2] && pfns[0] != pfns[1] && pfns[1] != pfns[2], "page frame numbers set");
    check(!(mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA), "not mapped yet");
    map = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmCached, NULL, FALSE, NormalPagePriority);
    check(map == mem + 16, "mapped at the buffer: %p (buffer %p)", map, mem + 16);
    check((mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) && mdl->MappedSystemVa == map, "mapped flag and address");
    check(MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority) == map, "same system address again");
    check(MmProtectMdlSystemAddress(mdl, PAGE_READONLY) == STATUS_SUCCESS && region_protect(mem) == PAGE_READONLY, "protect read-only: %#lx", region_protect(mem));
    check(MmProtectMdlSystemAddress(mdl, PAGE_READWRITE) == STATUS_SUCCESS && region_protect(mem) == PAGE_READWRITE, "protect read-write");
    MmUnmapLockedPages(map, mdl);
    check(!(mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) && (mdl->MdlFlags & MDL_PAGES_LOCKED), "unmapped, still locked: %#x", mdl->MdlFlags);
    map = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmCached, NULL, FALSE, NormalPagePriority);
    check(map == mem + 16, "mapped again");
    MmUnlockPages(mdl);
    check(!(mdl->MdlFlags & (MDL_PAGES_LOCKED | MDL_MAPPED_TO_SYSTEM_VA | MDL_WRITE_OPERATION)), "unlock releases the mapping and the lock: %#x", mdl->MdlFlags);
    /* user mode mapping returns the user address and keeps the mdl unmapped */
    status = try_lock(mdl, KernelMode, IoReadAccess);
    check(!status && (mdl->MdlFlags & MDL_PAGES_LOCKED) && !(mdl->MdlFlags & MDL_WRITE_OPERATION), "lock for read has no write flag: %#x", mdl->MdlFlags);
    check(MmMapLockedPagesSpecifyCache(mdl, UserMode, MmCached, NULL, FALSE, NormalPagePriority) == mem + 16, "user mode mapping");
    MmUnlockPages(mdl);
    IoFreeMdl(mdl);

    /* probing: what the pages permit */
    p = mem + 4096;
    {
        ULONG old; void *a = p; SIZE_T sz = 4096;
        pNtProtectVirtualMemory((HANDLE)-1, &a, &sz, PAGE_READONLY, &old);
    }
    mdl = IoAllocateMdl(p, 100, FALSE, FALSE, NULL);
    status = try_lock(mdl, KernelMode, IoReadAccess);
    check(!status && (mdl->MdlFlags & MDL_PAGES_LOCKED), "read-only page locks for read: %#lx", status);
    MmUnlockPages(mdl);
    status = try_lock(mdl, KernelMode, IoWriteAccess);
    check(status == STATUS_ACCESS_VIOLATION && !(mdl->MdlFlags & MDL_PAGES_LOCKED), "read-only page for write: %#lx", status);
    status = try_lock(mdl, KernelMode, IoModifyAccess);
    check(status == STATUS_ACCESS_VIOLATION, "read-only page for modify: %#lx", status);
    IoFreeMdl(mdl);
    {
        ULONG old; void *a = p; SIZE_T sz = 4096;
        pNtProtectVirtualMemory((HANDLE)-1, &a, &sz, PAGE_NOACCESS, &old);
    }
    mdl = IoAllocateMdl(p, 100, FALSE, FALSE, NULL);
    status = try_lock(mdl, KernelMode, IoReadAccess);
    check(status == STATUS_ACCESS_VIOLATION, "no-access page for read: %#lx", status);
    IoFreeMdl(mdl);
    /* a range crossing into the no-access page is refused as a whole */
    mdl = IoAllocateMdl(mem + 4000, 200, FALSE, FALSE, NULL);
    status = try_lock(mdl, KernelMode, IoReadAccess);
    check(status == STATUS_ACCESS_VIOLATION && !(mdl->MdlFlags & MDL_PAGES_LOCKED), "range reaching a no-access page: %#lx", status);
    IoFreeMdl(mdl);
    {
        SIZE_T sz = 0; void *a = mem;
        NtFreeVirtualMemory((HANDLE)-1, &a, &sz, MEM_RELEASE);
    }
    mdl = IoAllocateMdl(mem, 100, FALSE, FALSE, NULL);
    status = try_lock(mdl, KernelMode, IoReadAccess);
    check(status == STATUS_ACCESS_VIOLATION, "freed memory: %#lx", status);
    IoFreeMdl(mdl);
    remove_veh(veh);
}
