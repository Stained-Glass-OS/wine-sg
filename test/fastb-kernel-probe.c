/* fastb-kernel gate, user-mode half: loads fastb-kdrv.sys as a service and runs its checks (patch 3743) */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>
#include "fastb-kdrv.h"

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* the other process (this program started with "child") has these at the same addresses */
static unsigned char pattern[256];
static unsigned char scratch[256];

static const char *self_names[] = {
#define X(n) #n,
    SELF_CHECKS(X)
#undef X
};
static const char *copy_names[] = {
#define X(n) #n,
    COPY_CHECKS(X)
#undef X
};

static int child_main(const char *ready, const char *stop)
{
    HANDLE r = OpenEventA(EVENT_ALL_ACCESS, FALSE, ready), s = OpenEventA(EVENT_ALL_ACCESS, FALSE, stop);
    int i;
    for (i = 0; i < 256; i++) pattern[i] = (unsigned char)(i * 7 + 1);
    SetEvent(r);
    WaitForSingleObject(s, 60000);
    return 0;
}

static const struct { DWORD class; int relative; unsigned expect; const char *what; } prio[] =
{
    { NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_IDLE, 1, "normal/idle" },
    { NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST, 6, "normal/lowest" },
    { NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL, 7, "normal/below" },
    { NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL, 8, "normal/normal" },
    { NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL, 9, "normal/above" },
    { NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST, 10, "normal/highest" },
    { NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_TIME_CRITICAL, 15, "normal/time critical" },
    { HIGH_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL, 13, "high/normal" },
    { HIGH_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST, 15, "high/highest" },
    { HIGH_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST, 11, "high/lowest" },
    { HIGH_PRIORITY_CLASS, THREAD_PRIORITY_IDLE, 1, "high/idle" },
    { IDLE_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL, 4, "idle class/normal" },
    { IDLE_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST, 6, "idle class/highest" },
    { BELOW_NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL, 6, "below normal/normal" },
    { ABOVE_NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL, 10, "above normal/normal" },
    { REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL, 24, "realtime/normal" },
    { REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST, 26, "realtime/highest" },
    { REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_IDLE, 16, "realtime/idle" },
    { REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_TIME_CRITICAL, 31, "realtime/time critical" },
};

int main(int argc, char **argv)
{
    char ready[64], stop[64], cmd[MAX_PATH + 128], self[MAX_PATH];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    SC_HANDLE manager, service;
    HANDLE dev, ready_ev, stop_ev;
    struct self_results self_res;
    struct copy_input in;
    struct copy_results copy_res;
    unsigned char readback[256];
    SIZE_T got;
    DWORD ret, i;
    int good;

    if (argc >= 4 && !strcmp(argv[1], "child")) return child_main(argv[2], argv[3]);
    if (argc < 2) { printf("usage: probe driver.sys\n"); return 2; }

    sprintf(ready, "fastb_ready_%lu", GetCurrentProcessId());
    sprintf(stop, "fastb_stop_%lu", GetCurrentProcessId());
    ready_ev = CreateEventA(NULL, TRUE, FALSE, ready);
    stop_ev = CreateEventA(NULL, TRUE, FALSE, stop);
    GetModuleFileNameA(NULL, self, MAX_PATH);
    sprintf(cmd, "\"%s\" child %s %s", self, ready, stop);
    CHECK(CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi), "child: %lu", GetLastError());
    CHECK(WaitForSingleObject(ready_ev, 30000) == WAIT_OBJECT_0, "the child did not get ready");

    manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    CHECK(manager != NULL, "OpenSCManager %lu", GetLastError());
    service = CreateServiceA(manager, "FastBKernel", "FastBKernel", SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
                             SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, argv[1], NULL, NULL, NULL, NULL, NULL);
    if (!service && GetLastError() == ERROR_SERVICE_EXISTS) service = OpenServiceA(manager, "FastBKernel", SERVICE_ALL_ACCESS);
    CHECK(service != NULL, "CreateService %lu", GetLastError());
    if (!service || !StartServiceA(service, 0, NULL))
    {
        printf("FAIL the driver did not start: %lu\n", GetLastError());
        printf("RESULT: FAIL\n");
        return 1;
    }
    dev = CreateFileA("\\\\.\\FastBKernel", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(dev != INVALID_HANDLE_VALUE, "open the device: %lu", GetLastError());

    if (dev != INVALID_HANDLE_VALUE)
    {
        memset(&self_res, 0, sizeof(self_res));
        ret = 0;
        CHECK(DeviceIoControl(dev, FASTB_IOCTL_SELF, NULL, 0, &self_res, sizeof(self_res), &ret, NULL),
              "self ioctl %lu", GetLastError());
        CHECK(self_res.done == NUM_SELF_CHECKS, "only %u of %u checks ran", self_res.done, (unsigned)NUM_SELF_CHECKS);
        for (i = 0; i < NUM_SELF_CHECKS; i++)
            CHECK(self_res.ok[i], "%s (exception %#x)", self_names[i], self_res.code[i]);

        memset(&in, 0, sizeof(in));
        in.pid = pi.dwProcessId;
        in.src = (ULONG_PTR)pattern;
        in.dst = (ULONG_PTR)scratch;
        CHECK(DeviceIoControl(dev, FASTB_IOCTL_COPY, &in, sizeof(in), &copy_res, sizeof(copy_res), &ret, NULL),
              "copy ioctl %lu", GetLastError());
        CHECK(copy_res.done == NUM_COPY_CHECKS, "only %u of %u copy checks ran", copy_res.done, (unsigned)NUM_COPY_CHECKS);
        for (i = 0; i < NUM_COPY_CHECKS; i++) CHECK(copy_res.ok[i], "%s", copy_names[i]);
        memset(readback, 0, sizeof(readback));
        CHECK(ReadProcessMemory(pi.hProcess, scratch, readback, sizeof(readback), &got) && got == 256, "read back %lu", GetLastError());
        for (i = 0, good = 1; i < 256; i++) if (readback[i] != (unsigned char)(i ^ 0x5a)) good = 0;
        CHECK(good, "the bytes MmCopyVirtualMemory wrote into the other process");

        for (i = 0; i < sizeof(prio) / sizeof(prio[0]); i++)
        {
            ULONG p = 0xdead;
            BOOL set = SetPriorityClass(GetCurrentProcess(), prio[i].class);
            if (!set) { printf("note: cannot set the class for %s\n", prio[i].what); continue; }
            SetThreadPriority(GetCurrentThread(), prio[i].relative);
            CHECK(DeviceIoControl(dev, FASTB_IOCTL_PRIO, NULL, 0, &p, sizeof(p), &ret, NULL), "prio ioctl %lu", GetLastError());
            CHECK(p == prio[i].expect, "KeQueryPriorityThread for %s: %lu, expected %u", prio[i].what, p, prio[i].expect);
        }
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        CloseHandle(dev);
    }
    {
        SERVICE_STATUS st;
        ControlService(service, SERVICE_CONTROL_STOP, &st);
        for (i = 0; i < 50 && QueryServiceStatus(service, &st) && st.dwCurrentState == SERVICE_STOP_PENDING; i++) Sleep(100);
        DeleteService(service);
        CloseServiceHandle(service);
    }
    SetEvent(stop_ev);
    WaitForSingleObject(pi.hProcess, 10000);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
