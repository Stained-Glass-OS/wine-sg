/* Probe for patches/sg/5007: EFS on a file system that cannot encrypt.
 * FileEncryptionStatus says FILE_SYSTEM_NOT_SUPPORT for files that exist and
 * fails for ones that do not; EncryptFile fails ERROR_VOLUME_NOT_SUPPORT_EFS;
 * DecryptFile of a plain file succeeds, but needs the file to itself and
 * fails on a read-only one. A and W alike.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    char dir[MAX_PATH], file[MAX_PATH], missing[MAX_PATH];
    WCHAR fileW[MAX_PATH], missingW[MAX_PATH];
    unsigned int i;
    DWORD status, err;
    HANDLE h;
    BOOL ret;

    GetTempPathA(MAX_PATH, dir);
    sprintf(file, "%ssg-efs.txt", dir);
    sprintf(missing, "%ssg-efs-missing.txt", dir);
    MultiByteToWideChar(CP_ACP, 0, file, -1, fileW, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, missing, -1, missingW, MAX_PATH);
    h = CreateFileA(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
    DeleteFileA(missing);

    for (i = 0; i < 2; i++)
    {
        const char *w = i ? "W" : "A";

        status = 0xdeadbeef;
        ret = i ? FileEncryptionStatusW(fileW, &status) : FileEncryptionStatusA(file, &status);
        check(ret && status == FILE_SYSTEM_NOT_SUPPORT, "FileEncryptionStatus%s: %d status %lu", w, ret, status);
        SetLastError(0xdeadbeef);
        ret = i ? FileEncryptionStatusW(missingW, &status) : FileEncryptionStatusA(missing, &status);
        err = GetLastError();
        check(!ret && err == ERROR_FILE_NOT_FOUND, "FileEncryptionStatus%s of a missing file: %d %lu", w, ret, err);

        SetLastError(0xdeadbeef);
        ret = i ? EncryptFileW(fileW) : EncryptFileA(file);
        err = GetLastError();
        check(!ret && err == ERROR_VOLUME_NOT_SUPPORT_EFS, "EncryptFile%s: %d %lu", w, ret, err);
        SetLastError(0xdeadbeef);
        ret = i ? EncryptFileW(missingW) : EncryptFileA(missing);
        err = GetLastError();
        check(!ret && err == ERROR_FILE_NOT_FOUND, "EncryptFile%s of a missing file: %d %lu", w, ret, err);

        ret = i ? DecryptFileW(fileW, 0) : DecryptFileA(file, 0);
        check(ret, "DecryptFile%s of a plain file: %d %lu", w, ret, ret ? 0 : GetLastError());
        SetLastError(0xdeadbeef);
        ret = i ? DecryptFileW(missingW, 0) : DecryptFileA(missing, 0);
        err = GetLastError();
        check(!ret && err == ERROR_FILE_NOT_FOUND, "DecryptFile%s of a missing file: %d %lu", w, ret, err);

        h = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        SetLastError(0xdeadbeef);
        ret = i ? DecryptFileW(fileW, 0) : DecryptFileA(file, 0);
        err = GetLastError();
        check(!ret && err == ERROR_SHARING_VIOLATION, "DecryptFile%s of a file open elsewhere: %d %lu", w, ret, err);
        CloseHandle(h);

        SetFileAttributesA(file, FILE_ATTRIBUTE_READONLY);
        SetLastError(0xdeadbeef);
        ret = i ? DecryptFileW(fileW, 0) : DecryptFileA(file, 0);
        err = GetLastError();
        check(!ret && err == ERROR_FILE_READ_ONLY, "DecryptFile%s of a read-only file: %d %lu", w, ret, err);
        SetFileAttributesA(file, FILE_ATTRIBUTE_NORMAL);
    }
    DeleteFileA(file);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
