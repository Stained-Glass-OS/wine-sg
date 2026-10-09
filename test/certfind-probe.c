/* Certificate finds and stores (patches/sg/1693), run by test/certfind-gate.sh:
 * CertFindCertificateInStore by enhanced key usage (and its flags),
 * property, key spec, subject attribute, thumbprint string, private key and
 * public key MD5; CertFindCTLInStore by usage and subject and
 * CertFindSubjectInCTL; physical stores from CertOpenStore; CertGetNameString's UPN; bcrypt's invalid flags. These were
 * FIXMEs ("find type unimplemented", "unimplemented type") and stubs. */
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdio.h>

#ifndef CERT_FIND_HASH_STR
#define CERT_FIND_HASH_STR (20 << CERT_COMPARE_SHIFT)
#endif
#ifndef CERT_FIND_HAS_PRIVATE_KEY
#define CERT_FIND_HAS_PRIVATE_KEY (21 << CERT_COMPARE_SHIFT)
#endif
#ifndef CERT_STORE_PROV_PKCS12
#define CERT_STORE_PROV_PKCS12 ((LPCSTR)17)
#endif
#ifndef CTL_CERT_SUBJECT_TYPE
#define CTL_CERT_SUBJECT_TYPE 2
PCTL_ENTRY WINAPI CertFindSubjectInCTL(DWORD, DWORD, void *, PCCTL_CONTEXT, DWORD);
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static PCCERT_CONTEXT make_cert(const WCHAR *cn, const char *eku, BOOL upn, HCRYPTPROV prov, CRYPT_KEY_PROV_INFO *info)
{
    BYTE name_buf[256], eku_buf[128], san_buf[256], upn_buf[128];
    CERT_NAME_BLOB name = { sizeof(name_buf), name_buf };
    CERT_EXTENSION ext[2];
    CERT_EXTENSIONS exts = { 0, ext };
    DWORD size;

    CertStrToNameW(X509_ASN_ENCODING, cn, CERT_X500_NAME_STR, NULL, name_buf, &name.cbData, NULL);
    if (eku)
    {
        LPSTR ids[1] = { (LPSTR)eku };
        CERT_ENHKEY_USAGE usage = { 1, ids };
        size = sizeof(eku_buf);
        CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, eku_buf, &size);
        ext[exts.cExtension].pszObjId = (LPSTR)szOID_ENHANCED_KEY_USAGE;
        ext[exts.cExtension].fCritical = FALSE;
        ext[exts.cExtension].Value.cbData = size;
        ext[exts.cExtension].Value.pbData = eku_buf;
        exts.cExtension++;
    }
    if (upn)
    {
        CERT_NAME_VALUE value = { CERT_RDN_UTF8_STRING };
        CERT_OTHER_NAME other;
        CERT_ALT_NAME_ENTRY entry;
        CERT_ALT_NAME_INFO alt = { 1, &entry };

        value.Value.pbData = (BYTE *)L"probe@sg.example";
        value.Value.cbData = 0;
        size = sizeof(upn_buf);
        CryptEncodeObject(X509_ASN_ENCODING, X509_UNICODE_ANY_STRING, &value, upn_buf, &size);
        other.pszObjId = (LPSTR)szOID_NT_PRINCIPAL_NAME;
        other.Value.cbData = size;
        other.Value.pbData = upn_buf;
        entry.dwAltNameChoice = CERT_ALT_NAME_OTHER_NAME;
        entry.pOtherName = &other;
        size = sizeof(san_buf);
        CryptEncodeObject(X509_ASN_ENCODING, X509_ALTERNATE_NAME, &alt, san_buf, &size);
        ext[exts.cExtension].pszObjId = (LPSTR)szOID_SUBJECT_ALT_NAME2;
        ext[exts.cExtension].fCritical = FALSE;
        ext[exts.cExtension].Value.cbData = size;
        ext[exts.cExtension].Value.pbData = san_buf;
        exts.cExtension++;
    }
    return CertCreateSelfSignCertificate(prov, &name, 0, info, NULL, NULL, NULL, exts.cExtension ? &exts : NULL);
}

int main(void)
{
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL), phys, p12;
    PCCERT_CONTEXT client, server, plain, found, added = NULL;
    CRYPT_KEY_PROV_INFO info = { 0 };
    HCRYPTPROV prov;
    WCHAR container[64], hashstr[64], upn[64];
    BYTE hash[20];
    DWORD size = sizeof(hash), i, keyspec = AT_KEYEXCHANGE, propid = CERT_FRIENDLY_NAME_PROP_ID;
    LPSTR client_ids[1] = { (LPSTR)szOID_PKIX_KP_CLIENT_AUTH }, both_ids[2] = { (LPSTR)szOID_PKIX_KP_CLIENT_AUTH,
        (LPSTR)szOID_PKIX_KP_SERVER_AUTH };
    CERT_ENHKEY_USAGE want_client = { 1, client_ids }, want_both = { 2, both_ids };
    CRYPT_DATA_BLOB friendly = { 10, (BYTE *)L"Probe" }, pfx = { 0 };
    CERT_RDN_ATTR attr = { (LPSTR)szOID_COMMON_NAME, CERT_RDN_ANY_TYPE };
    CERT_RDN rdn = { 1, &attr };
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE h;
    int n;

    swprintf(container, 64, L"sg-certfind-%lu", GetCurrentProcessId());
    CryptAcquireContextW(&prov, container, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET);
    {
        HCRYPTKEY k;
        CryptGenKey(prov, AT_KEYEXCHANGE, 1024 << 16 | CRYPT_EXPORTABLE, &k);
        CryptDestroyKey(k);
    }
    info.pwszContainerName = container;
    info.dwProvType = PROV_RSA_FULL;
    info.dwKeySpec = AT_KEYEXCHANGE;
    client = make_cert(L"CN=SG client", szOID_PKIX_KP_CLIENT_AUTH, TRUE, prov, &info);
    server = make_cert(L"CN=SG server", szOID_PKIX_KP_SERVER_AUTH, FALSE, prov, &info);
    plain = make_cert(L"CN=SG plain", NULL, FALSE, prov, &info);
    check(client && server && plain, "three certificates");
    if (!client || !server || !plain) goto done;
    CertAddCertificateContextToStore(store, client, CERT_STORE_ADD_ALWAYS, &added);
    CertSetCertificateContextProperty(added, CERT_KEY_PROV_INFO_PROP_ID, 0, &info);
    CertSetCertificateContextProperty(added, CERT_FRIENDLY_NAME_PROP_ID, 0, &friendly);
    CertFreeCertificateContext(added);
    /* only the client keeps its key */
    CertAddCertificateContextToStore(store, server, CERT_STORE_ADD_ALWAYS, &added);
    CertSetCertificateContextProperty(added, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL);
    CertFreeCertificateContext(added);
    CertAddCertificateContextToStore(store, plain, CERT_STORE_ADD_ALWAYS, &added);
    CertSetCertificateContextProperty(added, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL);
    CertFreeCertificateContext(added);

#define COUNT(type, para, flags) \
    for (n = 0, found = NULL; (found = CertFindCertificateInStore(store, X509_ASN_ENCODING, flags, type, para, found)); n++) ;

    COUNT(CERT_FIND_ENHKEY_USAGE, &want_client, 0);
    check(n == 1, "ENHKEY_USAGE client auth: the client certificate");
    COUNT(CERT_FIND_ENHKEY_USAGE, &want_client, CERT_FIND_OPTIONAL_ENHKEY_USAGE_FLAG);
    check(n == 2, "and with OPTIONAL: the one with no usage too");
    COUNT(CERT_FIND_ENHKEY_USAGE, &want_both, CERT_FIND_OR_ENHKEY_USAGE_FLAG);
    check(n == 2, "client or server (OR): two");
    COUNT(CERT_FIND_ENHKEY_USAGE, &want_both, 0);
    check(n == 0, "client and server: none");
    COUNT(CERT_FIND_ENHKEY_USAGE, NULL, CERT_FIND_NO_ENHKEY_USAGE_FLAG);
    check(n == 1, "NO_ENHKEY_USAGE: the plain one");
    COUNT(CERT_FIND_ENHKEY_USAGE, NULL, 0);
    check(n == 2, "any usage: two");
    COUNT(CERT_FIND_PROPERTY, &propid, 0);
    check(n == 1, "PROPERTY (friendly name)");
    COUNT(CERT_FIND_KEY_SPEC, &keyspec, 0);
    if (n != 1) printf("      %d\n", n);
    check(n == 1, "KEY_SPEC AT_KEYEXCHANGE");
    COUNT(CERT_FIND_HAS_PRIVATE_KEY, NULL, 0);
    if (n != 1) printf("      %d\n", n);
    check(n == 1, "HAS_PRIVATE_KEY");
    attr.Value.pbData = (BYTE *)L"SG server";
    attr.Value.cbData = lstrlenW(L"SG server") * sizeof(WCHAR);
    COUNT(CERT_FIND_SUBJECT_ATTR, &rdn, CERT_UNICODE_IS_RDN_ATTRS_FLAG);
    check(n == 1, "SUBJECT_ATTR CN=SG server");
    CertGetCertificateContextProperty(server, CERT_SHA1_HASH_PROP_ID, hash, &size);
    for (i = 0; i < 20; i++) swprintf(hashstr + i * 3, 4, L"%02X ", hash[i]);
    found = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_HASH_STR, hashstr, NULL);
    check(found && CertCompareCertificate(X509_ASN_ENCODING, found->pCertInfo, server->pCertInfo),
          "HASH_STR: the thumbprint with spaces");
    if (found) CertFreeCertificateContext(found);
    {
        BYTE md5[16];
        CRYPT_HASH_BLOB blob = { 16, md5 };
        DWORD md5size = 16;
        CryptHashCertificate(0, CALG_MD5, 0, plain->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData,
                             plain->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData, md5, &md5size);
        COUNT(CERT_FIND_PUBKEY_MD5_HASH, &blob, 0);
        check(n == 3, "PUBKEY_MD5_HASH: the shared key's three");
    }

    /* UPN */
    check(CertGetNameStringW(client, CERT_NAME_UPN_TYPE, 0, NULL, upn, 64) > 1 && !lstrcmpW(upn, L"probe@sg.example"),
          "CertGetNameString(CERT_NAME_UPN_TYPE)");

    /* a CTL of the client certificate's thumbprint */
    {
        CTL_INFO ctl = { 0 };
        CTL_ENTRY entry = { 0 };
        LPSTR usages[1] = { (LPSTR)szOID_PKIX_KP_CLIENT_AUTH };
        CMSG_SIGNED_ENCODE_INFO sign = { sizeof(sign) };
        BYTE chash[20], *encoded = NULL;
        DWORD csize = sizeof(chash), esize = 0;
        PCCTL_CONTEXT cctx, cfound;
        CTL_FIND_USAGE_PARA upara = { sizeof(upara) };
        CTL_FIND_SUBJECT_PARA spara = { sizeof(spara) };

        CertGetCertificateContextProperty(client, CERT_SHA1_HASH_PROP_ID, chash, &csize);
        entry.SubjectIdentifier.cbData = 20;
        entry.SubjectIdentifier.pbData = chash;
        ctl.dwVersion = CTL_V1;
        ctl.SubjectUsage.cUsageIdentifier = 1;
        ctl.SubjectUsage.rgpszUsageIdentifier = usages;
        GetSystemTimeAsFileTime(&ctl.ThisUpdate);
        ctl.SubjectAlgorithm.pszObjId = (LPSTR)szOID_OIWSEC_sha1;
        ctl.cCTLEntry = 1;
        ctl.rgCTLEntry = &entry;
        CryptMsgEncodeAndSignCTL(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, &ctl, &sign, 0, NULL, &esize);
        encoded = HeapAlloc(GetProcessHeap(), 0, esize);
        if (encoded && CryptMsgEncodeAndSignCTL(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, &ctl, &sign, 0, encoded, &esize) &&
            (cctx = CertCreateCTLContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, encoded, esize)))
        {
            HCERTSTORE cstore = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);

            check(CertFindSubjectInCTL(X509_ASN_ENCODING, CTL_CERT_SUBJECT_TYPE, (void *)client, cctx, 0) != NULL,
                  "CertFindSubjectInCTL: the client (was a spec stub)");
            check(!CertFindSubjectInCTL(X509_ASN_ENCODING, CTL_CERT_SUBJECT_TYPE, (void *)server, cctx, 0),
                  "not the server");
            CertAddCTLContextToStore(cstore, cctx, CERT_STORE_ADD_ALWAYS, NULL);
            upara.SubjectUsage.cUsageIdentifier = 1;
            upara.SubjectUsage.rgpszUsageIdentifier = usages;
            upara.ListIdentifier.cbData = CTL_FIND_NO_LIST_ID_CBDATA;
            cfound = CertFindCTLInStore(cstore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CTL_FIND_USAGE, &upara, NULL);
            check(cfound != NULL, "CertFindCTLInStore(CTL_FIND_USAGE)");
            if (cfound) CertFreeCTLContext(cfound);
            spara.pUsagePara = &upara;
            spara.dwSubjectType = CTL_CERT_SUBJECT_TYPE;
            spara.pvSubject = (void *)server;
            cfound = CertFindCTLInStore(cstore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CTL_FIND_SUBJECT, &spara, NULL);
            check(!cfound, "CTL_FIND_SUBJECT for the server: none");
            spara.pvSubject = (void *)client;
            cfound = CertFindCTLInStore(cstore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CTL_FIND_SUBJECT, &spara, NULL);
            check(cfound != NULL, "for the client: the CTL");
            if (cfound) CertFreeCTLContext(cfound);
            CertFreeCTLContext(cctx);
            CertCloseStore(cstore, 0);
        }
        else check(0, "a CTL");
        HeapFree(GetProcessHeap(), 0, encoded);
    }

    /* stores */
    phys = CertOpenStore(CERT_STORE_PROV_PHYSICAL_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"My\\.Default");
    check(phys != NULL, "CertOpenStore(CERT_STORE_PROV_PHYSICAL_W, \"My\\.Default\") (was a stub)");
    if (phys) CertCloseStore(phys, 0);
    phys = CertOpenStore(sz_CERT_STORE_PROV_PHYSICAL, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG,
                         L"Root\\.LocalMachine");
    check(phys != NULL, "\"Physical\", \"Root\\.LocalMachine\"");
    if (phys) CertCloseStore(phys, 0);

    /* bcrypt flags */
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    check(BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0x80) == STATUS_INVALID_PARAMETER,
          "BCryptCreateHash with an unknown flag: STATUS_INVALID_PARAMETER");
    check(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0x4000) == STATUS_INVALID_PARAMETER,
          "BCryptOpenAlgorithmProvider with an unknown flag: STATUS_INVALID_PARAMETER");

done:
    CryptReleaseContext(prov, 0);
    CryptAcquireContextW(&prov, container, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
