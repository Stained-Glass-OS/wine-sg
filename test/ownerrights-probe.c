/* OWNER RIGHTS (wine-sg 0622): a directory made as CPython 3.12.4's
 * os.mkdir(path, 0o700) makes it -- SYSTEM, the administrators and its owner
 * (S-1-3-4) -- is its maker's to use. */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, FALSE };
    const WCHAR *sddl = argc > 2 ? argv[2] : L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;OW)";
    WCHAR file[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;

    if (argc < 2) return 2;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL))
        return 3;
    printf("mkdir=%d\n", CreateDirectoryW(argv[1], &sa));
    swprintf(file, MAX_PATH, L"%ls\\settings.ini", argv[1]);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    printf("create=%s error=%lu\n", h != INVALID_HANDLE_VALUE ? "ok" : "failed", h != INVALID_HANDLE_VALUE ? 0 : GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    swprintf(file, MAX_PATH, L"%ls\\*", argv[1]);
    h = FindFirstFileW(file, &fd);
    printf("list=%s\n", h != INVALID_HANDLE_VALUE ? "ok" : "failed");
    if (h != INVALID_HANDLE_VALUE) FindClose(h);
    h = CreateFileW(argv[1], READ_CONTROL | WRITE_DAC, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    printf("dac=%s\n", h != INVALID_HANDLE_VALUE ? "ok" : "failed");
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    return 0;
}
