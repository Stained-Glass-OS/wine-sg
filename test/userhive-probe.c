/* userhive-probe: what a user may do in their own HKCU after an elevated program wrote there (5948),
 * and which key a 32-bit program's HKCU\Software is (5949). Prints "NAME -> result" lines.
 *   userhive-probe access     (64-bit, as the user)
 *   userhive-probe plant      (64-bit: creates HKCU\Software\WOW6432Node\SgPlanted, as a program may)
 *   userhive-probe view       (32-bit: the names its keys resolve to)
 *   userhive-probe sid        (the user's SID, for HKU\<SID>)  */
#include <windows.h>
#include <winternl.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *NQK)(HANDLE, int, void *, ULONG, ULONG *);

static void openk(const char *what, HKEY root, const char *path, REGSAM sam)
{
    HKEY k;
    LONG r = RegOpenKeyExA(root, path, 0, sam, &k);
    printf("%s %s -> %ld\n", what, path, r);
    if (!r) RegCloseKey(k);
}

static void createk(const char *what, const char *path, REGSAM sam)
{
    HKEY k;
    DWORD d;
    LONG r = RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS | sam, NULL, &k, &d);
    if (!r)
    {
        r = RegSetValueExA(k, "v", 0, REG_SZ, (const BYTE *)"x", 2);
        RegCloseKey(k);
    }
    printf("%s %s -> %ld\n", what, path, r);
}

static void name(const char *what, HKEY root, const char *path, REGSAM sam)
{
    NQK q = (NQK)GetProcAddress(GetModuleHandleA("ntdll"), "NtQueryKey");
    BYTE buf[2048];
    ULONG len;
    HKEY k;
    LONG r = RegOpenKeyExA(root, path, 0, KEY_QUERY_VALUE | sam, &k);
    if (r) { printf("%s %s -> open %ld\n", what, path, r); return; }
    if (!q(k, 3 /* KeyNameInformation */, buf, sizeof(buf), &len))
    {
        ULONG n = *(ULONG *)buf / sizeof(WCHAR);
        WCHAR *w = (WCHAR *)(buf + sizeof(ULONG));
        int wow = 0;
        for (ULONG i = 0; i + 11 <= n; i++)
            if (!_wcsnicmp(w + i, L"Wow6432Node", 11)) wow = 1;
        printf("%s %s -> %s\n", what, path, wow ? "Wow6432Node" : "shared");
    }
    else printf("%s %s -> query failed\n", what, path);
    RegCloseKey(k);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "access";

    if (!strcmp(mode, "access"))
    {
        openk("OPENREAD", HKEY_CURRENT_USER, "Software", KEY_READ);
        openk("OPENRW", HKEY_CURRENT_USER, "Software\\SgElevated\\Sub", KEY_READ | KEY_WRITE);
        openk("OPENRW", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ | KEY_WRITE);
        createk("CREATE", "Software\\SgUserNew", 0);
        createk("CREATE", "Software\\SgElevated\\Sub\\Mine", 0);
        createk("CREATE", "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SgUserApp", 0);
    }
    else if (!strcmp(mode, "sid"))
    {
        HANDLE tok;
        BYTE b[512];
        DWORD l;
        char *str;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok) && GetTokenInformation(tok, TokenUser, b, sizeof(b), &l) &&
            ConvertSidToStringSidA(((TOKEN_USER *)b)->User.Sid, &str))
            printf("SID %s\n", str);
    }
    else if (!strcmp(mode, "plant"))
        createk("PLANT", "Software\\WOW6432Node\\SgPlanted", KEY_WOW64_64KEY);
    else if (!strcmp(mode, "view"))
    {
        name("VIEW32", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion", KEY_WOW64_32KEY);
        name("VIEW32", HKEY_CURRENT_USER, "Software\\SgUserNew", KEY_WOW64_32KEY);
        name("VIEW32", HKEY_LOCAL_MACHINE, "Software\\Microsoft", KEY_WOW64_32KEY);
        createk("CREATE32", "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SgUser32", KEY_WOW64_32KEY);
        openk("OPEN64", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SgUser32", KEY_READ | KEY_WOW64_64KEY);
    }
    return 0;
}
