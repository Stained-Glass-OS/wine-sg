/* userhive-probe: what a user may do in their own HKCU after an elevated program wrote there (5948),
 * and which key a 32-bit program's HKCU\Software is (5949). Prints "NAME -> result" lines.
 *   userhive-probe access     (64-bit, as the user)
 *   userhive-probe plant      (64-bit: creates HKCU\Software\WOW6432Node\SgPlanted, as a program may)
 *   userhive-probe view       (32-bit: the names its keys resolve to)
 *   userhive-probe sid        (the user's SID, for HKU\<SID>)
 *   userhive-probe aclmerge   (as Opera's installer: adds an ACE to HKCU\Software's DACL, then creates a key)
 *   userhive-probe other SID  (another user: opens HKU\SID\Software to write)  */
#include <windows.h>
#include <winternl.h>
#include <sddl.h>
#include <aclapi.h>
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
    else if (!strcmp(mode, "aclmerge"))
    {
        PACL dacl = NULL, merged = NULL;
        PSECURITY_DESCRIPTOR sd = NULL, sd2 = NULL;
        PSID owner = NULL, packages = NULL;
        EXPLICIT_ACCESS_W ea = { 0 };
        char *str = NULL;
        HANDLE tok;
        BYTE b[512];
        DWORD l, r;

        r = GetNamedSecurityInfoW(L"CURRENT_USER\\Software", SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                  &owner, NULL, &dacl, NULL, &sd);
        printf("GETSD -> %lu\n", r);
        if (!r && OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok) && GetTokenInformation(tok, TokenUser, b, sizeof(b), &l))
            printf("OWNERISUSER -> %d\n", EqualSid(owner, ((TOKEN_USER *)b)->User.Sid) ? 1 : 0);
        ConvertStringSidToSidA("S-1-15-2-1", &packages);   /* ALL APPLICATION PACKAGES */
        ea.grfAccessPermissions = KEY_READ;
        ea.grfAccessMode = GRANT_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.ptstrName = packages;
        r = SetEntriesInAclW(1, &ea, dacl, &merged);
        if (!r) r = SetNamedSecurityInfoW((WCHAR *)L"CURRENT_USER\\Software", SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, merged, NULL);
        printf("SETSD -> %lu\n", r);
        if (!GetNamedSecurityInfoW(L"CURRENT_USER\\Software", SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &sd2) &&
            ConvertSecurityDescriptorToStringSecurityDescriptorA(sd2, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &str, NULL))
            printf("DACL %s\n", str);
        createk("CREATE", "Software\\SgAfterAcl", 0);
        openk("OPENREAD", HKEY_CURRENT_USER, "Software", KEY_READ);
    }
    else if (!strcmp(mode, "other") && argc > 2)
    {
        char path[256];
        snprintf(path, sizeof(path), "%s\\Software", argv[2]);
        openk("OTHERWRITE", HKEY_USERS, path, KEY_SET_VALUE | KEY_CREATE_SUB_KEY);
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
