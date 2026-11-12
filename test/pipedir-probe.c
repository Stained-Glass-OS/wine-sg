/* The named pipe root directory lists the pipes (wine-sg 7744): FindFirstFile("\\.\pipe\*") and
 * NtQueryDirectoryFile on \\.\pipe\ give the pipe names, with EndOfFile the current instances and
 * AllocationSize the maximum, as NPFS does. Visual Studio's ServiceHub client checks for its
 * controller's pipe with Directory.GetFiles(@"\\.\pipe\", name); Wine failed with ERROR_BAD_DEV_TYPE.
 * Prints ok/FAIL lines and RESULT: PASS|FAIL. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <wchar.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct
{
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime, LastAccessTime, LastWriteTime, ChangeTime;
    LARGE_INTEGER EndOfFile, AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    WCHAR FileName[1];
} DIRINFO;

typedef struct
{
    ULONG NextEntryOffset;
    ULONG FileIndex;
    ULONG FileNameLength;
    WCHAR FileName[1];
} NAMESINFO;

static NTSTATUS (WINAPI *pNtQueryDirectoryFile)(HANDLE, HANDLE, void *, void *, IO_STATUS_BLOCK *, void *, ULONG,
                                                 FILE_INFORMATION_CLASS, BOOLEAN, UNICODE_STRING *, BOOLEAN);

static int listed(const WCHAR *pattern, const WCHAR *name, DWORD *err)
{
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW(pattern, &fd);
    int found = 0;

    *err = f == INVALID_HANDLE_VALUE ? GetLastError() : 0;
    if (f == INVALID_HANDLE_VALUE) return -1;
    do if (!wcscmp(fd.cFileName, name)) found++;
    while (FindNextFileW(f, &fd));
    FindClose(f);
    return found;
}

int main(void)
{
    static const WCHAR name[] = L"sg-pipedir-gate";
    HANDLE p1, p2, dir;
    DWORD err;
    int n;

    pNtQueryDirectoryFile = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryDirectoryFile");
    p1 = CreateNamedPipeW(L"\\\\.\\pipe\\sg-pipedir-gate", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 4, 512, 512, 0, NULL);
    p2 = CreateNamedPipeW(L"\\\\.\\pipe\\sg-pipedir-gate", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 4, 512, 512, 0, NULL);
    CHECK(p1 != INVALID_HANDLE_VALUE && p2 != INVALID_HANDLE_VALUE, "two instances of a pipe made");

    n = listed(L"\\\\.\\pipe\\*", name, &err);
    CHECK(n == 1, "\\\\.\\pipe\\* lists the pipe once (found %d, error %lu)", n, err);
    n = listed(L"\\\\.\\pipe\\sg-pipedir-gate", name, &err);
    CHECK(n == 1, "the pipe's own name finds it (found %d, error %lu)", n, err);
    n = listed(L"\\\\.\\pipe\\SG-PIPEDIR-*", name, &err);
    CHECK(n == 1, "a wildcard matches without regard to case (found %d, error %lu)", n, err);
    n = listed(L"\\\\.\\pipe\\sg-no-such-pipe-*", name, &err);
    CHECK(n == -1 && err == ERROR_FILE_NOT_FOUND, "no match gives ERROR_FILE_NOT_FOUND (%d, error %lu)", n, err);

    dir = CreateFileW(L"\\\\.\\pipe\\", FILE_LIST_DIRECTORY | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(dir != INVALID_HANDLE_VALUE, "the pipe root directory opens (error %lu)", GetLastError());
    if (dir != INVALID_HANDLE_VALUE)
    {
        union { DIRINFO d; char raw[4096]; } buf;
        UNICODE_STRING mask;
        IO_STATUS_BLOCK io;
        NTSTATUS status;

        RtlInitUnicodeString(&mask, name);
        status = pNtQueryDirectoryFile(dir, NULL, NULL, NULL, &io, &buf, sizeof(buf), 1 /* FileDirectoryInformation */,
                                       TRUE, &mask, TRUE);
        CHECK(status == 0 && buf.d.FileNameLength == wcslen(name) * sizeof(WCHAR) &&
              !memcmp(buf.d.FileName, name, buf.d.FileNameLength),
              "NtQueryDirectoryFile gives the pipe (status %#lx)", status);
        CHECK(status == 0 && buf.d.EndOfFile.QuadPart == 2 && buf.d.AllocationSize.QuadPart == 4,
              "EndOfFile is the 2 instances, AllocationSize the maximum 4 (%lld, %lld)",
              buf.d.EndOfFile.QuadPart, buf.d.AllocationSize.QuadPart);
        status = pNtQueryDirectoryFile(dir, NULL, NULL, NULL, &io, &buf, sizeof(buf), 1, TRUE, NULL, FALSE);
        CHECK(status == 0x80000006 /* STATUS_NO_MORE_FILES */, "the next query keeps the first mask: no more (status %#lx)", status);
        status = pNtQueryDirectoryFile(dir, NULL, NULL, NULL, &io, &buf, sizeof(buf), 12 /* FileNamesInformation */,
                                       FALSE, NULL, TRUE);
        CHECK(status == 0 && ((NAMESINFO *)&buf)->FileNameLength == wcslen(name) * sizeof(WCHAR),
              "a restarted scan in FileNamesInformation gives the pipe again (status %#lx)", status);
        CloseHandle(dir);
    }

    CloseHandle(p1);
    CloseHandle(p2);
    n = listed(L"\\\\.\\pipe\\*", name, &err);
    CHECK(n == 0, "a closed pipe is no longer listed (found %d, error %lu)", n, err);
    n = listed(L"C:\\windows\\*", L"system32", &err);
    CHECK(n == 1, "a disk directory still lists (found %d, error %lu)", n, err);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
