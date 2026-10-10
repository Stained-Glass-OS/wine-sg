/* The remaining wintrust certificate helpers (patches/sg/2413), run by
 * test/wintrustfind-gate.sh: WTHelperCertFindIssuerCertificate (which issuer,
 * and how far to trust it), WTHelperCheckCertUsage, WTHelperGetAgencyInfo,
 * WTHelperIsInRootStore and WTHelperOpenKnownStores. Certificates are made
 * here with the CryptoAPI: a CA, an impostor with the CA's name and another key,
 * and children issued by the CA. */
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <stdio.h>
#include <string.h>

#ifndef CERT_E_CHAINING
#define CERT_E_CHAINING ((HRESULT)0x800B010A)
#define CERT_E_EXPIRED ((HRESULT)0x800B0101)
#define TRUST_E_CERT_SIGNATURE ((HRESULT)0x80096004)
#endif

typedef PCCERT_CONTEXT (WINAPI *find_issuer_t)(PCCERT_CONTEXT, DWORD, HCERTSTORE *, FILETIME *, DWORD, DWORD *, DWORD *);
typedef BOOL (WINAPI *usage_t)(PCCERT_CONTEXT, LPCSTR);
typedef BOOL (WINAPI *agency_t)(PCCERT_CONTEXT, DWORD *, SPC_SP_AGENCY_INFO *);
typedef BOOL (WINAPI *inroot_t)(DWORD, CERT_INFO *);
typedef BOOL (WINAPI *openstores_t)(CRYPT_PROVIDER_DATA *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void add_days(FILETIME *ft, int days)
{
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime; u.HighPart = ft->dwHighDateTime;
    u.QuadPart += (LONGLONG)days * 24 * 3600 * 10000000LL;
    ft->dwLowDateTime = u.LowPart; ft->dwHighDateTime = u.HighPart;
}

static BYTE *encode(LPCSTR type, const void *data, DWORD *len)
{
    BYTE *buf;
    if (!CryptEncodeObject(X509_ASN_ENCODING, type, data, NULL, len)) return NULL;
    buf = HeapAlloc(GetProcessHeap(), 0, *len);
    if (!CryptEncodeObject(X509_ASN_ENCODING, type, data, buf, len)) return NULL;
    return buf;
}

static CERT_NAME_BLOB make_name(const char *cn)
{
    CERT_NAME_BLOB n = {0};
    char subj[96];
    sprintf(subj, "CN=%s", cn);
    CertStrToNameA(X509_ASN_ENCODING, subj, 0, NULL, NULL, &n.cbData, NULL);
    n.pbData = HeapAlloc(GetProcessHeap(), 0, n.cbData);
    CertStrToNameA(X509_ASN_ENCODING, subj, 0, NULL, n.pbData, &n.cbData, NULL);
    return n;
}

struct spec
{
    const char *cn, *container;
    const char *issuer_cn;       /* NULL: self-issued */
    HCRYPTPROV signer;           /* the issuer's key; 0: its own */
    int nb_days, na_days;
    BYTE ski[20]; int has_ski;
    BYTE aki[20]; int has_aki;
    int ca;                      /* basic constraints, CA true */
    const char *eku;             /* one purpose OID, or NULL for none named */
    const WCHAR *agency;         /* policy display text, or NULL */
    BYTE serial;
};

static PCCERT_CONTEXT make_cert(struct spec *s, HCRYPTPROV *own)
{
    CERT_INFO info = {0};
    CRYPT_ALGORITHM_IDENTIFIER alg = { (char *)szOID_RSA_SHA1RSA, { 0, NULL } };
    CERT_EXTENSION ext[5];
    DWORD next = 0, len = 0, pklen = 0;
    CERT_PUBLIC_KEY_INFO *pk;
    CERT_NAME_BLOB subject = make_name(s->cn), issuer = s->issuer_cn ? make_name(s->issuer_cn) : subject;
    BYTE *enc;
    HCRYPTKEY key;
    FILETIME now;
    BYTE serial;
    PCCERT_CONTEXT ret = NULL;

    CryptAcquireContextA(own, s->container, MS_DEF_PROV_A, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    if (!CryptAcquireContextA(own, s->container, MS_DEF_PROV_A, PROV_RSA_FULL, CRYPT_NEWKEYSET)) return NULL;
    if (!CryptGenKey(*own, AT_KEYEXCHANGE, 1024 << 16, &key)) return NULL;
    CryptDestroyKey(key);
    CryptExportPublicKeyInfo(*own, AT_KEYEXCHANGE, X509_ASN_ENCODING, NULL, &pklen);
    pk = HeapAlloc(GetProcessHeap(), 0, pklen);
    CryptExportPublicKeyInfo(*own, AT_KEYEXCHANGE, X509_ASN_ENCODING, pk, &pklen);

    if (s->has_ski)
    {
        CRYPT_DATA_BLOB b = { 20, s->ski };
        ext[next].pszObjId = (char *)szOID_SUBJECT_KEY_IDENTIFIER; ext[next].fCritical = FALSE;
        ext[next].Value.pbData = encode(X509_OCTET_STRING, &b, &ext[next].Value.cbData); next++;
    }
    if (s->has_aki)
    {
        CERT_AUTHORITY_KEY_ID2_INFO a = {0};
        a.KeyId.cbData = 20; a.KeyId.pbData = s->aki;
        ext[next].pszObjId = (char *)szOID_AUTHORITY_KEY_IDENTIFIER2; ext[next].fCritical = FALSE;
        ext[next].Value.pbData = encode(X509_AUTHORITY_KEY_ID2, &a, &ext[next].Value.cbData); next++;
    }
    if (s->ca)
    {
        CERT_BASIC_CONSTRAINTS2_INFO bc = { TRUE, FALSE, 0 };
        ext[next].pszObjId = (char *)szOID_BASIC_CONSTRAINTS2; ext[next].fCritical = FALSE;
        ext[next].Value.pbData = encode(X509_BASIC_CONSTRAINTS2, &bc, &ext[next].Value.cbData); next++;
    }
    if (s->eku)
    {
        LPSTR oid = (LPSTR)s->eku;
        CERT_ENHKEY_USAGE u = { 1, &oid };
        ext[next].pszObjId = (char *)szOID_ENHANCED_KEY_USAGE; ext[next].fCritical = FALSE;
        ext[next].Value.pbData = encode(X509_ENHANCED_KEY_USAGE, &u, &ext[next].Value.cbData); next++;
    }
    if (s->agency)
    {
        SPC_SP_AGENCY_INFO ai = {0};
        ai.pwszPolicyDisplayText = (WCHAR *)s->agency;
        ext[next].pszObjId = (char *)SPC_SP_AGENCY_INFO_OBJID; ext[next].fCritical = FALSE;
        ext[next].Value.pbData = encode(SPC_SP_AGENCY_INFO_STRUCT, &ai, &ext[next].Value.cbData); next++;
    }

    GetSystemTimeAsFileTime(&now);
    info.dwVersion = CERT_V3;
    serial = s->serial;
    info.SerialNumber.cbData = 1; info.SerialNumber.pbData = &serial;
    info.SignatureAlgorithm = alg;
    info.Issuer = issuer;
    info.Subject = subject;
    info.NotBefore = now; add_days(&info.NotBefore, s->nb_days);
    info.NotAfter = now; add_days(&info.NotAfter, s->na_days);
    info.SubjectPublicKeyInfo = *pk;
    info.cExtension = next;
    info.rgExtension = ext;
    if (CryptSignAndEncodeCertificate(s->signer ? s->signer : *own, AT_KEYEXCHANGE, X509_ASN_ENCODING,
                                      X509_CERT_TO_BE_SIGNED, &info, &alg, NULL, NULL, &len))
    {
        enc = HeapAlloc(GetProcessHeap(), 0, len);
        if (CryptSignAndEncodeCertificate(s->signer ? s->signer : *own, AT_KEYEXCHANGE, X509_ASN_ENCODING,
                                          X509_CERT_TO_BE_SIGNED, &info, &alg, NULL, enc, &len))
            ret = CertCreateCertificateContext(X509_ASN_ENCODING, enc, len);
    }
    return ret;
}

static int same_cert(PCCERT_CONTEXT a, PCCERT_CONTEXT b)
{
    return a && b && a->cbCertEncoded == b->cbCertEncoded && !memcmp(a->pbCertEncoded, b->pbCertEncoded, a->cbCertEncoded);
}

static HCERTSTORE store_of(PCCERT_CONTEXT a, PCCERT_CONTEXT b)
{
    HCERTSTORE s = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
    if (a) CertAddCertificateContextToStore(s, a, CERT_STORE_ADD_ALWAYS, NULL);
    if (b) CertAddCertificateContextToStore(s, b, CERT_STORE_ADD_ALWAYS, NULL);
    return s;
}

static int nstores;
static HCERTSTORE added[16];
static BOOL WINAPI fake_add_store(CRYPT_PROVIDER_DATA *d, HCERTSTORE s)
{
    if (nstores < 16) added[nstores++] = CertDuplicateStore(s);
    return TRUE;
}

int main(void)
{
    HMODULE wt = LoadLibraryA("wintrust.dll");
    find_issuer_t find = (find_issuer_t)GetProcAddress(wt, "WTHelperCertFindIssuerCertificate");
    usage_t usage = (usage_t)GetProcAddress(wt, "WTHelperCheckCertUsage");
    agency_t agency = (agency_t)GetProcAddress(wt, "WTHelperGetAgencyInfo");
    inroot_t inroot = (inroot_t)GetProcAddress(wt, "WTHelperIsInRootStore");
    openstores_t openstores = (openstores_t)GetProcAddress(wt, "WTHelperOpenKnownStores");
    HCRYPTPROV pca, pimp, pc1, pc2, pc3;
    struct spec ca = { "SgFindCA", "sgfind-ca", NULL, 0, -100, 3650, {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}, 1, {0}, 0, 1, NULL, NULL, 1 };
    struct spec imp = { "SgFindCA", "sgfind-imp", NULL, 0, -100, 3650, {0}, 0, {0}, 0, 1, NULL, NULL, 2 };
    struct spec c1 = { "SgFindChild1", "sgfind-c1", "SgFindCA", 0, -10, 300, {0}, 0, {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}, 1, 0,
                       "1.3.6.1.5.5.7.3.3", L"Policy of SgFind", 3 };
    struct spec c2 = { "SgFindChild2", "sgfind-c2", "SgFindCA", 0, -10, 300, {0}, 0, {0}, 0, 0, NULL, NULL, 4 };
    struct spec c3 = { "SgFindChild3", "sgfind-c3", "SgFindCA", 0, -10, 4000, {0}, 0, {0}, 0, 0, NULL, NULL, 5 };
    PCCERT_CONTEXT cac, impc, child1, child2, child3, got;
    HCERTSTORE stores[3], empty, root;
    DWORD conf, err, size;
    FILETIME later;
    CRYPT_PROVIDER_DATA pd;
    CRYPT_PROVIDER_FUNCTIONS fns;
    SPC_SP_AGENCY_INFO *ai;
    BYTE agbuf[512];
    unsigned int i;
    const DWORD HIGH = CERT_CONFIDENCE_SIG | CERT_CONFIDENCE_TIME | CERT_CONFIDENCE_TIMENEST |
                       CERT_CONFIDENCE_AUTHIDEXT | CERT_CONFIDENCE_HYGIENE;

    check(find && usage && agency && inroot && openstores, "the five helpers are exported");
    if (!find || !usage || !agency || !inroot || !openstores) goto done;

    cac = make_cert(&ca, &pca);
    impc = make_cert(&imp, &pimp);
    c1.signer = pca; c2.signer = pca; c3.signer = pca;
    child1 = make_cert(&c1, &pc1);
    child2 = make_cert(&c2, &pc2);
    child3 = make_cert(&c3, &pc3);
    check(cac && impc && child1 && child2 && child3, "test certificates made");
    if (!cac || !impc || !child1 || !child2 || !child3) goto done;

    empty = store_of(NULL, NULL);

    /* WTHelperCertFindIssuerCertificate */
    stores[0] = empty; stores[1] = store_of(cac, NULL);
    conf = err = 0xdeadbeef;
    got = find(child1, 2, stores, NULL, X509_ASN_ENCODING, &conf, &err);
    check(same_cert(got, cac), "finds the issuer in the second of two stores");
    check(conf == HIGH && err == 0, "a good issuer has full confidence and no error");
    if (got) CertFreeCertificateContext(got);

    stores[0] = store_of(impc, cac); /* the impostor first */
    got = find(child1, 1, stores, NULL, X509_ASN_ENCODING, &conf, &err);
    check(same_cert(got, cac), "of the impostor (same name, other key) and the real issuer, the real one");
    if (got) CertFreeCertificateContext(got);

    stores[0] = store_of(impc, NULL);
    got = find(child2, 1, stores, NULL, X509_ASN_ENCODING, &conf, &err);
    check(same_cert(got, impc) && !(conf & CERT_CONFIDENCE_SIG) && err == (DWORD)TRUST_E_CERT_SIGNATURE,
          "only the impostor: returned, without SIG confidence, error TRUST_E_CERT_SIGNATURE");
    if (got) CertFreeCertificateContext(got);

    stores[0] = empty;
    SetLastError(0);
    got = find(child1, 1, stores, NULL, X509_ASN_ENCODING, &conf, &err);
    check(!got && conf == 0 && err == (DWORD)CERT_E_CHAINING && GetLastError() == (DWORD)CRYPT_E_NOT_FOUND,
          "no issuer anywhere: NULL, no confidence, CERT_E_CHAINING, CRYPT_E_NOT_FOUND");

    stores[0] = store_of(cac, NULL);
    GetSystemTimeAsFileTime(&later);
    add_days(&later, 4000);
    got = find(child2, 1, stores, &later, X509_ASN_ENCODING, &conf, &err);
    check(same_cert(got, cac) && !(conf & CERT_CONFIDENCE_TIME) && (conf & CERT_CONFIDENCE_SIG) &&
          err == (DWORD)CERT_E_EXPIRED, "verified as of a date after the issuer expires: no TIME confidence, CERT_E_EXPIRED");
    if (got) CertFreeCertificateContext(got);

    got = find(child3, 1, stores, NULL, X509_ASN_ENCODING, &conf, &err);
    check(same_cert(got, cac) && !(conf & CERT_CONFIDENCE_TIMENEST) && (conf & CERT_CONFIDENCE_SIG) && err != 0,
          "a child valid beyond its issuer: no TIMENEST confidence and an error");
    if (got) CertFreeCertificateContext(got);

    got = find(child2, 1, stores, NULL, X509_ASN_ENCODING, &conf, &err);
    check(same_cert(got, cac) && !(conf & CERT_CONFIDENCE_AUTHIDEXT) && (conf & CERT_CONFIDENCE_SIG) && err == 0,
          "a child without an authority key identifier: no AUTHIDEXT, still no error");
    if (got) CertFreeCertificateContext(got);

    SetLastError(0);
    got = find(NULL, 1, stores, NULL, X509_ASN_ENCODING, &conf, &err);
    check(!got && GetLastError() == ERROR_INVALID_PARAMETER, "no child: NULL, ERROR_INVALID_PARAMETER");

    /* WTHelperCheckCertUsage */
    check(usage(child1, "1.3.6.1.5.5.7.3.3"), "a certificate naming code signing may be used for it");
    SetLastError(0);
    check(!usage(child1, "1.3.6.1.5.5.7.3.1") && GetLastError() == CRYPT_E_NOT_FOUND,
          "and not for server authentication (CRYPT_E_NOT_FOUND)");
    check(usage(child2, "1.3.6.1.5.5.7.3.1") && usage(child2, "1.2.3.4"), "a certificate naming no purposes is good for any");
    SetLastError(0);
    check(!usage(NULL, "1.2.3") && GetLastError() == ERROR_INVALID_PARAMETER, "no certificate: ERROR_INVALID_PARAMETER");
    SetLastError(0);
    check(!usage(child1, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "no OID: ERROR_INVALID_PARAMETER");

    /* WTHelperGetAgencyInfo */
    size = 0;
    check(agency(child1, &size, NULL) && size > sizeof(SPC_SP_AGENCY_INFO), "agency info: the size is asked for");
    SetLastError(0);
    {
        DWORD small = 4;
        check(!agency(child1, &small, (SPC_SP_AGENCY_INFO *)agbuf) && GetLastError() == ERROR_MORE_DATA && small >= size,
              "a short buffer: ERROR_MORE_DATA and the size");
    }
    size = sizeof(agbuf);
    ai = (SPC_SP_AGENCY_INFO *)agbuf;
    check(agency(child1, &size, ai) && ai->pwszPolicyDisplayText && !wcscmp(ai->pwszPolicyDisplayText, L"Policy of SgFind"),
          "and the policy text comes back");
    SetLastError(0);
    size = sizeof(agbuf);
    check(!agency(child2, &size, ai) && GetLastError() == CRYPT_E_NOT_FOUND, "a certificate without agency info: CRYPT_E_NOT_FOUND");
    SetLastError(0);
    check(!agency(NULL, &size, ai) && GetLastError() == ERROR_INVALID_PARAMETER, "no certificate: ERROR_INVALID_PARAMETER");


    /* the agency info codec against hand-made DER: SEQUENCE { [0] url link, [1] text } and a logo link */
    {
        static const BYTE der[] = { 0x30, 0x14, 0xa0, 0x0a, 0x80, 0x08, 'h','t','t','p',':','/','/','x',
                                    0xa1, 0x06, 0x80, 0x04, 0x00, 0x48, 0x00, 0x69 };
        static const BYTE der2[] = { 0x30, 0x0c, 0xa3, 0x0a, 0x80, 0x08, 'h','t','t','p',':','/','/','y' };
        SPC_LINK link = {0}, logo = {0};
        SPC_SP_AGENCY_INFO in = {0}, *out;
        BYTE enc[128], dec[256];
        DWORD elen = sizeof(enc), dlen = sizeof(dec);

        link.dwLinkChoice = SPC_URL_LINK_CHOICE; link.pwszUrl = (WCHAR *)L"http://x";
        in.pPolicyInformation = &link; in.pwszPolicyDisplayText = (WCHAR *)L"Hi";
        check(CryptEncodeObject(X509_ASN_ENCODING, SPC_SP_AGENCY_INFO_STRUCT, &in, enc, &elen) &&
              elen == sizeof(der) && !memcmp(enc, der, elen), "agency info encodes link + text to the expected DER");
        out = (SPC_SP_AGENCY_INFO *)dec;
        check(CryptDecodeObject(X509_ASN_ENCODING, SPC_SP_AGENCY_INFO_STRUCT, der, sizeof(der), 0, dec, &dlen) &&
              out->pPolicyInformation && out->pPolicyInformation->dwLinkChoice == SPC_URL_LINK_CHOICE &&
              !wcscmp(out->pPolicyInformation->pwszUrl, L"http://x") && !wcscmp(out->pwszPolicyDisplayText, L"Hi") &&
              !out->pLogoImage && !out->pLogoLink, "and decodes back");
        memset(&in, 0, sizeof(in));
        logo.dwLinkChoice = SPC_URL_LINK_CHOICE; logo.pwszUrl = (WCHAR *)L"http://y";
        in.pLogoLink = &logo;
        elen = sizeof(enc);
        check(CryptEncodeObject(X509_ASN_ENCODING, SPC_SP_AGENCY_INFO_STRUCT, &in, enc, &elen) &&
              elen == sizeof(der2) && !memcmp(enc, der2, elen), "a logo link alone encodes as [3]");
        dlen = sizeof(dec);
        check(CryptDecodeObject(X509_ASN_ENCODING, SPC_SP_AGENCY_INFO_STRUCT, der2, sizeof(der2), 0, dec, &dlen) &&
              !out->pPolicyInformation && !out->pwszPolicyDisplayText && out->pLogoLink &&
              !wcscmp(out->pLogoLink->pwszUrl, L"http://y"), "and decodes back");
    }

    /* WTHelperIsInRootStore */
    root = CertOpenSystemStoreA(0, "ROOT");
    check(root != NULL, "the root store opens");
    if (root)
    {
        PCCERT_CONTEXT first = CertEnumCertificatesInStore(root, NULL), second;

        check(first != NULL, "and holds certificates");
        if (first)
        {
            check(inroot(X509_ASN_ENCODING, first->pCertInfo), "the first certificate of the root store is in it");
            second = CertEnumCertificatesInStore(root, first);  /* frees first */
            check(second && inroot(X509_ASN_ENCODING, second->pCertInfo), "and so is the second");
            if (second) CertFreeCertificateContext(second);
        }
        CertCloseStore(root, 0);
    }
    check(!inroot(X509_ASN_ENCODING, cac->pCertInfo), "a certificate that is not in the root store: not in it");
    check(!inroot(X509_ASN_ENCODING, impc->pCertInfo), "nor another, with the same name");
    check(!inroot(X509_ASN_ENCODING, child1->pCertInfo), "nor the child");
    SetLastError(0);
    check(!inroot(X509_ASN_ENCODING, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "no certificate: ERROR_INVALID_PARAMETER");

    /* WTHelperOpenKnownStores */
    memset(&pd, 0, sizeof(pd));
    memset(&fns, 0, sizeof(fns));
    fns.pfnAddStore2Chain = (PFN_CPD_ADD_STORE)fake_add_store; /* the SDK says WINAPI, mingw's header does not */
    pd.cbStruct = sizeof(pd);
    pd.psPfns = &fns;
    check(openstores(&pd) && nstores == 4, "the known stores are opened and added: Root, Trust, CA, My");
    for (i = 0; i < (unsigned)nstores; i++) check(added[i] != NULL, "each is a store");
    SetLastError(0);
    check(!openstores(NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "no provider data: ERROR_INVALID_PARAMETER");

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
