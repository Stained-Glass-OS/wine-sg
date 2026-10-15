/* Probe for patches/sg/5009: RegQueryReflectionKey reports reflection off for
 * an open key, RegEnableReflectionKey/RegDisableReflectionKey succeed with no
 * effect; a closed or NULL key fails ERROR_INVALID_HANDLE, a NULL out pointer
 * ERROR_INVALID_PARAMETER.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    HKEY key, closed;
    BOOL off;
    LONG ret;

    ret = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\sg-reflection", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    check(!ret, "create key %ld", ret);
    RegOpenKeyExA(HKEY_CURRENT_USER, "Software", 0, KEY_READ, &closed);
    RegCloseKey(closed);

    off = 0x55;
    ret = RegQueryReflectionKey(key, &off);
    check(!ret && off == TRUE, "query an open key: %ld disabled %d", ret, off);
    ret = RegQueryReflectionKey(key, NULL);
    check(ret == ERROR_INVALID_PARAMETER, "query with no out pointer: %ld", ret);
    ret = RegQueryReflectionKey(closed, &off);
    check(ret == ERROR_INVALID_HANDLE, "query a closed key: %ld", ret);
    ret = RegQueryReflectionKey(NULL, &off);
    check(ret == ERROR_INVALID_HANDLE, "query a NULL key: %ld", ret);

    ret = RegEnableReflectionKey(key);
    check(!ret, "enable on an open key: %ld", ret);
    ret = RegDisableReflectionKey(key);
    check(!ret, "disable on an open key: %ld", ret);
    ret = RegEnableReflectionKey(closed);
    check(ret == ERROR_INVALID_HANDLE, "enable on a closed key: %ld", ret);
    off = 0x55;
    ret = RegQueryReflectionKey(key, &off);
    check(!ret && off == TRUE, "still reported disabled: %ld %d", ret, off);

    RegCloseKey(key);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\sg-reflection");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
