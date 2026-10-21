/* msiperuser-probe PRODUCTCODE: what Windows Installer says of a product, and where its Apps &
 * features entry is (test/msiperuser-gate.sh, patches/sg/5944, 5945). Prints
 *   STATE <MsiQueryProductState>
 *   ARP_HKLM <0 found, else the error>
 *   ARP_HKCU <0 found, else the error>
 *   SID <the caller's>
 *   OTHER <RegCreateKeyEx of HKLM\...\Installer\UserData\ARGV2\Products\SgOther, when given> */
#include <windows.h>
#include <sddl.h>
#include <msi.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    WCHAR path[256];
    HKEY k;
    LONG r;

    if (argc < 2) return 2;
    printf("STATE %d\n", MsiQueryProductStateW(argv[1]));
    _snwprintf(path, ARRAYSIZE(path), L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%ls", argv[1]);
    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ | KEY_WOW64_64KEY, &k);
    printf("ARP_HKLM %ld\n", r);
    if (!r) RegCloseKey(k);
    r = RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &k);
    printf("ARP_HKCU %ld\n", r);
    if (!r) RegCloseKey(k);
    {
        BYTE buf[256];
        DWORD len;
        HANDLE token;
        WCHAR *sid = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) &&
            GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len) &&
            ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &sid))
            printf("SID %ls\n", sid);
    }
    if (argc > 2)
    {
        _snwprintf(path, ARRAYSIZE(path), L"Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\%ls\\Products\\SgOther", argv[2]);
        r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &k, NULL);
        printf("OTHER %ld\n", r);
        if (!r) { RegDeleteKeyW(k, L""); RegCloseKey(k); }
    }
    fflush(stdout);
    return 0;
}
