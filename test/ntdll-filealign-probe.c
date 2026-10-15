/* Probe for patches/sg/5006: FileAlignmentInformation (NtQueryInformationFile and
 * GetFileInformationByHandleEx(FileAlignmentInfo)) reports FILE_BYTE_ALIGNMENT for
 * a file and a directory on disk. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define FileAlignmentInformation_ 17

int main(int argc, char **argv)
{
    NTSTATUS (WINAPI *query)(HANDLE, IO_STATUS_BLOCK *, void *, ULONG, int) =
            (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationFile");
    char path[MAX_PATH];
    const char *names[2];
    unsigned int i;

    GetTempPathA(MAX_PATH, path);
    names[1] = path;
    strcat(path, "");
    {
        static char file[MAX_PATH];
        GetTempFileNameA(path, "aln", 0, file);
        names[0] = file;
    }
    for (i = 0; i < 2; i++)
    {
        HANDLE h = CreateFileA(names[i], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                               OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        IO_STATUS_BLOCK io;
        ULONG align = 0xdead;
        FILE_ALIGNMENT_INFO info = {0xdead};
        NTSTATUS status = query(h, &io, &align, sizeof(align), FileAlignmentInformation_);
        check(!status && align == 0 && io.Information == sizeof(align), "%s: NtQueryInformationFile %#lx %lu",
              i ? "directory" : "file", status, align);
        check(GetFileInformationByHandleEx(h, FileAlignmentInfo, &info, sizeof(info)) && info.AlignmentRequirement == 0,
              "%s: GetFileInformationByHandleEx(FileAlignmentInfo) %lu", i ? "directory" : "file", info.AlignmentRequirement);
        CloseHandle(h);
    }
    DeleteFileA(names[0]);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
