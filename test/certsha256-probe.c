/* certsha256-probe.c: CERT_SHA256_HASH_PROP_ID (107) of a certificate (wine-sg 7783).
 * probe.exe CERT.DER prints the property's size and value, the SHA-1 thumbprint for
 * comparison, and whether a stored value wins over the computed one. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

#ifndef CERT_SHA256_HASH_PROP_ID
#define CERT_SHA256_HASH_PROP_ID 107
#endif

static void hex(const char *name, const BYTE *p, DWORD n)
{
    DWORD i;
    printf("%s=", name);
    for (i = 0; i < n; i++) printf("%02x", p[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    static BYTE der[65536];
    BYTE buf[64], fake[32];
    PCCERT_CONTEXT cert;
    CRYPT_DATA_BLOB blob;
    DWORD n, size;
    BOOL ret;
    FILE *f;

    if (argc < 2 || !(f = fopen(argv[1], "rb"))) { printf("NoInput\n"); return 1; }
    n = fread(der, 1, sizeof(der), f);
    fclose(f);
    cert = CertCreateCertificateContext(X509_ASN_ENCODING, der, n);
    printf("Created=%d\n", cert != NULL);
    if (!cert) return 1;

    size = 0;
    SetLastError(0xdeadbeef);
    ret = CertGetCertificateContextProperty(cert, CERT_SHA256_HASH_PROP_ID, NULL, &size);
    printf("SizeQuery=%d %lu 0x%08lx\n", ret, size, ret ? 0 : GetLastError());

    size = 8;
    SetLastError(0xdeadbeef);
    ret = CertGetCertificateContextProperty(cert, CERT_SHA256_HASH_PROP_ID, buf, &size);
    printf("Short=%d %lu %lu\n", ret, size, ret ? 0 : GetLastError());

    size = sizeof(buf);
    ret = CertGetCertificateContextProperty(cert, CERT_SHA256_HASH_PROP_ID, buf, &size);
    printf("Get=%d %lu\n", ret, size);
    if (ret) hex("Sha256", buf, size);

    size = sizeof(buf);
    ret = CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, buf, &size);
    if (ret) hex("Sha1", buf, size);

    /* the computed value is kept: it is listed among the properties now */
    n = 0;
    while ((n = CertEnumCertificateContextProperties(cert, n)))
        if (n == CERT_SHA256_HASH_PROP_ID) break;
    printf("Listed=%d\n", n == CERT_SHA256_HASH_PROP_ID);

    /* a value set on the context is what a later get gives */
    memset(fake, 0x5a, sizeof(fake));
    blob.cbData = sizeof(fake);
    blob.pbData = fake;
    ret = CertSetCertificateContextProperty(cert, CERT_SHA256_HASH_PROP_ID, 0, &blob);
    size = sizeof(buf);
    printf("Set=%d\n", ret);
    if (CertGetCertificateContextProperty(cert, CERT_SHA256_HASH_PROP_ID, buf, &size))
        printf("AfterSet=%d\n", size == sizeof(fake) && !memcmp(buf, fake, size));
    else
        printf("AfterSet=-1\n");

    CertFreeCertificateContext(cert);
    return 0;
}
