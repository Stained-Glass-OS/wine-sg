/* Host side of the kernel driver probes (test/*-driver.c with sg-driver.h):
 * installs the probe driver built next to the exe (sgdrv64.sys / sgdrv32.sys)
 * as a demand-start kernel driver service, starts it, asks \\.\SgProbe for
 * its results over an IOCTL, prints them and removes the service. Like Wine's
 * own ntoskrnl tests, a 32-bit host under WoW64 cannot load drivers and only
 * prints a skip and RESULT: PASS. A probe may define SG_HOST_BEFORE_IOCTL()
 * to prepare the calling thread, which is the driver's current thread during
 * the IOCTL. */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>

#define SG_PROBE_IOCTL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

static void sg_remove_service(SC_HANDLE manager)
{
    SERVICE_STATUS status;
    SC_HANDLE service = OpenServiceA(manager, "SgProbe", SERVICE_ALL_ACCESS);
    if (!service) return;
    ControlService(service, SERVICE_CONTROL_STOP, &status);
    DeleteService(service);
    CloseServiceHandle(service);
}

int main(void)
{
    static char results[65536];
    char path[MAX_PATH], *p;
    SC_HANDLE manager, service;
    SERVICE_STATUS status;
    BOOL wow64 = FALSE;
    DWORD got = 0, i;
    HANDLE dev;

    if (IsWow64Process(GetCurrentProcess(), &wow64) && wow64)
    {
        printf("ok   skipped: a WoW64 process cannot load kernel drivers\nRESULT: PASS\n");
        return 0;
    }
    GetModuleFileNameA(NULL, path, MAX_PATH);
    if ((p = strrchr(path, '\\'))) p[1] = 0;
    strcat(path, sizeof(void *) == 8 ? "sgdrv64.sys" : "sgdrv32.sys");

    if (!(manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS)))
    {
        printf("FAIL OpenSCManager %lu\nRESULT: FAIL\n", GetLastError());
        return 1;
    }
    sg_remove_service(manager);
    service = CreateServiceA(manager, "SgProbe", "SgProbe", SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
                             SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
    if (!service || !StartServiceA(service, 0, NULL))
    {
        printf("FAIL the probe driver did not start (%s): %lu\nRESULT: FAIL\n", path, GetLastError());
        sg_remove_service(manager);
        return 1;
    }
    for (i = 0; i < 100 && QueryServiceStatus(service, &status) && status.dwCurrentState != SERVICE_RUNNING; i++) Sleep(50);

#ifdef SG_HOST_BEFORE_IOCTL
    SG_HOST_BEFORE_IOCTL();
#endif
    dev = CreateFileA("\\\\.\\SgProbe", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (dev == INVALID_HANDLE_VALUE || !DeviceIoControl(dev, SG_PROBE_IOCTL, NULL, 0, results, sizeof(results) - 1, &got, NULL))
    {
        printf("FAIL no answer from the probe driver: %lu\nRESULT: FAIL\n", GetLastError());
        got = 0;
    }
    else
    {
        results[got] = 0;
        fputs(results, stdout);
    }
    if (dev != INVALID_HANDLE_VALUE) CloseHandle(dev);
    ControlService(service, SERVICE_CONTROL_STOP, &status);
    DeleteService(service);
    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return got && strstr(results, "RESULT: PASS") ? 0 : 1;
}
