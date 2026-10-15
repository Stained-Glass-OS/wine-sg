/* Probe driver for patches/sg/5052: IoAllocateDriverObjectExtension makes a
 * zeroed per-driver area for an identifier and IoGetDriverObjectExtension
 * finds it; the same identifier again fails STATUS_OBJECT_NAME_COLLISION with
 * a NULL pointer, another identifier has its own area, an unknown one gives
 * NULL (ntoskrnl driver tests, run on Windows). Both were stubs. */
#include "sg-driver.h"

static void run_probe(DRIVER_OBJECT *driver)
{
    static const struct { void *id; ULONG size; } ids[] = {{NULL, 100}, {(void *)0x1234, 8}, {(void *)run_probe, 4096}};
    void *ext[3], *again, *got;
    unsigned int i, j;
    NTSTATUS status;

    for (i = 0; i < 3; i++)
    {
        BOOLEAN zero = TRUE;

        ext[i] = NULL;
        status = IoAllocateDriverObjectExtension(driver, ids[i].id, ids[i].size, &ext[i]);
        check(status == STATUS_SUCCESS && ext[i], "allocate for id %p (%lu bytes): %#lx %p", ids[i].id, ids[i].size, status, ext[i]);
        if (!ext[i]) continue;
        for (j = 0; j < ids[i].size; j++) if (((UCHAR *)ext[i])[j]) zero = FALSE;
        check(zero, "id %p: zero-filled", ids[i].id);
        memset(ext[i], 0x40 + i, ids[i].size);
    }
    check(ext[0] != ext[1] && ext[1] != ext[2], "each id has its own area");
    for (i = 0; i < 3; i++)
    {
        got = IoGetDriverObjectExtension(driver, ids[i].id);
        check(got == ext[i] && (!got || ((UCHAR *)got)[ids[i].size - 1] == 0x40 + i), "get id %p: %p", ids[i].id, got);
        again = (void *)0xdeadbeef;
        status = IoAllocateDriverObjectExtension(driver, ids[i].id, 16, &again);
        check(status == STATUS_OBJECT_NAME_COLLISION && again == NULL, "id %p again: %#lx %p", ids[i].id, status, again);
    }
    got = IoGetDriverObjectExtension(driver, (void *)0xdead);
    check(got == NULL, "an unknown id: %p", got);
}
