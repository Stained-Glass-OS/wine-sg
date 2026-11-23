/* wine-sg 9708: RegReplaceKey (advapi32, NtReplaceKey) -- the file a loaded hive came from becomes the backup
 * and the new file takes its name, so the next load of the hive has the new keys; SeRestorePrivilege needed.
 * The stub did nothing and said it succeeded (MediaInfo imports it). */
#include <windows.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static void privilege(const char *name, BOOL on)
{
    HANDLE token;
    TOKEN_PRIVILEGES tp;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token);
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueA(NULL, name, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = on ? SE_PRIVILEGE_ENABLED : 0;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    CloseHandle(token);
}

static DWORD value_of(HKEY root, const char *path)
{
    DWORD v = 0, size = sizeof(v);
    RegGetValueA(root, path, "v", RRF_RT_REG_DWORD, NULL, &v, &size);
    return v;
}

static BOOL exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

int main(void)
{
    char dir[MAX_PATH], hive1[MAX_PATH], hive2[MAX_PATH], old[MAX_PATH];
    DWORD v;
    HKEY key;
    LONG ret;

    GetTempPathA(sizeof(dir), dir);
    sprintf(hive1, "%ssg-rr-hive1.reg", dir);
    sprintf(hive2, "%ssg-rr-hive2.reg", dir);
    sprintf(old, "%ssg-rr-old.reg", dir);
    DeleteFileA(hive1); DeleteFileA(hive2); DeleteFileA(old);
    privilege("SeBackupPrivilege", TRUE);
    privilege("SeRestorePrivilege", TRUE);

    RegCreateKeyA(HKEY_CURRENT_USER, "Software\\sg-regreplace", &key);
    v = 1; RegSetValueExA(key, "v", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    ret = RegSaveKeyA(key, hive1, NULL);
    v = 2; RegSetValueExA(key, "v", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    ret |= RegSaveKeyA(key, hive2, NULL);
    RegCloseKey(key);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\sg-regreplace");
    CHECK(!ret, "two hive files saved (v=1, v=2)");
    ret = RegLoadKeyA(HKEY_USERS, "sg-rr-hive", hive1);
    CHECK(!ret && value_of(HKEY_USERS, "sg-rr-hive") == 1, "the first loaded (ret %ld, v=%lu)", ret, value_of(HKEY_USERS, "sg-rr-hive"));

    ret = RegReplaceKeyA(HKEY_CURRENT_USER, "Software", hive2, old);
    CHECK(ret == ERROR_INVALID_PARAMETER && exists(hive2) && !exists(old),
          "a key that is no loaded hive: ERROR_INVALID_PARAMETER, nothing moved (%ld)", ret);
    privilege("SeRestorePrivilege", FALSE);
    ret = RegReplaceKeyA(HKEY_USERS, "sg-rr-hive", hive2, old);
    CHECK(ret == ERROR_PRIVILEGE_NOT_HELD && exists(hive2) && !exists(old),
          "without SeRestorePrivilege: ERROR_PRIVILEGE_NOT_HELD (%ld)", ret);
    privilege("SeRestorePrivilege", TRUE);
    ret = RegReplaceKeyA(HKEY_USERS, "sg-rr-hive", hive2, old);
    CHECK(!ret, "RegReplaceKey (ret %ld)", ret);
    CHECK(exists(old) && !exists(hive2) && exists(hive1), "the hive's file became the backup, the new file took its name");
    CHECK(value_of(HKEY_USERS, "sg-rr-hive") == 1, "the hive in memory is unchanged until loaded again");
    ret = RegUnLoadKeyA(HKEY_USERS, "sg-rr-hive");
    ret |= RegLoadKeyA(HKEY_USERS, "sg-rr-hive", hive1);
    CHECK(!ret && value_of(HKEY_USERS, "sg-rr-hive") == 2, "loaded again: the new file's keys (v=%lu)", value_of(HKEY_USERS, "sg-rr-hive"));
    CopyFileA(old, hive2, FALSE);
    ret = RegReplaceKeyA(HKEY_USERS, "sg-rr-hive", hive2, old);
    CHECK(ret == ERROR_ALREADY_EXISTS && exists(hive2), "a backup name that exists: ERROR_ALREADY_EXISTS, nothing moved (%ld)", ret);
    RegUnLoadKeyA(HKEY_USERS, "sg-rr-hive");
    DeleteFileA(hive1); DeleteFileA(hive2); DeleteFileA(old);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
