/* userroots-probe: whether a standard user trusts the machine's roots
 * (patches/sg/0447), for test/userroots-gate.sh.
 *
 *   userroots-probe lock     make HKLM\...\SystemCertificates\Root read-only
 *                            for everyone (a standard user's view of it)
 *   userroots-probe find     count=N of the user's Root store (HKCU + HKLM),
 *                            and isrg=0/1: whether ISRG Root X1 is in it
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <aclapi.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static const BYTE isrg_sha1[20] = { 0xca,0xbd,0x2a,0x79,0xa1,0x07,0x6a,0x31,0xf2,0x1d,
                                    0x25,0x36,0x35,0xcb,0x03,0x9d,0x43,0x29,0xa5,0xe8 };

static int lock_key(const char *path)
{
    EXPLICIT_ACCESS_A ea = {0};
    PACL acl = NULL;
    HKEY key;
    LONG rc;

    if ((rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, WRITE_DAC | READ_CONTROL, &key))) return rc;
    ea.grfAccessPermissions = KEY_READ;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea.Trustee.ptstrName = (char *)"EVERYONE";
    if ((rc = SetEntriesInAclA(1, &ea, NULL, &acl))) return rc;
    rc = SetSecurityInfo(key, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                         NULL, NULL, acl, NULL);
    RegCloseKey(key);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "lock"))
    {
        LONG a = lock_key("Software\\Microsoft\\SystemCertificates\\Root");
        LONG b = lock_key("Software\\Microsoft\\SystemCertificates\\Root\\Certificates");
        HKEY key;
        LONG w = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\SystemCertificates\\Root", 0, KEY_ALL_ACCESS, &key);
        printf("lock=%ld,%ld write=%ld\n", a, b, w);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "find"))
    {
        HCERTSTORE store = CertOpenSystemStoreA(0, "ROOT");
        PCCERT_CONTEXT cert = NULL;
        int count = 0, isrg = 0;
        BYTE hash[20];
        DWORD len;

        if (!store) { printf("open failed %lu\n", GetLastError()); return 1; }
        while ((cert = CertEnumCertificatesInStore(store, cert)))
        {
            count++;
            len = sizeof(hash);
            if (CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash, &len) && !memcmp(hash, isrg_sha1, 20))
                isrg = 1;
        }
        printf("count=%d isrg=%d\n", count > 0 ? (count > 20 ? 21 : count) : 0, isrg);
        CertCloseStore(store, 0);
        return 0;
    }
    fprintf(stderr, "usage: userroots-probe lock|find\n");
    return 2;
}
