/* Probe driver for patches/sg/5055: IoSetThreadHardErrorMode "returns TRUE if
 * hard errors were previously enabled" - the mode is per thread, enabled at
 * first. */
#include "sg-driver.h"

static void run_probe(DRIVER_OBJECT *driver)
{
    static const BOOLEAN seq[] = {FALSE, FALSE, TRUE, FALSE, TRUE};
    BOOLEAN enabled = TRUE, prev;
    unsigned int i;

    for (i = 0; i < 5; i++)
    {
        prev = IoSetThreadHardErrorMode(seq[i]);
        check(prev == enabled, "set %u: previous %u (want %u)", seq[i], prev, enabled);
        enabled = seq[i];
    }
}
