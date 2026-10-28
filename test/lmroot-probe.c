/* lmroot-probe: the machine's roots in a user's Root store (patches/sg/7604)
 *   lmroot-probe add DER     add DER to the machine's Root store (as SYSTEM)
 *   lmroot-probe find NAME   CertOpenSystemStore("Root"), as a chain engine opens it: NAME there? */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "add")) {
        HANDLE f = CreateFileA(argv[2], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        BYTE buf[8192];
        DWORD len = 0;
        HCERTSTORE s = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, "Root");
        if (f == INVALID_HANDLE_VALUE || !ReadFile(f, buf, sizeof(buf), &len, NULL) || !s) { printf("ERROR\n"); return 1; }
        printf(CertAddEncodedCertificateToStore(s, X509_ASN_ENCODING, buf, len, CERT_STORE_ADD_REPLACE_EXISTING, NULL) ? "ADDED\n" : "ERROR add\n");
        CertCloseStore(s, 0);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "find")) {
        HCERTSTORE s = CertOpenSystemStoreA(0, "Root");
        PCCERT_CONTEXT c = NULL;
        int n = 0, found = 0;
        char name[256];
        while (s && (c = CertEnumCertificatesInStore(s, c))) {
            n++;
            CertGetNameStringA(c, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, sizeof(name));
            if (!strcmp(name, argv[2])) found = 1;
        }
        printf("FOUND %d ROOTS %d\n", found, n);
        return 0;
    }
    return 2;
}
