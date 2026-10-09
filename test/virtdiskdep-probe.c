/* virtdiskdep-probe: GetStorageDependencyInformation on a file that is not
 * on a virtual disk (patches/sg/1528). Windows answers
 * ERROR_VIRTDISK_NOT_VIRTUAL_DISK; Office opening a workbook read a host
 * volume name from the empty buffer after a success with no entries.
 * Prints "file=<err> vol=<err> zero=<err> null=<err>" in hex */
#include <windows.h>
#include <stdio.h>
#include <virtdisk.h>

int main(void)
{
    WCHAR path[MAX_PATH], dir[MAX_PATH];
    ULONG buf[256], used = 0;
    STORAGE_DEPENDENCY_INFO *info = (STORAGE_DEPENDENCY_INFO *)buf;
    DWORD file, vol, zero, nul;
    HANDLE h;

    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"vdp", 0, path);
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    memset(buf, 0xcc, sizeof(buf));
    info->Version = STORAGE_DEPENDENCY_INFO_VERSION_2;
    file = GetStorageDependencyInformation(h, GET_STORAGE_DEPENDENCY_FLAG_HOST_VOLUMES, sizeof(buf), info, &used);
    zero = GetStorageDependencyInformation(h, GET_STORAGE_DEPENDENCY_FLAG_HOST_VOLUMES, 0, info, &used);
    nul = GetStorageDependencyInformation(h, GET_STORAGE_DEPENDENCY_FLAG_HOST_VOLUMES, sizeof(buf), NULL, &used);
    CloseHandle(h);
    DeleteFileW(path);

    h = CreateFileW(L"\\\\.\\C:", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    info->Version = STORAGE_DEPENDENCY_INFO_VERSION_2;
    vol = GetStorageDependencyInformation(h, GET_STORAGE_DEPENDENCY_FLAG_HOST_VOLUMES | GET_STORAGE_DEPENDENCY_FLAG_DISK_HANDLE, sizeof(buf), info, &used);
    CloseHandle(h);
    printf("file=%lx vol=%lx zero=%lx null=%lx\n", file, vol, zero, nul);
    return 0;
}
