/* Probe for patches/sg/2442: a Unicode name infers the type of a CERT_RDN_ANY_TYPE value; a registry store opens on a predefined key. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static BYTE *encode_name(const WCHAR *text, DWORD *size)
{
    static char oid_cn[] = szOID_COMMON_NAME;
    CERT_RDN_ATTR attr;
    CERT_RDN rdn;
    CERT_NAME_INFO info;
    BYTE *buf = NULL;

    attr.pszObjId = oid_cn;
    attr.dwValueType = CERT_RDN_ANY_TYPE;
    attr.Value.cbData = 0;
    attr.Value.pbData = (BYTE *)text;
    rdn.cRDNAttr = 1; rdn.rgRDNAttr = &attr;
    info.cRDN = 1; info.rgRDN = &rdn;
    *size = 0;
    if (!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_UNICODE_NAME, &info, CRYPT_ENCODE_ALLOC_FLAG, NULL, &buf, size))
        return NULL;
    return buf;
}

int main(void)
{
    BYTE *buf;
    DWORD size, i;
    int tag = 0;
    HCERTSTORE store;
    CERT_NAME_VALUE nv;
    BYTE *vbuf = NULL;
    DWORD vsize;
    BOOL ret;

    buf = encode_name(L"Juan Lang", &size);
    check("a printable string encodes", buf != NULL);
    if (buf)
    {
        /* SEQUENCE { SET { SEQUENCE { OID cn, <string> } } }: the string tag follows the 5-byte OID */
        for (i = 0; i + 1 < size; i++) if (buf[i] == 0x06 && buf[i + 1] == 0x03) { tag = buf[i + 5]; break; }
        check("printable text becomes a PrintableString (0x13)", tag == 0x13);
        LocalFree(buf);
    }
    buf = encode_name(L"Ju\x0107n \x15b" L"ang", &size);
    check("text with other characters encodes", buf != NULL);
    if (buf)
    {
        tag = 0;
        for (i = 0; i + 1 < size; i++) if (buf[i] == 0x06 && buf[i + 1] == 0x03) { tag = buf[i + 5]; break; }
        check("other text becomes a BMPString (0x1e)", tag == 0x1e);
        LocalFree(buf);
    }

    /* one value, no name: still no inferring */
    memset(&nv, 0, sizeof(nv));
    nv.dwValueType = CERT_RDN_ANY_TYPE;
    nv.Value.pbData = (BYTE *)L"x"; nv.Value.cbData = 0;
    SetLastError(0xdeadbeef);
    ret = CryptEncodeObjectEx(X509_ASN_ENCODING, X509_UNICODE_NAME_VALUE, &nv, CRYPT_ENCODE_ALLOC_FLAG, NULL, &vbuf, &vsize);
    check("X509_UNICODE_NAME_VALUE with ANY_TYPE: not a character string", !ret && GetLastError() == CRYPT_E_NOT_CHAR_STRING);

    /* a registry store on a predefined key */
    SetLastError(0xdeadbeef);
    store = CertOpenStore(CERT_STORE_PROV_REG, 0, 0, 0, HKEY_CURRENT_USER);
    check("CertOpenStore(CERT_STORE_PROV_REG, HKEY_CURRENT_USER)", store != NULL);
    if (store) CertCloseStore(store, 0);
    store = CertOpenStore(CERT_STORE_PROV_REG, 0, 0, CERT_STORE_READONLY_FLAG, HKEY_CURRENT_USER);
    check("and read-only", store != NULL);
    if (store) CertCloseStore(store, 0);
    SetLastError(0xdeadbeef);
    store = CertOpenStore(CERT_STORE_PROV_REG, 0, 0, 0, (HKEY)(ULONG_PTR)0xdeadbeef);
    check("a bogus key is still refused", store == NULL);

    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
