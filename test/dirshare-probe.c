/* dirshare-probe mkdir|setsd|plaindir|plainfile PATH -- make PATH a folder with a descriptor
 * that gives the users and everyone read only (its owner all: mode 0755), or set that
 * descriptor on PATH, or make a folder or a file with no descriptor (as an
 * installer does); prints "ok" or the error. For dirshare-gate.sh. */
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <stdio.h>
#define SDDL L"D:(A;OICI;FA;;;OW)(A;OICI;0x1200a9;;;BU)(A;OICI;0x1200a9;;;WD)"
int wmain(int argc, WCHAR **argv)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa) };
    PSECURITY_DESCRIPTOR sd;
    if (argc < 3) return 2;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(SDDL, SDDL_REVISION_1, &sd, NULL)) { printf("sddl %lu\n", GetLastError()); return 1; }
    if (!lstrcmpW(argv[1], L"plaindir"))
    {
        if (!CreateDirectoryW(argv[2], NULL)) { printf("plaindir %lu\n", GetLastError()); return 1; }
    }
    else if (!lstrcmpW(argv[1], L"plainfile"))
    {
        HANDLE h = CreateFileW(argv[2], GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { printf("plainfile %lu\n", GetLastError()); return 1; }
        CloseHandle(h);
    }
    else if (!lstrcmpW(argv[1], L"mkdir"))
    {
        sa.lpSecurityDescriptor = sd;
        if (!CreateDirectoryW(argv[2], &sa)) { printf("mkdir %lu\n", GetLastError()); return 1; }
    }
    else
    {
        BOOL present, defaulted;
        PACL dacl;
        DWORD r;
        GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted);
        if ((r = SetNamedSecurityInfoW(argv[2], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, dacl, NULL))) { printf("setsd %lu\n", r); return 1; }
    }
    printf("ok\n");
    return 0;
}
