/* reparseinfo-probe: a symlink made with FSCTL_SET_REPARSE_POINT, opened with
 * and without FILE_FLAG_OPEN_REPARSE_POINT; prints the attributes the
 * handle's FileBasicInfo and GetFileInformationByHandle report.
 * (patches/sg/0471) */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>

typedef struct { ULONG Tag; USHORT Len, Res, SubOff, SubLen, PrOff, PrLen; ULONG Flags; WCHAR Path[1]; } SYMREP;

static void report(const char *what, DWORD flags)
{
    CREATEFILE2_EXTENDED_PARAMETERS p;
    FILE_BASIC_INFO bi;
    BY_HANDLE_FILE_INFORMATION hi;
    HANDLE h;

    memset(&p, 0, sizeof(p));
    p.dwSize = sizeof(p);
    p.dwFileFlags = flags;
    h = CreateFile2(L"C:\\rp\\link.dll", FILE_READ_ATTRIBUTES, 7, OPEN_EXISTING, &p);
    memset(&bi, 0, sizeof(bi));
    memset(&hi, 0, sizeof(hi));
    GetFileInformationByHandleEx(h, FileBasicInfo, &bi, sizeof(bi));
    GetFileInformationByHandle(h, &hi);
    printf("%s basic %08lx handle %08lx\n", what, bi.FileAttributes, hi.dwFileAttributes);
    CloseHandle(h);
}

int main(void)
{
    static const WCHAR target[] = L"\\??\\C:\\rp\\target.dll";
    BYTE buf[2048];
    SYMREP *r = (SYMREP *)buf;
    DWORD n, tl = lstrlenW(target) * 2;
    HANDLE h;

    CreateDirectoryW(L"C:\\rp", NULL);
    h = CreateFileW(L"C:\\rp\\target.dll", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
    WriteFile(h, "MZ", 2, &n, NULL);
    CloseHandle(h);
    DeleteFileW(L"C:\\rp\\link.dll");
    h = CreateFileW(L"C:\\rp\\link.dll", GENERIC_WRITE, 0, NULL, CREATE_NEW,
                    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    memset(buf, 0, sizeof(buf));
    r->Tag = IO_REPARSE_TAG_SYMLINK;
    r->SubLen = tl; r->PrOff = tl; r->PrLen = tl;
    memcpy(r->Path, target, tl);
    memcpy((BYTE *)r->Path + tl, target, tl);
    r->Len = 12 + 2 * tl;
    printf("link %d\n", DeviceIoControl(h, FSCTL_SET_REPARSE_POINT, buf, 8 + r->Len, NULL, 0, &n, NULL));
    CloseHandle(h);
    report("link", FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS);
    report("target", 0);
    return 0;
}
