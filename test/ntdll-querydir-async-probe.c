/* Probe for patches/sg/5068: NtQueryDirectoryFile with an event, an APC routine or a
 * completion port (it returned STATUS_NOT_IMPLEMENTED for an event or an APC).
 * - the listing is the same as without them;
 * - the event is set, the APC runs once with the IO_STATUS_BLOCK, the completion port
 *   gets one packet carrying the ApcContext and the byte count;
 * - the end of the listing (STATUS_NO_MORE_FILES, a warning) completes the same way;
 * - a request that fails (an unknown class) sets nothing and queues nothing.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_INFO_CLASS ((NTSTATUS)0xc0000003)
#ifndef STATUS_NO_MORE_FILES
#define STATUS_NO_MORE_FILES ((NTSTATUS)0x80000006)
#endif
#define FileDirectoryInformation_ 1
#define FileNamesInformation_ 12

typedef NTSTATUS (WINAPI *qdf_t)(HANDLE, HANDLE, PIO_APC_ROUTINE, void *, IO_STATUS_BLOCK *, void *, ULONG,
                                 int, BOOLEAN, UNICODE_STRING *, BOOLEAN);
static qdf_t pNtQueryDirectoryFile;

static int apc_calls;
static IO_STATUS_BLOCK *apc_io;
static void *apc_ctx;
static void WINAPI apc(void *ctx, IO_STATUS_BLOCK *io, ULONG reserved)
{
    apc_calls++;
    apc_io = io;
    apc_ctx = ctx;
}

static HANDLE open_dir(const WCHAR *path, BOOL overlapped)
{
    return CreateFileW(path, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | (overlapped ? FILE_FLAG_OVERLAPPED : 0), NULL);
}

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    BYTE buf[4096], buf2[4096];
    IO_STATUS_BLOCK io, io2;
    HANDLE h, h2, event, port;
    NTSTATUS st, st2;
    unsigned int i;
    DWORD bytes;
    ULONG_PTR key;
    OVERLAPPED *ov;
    BOOL ret;

    pNtQueryDirectoryFile = (qdf_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryDirectoryFile");
    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"sgqdir");
    CreateDirectoryW(dir, NULL);
    for (i = 0; i < 3; i++)
    {
        wsprintfW(path, L"%s\\f%u.txt", dir, i);
        CloseHandle(CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    }

    /* an event, on an asynchronous handle, against the same listing without one */
    h = open_dir(dir, TRUE);
    h2 = open_dir(dir, FALSE);
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    memset(&io, 0xcc, sizeof(io));
    st = pNtQueryDirectoryFile(h, event, NULL, NULL, &io, buf, sizeof(buf), FileNamesInformation_, FALSE, NULL, TRUE);
    st2 = pNtQueryDirectoryFile(h2, NULL, NULL, NULL, &io2, buf2, sizeof(buf2), FileNamesInformation_, FALSE, NULL, TRUE);
    check(st == STATUS_SUCCESS && io.Status == STATUS_SUCCESS && io.Information == io2.Information &&
          !memcmp(buf, buf2, io.Information), "with an event: %#lx, the same %lu bytes", st, (ULONG)io.Information);
    check(WaitForSingleObject(event, 0) == WAIT_OBJECT_0, "the event is set");
    ResetEvent(event);
    st = pNtQueryDirectoryFile(h, event, NULL, NULL, &io, buf, sizeof(buf), FileNamesInformation_, FALSE, NULL, FALSE);
    check(st == STATUS_NO_MORE_FILES && io.Status == STATUS_NO_MORE_FILES && WaitForSingleObject(event, 0) == WAIT_OBJECT_0,
          "the end of the listing with an event: %#lx", st);
    ResetEvent(event);
    st = pNtQueryDirectoryFile(h, event, NULL, NULL, &io, buf, sizeof(buf), 99, FALSE, NULL, TRUE);
    check(st == STATUS_INVALID_INFO_CLASS && WaitForSingleObject(event, 0) == WAIT_TIMEOUT, "a failed request: %#lx, no event", st);

    /* an APC */
    apc_calls = 0;
    st = pNtQueryDirectoryFile(h, NULL, apc, (void *)0x1234, &io, buf, sizeof(buf), FileDirectoryInformation_, FALSE, NULL, TRUE);
    check(st == STATUS_SUCCESS && io.Information > 0, "with an APC: %#lx", st);
    check(apc_calls == 0, "the APC waits for an alertable wait");
    SleepEx(0, TRUE);
    check(apc_calls == 1 && apc_io == &io && apc_ctx == (void *)0x1234, "the APC ran once: %d", apc_calls);
    apc_calls = 0;
    st = pNtQueryDirectoryFile(h, NULL, apc, NULL, &io, buf, sizeof(buf), 99, FALSE, NULL, TRUE);
    SleepEx(0, TRUE);
    check(st == STATUS_INVALID_INFO_CLASS && apc_calls == 0, "a failed request queues no APC: %#lx", st);
    CloseHandle(h);

    /* a completion port */
    h = open_dir(dir, TRUE);
    port = CreateIoCompletionPort(h, NULL, 0x77, 0);
    check(port != NULL, "completion port");
    st = pNtQueryDirectoryFile(h, NULL, NULL, (void *)0x4321, &io, buf, sizeof(buf), FileNamesInformation_, FALSE, NULL, TRUE);
    bytes = 0; key = 0; ov = NULL;
    ret = GetQueuedCompletionStatus(port, &bytes, &key, &ov, 1000);
    check(st == STATUS_SUCCESS && ret && key == 0x77 && ov == (OVERLAPPED *)0x4321 && bytes == io.Information,
          "one packet: %d key %#Ix context %p bytes %lu", ret, key, ov, bytes);
    ret = GetQueuedCompletionStatus(port, &bytes, &key, &ov, 0);
    check(!ret, "and only one");
    CloseHandle(h);
    CloseHandle(port);
    CloseHandle(h2);
    CloseHandle(event);

    for (i = 0; i < 3; i++) { wsprintfW(path, L"%s\\f%u.txt", dir, i); DeleteFileW(path); }
    RemoveDirectoryW(dir);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
