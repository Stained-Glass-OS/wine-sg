/* hostroots-probe lock | count  (patches/sg/5940)
 *   lock:  empties HKLM\...\SystemCertificates\Root\Certificates and denies
 *          everyone write access to it -- a machine whose Root store no
 *          administrator's program has filled, as a standard user sees it
 *   count: the Root store a program opens (CertOpenSystemStore "Root", as
 *          wininet's chain engine does) and the machine's; prints how many
 *          roots each holds and whether ISRG Root X1 (Let's Encrypt) is there */
#include <windows.h>
#include <wincrypt.h>
#include <aclapi.h>
#include <stdio.h>

#define KEYPATH L"Software\\Microsoft\\SystemCertificates\\Root\\Certificates"

static int lock(void)
{
    HKEY key;
    EXPLICIT_ACCESS_W ea[2] = {0};
    PACL acl = NULL;
    LONG rc;

    RegDeleteTreeW(HKEY_LOCAL_MACHINE, KEYPATH);
    if ((rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, KEYPATH, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)))
    { printf("create failed %ld\n", rc); return 1; }
    ea[0].grfAccessPermissions = KEY_READ;
    ea[0].grfAccessMode = GRANT_ACCESS;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea[0].Trustee.ptstrName = (WCHAR *)L"EVERYONE";
    ea[1].grfAccessPermissions = KEY_SET_VALUE | KEY_CREATE_SUB_KEY | DELETE | WRITE_DAC;
    ea[1].grfAccessMode = DENY_ACCESS;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea[1].Trustee.ptstrName = (WCHAR *)L"EVERYONE";
    if (SetEntriesInAclW(2, ea, NULL, &acl)) { printf("acl failed\n"); return 1; }
    rc = SetSecurityInfo(key, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                         NULL, NULL, acl, NULL);
    RegCloseKey(key);
    if (rc) { printf("setsecurity failed %ld\n", rc); return 1; }
    rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, KEYPATH, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    printf("LOCKED (write open now %s)\n", rc ? "denied" : "ALLOWED");
    if (!rc) RegCloseKey(key);
    return rc ? 0 : 1;
}

static void count(const char *what, HCERTSTORE s)
{
    PCCERT_CONTEXT c = NULL;
    int n = 0, isrg = 0;
    WCHAR name[256];
    while ((c = CertEnumCertificatesInStore(s, c)))
    {
        n++;
        if (CertGetNameStringW(c, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, 256) && !wcscmp(name, L"ISRG Root X1"))
            isrg = 1;
    }
    printf("%s roots=%d isrg=%d\n", what, n, isrg);
}

int main(int argc, char **argv)
{
    HCERTSTORE s;
    HKEY key;
    DWORD subkeys = 0;
    if (argc > 1 && !strcmp(argv[1], "lock")) return lock();
    s = CertOpenSystemStoreW(0, L"Root");
    if (!s) { printf("user Root store: none (%lu)\n", GetLastError()); return 1; }
    count("user", s);
    CertCloseStore(s, 0);
    s = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, L"Root");
    if (s) { count("machine", s); CertCloseStore(s, 0); }
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, KEYPATH, 0, KEY_READ, &key))
    {
        RegQueryInfoKeyW(key, NULL, NULL, NULL, &subkeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        RegCloseKey(key);
    }
    printf("registry roots=%lu\n", subkeys);
    return 0;
}
