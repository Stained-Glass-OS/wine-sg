/* Probe driver for patches/sg/5073: IoInitializeTimer, IoStartTimer, IoStopTimer.
 * - IoInitializeTimer succeeds and the routine is not called before IoStartTimer;
 * - once started it is called about once a second with the device and the context;
 * - after IoStopTimer it is not called again; started again, it runs again. */
#include "sg-driver.h"

static LONG calls;
static DEVICE_OBJECT *seen_device;
static void *seen_context;

static void NTAPI timer_routine(DEVICE_OBJECT *device, void *context)
{
    seen_device = device;
    seen_context = context;
    InterlockedIncrement(&calls);
}

static void wait_ms(LONG ms)
{
    LARGE_INTEGER t;
    t.QuadPart = -10000LL * ms;
    KeDelayExecutionThread(KernelMode, FALSE, &t);
}

static void run_probe(DRIVER_OBJECT *driver)
{
    NTSTATUS st;
    LONG n;

    st = IoInitializeTimer(sg_device, timer_routine, (void *)0x5a);
    check(st == STATUS_SUCCESS, "IoInitializeTimer %#lx", st);
    if (st) return;
    wait_ms(1300);
    check(calls == 0, "not called before IoStartTimer: %ld", calls);

    IoStartTimer(sg_device);
    wait_ms(3400);
    n = calls;
    check(n >= 2 && n <= 4, "about once a second: %ld calls in 3.4 s", n);
    check(seen_device == sg_device && seen_context == (void *)0x5a, "with the device and the context: %p %p",
          seen_device, seen_context);

    IoStopTimer(sg_device);
    n = calls;
    wait_ms(1500);
    check(calls == n, "not called after IoStopTimer: %ld then %ld", n, calls);

    IoStartTimer(sg_device);
    wait_ms(1500);
    check(calls > n, "started again: %ld", calls);
    IoStopTimer(sg_device);
}
