/* Probe driver for patches/sg/5056: ObReferenceObjectByPointer takes a
 * reference when the object is of the given type (or no type is given) and
 * fails STATUS_OBJECT_TYPE_MISMATCH otherwise. */
#include "sg-driver.h"

NTSTATUS NTAPI PsLookupProcessByProcessId(HANDLE, PEPROCESS *);
NTSTATUS NTAPI PsLookupThreadByThreadId(HANDLE, PETHREAD *);
/* the exported type variables, through their import slots */
#ifdef _WIN64
extern POBJECT_TYPE *sg_process_type __asm__("__imp_PsProcessType");
extern POBJECT_TYPE *sg_thread_type __asm__("__imp_PsThreadType");
#else
extern POBJECT_TYPE *sg_process_type __asm__("__imp__PsProcessType");
extern POBJECT_TYPE *sg_thread_type __asm__("__imp__PsThreadType");
#endif

static void run_probe(DRIVER_OBJECT *driver)
{
    PEPROCESS process = NULL;
    PETHREAD thread = NULL;
    NTSTATUS st;

    PsLookupProcessByProcessId(PsGetCurrentProcessId(), &process);
    PsLookupThreadByThreadId(PsGetCurrentThreadId(), &thread);
    check(process && thread, "process %p and thread %p looked up", process, thread);
    if (!process || !thread) return;

    st = ObReferenceObjectByPointer(process, 0, *sg_process_type, KernelMode);
    check(st == STATUS_SUCCESS, "a process as a process: %#lx", st);
    if (!st) ObDereferenceObject(process);
    st = ObReferenceObjectByPointer(thread, 0, *sg_thread_type, KernelMode);
    check(st == STATUS_SUCCESS, "a thread as a thread: %#lx", st);
    if (!st) ObDereferenceObject(thread);
    st = ObReferenceObjectByPointer(process, 0, *sg_thread_type, KernelMode);
    check(st == STATUS_OBJECT_TYPE_MISMATCH, "a process as a thread: %#lx", st);
    st = ObReferenceObjectByPointer(driver, 0, NULL, KernelMode);
    check(st == STATUS_SUCCESS, "the driver object with no type: %#lx", st);
    if (!st) ObDereferenceObject(driver);
    ObDereferenceObject(process);
    ObDereferenceObject(thread);
}
