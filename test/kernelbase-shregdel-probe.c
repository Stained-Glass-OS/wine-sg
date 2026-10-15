/* Probe for patches/sg/5041: SHRegDeleteUSValue and SHRegDeleteEmptyUSKey
 * (A/W) delete as SHREGDEL_FLAGS say: DEFAULT from HKCU, else (not found
 * there) from HKLM; HKCU or HKLM from that one; BOTH from both; an empty
 * subkey only (one with subkeys: ERROR_KEY_HAS_CHILDREN). They returned
 * ERROR_SUCCESS and deleted nothing.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define PATH L"Software\\sg-shregdel"

static BOOL has_value(HKEY root, const WCHAR *name)
{
    return !RegGetValueW(root, PATH, name, RRF_RT_ANY, NULL, NULL, NULL);
}
static BOOL has_key(HKEY root, const WCHAR *name)
{
    WCHAR path[MAX_PATH];
    HKEY key;
    wsprintfW(path, L"%s\\%s", PATH, name);
    if (RegOpenKeyExW(root, path, 0, KEY_READ, &key)) return FALSE;
    RegCloseKey(key);
    return TRUE;
}

int main(void)
{
    static const HKEY roots[2] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
    DWORD one = 1;
    HUSKEY us = NULL;
    HKEY key;
    LONG ret;
    unsigned int i;

    for (i = 0; i < 2; i++)
    {
        RegCreateKeyExW(roots[i], PATH, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
        RegSetValueExW(key, L"v", 0, REG_DWORD, (BYTE *)&one, 4);
        RegSetValueExW(key, L"w", 0, REG_DWORD, (BYTE *)&one, 4);
        RegCloseKey(key);
    }
    RegSetKeyValueW(HKEY_CURRENT_USER, PATH, L"a", REG_DWORD, &one, 4);
    RegCreateKeyExW(HKEY_CURRENT_USER, PATH L"\\empty", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL); RegCloseKey(key);
    RegCreateKeyExW(HKEY_CURRENT_USER, PATH L"\\full\\child", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL); RegCloseKey(key);
    check(has_value(HKEY_LOCAL_MACHINE, L"v"), "HKLM value written (the prefix allows it)");

    ret = SHRegOpenUSKeyW(PATH, KEY_ALL_ACCESS, NULL, &us, FALSE);
    check(!ret && us, "SHRegOpenUSKeyW: %ld", ret);
    if (!us) { printf("RESULT: FAIL\n"); return 1; }

    ret = SHRegDeleteUSValueW(us, L"v", SHREGDEL_DEFAULT);
    check(!ret && !has_value(HKEY_CURRENT_USER, L"v") && has_value(HKEY_LOCAL_MACHINE, L"v"), "DEFAULT deletes from HKCU only: %ld", ret);
    ret = SHRegDeleteUSValueW(us, L"v", SHREGDEL_DEFAULT);
    check(!ret && !has_value(HKEY_LOCAL_MACHINE, L"v"), "DEFAULT, not in HKCU: from HKLM: %ld", ret);
    ret = SHRegDeleteUSValueW(us, L"v", SHREGDEL_DEFAULT);
    check(ret == ERROR_FILE_NOT_FOUND, "DEFAULT, in neither: %ld", ret);
    ret = SHRegDeleteUSValueW(us, L"w", SHREGDEL_BOTH);
    check(!ret && !has_value(HKEY_CURRENT_USER, L"w") && !has_value(HKEY_LOCAL_MACHINE, L"w"), "BOTH deletes from both: %ld", ret);
    ret = SHRegDeleteUSValueA(us, "a", SHREGDEL_HKLM);
    check(ret == ERROR_FILE_NOT_FOUND && has_value(HKEY_CURRENT_USER, L"a"), "A, HKLM only, not there: %ld", ret);
    ret = SHRegDeleteUSValueA(us, "a", SHREGDEL_HKCU);
    check(!ret && !has_value(HKEY_CURRENT_USER, L"a"), "A, HKCU: %ld", ret);
    ret = SHRegDeleteUSValueW(us, L"a", 0x100);
    check(ret == ERROR_INVALID_PARAMETER, "unknown flags: %ld", ret);
    ret = SHRegDeleteUSValueW(NULL, L"a", SHREGDEL_HKCU);
    check(ret == ERROR_INVALID_PARAMETER, "NULL key: %ld", ret);

    ret = SHRegDeleteEmptyUSKeyW(us, L"full", SHREGDEL_HKCU);
    check(ret == ERROR_KEY_HAS_CHILDREN && has_key(HKEY_CURRENT_USER, L"full"), "a key with subkeys stays: %ld", ret);
    ret = SHRegDeleteEmptyUSKeyA(us, "empty", SHREGDEL_DEFAULT);
    check(!ret && !has_key(HKEY_CURRENT_USER, L"empty"), "an empty key goes: %ld", ret);
    SHRegCloseUSKey(us);

    RegDeleteTreeW(HKEY_CURRENT_USER, PATH);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, PATH);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
