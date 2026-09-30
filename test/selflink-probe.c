/* A shell folder is never linked to itself (patches/sg/0583).
 * With HOME the user's Windows profile (a shared home), ask the mount manager
 * -- as winecfg does -- to link the profile's Documents to $HOME/Documents,
 * which is the same folder. Prints "selflink=kept" when Documents is still a
 * folder holding its file, "selflink=LOST" otherwise. */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>

#define MOUNTMGRCONTROLTYPE 0x0000006D
#define IOCTL_MOUNTMGR_DEFINE_SHELL_FOLDER CTL_CODE(MOUNTMGRCONTROLTYPE, 34, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
struct mountmgr_shell_folder { BOOL create_backup; ULONG folder_offset, folder_size, symlink_offset; };

int main(int argc, char **argv)
{
    WCHAR docs[MAX_PATH], nt[MAX_PATH + 8], file[MAX_PATH];
    const char *link = "$HOME/Documents";
    char buf[4096];
    struct mountmgr_shell_folder *in = (void *)buf;
    HANDLE mgr, h;
    DWORD attrs;

    GetEnvironmentVariableW(L"USERPROFILE", docs, MAX_PATH);
    lstrcatW(docs, L"\\Documents");
    CreateDirectoryW(docs, NULL);
    swprintf(file, MAX_PATH, L"%ls\\keep-me.txt", docs);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);

    swprintf(nt, ARRAYSIZE(nt), L"\\??\\%ls", docs);
    memset(buf, 0, sizeof(buf));
    in->create_backup = TRUE;
    in->folder_offset = sizeof(*in);
    in->folder_size = lstrlenW(nt) * sizeof(WCHAR);
    memcpy(buf + in->folder_offset, nt, in->folder_size);
    in->symlink_offset = in->folder_offset + in->folder_size;
    strcpy(buf + in->symlink_offset, link);
    mgr = CreateFileW(L"\\\\.\\MountPointManager", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, OPEN_EXISTING, 0, NULL);
    DeviceIoControl(mgr, IOCTL_MOUNTMGR_DEFINE_SHELL_FOLDER, buf, in->symlink_offset + strlen(link) + 1,
                    NULL, 0, NULL, NULL);
    CloseHandle(mgr);

    attrs = GetFileAttributesW(file);
    printf(attrs != INVALID_FILE_ATTRIBUTES ? "selflink=kept\n" : "selflink=LOST\n");
    return attrs != INVALID_FILE_ATTRIBUTES ? 0 : 1;
}
