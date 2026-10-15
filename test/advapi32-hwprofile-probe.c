/* Probe for patches/sg/5067: GetCurrentHwProfileW / GetCurrentHwProfileA.
 * - W succeeds (it failed) with a "{...}" GUID string and a name; A reports the same;
 * - both read the current profile's HwProfileGuid and FriendlyName values from
 *   HKLM\System\CurrentControlSet\Control\IDConfigDB\Hardware Profiles\0001,
 *   so a name set there is what they report;
 * - a NULL pointer is ERROR_INVALID_PARAMETER.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const WCHAR key_name[] = L"System\\CurrentControlSet\\Control\\IDConfigDB\\Hardware Profiles\\0001";

int main(void)
{
    static const WCHAR *const names[] = {L"SG probe profile", L"Profil n\x00b0 2"};
    HW_PROFILE_INFOW w;
    HW_PROFILE_INFOA a;
    WCHAR guid[39], conv[80];
    unsigned int i;
    HKEY key;
    BOOL ret;

    memset(&w, 0xcc, sizeof(w));
    ret = GetCurrentHwProfileW(&w);
    check(ret, "GetCurrentHwProfileW %d %lu", ret, ret ? 0 : GetLastError());
    check(ret && lstrlenW(w.szHwProfileGuid) == 38 && w.szHwProfileGuid[0] == '{' && w.szHwProfileGuid[37] == '}',
          "a GUID string: %ls", ret ? w.szHwProfileGuid : L"");
    lstrcpyW(guid, w.szHwProfileGuid);
    memset(&a, 0xcc, sizeof(a));
    ret = GetCurrentHwProfileA(&a);
    MultiByteToWideChar(CP_ACP, 0, a.szHwProfileGuid, -1, conv, ARRAYSIZE(conv));
    check(ret && !lstrcmpW(conv, guid) && a.dwDockInfo == w.dwDockInfo, "A has the same GUID: %s", a.szHwProfileGuid);

    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_name, 0, KEY_SET_VALUE, &key))
    {
        for (i = 0; i < ARRAYSIZE(names); i++)
        {
            RegSetValueExW(key, L"FriendlyName", 0, REG_SZ, (const BYTE *)names[i], (lstrlenW(names[i]) + 1) * sizeof(WCHAR));
            ret = GetCurrentHwProfileW(&w);
            check(ret && !lstrcmpW(w.szHwProfileName, names[i]) && !lstrcmpW(w.szHwProfileGuid, guid),
                  "name %u from the registry: \"%ls\"", i, w.szHwProfileName);
            ret = GetCurrentHwProfileA(&a);
            MultiByteToWideChar(CP_ACP, 0, a.szHwProfileName, -1, conv, ARRAYSIZE(conv));
            check(ret && !lstrcmpW(conv, names[i]), "name %u through A: \"%s\"", i, a.szHwProfileName);
        }
        RegDeleteValueW(key, L"FriendlyName");
        RegCloseKey(key);
    }
    else check(0, "the profile key exists");

    SetLastError(0xdeadbeef);
    ret = GetCurrentHwProfileW(NULL);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "W NULL: %d %lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = GetCurrentHwProfileA(NULL);
    check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "A NULL: %d %lu", ret, GetLastError());
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
