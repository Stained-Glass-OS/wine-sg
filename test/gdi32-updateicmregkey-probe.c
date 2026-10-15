/* Probe for patches/sg/5223: UpdateICMRegKey installs (ICM_ADDPROFILE), queries (ICM_QUERYPROFILE) and
 * uninstalls (ICM_DELETEPROFILE) a colour profile through the colour profile store, validates its
 * arguments and refuses the commands it does not implement.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <icm.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define ADD 1
#define DEL 2
#define QUERY 3

int main(void)
{
    char srgb[MAX_PATH], tmp[MAX_PATH], dir[MAX_PATH], installed[MAX_PATH * 2];
    WCHAR srgbW[MAX_PATH], dirW[MAX_PATH];
    DWORD size = sizeof(srgb);
    HANDLE f;
    BOOL ret;
    char *base;
    BYTE *data;
    DWORD got, len;

    setvbuf(stdout, NULL, _IONBF, 0);
    size = sizeof(srgbW);
    if (!GetStandardColorSpaceProfileW(NULL, 0x73524742, srgbW, &size))
    { printf("ok   no standard profile (%lu)\nRESULT: PASS\n", GetLastError()); return 0; }
    WideCharToMultiByte(CP_ACP, 0, srgbW, -1, srgb, sizeof(srgb), NULL, NULL);
    size = sizeof(dirW);
    GetColorDirectoryW(NULL, dirW, &size);
    WideCharToMultiByte(CP_ACP, 0, dirW, -1, dir, sizeof(dir), NULL, NULL);

    /* a copy of the profile under another name, outside the colour directory */
    GetTempPathA(sizeof(tmp), tmp);
    strcat(tmp, "sgprobe-profile.icm");
    f = CreateFileA(srgb, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    len = GetFileSize(f, NULL);
    data = malloc(len);
    ReadFile(f, data, len, &got, NULL);
    CloseHandle(f);
    f = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, data, len, &got, NULL);
    CloseHandle(f);
    base = strrchr(tmp, '\\') + 1;
    snprintf(installed, sizeof(installed), "%s\\%s", dir, base);
    DeleteFileA(installed);

    SetLastError(0xdeadbeef);
    ret = UpdateICMRegKeyA(0, NULL, tmp, QUERY);
    check(!ret, "not installed yet: query %d", ret);

    ret = UpdateICMRegKeyA(0, NULL, tmp, ADD);
    check(ret, "ICM_ADDPROFILE (%lu)", GetLastError());
    check(GetFileAttributesA(installed) != INVALID_FILE_ATTRIBUTES, "the profile is in the colour directory");
    ret = UpdateICMRegKeyA(0, NULL, tmp, QUERY);
    check(ret, "installed: query %d", ret);
    ret = UpdateICMRegKeyA(0, NULL, installed, QUERY);
    check(ret, "installed: query by its installed path %d", ret);
    {
        WCHAR tmpW[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, tmp, -1, tmpW, MAX_PATH);
        ret = UpdateICMRegKeyW(0, NULL, tmpW, QUERY);
        check(ret, "the wide version agrees %d", ret);
    }

    ret = UpdateICMRegKeyA(0, NULL, tmp, DEL);
    check(ret, "ICM_DELETEPROFILE (%lu)", GetLastError());
    check(GetFileAttributesA(installed) == INVALID_FILE_ATTRIBUTES, "the profile is out of the colour directory");
    check(GetFileAttributesA(tmp) != INVALID_FILE_ATTRIBUTES, "the original file stays");
    ret = UpdateICMRegKeyA(0, NULL, tmp, QUERY);
    check(!ret, "uninstalled: query %d", ret);

    /* arguments */
    SetLastError(0xdeadbeef);
    ret = UpdateICMRegKeyA(1, NULL, tmp, ADD);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "reserved must be zero (%d, %lu)", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = UpdateICMRegKeyA(0, NULL, NULL, ADD);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no file name (%d, %lu)", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = UpdateICMRegKeyA(0, NULL, tmp, 4);
    check(!ret && GetLastError() == ERROR_NOT_SUPPORTED, "ICM_SETDEFAULTPROFILE is not supported (%d, %lu)", ret, GetLastError());
    ret = UpdateICMRegKeyA(0, NULL, tmp, 99);
    check(!ret, "an unknown command fails");

    DeleteFileA(tmp);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
