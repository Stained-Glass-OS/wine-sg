/* Host for test/ntoskrnl-shutdown-driver.c: asks services.exe to send the shutdown control
 * to the services that accept it, the way wineboot --shutdown does, after it asked the driver what it
 * registered; what the driver received is written to C:\\sgshutdown.txt. */
#include <windows.h>
#include <stdio.h>
static void sg_shutdown_services(void)
{
    HANDLE request = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"__wine_SvcctlShutdownEvent");
    HANDLE done = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"__wine_SvcctlShutdownDoneEvent");
    DWORD wait = WAIT_FAILED;

    if (request && done)
    {
        ResetEvent(done);
        SetEvent(request);
        wait = WaitForSingleObject(done, 10000);
    }
    printf("info shutdown request: %s (%lu)\n", request && done ? "sent" : "no events", wait);
}
#define SG_HOST_AFTER_IOCTL() sg_shutdown_services()

#include "sg-driver-host.h"
