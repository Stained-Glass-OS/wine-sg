/* File information classes Wine 10 did not have (patches/sg/1601).
 *
 *  - SetFileInformationByHandle(FileAllocationInfo) reserves space (the end
 *    of file stays) and, below the end of file, truncates there; it failed
 *    with ERROR_CALL_NOT_IMPLEMENTED;
 *  - FileRenameInfoEx renames with flags (FILE_RENAME_FLAG_REPLACE_IF_EXISTS);
 *  - the query-only classes are refused with ERROR_INVALID_PARAMETER;
 *  - GetFileInformationByHandleEx answers FileStorageInfo,
 *    FileCaseSensitiveInfo and FileNormalizedNameInfo.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef FILE_RENAME_FLAG_REPLACE_IF_EXISTS
#define FILE_RENAME_FLAG_REPLACE_IF_EXISTS 0x1
#endif

/* not in every mingw-w64 */
typedef struct
{
    ULONG LogicalBytesPerSector;
    ULONG PhysicalBytesPerSectorForAtomicity;
    ULONG PhysicalBytesPerSectorForPerformance;
    ULONG FileSystemEffectivePhysicalBytesPerSectorForAtomicity;
    ULONG Flags;
    ULONG ByteOffsetForSectorAlignment;
    ULONG ByteOffsetForPartitionAlignment;
} storage_info;

enum { class_alloc = 5, class_storage = 16, class_rename_ex = 22, class_case = 23, class_normalized = 24,
       class_stream = 7, class_name = 2 };

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], path2[MAX_PATH], buf[1024];
    FILE_ALLOCATION_INFO alloc;
    FILE_STANDARD_INFO std;
    storage_info storage;
    ULONG cs;
    BYTE renbuf[sizeof(FILE_RENAME_INFO) + MAX_PATH * sizeof(WCHAR)];
    FILE_RENAME_INFO *ren = (FILE_RENAME_INFO *)renbuf;
    FILE_NAME_INFO *name = (FILE_NAME_INFO *)buf;
    HANDLE file, f2;
    DWORD written;
    BOOL ok;
    char data[8192];

    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%lsfileinfo-probe-a.bin", dir);
    swprintf(path2, MAX_PATH, L"%lsfileinfo-probe-b.bin", dir);
    DeleteFileW(path);
    DeleteFileW(path2);

    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) { printf("FAIL  create (%lu)\nRESULT: FAIL\n", GetLastError()); return 1; }
    memset(data, 'x', sizeof(data));
    WriteFile(file, data, sizeof(data), &written, NULL);

    /* reserve 1 MB: end of file stays at 8192 */
    alloc.AllocationSize.QuadPart = 1 << 20;
    SetLastError(0xdeadbeef);
    ok = SetFileInformationByHandle(file, class_alloc, &alloc, sizeof(alloc));
    printf("FileAllocationInfo 1 MB: ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(ok, "FileAllocationInfo above the end of file succeeds");
    ok = GetFileInformationByHandleEx(file, FileStandardInfo, &std, sizeof(std));
    printf("end of file %lld, allocation %lld\n", std.EndOfFile.QuadPart, std.AllocationSize.QuadPart);
    check(ok && std.EndOfFile.QuadPart == 8192, "... and leaves the end of file");
    check(ok && std.AllocationSize.QuadPart >= (1 << 20), "... and reserves the space");

    /* below the end of file: truncates */
    alloc.AllocationSize.QuadPart = 1000;
    ok = SetFileInformationByHandle(file, class_alloc, &alloc, sizeof(alloc));
    GetFileInformationByHandleEx(file, FileStandardInfo, &std, sizeof(std));
    printf("allocation 1000: ok %d, end of file %lld\n", ok, std.EndOfFile.QuadPart);
    check(ok && std.EndOfFile.QuadPart == 1000, "FileAllocationInfo below the end of file truncates there");

    /* query-only classes are refused as Windows refuses them */
    SetLastError(0xdeadbeef);
    ok = SetFileInformationByHandle(file, class_name, buf, sizeof(buf));
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "FileNameInfo cannot be set (ERROR_INVALID_PARAMETER)");
    SetLastError(0xdeadbeef);
    ok = SetFileInformationByHandle(file, class_stream, buf, sizeof(buf));
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "FileStreamInfo cannot be set (ERROR_INVALID_PARAMETER)");

    /* queries */
    memset(&storage, 0xcc, sizeof(storage));
    SetLastError(0xdeadbeef);
    ok = GetFileInformationByHandleEx(file, class_storage, &storage, sizeof(storage));
    printf("FileStorageInfo: ok %d err %lu logical %lu physical %lu flags %#lx\n", ok, ok ? 0 : GetLastError(),
           storage.LogicalBytesPerSector, storage.PhysicalBytesPerSectorForPerformance, storage.Flags);
    check(ok && storage.LogicalBytesPerSector == 512 &&
          (storage.PhysicalBytesPerSectorForPerformance == 512 || storage.PhysicalBytesPerSectorForPerformance == 4096),
          "FileStorageInfo gives the sector sizes");
    cs = 0xdeadbeef;
    ok = GetFileInformationByHandleEx(file, class_case, &cs, sizeof(cs));
    check(ok && cs == 0, "FileCaseSensitiveInfo: not case sensitive");
    memset(buf, 0, sizeof(buf));
    ok = GetFileInformationByHandleEx(file, class_normalized, buf, sizeof(buf));
    printf("FileNormalizedNameInfo: ok %d name %.*ls\n", ok, ok ? (int)(name->FileNameLength / 2) : 0, name->FileName);
    check(ok && name->FileNameLength && wcsstr(name->FileName, L"fileinfo-probe-a.bin"), "FileNormalizedNameInfo gives the file's name");
    SetLastError(0xdeadbeef);
    ok = GetFileInformationByHandleEx(file, class_rename_ex, buf, sizeof(buf));
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "FileRenameInfoEx cannot be queried");

    /* rename with FILE_RENAME_FLAG_REPLACE_IF_EXISTS over an existing file */
    f2 = CreateFileW(path2, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(f2);
    memset(renbuf, 0, sizeof(renbuf));
    ren->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS;
    ren->FileNameLength = wcslen(path2) * sizeof(WCHAR);
    wcscpy(ren->FileName, path2);
    SetLastError(0xdeadbeef);
    ok = SetFileInformationByHandle(file, class_rename_ex, ren, sizeof(renbuf));
    printf("FileRenameInfoEx: ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(ok && GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES && GetFileAttributesW(path2) != INVALID_FILE_ATTRIBUTES,
          "FileRenameInfoEx renames over an existing file");
    CloseHandle(file);
    DeleteFileW(path);
    DeleteFileW(path2);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
