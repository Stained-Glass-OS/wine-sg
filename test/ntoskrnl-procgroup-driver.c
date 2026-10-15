/* Probe driver for patches/sg/5054: KeQueryGroupAffinity(0) is the active
 * processor set (one group here), other groups are empty;
 * KeGetCurrentProcessorNumberEx fills group 0 and the number it returns. */
#include "sg-driver.h"

static void run_probe(DRIVER_OBJECT *driver)
{
    KAFFINITY active = KeQueryActiveProcessors(), group0 = KeQueryGroupAffinity(0);
    PROCESSOR_NUMBER pn;
    ULONG n;

    check(group0 == active && group0, "group 0 affinity %#Ix (active %#Ix)", (ULONG_PTR)group0, (ULONG_PTR)active);
    check(KeQueryGroupAffinity(1) == 0, "group 1 is empty");
    memset(&pn, 0xcc, sizeof(pn));
    n = KeGetCurrentProcessorNumberEx(&pn);
    check(pn.Group == 0 && pn.Number == n && !pn.Reserved && (active & ((KAFFINITY)1 << n)), "processor %lu in group %u", n, pn.Group);
}
