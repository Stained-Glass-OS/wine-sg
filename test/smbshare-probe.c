/* A network share as Windows programs see it (patches/sg/0822, 0823), on a
 * stand-in SMB share (test/smbshim.c):
 *   list=MS entries=N hidden=N   reading \\server\share (FindFirstFile)
 *   vol=OK|ERR<n> serial=X       GetVolumeInformation(\\server\share\)
 *   sub=ERR<n>                   GetVolumeInformation(\\server\share\folder\)
 *   drive=OK|ERR<n>              GetVolumeInformation on a handle to a folder of the share */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    WCHAR pat[MAX_PATH], root[MAX_PATH], sub[MAX_PATH], label[64];
    WIN32_FIND_DATAW fd;
    LARGE_INTEGER f, a, b;
    DWORD serial = 0, len, flags;
    int n = 0, hidden = 0;
    HANDLE h;

    swprintf(pat, MAX_PATH, L"%ls\\*", argv[1]);
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    if ((h = FindFirstFileW(pat, &fd)) != INVALID_HANDLE_VALUE)
    {
        do { n++; if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) hidden++; } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    QueryPerformanceCounter(&b);
    printf("list=%lu entries=%d hidden=%d\n", (DWORD)((b.QuadPart - a.QuadPart) * 1000 / f.QuadPart), n, hidden);

    swprintf(root, MAX_PATH, L"%ls\\", argv[1]);
    if (GetVolumeInformationW(root, label, ARRAYSIZE(label), &serial, &len, &flags, NULL, 0))
        printf("vol=OK serial=%08lx\n", serial);
    else printf("vol=ERR%lu\n", GetLastError());
    swprintf(sub, MAX_PATH, L"%ls\\%ls\\", argv[1], argv[2]);
    if (GetVolumeInformationW(sub, label, ARRAYSIZE(label), &serial, &len, &flags, NULL, 0)) printf("sub=OK\n");
    else printf("sub=ERR%lu\n", GetLastError());
    swprintf(sub, MAX_PATH, L"%ls\\%ls", argv[1], argv[2]);
    h = CreateFileW(sub, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h != INVALID_HANDLE_VALUE && GetVolumeInformationByHandleW(h, label, ARRAYSIZE(label), &serial, NULL, NULL, NULL, 0))
        printf("drive=OK\n");
    else printf("drive=ERR%lu\n", GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    return 0;
}
