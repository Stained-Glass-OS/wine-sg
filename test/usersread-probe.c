/* A folder whose descriptor gives the users read and execute stays every user's to open and run
 * from (6114). CCleaner 7's installer, elevated, gives its folders SYSTEM, Administrators and
 * Users (read, execute); the Users ACE became the group's bits only, the group was the elevated
 * account's own, and other::--- shut every user out: CCleaner did not start and its uninstaller
 * failed with error 5.
 *   probe make DIR    (SYSTEM) makes DIR and DIR\app.txt with that descriptor
 *   probe use DIR     (a user) lists DIR and reads DIR\app.txt */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    if (argc < 3) return 2;
    sprintf(path, "%s\\app.txt", argv[2]);
    if (!strcmp(argv[1], "make"))
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa) };
        PSECURITY_DESCRIPTOR sd;
        HANDLE h;
        DWORD n;
        ConvertStringSecurityDescriptorToSecurityDescriptorA(
            "D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;0x1200a9;;;BU)", SDDL_REVISION_1, &sd, NULL);
        sa.lpSecurityDescriptor = sd;
        printf("MKDIR %lu\n", CreateDirectoryA(argv[2], &sa) ? 0 : GetLastError());
        h = CreateFileA(path, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, 0, NULL);
        printf("FILE %lu\n", h != INVALID_HANDLE_VALUE ? 0 : GetLastError());
        if (h != INVALID_HANDLE_VALUE) { WriteFile(h, "data", 4, &n, NULL); CloseHandle(h); }
        /* and set again on the folder, as SetSecurityInfo after unpacking does */
        printf("SETSEC %lu\n", SetFileSecurityA(argv[2], DACL_SECURITY_INFORMATION, sd) ? 0 : GetLastError());
        return 0;
    }
    {
        WIN32_FIND_DATAA fd;
        char pat[MAX_PATH], buf[8] = {0};
        HANDLE h;
        DWORD n = 0;
        int found = 0;
        sprintf(pat, "%s\\*", argv[2]);
        if ((h = FindFirstFileA(pat, &fd)) != INVALID_HANDLE_VALUE)
        {
            do if (!strcmp(fd.cFileName, "app.txt")) found = 1; while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        printf("LIST %d\n", found);
        h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, 4, &n, NULL); CloseHandle(h); }
        printf("READ %s\n", buf[0] ? buf : "none");
        h = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        printf("WRITE %lu\n", h != INVALID_HANDLE_VALUE ? 0 : GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    return 0;
}
