/* Probe driver for patches/sg/5053: KeInitializeDpc makes a DPC object
 * (Type 0x13 DpcObject, MediumImportance, Number 0, routine and context; the
 * ntoskrnl driver tests check this on Windows) and KeSetImportanceDpc changes
 * its importance. */
#include "sg-driver.h"

static void NTAPI dpc_func(KDPC *dpc, void *context, void *arg1, void *arg2) {}

static void run_probe(DRIVER_OBJECT *driver)
{
    static const KDPC_IMPORTANCE levels[] = {LowImportance, HighImportance, MediumHighImportance, MediumImportance};
    KDPC dpc;
    int data;
    unsigned int i;

    memset(&dpc, 0xcc, sizeof(dpc));
    KeInitializeDpc(&dpc, dpc_func, &data);
    check(dpc.Type == 0x13, "Type %u", dpc.Type);
    check(dpc.Importance == MediumImportance, "Importance %u", dpc.Importance);
    check(dpc.Number == 0, "Number %u", dpc.Number);
    check(dpc.DeferredRoutine == dpc_func && dpc.DeferredContext == &data, "routine and context");
    for (i = 0; i < 4; i++)
    {
        KeSetImportanceDpc(&dpc, levels[i]);
        check(dpc.Importance == levels[i] && dpc.Type == 0x13, "KeSetImportanceDpc(%u): %u", levels[i], dpc.Importance);
    }
}
