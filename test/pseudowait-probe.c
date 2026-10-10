/* Waits on pseudo handles (patches/sg/2214): WaitForSingleObject and
 * NtWaitForSingleObject accept the pseudo handles of the current process and
 * thread, a wait on several objects (WaitForMultipleObjects,
 * NtWaitForMultipleObjects) refuses them with ERROR_INVALID_HANDLE /
 * STATUS_INVALID_HANDLE.  No child process. */
#include <windows.h>
#include <stdio.h>

typedef LONG NTSTATUS;
#define STATUS_INVALID_HANDLE_ ((NTSTATUS)0xC0000008)

static NTSTATUS (WINAPI *pNtWaitForSingleObject)(HANDLE, BOOLEAN, LARGE_INTEGER *);
static NTSTATUS (WINAPI *pNtWaitForMultipleObjects)(ULONG, const HANDLE *, ULONG, BOOLEAN, LARGE_INTEGER *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int apc_ran;
static void CALLBACK apc(ULONG_PTR p) { apc_ran = 1; }

int main(void)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    HANDLE ev[2], h[2];
    LARGE_INTEGER to;
    DWORD r;
    NTSTATUS st;

    pNtWaitForSingleObject = (void *)GetProcAddress(nt, "NtWaitForSingleObject");
    pNtWaitForMultipleObjects = (void *)GetProcAddress(nt, "NtWaitForMultipleObjects");
    ev[0] = CreateEventA(NULL, TRUE, TRUE, NULL);    /* signaled */
    ev[1] = CreateEventA(NULL, TRUE, FALSE, NULL);   /* not */

    /* single object waits take pseudo handles */
    r = WaitForSingleObject(GetCurrentProcess(), 50);
    check(r == WAIT_TIMEOUT, "WaitForSingleObject(current process pseudo handle) waits (times out)");
    r = WaitForSingleObject(GetCurrentThread(), 50);
    check(r == WAIT_TIMEOUT, "WaitForSingleObject(current thread pseudo handle) waits (times out)");
    r = WaitForSingleObjectEx(GetCurrentProcess(), 50, FALSE);
    check(r == WAIT_TIMEOUT, "WaitForSingleObjectEx too");
    to.QuadPart = -500000;
    check(pNtWaitForSingleObject(GetCurrentProcess(), FALSE, &to) == (NTSTATUS)0x102, "NtWaitForSingleObject(current process)");
    to.QuadPart = -500000;
    check(pNtWaitForSingleObject(GetCurrentThread(), FALSE, &to) == (NTSTATUS)0x102, "NtWaitForSingleObject(current thread)");

    /* waits on several objects refuse them */
    h[0] = GetCurrentProcess();
    SetLastError(0xdeadbeef);
    r = WaitForMultipleObjects(1, h, FALSE, 50);
    check(r == WAIT_FAILED && GetLastError() == ERROR_INVALID_HANDLE, "WaitForMultipleObjects(current process) is WAIT_FAILED / ERROR_INVALID_HANDLE");
    h[0] = GetCurrentThread();
    SetLastError(0xdeadbeef);
    r = WaitForMultipleObjects(1, h, FALSE, 50);
    check(r == WAIT_FAILED && GetLastError() == ERROR_INVALID_HANDLE, "WaitForMultipleObjects(current thread) is WAIT_FAILED / ERROR_INVALID_HANDLE");
    h[0] = ev[0];
    h[1] = GetCurrentProcess();
    SetLastError(0xdeadbeef);
    r = WaitForMultipleObjects(2, h, FALSE, 50);
    check(r == WAIT_FAILED && GetLastError() == ERROR_INVALID_HANDLE, "...also as the second of two (a signaled event first does not hide it)");
    to.QuadPart = -500000;
    st = pNtWaitForMultipleObjects(1, &h[1], TRUE, FALSE, &to);
    check(st == STATUS_INVALID_HANDLE_, "NtWaitForMultipleObjects(current process) is STATUS_INVALID_HANDLE");
    h[1] = GetCurrentThread();
    to.QuadPart = -500000;
    st = pNtWaitForMultipleObjects(2, h, TRUE, FALSE, &to);
    check(st == STATUS_INVALID_HANDLE_, "NtWaitForMultipleObjects(event, current thread) is STATUS_INVALID_HANDLE");

    /* real objects keep working */
    h[0] = ev[1];
    h[1] = ev[0];
    r = WaitForMultipleObjects(2, h, FALSE, 50);
    check(r == WAIT_OBJECT_0 + 1, "WaitForMultipleObjects(any) returns the index of the signaled event");
    r = WaitForMultipleObjects(2, h, TRUE, 50);
    check(r == WAIT_TIMEOUT, "WaitForMultipleObjects(all) times out while one is not signaled");
    SetEvent(ev[1]);
    r = WaitForMultipleObjects(2, h, TRUE, 50);
    check(r == WAIT_OBJECT_0, "...and succeeds when both are");
    check(WaitForSingleObject(ev[0], 0) == WAIT_OBJECT_0, "WaitForSingleObject on a signaled event");
    ResetEvent(ev[1]);
    check(WaitForSingleObject(ev[1], 20) == WAIT_TIMEOUT, "...and on one that is not (times out)");
    SetLastError(0xdeadbeef);
    r = WaitForSingleObject((HANDLE)0xdeadbeef, 0);
    check(r == WAIT_FAILED && GetLastError() == ERROR_INVALID_HANDLE, "WaitForSingleObject on a bad handle is WAIT_FAILED / ERROR_INVALID_HANDLE");
    SetLastError(0xdeadbeef);
    r = WaitForMultipleObjects(0, h, FALSE, 0);
    check(r == WAIT_FAILED && GetLastError() == ERROR_INVALID_PARAMETER, "a count of 0 is ERROR_INVALID_PARAMETER");

    /* an alertable single wait still runs an APC */
    apc_ran = 0;
    QueueUserAPC(apc, GetCurrentThread(), 0);
    r = WaitForSingleObjectEx(ev[1], 1000, TRUE);
    check(r == WAIT_IO_COMPLETION && apc_ran, "WaitForSingleObjectEx(alertable) returns WAIT_IO_COMPLETION after running an APC");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
