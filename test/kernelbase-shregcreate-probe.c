/* Probe for patches/sg/5042: SHRegCreateUSKeyW makes the key in the hives its
 * flags name - SHREGSET_HKCU/FORCE_HKCU in HKCU, SHREGSET_HKLM/FORCE_HKLM in
 * HKLM, SHREGSET_DEFAULT (FORCE_HKCU | HKLM) in both - and gives back a USKEY
 * that writes there. Other flags returned ERROR_SUCCESS and a NULL key.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define PATH L"Software\\sg-shregcreate"

static BOOL has_key(HKEY root)
{
    HKEY key;
    if (RegOpenKeyExW(root, PATH, 0, KEY_READ, &key)) return FALSE;
    RegCloseKey(key);
    return TRUE;
}

int main(void)
{
    static const struct { const char *what; DWORD flags; BOOL hkcu, hklm; LONG ret; } tests[] =
    {
        {"FORCE_HKCU", SHREGSET_FORCE_HKCU, TRUE, FALSE, 0},
        {"HKCU", SHREGSET_HKCU, TRUE, FALSE, 0},
        {"FORCE_HKLM", SHREGSET_FORCE_HKLM, FALSE, TRUE, 0},
        {"HKLM", SHREGSET_HKLM, FALSE, TRUE, 0},
        {"DEFAULT", SHREGSET_DEFAULT, TRUE, TRUE, 0},
        {"unknown bit", SHREGSET_FORCE_HKCU | 0x100, FALSE, FALSE, ERROR_INVALID_PARAMETER},
    };
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        HUSKEY us = (HUSKEY)0xdeadbeef;
        DWORD v = i, got = 0, size = sizeof(got);
        LONG ret;

        RegDeleteTreeW(HKEY_CURRENT_USER, PATH);
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, PATH);
        ret = SHRegCreateUSKeyW(PATH, KEY_ALL_ACCESS, NULL, &us, tests[i].flags);
        check(ret == tests[i].ret && (ret ? us == NULL : us != NULL), "%s: %ld %p", tests[i].what, ret, us);
        check(has_key(HKEY_CURRENT_USER) == tests[i].hkcu && has_key(HKEY_LOCAL_MACHINE) == tests[i].hklm,
              "%s: made in HKCU %d, HKLM %d", tests[i].what, has_key(HKEY_CURRENT_USER), has_key(HKEY_LOCAL_MACHINE));
        if (!ret)
        {
            ret = SHRegWriteUSValueW(us, L"x", REG_DWORD, &v, sizeof(v), tests[i].hkcu ? SHREGSET_FORCE_HKCU : SHREGSET_FORCE_HKLM);
            RegGetValueW(tests[i].hkcu ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, PATH, L"x", RRF_RT_DWORD, NULL, &got, &size);
            check(!ret && got == v, "%s: the USKEY writes there (%ld, %lu)", tests[i].what, ret, got);
            SHRegCloseUSKey(us);
        }
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, PATH);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, PATH);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
