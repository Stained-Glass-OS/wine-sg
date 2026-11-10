/* wowdrivers gate probe (wine-sg 7727): a 32-bit program's System32\drivers
 * (SysWOW64\drivers) exists and can be written. Malwarebytes' setup checks
 * "cannot write to directory: C:\windows\system32\drivers" and refused to
 * install (exit 204). Built 32-bit. */
#include <windows.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what, DWORD err)
{
    printf("%s %s (%lu)\n", ok ? "ok  " : "FAIL", what, err);
    if (!ok) failures++;
}

int main(void)
{
    char sys[MAX_PATH], dir[MAX_PATH], file[MAX_PATH], wow[MAX_PATH];
    DWORD attr;
    HANDLE h;

    GetSystemDirectoryA(sys, MAX_PATH);      /* C:\windows\system32 (redirected) */
    GetSystemWow64DirectoryA(wow, MAX_PATH); /* C:\windows\syswow64 */
    sprintf(dir, "%s\\drivers", sys);
    attr = GetFileAttributesA(dir);
    check(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY), "System32\\drivers is a directory for a 32-bit program", GetLastError());

    sprintf(file, "%s\\sgwowdrv.tmp", dir);
    h = CreateFileA(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    check(h != INVALID_HANDLE_VALUE, "a file can be created there", GetLastError());
    if (h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
        sprintf(file, "%s\\drivers\\sgwowdrv.tmp", wow);
        check(GetFileAttributesA(file) != INVALID_FILE_ATTRIBUTES, "it lands in SysWOW64\\drivers", GetLastError());
        DeleteFileA(file);
    }

    sprintf(dir, "%s\\drivers\\etc\\hosts", sys);
    check(GetFileAttributesA(dir) != INVALID_FILE_ATTRIBUTES, "drivers\\etc stays shared (hosts is found)", GetLastError());

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
