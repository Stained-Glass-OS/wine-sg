/* kurestrict-probe (patches/sg/9654, test/kurestrict-gate.sh): crypt32's key usage restriction
 * extension (szOID_KEY_USAGE_RESTRICTION 2.5.29.4, X509_KEY_USAGE_RESTRICTION), which
 * AutoHotkey's setup encodes for the certificate it signs AutoHotkey*_UIA.exe with. Prints:
 *   AHK <hex>          the encoding of AutoHotkey's value (no policies; digital signature and
 *                      data encipherment key usage, 0x90)
 *   POL <hex>          two policies (2 OIDs, 1 OID) and key usage 0x80, by the struct id
 *   ROUND <ok|...>     the second decoded again (allocated), the same value
 *   NONE <hex>         an empty value */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static void hex(const char *tag, const BYTE *p, DWORD n)
{
    DWORD i;
    printf("%s ", tag);
    for (i = 0; i < n; i++) printf("%02x", p[i]);
    printf("\n");
}

int main(void)
{
    BYTE ku1 = 0x90, ku2 = 0x80, buf[256];
    CERT_KEY_USAGE_RESTRICTION_INFO info = { 0 }, *out = NULL;
    LPSTR p1[] = { (LPSTR)"1.3.6.1.4.1.311.10.3.1", (LPSTR)"1.3.6.1.5.5.7.3.3" }, p2[] = { (LPSTR)"2.5.29.32.0" };
    CERT_POLICY_ID pol[2];
    DWORD n = sizeof(buf), size = 0;

    info.RestrictedKeyUsage.cbData = 1;
    info.RestrictedKeyUsage.pbData = &ku1;
    if (CryptEncodeObject(X509_ASN_ENCODING, szOID_KEY_USAGE_RESTRICTION, &info, buf, &n)) hex("AHK", buf, n);
    else printf("AHK fail %08lx\n", GetLastError());

    pol[0].cCertPolicyElementId = 2;
    pol[0].rgpszCertPolicyElementId = p1;
    pol[1].cCertPolicyElementId = 1;
    pol[1].rgpszCertPolicyElementId = p2;
    info.cCertPolicyId = 2;
    info.rgCertPolicyId = pol;
    info.RestrictedKeyUsage.pbData = &ku2;
    n = sizeof(buf);
    if (CryptEncodeObject(X509_ASN_ENCODING, X509_KEY_USAGE_RESTRICTION, &info, buf, &n))
    {
        hex("POL", buf, n);
        if (CryptDecodeObjectEx(X509_ASN_ENCODING, szOID_KEY_USAGE_RESTRICTION, buf, n, CRYPT_DECODE_ALLOC_FLAG,
                                NULL, &out, &size))
        {
            int ok = out->cCertPolicyId == 2 && out->rgCertPolicyId[0].cCertPolicyElementId == 2 &&
                     !strcmp(out->rgCertPolicyId[0].rgpszCertPolicyElementId[0], p1[0]) &&
                     !strcmp(out->rgCertPolicyId[0].rgpszCertPolicyElementId[1], p1[1]) &&
                     out->rgCertPolicyId[1].cCertPolicyElementId == 1 &&
                     !strcmp(out->rgCertPolicyId[1].rgpszCertPolicyElementId[0], p2[0]) &&
                     out->RestrictedKeyUsage.cbData == 1 && out->RestrictedKeyUsage.pbData[0] == 0x80;
            printf("ROUND %s\n", ok ? "ok" : "different");
            LocalFree(out);
        }
        else printf("ROUND fail %08lx\n", GetLastError());
    }
    else printf("POL fail %08lx\n", GetLastError());

    memset(&info, 0, sizeof(info));
    n = sizeof(buf);
    if (CryptEncodeObject(X509_ASN_ENCODING, X509_KEY_USAGE_RESTRICTION, &info, buf, &n)) hex("NONE", buf, n);
    else printf("NONE fail %08lx\n", GetLastError());
    fflush(stdout);
    return 0;
}
