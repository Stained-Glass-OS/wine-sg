/* rooteku-probe: a root's enhanced key usage property limits its chains (patches/sg/7602).
 *   rooteku-probe ROOT.der LEAF.der prop|noprop
 * adds ROOT to the machine's Root store (with prop: allowed for Code Signing only, as Windows'
 * root program marks a code signing root), then builds LEAF's chain for server authentication
 * and for code signing and prints each chain's error status */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static BYTE *slurp(const char *path, DWORD *len)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *buf;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    *len = GetFileSize(f, NULL);
    buf = malloc(*len);
    ReadFile(f, buf, *len, len, NULL);
    CloseHandle(f);
    return buf;
}

static DWORD chain_status(PCCERT_CONTEXT leaf, HCERTSTORE extra, const char *oid)
{
    CERT_CHAIN_PARA para = { sizeof(para) };
    PCCERT_CHAIN_CONTEXT chain;
    LPSTR usage = (LPSTR)oid;
    DWORD st;
    para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    para.RequestedUsage.Usage.cUsageIdentifier = 1;
    para.RequestedUsage.Usage.rgpszUsageIdentifier = &usage;
    if (!CertGetCertificateChain(NULL, leaf, NULL, extra, &para, 0, NULL, &chain)) return 0xffffffff;
    st = chain->TrustStatus.dwErrorStatus;
    if (getenv("ROOTEKU_DEBUG"))
    {
        PCERT_SIMPLE_CHAIN simple = chain->rgpChain[0];
        DWORD e, size;
        for (e = 0; e < simple->cElement; e++)
        {
            size = 0;
            printf("DEBUG element %lu prop=%d size=%lu status=%08lx\n", e,
                   CertGetCertificateContextProperty(simple->rgpElement[e]->pCertContext, CERT_ENHKEY_USAGE_PROP_ID, NULL, &size),
                   size, simple->rgpElement[e]->TrustStatus.dwErrorStatus);
        }
    }
    CertFreeCertificateChain(chain);
    return st;
}

int main(int argc, char **argv)
{
    DWORD rlen, llen, plen = 0;
    BYTE *root = argc > 3 ? slurp(argv[1], &rlen) : NULL, *leaf = argc > 3 ? slurp(argv[2], &llen) : NULL, *pbuf;
    HCERTSTORE machine, extra;
    PCCERT_CONTEXT rctx, lctx;
    LPSTR cs = (LPSTR)szOID_PKIX_KP_CODE_SIGNING;
    CERT_ENHKEY_USAGE eku = { 1, &cs };
    CRYPT_DATA_BLOB blob;

    if (!root || !leaf) { printf("usage: rooteku-probe ROOT.der LEAF.der prop|noprop\n"); return 2; }
    machine = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, "Root");
    if (!machine || !CertAddEncodedCertificateToStore(machine, X509_ASN_ENCODING, root, rlen,
                                                      CERT_STORE_ADD_REPLACE_EXISTING, &rctx))
    { printf("ERROR root %lu\n", GetLastError()); return 1; }
    if (!strcmp(argv[3], "prop"))
    {
        CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eku, NULL, &plen);
        pbuf = malloc(plen);
        CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eku, pbuf, &plen);
        blob.cbData = plen; blob.pbData = pbuf;
        if (!CertSetCertificateContextProperty(rctx, CERT_ENHKEY_USAGE_PROP_ID, 0, &blob)) { printf("ERROR prop %lu\n", GetLastError()); return 1; }
    }
    else CertSetCertificateContextProperty(rctx, CERT_ENHKEY_USAGE_PROP_ID, 0, NULL);
    CertFreeCertificateContext(rctx);
    CertCloseStore(machine, 0);
    extra = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);
    CertAddEncodedCertificateToStore(extra, X509_ASN_ENCODING, leaf, llen, CERT_STORE_ADD_ALWAYS, &lctx);
    printf("SERVER %08lx\n", chain_status(lctx, extra, szOID_PKIX_KP_SERVER_AUTH));
    printf("CODE %08lx\n", chain_status(lctx, extra, szOID_PKIX_KP_CODE_SIGNING));
    return 0;
}
