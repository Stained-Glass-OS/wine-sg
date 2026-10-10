/* NtSetInformationFile with a buffer too short for the class is
 * STATUS_INFO_LENGTH_MISMATCH, and an FSCTL an ordinary file does not take is
 * STATUS_INVALID_DEVICE_REQUEST (patches/sg/2218).  The file's own locking waits
 * (LockFileEx blocking) must keep working: they travel as an ioctl too. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG NTSTATUS;
typedef struct { union { NTSTATUS Status; PVOID Pointer; }; ULONG_PTR Information; } IOSB;

static NTSTATUS (WINAPI *pNtSetInformationFile)(HANDLE, IOSB *, void *, ULONG, ULONG);
static NTSTATUS (WINAPI *pNtFsControlFile)(HANDLE, HANDLE, void *, void *, IOSB *, ULONG, void *, ULONG, void *, ULONG);

#define STATUS_INFO_LENGTH_MISMATCH_ ((NTSTATUS)0xC0000004)
#define STATUS_INVALID_DEVICE_REQUEST_ ((NTSTATUS)0xC0000010)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static DWORD WINAPI unlocker(void *arg)
{
    HANDLE h = arg;
    OVERLAPPED ov = { 0 };

    Sleep(150);
    UnlockFileEx(h, 0, 1, 0, &ov);
    return 0;
}

int main(void)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    HANDLE f, f2, th;
    IOSB io;
    NTSTATUS st;
    char buf[64];
    struct { const char *name; ULONG cls; } classes[] =
    {
        { "FileBasicInformation", 4 }, { "FileRenameInformation", 10 }, { "FileLinkInformation", 11 },
        { "FileDispositionInformation", 13 }, { "FilePositionInformation", 14 }, { "FileAllocationInformation", 19 },
        { "FileEndOfFileInformation", 20 },
    };
    OVERLAPPED ov = { 0 };
    DWORD t0;
    unsigned i;

    pNtSetInformationFile = (void *)GetProcAddress(nt, "NtSetInformationFile");
    pNtFsControlFile = (void *)GetProcAddress(nt, "NtFsControlFile");
    DeleteFileA("sgfioctl.tmp");
    f = CreateFileA("sgfioctl.tmp", GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { printf("FAIL  create\n"); return 1; }

    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++)
    {
        char what[100];
        io.Status = 0xdeadbeef;
        st = pNtSetInformationFile(f, &io, buf, 0, classes[i].cls);
        snprintf(what, sizeof(what), "%s with a 0 byte buffer is STATUS_INFO_LENGTH_MISMATCH (got %08lx)", classes[i].name, (unsigned long)st);
        check(st == STATUS_INFO_LENGTH_MISMATCH_, what);
    }

    /* unknown / foreign FSCTLs on a file */
    memset(&io, 0x55, sizeof(io));
    st = pNtFsControlFile(f, NULL, NULL, NULL, &io, 0xdeadbeef, NULL, 0, NULL, 0);
    check(st == STATUS_INVALID_DEVICE_REQUEST_, "an unknown FSCTL on a file is STATUS_INVALID_DEVICE_REQUEST");
    st = pNtFsControlFile(f, NULL, NULL, NULL, &io, 0x11400c, NULL, 0, buf, sizeof(buf));
    check(st == STATUS_INVALID_DEVICE_REQUEST_, "a pipe FSCTL on a file is STATUS_INVALID_DEVICE_REQUEST");
    check(io.Status == 0x55555555, "...and the status block is left alone");

    /* locking still waits and wakes (the lock wait is an ioctl to the server) */
    f2 = CreateFileA("sgfioctl.tmp", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    check(f2 != INVALID_HANDLE_VALUE, "a second handle");
    check(LockFileEx(f, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov), "an exclusive lock");
    SetLastError(0);
    check(!LockFileEx(f2, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov) && GetLastError() == ERROR_LOCK_VIOLATION,
          "a second handle's lock without waiting is ERROR_LOCK_VIOLATION");
    th = CreateThread(NULL, 0, unlocker, f, 0, NULL);
    t0 = GetTickCount();
    check(LockFileEx(f2, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov), "...and with waiting it gets the lock once the first is released");
    check(GetTickCount() - t0 >= 100, "...after a real wait");
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    UnlockFileEx(f2, 0, 1, 0, &ov);

    CloseHandle(f2);
    CloseHandle(f);
    DeleteFileA("sgfioctl.tmp");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
