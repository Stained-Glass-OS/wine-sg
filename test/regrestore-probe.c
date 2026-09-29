/* regrestore-gate.sh's probe (0507): RegRestoreKeyW of a binary hive and of
 * a file RegSaveKeyW wrote, over keys holding something else first. */
#include <windows.h>
#include <stdio.h>

static void enable(const WCHAR *name)
{
    HANDLE token;
    TOKEN_PRIVILEGES tp = { 1 };
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) return;
    LookupPrivilegeValueW(NULL, name, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    CloseHandle(token);
}

static const char *utf8(const WCHAR *s)
{
    static char buf[4][1024];
    static int n;
    char *b = buf[n++ & 3];
    WideCharToMultiByte(CP_UTF8, 0, s, -1, b, 1024, NULL, NULL);
    return b;
}

/* key/value lines, depth-first, names and types and sizes and a checksum */
static void dump(HKEY key, const WCHAR *path)
{
    WCHAR name[256], sub[1024];
    BYTE *data = malloc(65536);
    DWORD i, nlen, dlen, type;

    for (i = 0;; i++)
    {
        DWORD sum = 0, j;
        nlen = 256; dlen = 65536;
        if (RegEnumValueW(key, i, name, &nlen, NULL, &type, data, &dlen)) break;
        for (j = 0; j < dlen; j++) sum = sum * 31 + data[j];
        printf("value %s/%s %lu %lu %08lx\n", utf8(path), utf8(name[0] ? name : L"@"), type, dlen, sum);
        if (type == REG_SZ) printf("sz %s/%s %s\n", utf8(path), utf8(name[0] ? name : L"@"), utf8((WCHAR *)data));
    }
    free(data);
    for (i = 0;; i++)
    {
        HKEY child;
        nlen = 256;
        if (RegEnumKeyExW(key, i, name, &nlen, NULL, NULL, NULL, NULL)) break;
        swprintf(sub, 1024, L"%ls/%ls", path, name);
        printf("key %s\n", utf8(sub));
        if (!RegOpenKeyExW(key, name, 0, KEY_READ, &child)) { dump(child, sub); RegCloseKey(child); }
    }
}

int main(int argc, char **argv)
{
    WCHAR hive[MAX_PATH], saved[MAX_PATH];
    HKEY key, copy;
    LONG r;

    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, hive, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, saved, MAX_PATH);
    enable(L"SeRestorePrivilege");
    enable(L"SeBackupPrivilege");

    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SGRestoreTest", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"Stale", 0, REG_SZ, (const BYTE *)L"old", 8);
    RegCreateKeyExW(key, L"StaleKey\\Inner", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &copy, NULL);
    RegCloseKey(copy);

    r = RegRestoreKeyW(key, hive, 0);
    printf("restore %ld\n", r);
    dump(key, L"T");

    DeleteFileW(saved);
    r = RegSaveKeyW(key, saved, NULL);
    printf("save %ld\n", r);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SGRestoreCopy", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &copy, NULL);
    RegSetValueExW(copy, L"Stale", 0, REG_SZ, (const BYTE *)L"old", 8);
    r = RegRestoreKeyW(copy, saved, 0);
    printf("restoretext %ld\n", r);
    dump(copy, L"T");

    r = RegRestoreKeyW(copy, L"C:\\windows\\win.ini", 0);
    printf("notahive %ld\n", r);
    return 0;
}
