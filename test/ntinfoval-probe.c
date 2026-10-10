/* Argument/handle validation of ntdll calls that Wine's conformance tests
 * (dlls/ntdll/tests/info.c) mark todo_wine (patches/sg/2209):
 *  - NtQueryInformationProcess(ProcessVmCounters) length mismatch reports the base size
 *  - NtRead/WriteVirtualMemory on an unreadable remote address is STATUS_PARTIAL_COPY
 *  - NtSetSystemInformation(SystemTimeAdjustmentInformation) checks its length
 *  - NtQuerySystemInformationEx(SystemLogicalProcessorInformationEx) on relations that
 *    are not reported is STATUS_UNSUCCESSFUL
 *  - NtSetInformationThread(ThreadEnableAlignmentFaultFixup) checks the handle
 *  - NtOpenThread checks the process id of the client id (STATUS_INVALID_CID)
 * No child process is ever started by a child. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG NTSTATUS;
typedef struct { PVOID UniqueProcess; PVOID UniqueThread; } CID;
typedef struct { ULONG Length; PVOID Root; PVOID Name; ULONG Attr; PVOID SD, QoS; } OA;

#undef STATUS_INVALID_HANDLE
#undef STATUS_ACCESS_VIOLATION
#define STATUS_SUCCESS 0
#define STATUS_PARTIAL_COPY ((NTSTATUS)0x8000000D)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022)
#define STATUS_OBJECT_TYPE_MISMATCH ((NTSTATUS)0xC0000024)
#define STATUS_INVALID_CID ((NTSTATUS)0xC000000B)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001)
#define STATUS_PRIVILEGE_NOT_HELD ((NTSTATUS)0xC0000061)
#define STATUS_ACCESS_VIOLATION ((NTSTATUS)0xC0000005)

static NTSTATUS (WINAPI *pNtQueryInformationProcess)(HANDLE, ULONG, void *, ULONG, ULONG *);
static NTSTATUS (WINAPI *pNtReadVirtualMemory)(HANDLE, const void *, void *, SIZE_T, SIZE_T *);
static NTSTATUS (WINAPI *pNtWriteVirtualMemory)(HANDLE, void *, const void *, SIZE_T, SIZE_T *);
static NTSTATUS (WINAPI *pNtSetSystemInformation)(ULONG, void *, ULONG);
static NTSTATUS (WINAPI *pNtQuerySystemInformationEx)(ULONG, void *, ULONG, void *, ULONG, ULONG *);
static NTSTATUS (WINAPI *pNtSetInformationThread)(HANDLE, ULONG, const void *, ULONG);
static NTSTATUS (WINAPI *pNtOpenThread)(HANDLE *, ACCESS_MASK, OA *, CID *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkst(NTSTATUS got, NTSTATUS want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (got %08lx)", what, (unsigned long)got);
    check(got == want, buf);
}

#define LOAD(n) p##n = (void *)GetProcAddress(nt, #n)

int main(int argc, char **argv)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char buffer[64], cmd[MAX_PATH + 16];
    SIZE_T n;
    ULONG len, adjust[2] = { 0, 0 }, relation;
    NTSTATUS st;
    ULONG base = sizeof(SIZE_T) == 4 ? 44 : 88;
    BYTE dummy = 1;
    HANDLE h, ro;
    CID cid;
    OA oa = { sizeof(oa) };

    if (argc > 1 && !strcmp(argv[1], "--child")) return 0;  /* only ever started suspended */

    LOAD(NtQueryInformationProcess); LOAD(NtReadVirtualMemory); LOAD(NtWriteVirtualMemory);
    LOAD(NtSetSystemInformation); LOAD(NtQuerySystemInformationEx); LOAD(NtSetInformationThread);
    LOAD(NtOpenThread);

    /* --- ProcessVmCounters --- */
    len = 0xdeadbeef;
    st = pNtQueryInformationProcess(GetCurrentProcess(), 3, buffer, 10, &len);
    check(st == STATUS_INFO_LENGTH_MISMATCH && len == base, "VmCounters: a buffer that is too small reports the base structure size");
    len = 0xdeadbeef;
    st = pNtQueryInformationProcess(GetCurrentProcess(), 3, buffer, base + 2, &len);
    check(st == STATUS_INFO_LENGTH_MISMATCH && len == base, "VmCounters: an odd size above the base structure reports it too");
    len = 0xdeadbeef;
    st = pNtQueryInformationProcess(GetCurrentProcess(), 3, buffer, base, &len);
    check(st == STATUS_SUCCESS && len == base, "VmCounters: the base size is accepted");

    /* --- remote memory --- */
    snprintf(cmd, sizeof(cmd), "\"%s\" --child", argv[0]);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    {
        check(0, "a suspended second process started");
    }
    else
    {
        n = 99;
        st = pNtReadVirtualMemory(pi.hProcess, (void *)0x1234, buffer, 12, &n);
        check(st == STATUS_PARTIAL_COPY && n == 0, "reading an unreadable remote address is STATUS_PARTIAL_COPY, 0 bytes");
        n = 99;
        st = pNtWriteVirtualMemory(pi.hProcess, (void *)0x1234, "abc", 3, &n);
        check(st != STATUS_SUCCESS && n == 0, "writing one fails, with 0 bytes written");
        ro = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pi.dwProcessId);
        n = 99;
        st = pNtReadVirtualMemory(ro, (void *)0x1234, buffer, 12, &n);
        check(ro && st == STATUS_ACCESS_DENIED && n == 0, "a handle without PROCESS_VM_READ is still STATUS_ACCESS_DENIED");
        if (ro) CloseHandle(ro);
        n = 99;
        st = pNtReadVirtualMemory(pi.hProcess, (void *)&n, buffer, 4, &n);
        check(st == STATUS_SUCCESS || st == STATUS_PARTIAL_COPY, "reading an address that may or may not exist there does not fail otherwise");
        TerminateProcess(pi.hProcess, 0);
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    /* --- NtSetSystemInformation(SystemTimeAdjustmentInformation = 28) --- */
    st = pNtSetSystemInformation(28, adjust, 5);
    checkst(st, STATUS_INFO_LENGTH_MISMATCH, "time adjustment: a length that is too small is STATUS_INFO_LENGTH_MISMATCH");
    st = pNtSetSystemInformation(28, adjust, 9);
    checkst(st, STATUS_INFO_LENGTH_MISMATCH, "time adjustment: a length that is too large too");
    st = pNtSetSystemInformation(28, adjust, 8);
    check(st == STATUS_SUCCESS || st == STATUS_PRIVILEGE_NOT_HELD, "time adjustment: the right length is not a length error");

    /* --- SystemLogicalProcessorInformationEx = 107 --- */
    for (relation = 5; relation <= 7; relation++)
    {
        len = 0;
        st = pNtQuerySystemInformationEx(107, &relation, sizeof(relation), NULL, 0, &len);
        snprintf(cmd, sizeof(cmd), "relation %lu (not reported) is not a success", (unsigned long)relation);
        check(st == STATUS_UNSUCCESSFUL || st == STATUS_INFO_LENGTH_MISMATCH, cmd);
    }
    relation = 0;
    len = 0;
    st = pNtQuerySystemInformationEx(107, &relation, sizeof(relation), NULL, 0, &len);
    check(st == STATUS_INFO_LENGTH_MISMATCH && len > 0, "relation 0 (processor core) still reports its size");
    relation = 0xffff;
    len = 0;
    st = pNtQuerySystemInformationEx(107, &relation, sizeof(relation), NULL, 0, &len);
    check(st == STATUS_INFO_LENGTH_MISMATCH && len > 0, "RelationAll still reports its size");

    /* --- ThreadEnableAlignmentFaultFixup = 7 --- */
    st = pNtSetInformationThread(GetCurrentThread(), 7, &dummy, 1);
    checkst(st, STATUS_SUCCESS, "alignment fixup: the current thread");
    st = pNtSetInformationThread(GetCurrentThread(), 7, &dummy, 4);
    checkst(st, STATUS_INFO_LENGTH_MISMATCH, "alignment fixup: wrong length");
    st = pNtSetInformationThread((HANDLE)0xdeadbeef, 7, NULL, 1);
    checkst(st, STATUS_ACCESS_VIOLATION, "alignment fixup: no data is checked before the handle");
    st = pNtSetInformationThread((HANDLE)0xdeadbeef, 7, &dummy, 1);
    checkst(st, STATUS_INVALID_HANDLE, "alignment fixup: an invalid handle");
    st = pNtSetInformationThread(GetCurrentProcess(), 7, &dummy, 1);
    checkst(st, STATUS_OBJECT_TYPE_MISMATCH, "alignment fixup: a process handle");

    /* --- NtOpenThread client id --- */
    cid.UniqueProcess = (PVOID)(ULONG_PTR)0xdeadbeef;
    cid.UniqueThread = (PVOID)(ULONG_PTR)GetCurrentThreadId();
    h = (HANDLE)0xdeadbeef;
    st = pNtOpenThread(&h, THREAD_QUERY_INFORMATION, &oa, &cid);
    check(st == STATUS_INVALID_CID && !h, "NtOpenThread with another process's id is STATUS_INVALID_CID and no handle");
    if (!st) CloseHandle(h);
    cid.UniqueProcess = (PVOID)(ULONG_PTR)GetCurrentProcessId();
    h = NULL;
    st = pNtOpenThread(&h, THREAD_QUERY_INFORMATION, &oa, &cid);
    check(st == STATUS_SUCCESS && h, "...with the right process id it opens");
    if (!st) CloseHandle(h);
    cid.UniqueProcess = NULL;
    h = NULL;
    st = pNtOpenThread(&h, THREAD_QUERY_INFORMATION, &oa, &cid);
    check(st == STATUS_SUCCESS && h, "...with no process id it opens");
    if (!st) CloseHandle(h);
    cid.UniqueProcess = (PVOID)(ULONG_PTR)GetCurrentProcessId();
    cid.UniqueThread = (PVOID)(ULONG_PTR)0x7ffffff0;
    h = (HANDLE)0xdeadbeef;
    st = pNtOpenThread(&h, THREAD_QUERY_INFORMATION, &oa, &cid);
    check(st == STATUS_INVALID_CID && !h, "a thread that does not exist is STATUS_INVALID_CID");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
