/* Probe driver for patches/sg/5085: PsSetLoadImageNotifyRoutineEx.
 * - only PS_IMAGE_NOTIFY_CONFLICTING_ARCHITECTURE (1) is a valid flag, others are
 *   STATUS_INVALID_PARAMETER_2;
 * - up to 64 routines can be registered, then STATUS_INSUFFICIENT_RESOURCES; removing one makes
 *   room again; removing a routine that is not registered is STATUS_PROCEDURE_NOT_FOUND. */
#include "sg-driver.h"

typedef NTSTATUS (NTAPI *set_ex_t)(PLOAD_IMAGE_NOTIFY_ROUTINE routine, ULONG_PTR flags);

static void NTAPI notify(UNICODE_STRING *name, HANDLE pid, IMAGE_INFO *info) { }
static void NTAPI other(UNICODE_STRING *name, HANDLE pid, IMAGE_INFO *info) { }

static void run_probe(DRIVER_OBJECT *driver)
{
    UNICODE_STRING str;
    set_ex_t set_ex;
    NTSTATUS status;
    int i;

    RtlInitUnicodeString(&str, L"PsSetLoadImageNotifyRoutineEx");
    set_ex = MmGetSystemRoutineAddress(&str);
    check(set_ex != NULL, "PsSetLoadImageNotifyRoutineEx is exported");
    if (!set_ex) return;

    status = set_ex(notify, 2);
    check(status == STATUS_INVALID_PARAMETER_2, "flag 2: %#lx", status);
    status = set_ex(notify, 0x100);
    check(status == STATUS_INVALID_PARAMETER_2, "flag 0x100: %#lx", status);
    status = set_ex(notify, 3);
    check(status == STATUS_INVALID_PARAMETER_2, "flags 3: %#lx", status);
    status = set_ex(notify, 1);
    check(status == STATUS_SUCCESS, "flag 1 (conflicting architecture): %#lx", status);
    status = PsSetLoadImageNotifyRoutine(notify);
    check(status == STATUS_SUCCESS, "PsSetLoadImageNotifyRoutine: %#lx", status);
    status = PsRemoveLoadImageNotifyRoutine(other);
    check(status == STATUS_PROCEDURE_NOT_FOUND, "an unregistered routine: %#lx", status);
    check(PsRemoveLoadImageNotifyRoutine(notify) == STATUS_SUCCESS && PsRemoveLoadImageNotifyRoutine(notify) == STATUS_SUCCESS, "both registrations removed");
    check(PsRemoveLoadImageNotifyRoutine(notify) == STATUS_PROCEDURE_NOT_FOUND, "and no more");

    for (i = 0; i < 64; i++)
    {
        status = PsSetLoadImageNotifyRoutine(notify);
        if (status) break;
    }
    check(i == 64, "64 routines can be registered: stopped at %d (%#lx)", i, status);
    status = PsSetLoadImageNotifyRoutine(notify);
    check(status == STATUS_INSUFFICIENT_RESOURCES, "the 65th: %#lx", status);
    check(PsRemoveLoadImageNotifyRoutine(notify) == STATUS_SUCCESS, "remove one");
    status = PsSetLoadImageNotifyRoutine(notify);
    check(status == STATUS_SUCCESS, "room again: %#lx", status);
    for (i = 0; i < 64; i++) PsRemoveLoadImageNotifyRoutine(notify);
    check(PsRemoveLoadImageNotifyRoutine(notify) == STATUS_PROCEDURE_NOT_FOUND, "all removed");
}
