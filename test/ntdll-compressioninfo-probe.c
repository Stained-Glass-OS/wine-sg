/* Probe for patches/sg/5025: FileCompressionInformation
 * (GetFileInformationByHandleEx FileCompressionInfo) answers for files and
 * directories: not compressed (COMPRESSION_FORMAT_NONE), compressed size =
 * the size GetCompressedFileSize gives; a short buffer fails.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const DWORD sizes[] = {0, 1, 1234, 70000};
    NTSTATUS (WINAPI *pNtQueryInformationFile)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int) =
        (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationFile");
    char dir[MAX_PATH], path[MAX_PATH];
    static BYTE data[70000];
    FILE_COMPRESSION_INFO info;
    IO_STATUS_BLOCK io;
    unsigned int i;
    HANDLE h;
    BOOL ret;

    GetTempPathA(MAX_PATH, dir);
    sprintf(path, "%ssg-compression.bin", dir);
    for (i = 0; i < ARRAYSIZE(sizes); i++)
    {
        DWORD written, high = 0, csize;

        h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(h, data, sizes[i], &written, NULL);
        memset(&info, 0x55, sizeof(info));
        ret = GetFileInformationByHandleEx(h, FileCompressionInfo, &info, sizeof(info));
        check(ret && info.CompressedFileSize.QuadPart == sizes[i] && info.CompressionFormat == COMPRESSION_FORMAT_NONE,
              "file of %lu bytes: %d size %lu format %u", sizes[i], ret, info.CompressedFileSize.LowPart, info.CompressionFormat);
        CloseHandle(h);
        csize = GetCompressedFileSizeA(path, &high);
        check(csize == info.CompressedFileSize.LowPart && !high, "file of %lu bytes: GetCompressedFileSize agrees (%lu)", sizes[i], csize);
    }

    h = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    memset(&io, 0, sizeof(io));
    check(pNtQueryInformationFile(h, &io, &info, sizeof(info) - 1, 28) == 0xc0000004 /* STATUS_INFO_LENGTH_MISMATCH */,
          "a short buffer fails");
    CloseHandle(h);
    DeleteFileA(path);

    h = CreateFileA(dir, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, NULL);
    memset(&info, 0x55, sizeof(info));
    ret = GetFileInformationByHandleEx(h, FileCompressionInfo, &info, sizeof(info));
    check(ret && info.CompressedFileSize.QuadPart == 0 && info.CompressionFormat == COMPRESSION_FORMAT_NONE,
          "a directory: %d size %lu format %u", ret, info.CompressedFileSize.LowPart, info.CompressionFormat);
    CloseHandle(h);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
