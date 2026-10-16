/* NtSetInformationThread(ThreadPriority): the absolute priority 1..31 set through the class' base (patch 3743) */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xc0000004)
#define STATUS_ACCESS_VIOLATION ((NTSTATUS)0xc0000005)
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#ifndef ThreadPriority
#define ThreadPriority 2
#endif

static NTSTATUS (WINAPI *pSet)(HANDLE, int, const void *, ULONG);

static const struct { DWORD class; LONG value; int expect; const char *what; } cases[] =
{
    { NORMAL_PRIORITY_CLASS, 1, THREAD_PRIORITY_IDLE, "normal 1 = idle" },
    { NORMAL_PRIORITY_CLASS, 15, THREAD_PRIORITY_TIME_CRITICAL, "normal 15 = time critical" },
    { NORMAL_PRIORITY_CLASS, 8, 0, "normal 8" },
    { NORMAL_PRIORITY_CLASS, 9, 1, "normal 9" },
    { NORMAL_PRIORITY_CLASS, 10, 2, "normal 10" },
    { NORMAL_PRIORITY_CLASS, 6, -2, "normal 6" },
    { NORMAL_PRIORITY_CLASS, 7, -1, "normal 7" },
    { NORMAL_PRIORITY_CLASS, 12, 2, "normal 12 is kept to +2" },
    { NORMAL_PRIORITY_CLASS, 5, -2, "normal 5 is kept to -2" },
    { HIGH_PRIORITY_CLASS, 13, 0, "high 13" },
    { HIGH_PRIORITY_CLASS, 11, -2, "high 11" },
    { HIGH_PRIORITY_CLASS, 15, THREAD_PRIORITY_TIME_CRITICAL, "high 15 = time critical" },
    { IDLE_PRIORITY_CLASS, 4, 0, "idle class 4" },
    { IDLE_PRIORITY_CLASS, 1, THREAD_PRIORITY_IDLE, "idle class 1 = idle" },
    { BELOW_NORMAL_PRIORITY_CLASS, 6, 0, "below normal 6" },
    { ABOVE_NORMAL_PRIORITY_CLASS, 9, -1, "above normal 9" },
    { REALTIME_PRIORITY_CLASS, 24, 0, "realtime 24" },
    { REALTIME_PRIORITY_CLASS, 26, 2, "realtime 26" },
    { REALTIME_PRIORITY_CLASS, 16, THREAD_PRIORITY_IDLE, "realtime 16 = idle" },
    { REALTIME_PRIORITY_CLASS, 31, THREAD_PRIORITY_TIME_CRITICAL, "realtime 31 = time critical" },
};

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    HANDLE thread;
    LONG value;
    NTSTATUS status;
    unsigned int i;

    pSet = (void *)GetProcAddress(ntdll, "NtSetInformationThread");
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (!SetPriorityClass(GetCurrentProcess(), cases[i].class)) { printf("note: no class for %s\n", cases[i].what); continue; }
        value = cases[i].value;
        status = pSet(GetCurrentThread(), ThreadPriority, &value, sizeof(value));
        CHECK(status == 0, "%s: status %#lx", cases[i].what, status);
        CHECK(GetThreadPriority(GetCurrentThread()) == cases[i].expect, "%s: GetThreadPriority %d, expected %d", cases[i].what,
              GetThreadPriority(GetCurrentThread()), cases[i].expect);
    }
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), 0);

    value = 0; status = pSet(GetCurrentThread(), ThreadPriority, &value, 4); CHECK(status == STATUS_INVALID_PARAMETER, "0: %#lx", status);
    value = 32; status = pSet(GetCurrentThread(), ThreadPriority, &value, 4); CHECK(status == STATUS_INVALID_PARAMETER, "32: %#lx", status);
    value = -1; status = pSet(GetCurrentThread(), ThreadPriority, &value, 4); CHECK(status == STATUS_INVALID_PARAMETER, "-1: %#lx", status);
    value = 8; status = pSet(GetCurrentThread(), ThreadPriority, &value, 8); CHECK(status == STATUS_INFO_LENGTH_MISMATCH, "length 8: %#lx", status);
    status = pSet(GetCurrentThread(), ThreadPriority, NULL, 4); CHECK(status == STATUS_ACCESS_VIOLATION, "NULL: %#lx", status);
    status = pSet((HANDLE)0xdeadbeef, ThreadPriority, &value, 4); CHECK(status == STATUS_INVALID_HANDLE, "bad handle: %#lx", status);

    /* another thread of this process */
    thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Sleep, (void *)2000, CREATE_SUSPENDED, NULL);
    value = 10;
    status = pSet(thread, ThreadPriority, &value, 4);
    CHECK(status == 0 && GetThreadPriority(thread) == 2, "another thread: %#lx %d", status, GetThreadPriority(thread));
    TerminateThread(thread, 0);
    CloseHandle(thread);

    /* the thread of another process: the base is that process' class */
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[] = "cmd.exe /c exit";
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED | HIGH_PRIORITY_CLASS, NULL, NULL, &si, &pi))
        {
            SetPriorityClass(pi.hProcess, HIGH_PRIORITY_CLASS);
            CHECK(GetPriorityClass(pi.hProcess) == HIGH_PRIORITY_CLASS, "child class %#lx", GetPriorityClass(pi.hProcess));
            value = 13;
            status = pSet(pi.hThread, ThreadPriority, &value, 4);
            CHECK(status == 0 && GetThreadPriority(pi.hThread) == 0, "thread of a high class process: %#lx %d", status, GetThreadPriority(pi.hThread));
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
        else CHECK(0, "child process %lu", GetLastError());
    }

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
