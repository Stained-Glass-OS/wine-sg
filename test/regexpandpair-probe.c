/* regexpandpair-probe: RegGetValue with RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ and no
 * RRF_NOEXPAND (patches/sg/7004), as Python's py.exe reads PythonCore\X.Y\InstallPath.
 * Prints key=1|0 lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    HKEY key;
    WCHAR buf[MAX_PATH], expected[MAX_PATH];
    DWORD size, type;
    LSTATUS r;
    const DWORD pair = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ;

    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SgRegExpandPair\\3.13\\InstallPath", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, NULL, 0, REG_SZ, (const BYTE *)L"C:\\Python313\\", sizeof(L"C:\\Python313\\"));
    RegSetValueExW(key, L"Exp", 0, REG_EXPAND_SZ, (const BYTE *)L"%SystemRoot%\\py", sizeof(L"%SystemRoot%\\py"));
    RegCloseKey(key);

    /* size first, as py.exe does: no buffer, subkey given */
    size = 0;
    r = RegGetValueW(HKEY_CURRENT_USER, L"Software\\SgRegExpandPair\\3.13\\InstallPath", NULL, pair, NULL, NULL, &size);
    printf("size_query r=%ld size=%lu\n", r, size);
    printf("pair_size_ok=%d\n", r == ERROR_SUCCESS && size == sizeof(L"C:\\Python313\\"));

    size = sizeof(buf);
    r = RegGetValueW(HKEY_CURRENT_USER, L"Software\\SgRegExpandPair\\3.13\\InstallPath", NULL, pair, &type, buf, &size);
    printf("pair_sz_ok=%d\n", r == ERROR_SUCCESS && type == REG_SZ && !lstrcmpW(buf, L"C:\\Python313\\"));

    /* a REG_EXPAND_SZ value comes back expanded, as REG_SZ */
    ExpandEnvironmentStringsW(L"%SystemRoot%\\py", expected, MAX_PATH);
    size = sizeof(buf);
    r = RegGetValueW(HKEY_CURRENT_USER, L"Software\\SgRegExpandPair\\3.13\\InstallPath", L"Exp", pair, &type, buf, &size);
    printf("pair_expanded_ok=%d\n", r == ERROR_SUCCESS && type == REG_SZ && !lstrcmpW(buf, expected));

    /* REG_EXPAND_SZ alone without RRF_NOEXPAND stays invalid (Wine's conformance tests) */
    size = sizeof(buf);
    r = RegGetValueW(HKEY_CURRENT_USER, L"Software\\SgRegExpandPair\\3.13\\InstallPath", L"Exp", RRF_RT_REG_EXPAND_SZ, &type, buf, &size);
    printf("expand_alone_invalid=%d\n", r == ERROR_INVALID_PARAMETER);

    /* the ANSI one too */
    size = 0;
    r = RegGetValueA(HKEY_CURRENT_USER, "Software\\SgRegExpandPair\\3.13\\InstallPath", NULL, pair, NULL, NULL, &size);
    printf("pair_ansi_ok=%d\n", r == ERROR_SUCCESS && size == sizeof("C:\\Python313\\"));

    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SgRegExpandPair");
    printf("done=1\n");
    return 0;
}
