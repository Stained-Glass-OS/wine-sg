/* Probe driver for patches/sg/5074: IoRegisterDriverReinitialization and
 * IoRegisterBootDriverReinitialization.
 * - routines registered in DriverEntry run once DriverEntry returned, in order, with the
 *   driver object, their context and Count, the number of calls so far this one included;
 * - a routine that registers itself again runs again after the others. */
#include <stdarg.h>
#include <ntddk.h>

static struct { void *context; ULONG count; DRIVER_OBJECT *driver; } calls[8];
static int ncalls;

static void NTAPI reinit(DRIVER_OBJECT *driver, void *context, ULONG count)
{
    if (ncalls < 8)
    {
        calls[ncalls].context = context;
        calls[ncalls].count = count;
        calls[ncalls].driver = driver;
        ncalls++;
    }
    if (context == (void *)1) IoRegisterDriverReinitialization(driver, reinit, (void *)3);
}

#define SG_DRIVER_ENTRY_HOOK(driver) do { \
    IoRegisterDriverReinitialization(driver, reinit, (void *)1); \
    IoRegisterBootDriverReinitialization(driver, reinit, (void *)2); \
    entry_calls = ncalls; } while (0)
static int entry_calls = -1;

#include "sg-driver.h"

static void run_probe(DRIVER_OBJECT *driver)
{
    static const struct { ULONG_PTR context; ULONG count; } expect[] = { {1, 1}, {2, 2}, {3, 3} };
    int i;

    check(entry_calls == 0, "nothing ran during DriverEntry: %d", entry_calls);
    check(ncalls == 3, "three calls: %d", ncalls);
    for (i = 0; i < 3 && i < ncalls; i++)
        check(calls[i].context == (void *)expect[i].context && calls[i].count == expect[i].count && calls[i].driver == driver,
              "call %d: context %p count %lu", i, calls[i].context, calls[i].count);
}
