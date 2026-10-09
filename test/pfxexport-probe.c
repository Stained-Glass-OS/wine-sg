/* PFX export (patches/sg/1694), run by test/pfxexport-gate.sh: a certificate
 * and its exportable key go out with PFXExportCertStoreEx (a stub), with and
 * without a password; PFXVerifyPassword (a stub) tells the password;
 * PFXImportCertStore brings them back and the key signs what the
 * certificate's public key verifies; CertOpenStore(CERT_STORE_PROV_PKCS12)
 * opens a PFX with no password; REPORT_NO_PRIVATE_KEY refuses a certificate
 * without one. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

#ifndef CERT_STORE_PROV_PKCS12
#define CERT_STORE_PROV_PKCS12 ((LPCSTR)17)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BOOL export_store(HCERTSTORE store, const WCHAR *password, DWORD flags, CRYPT_DATA_BLOB *pfx)
{
    pfx->cbData = 0;
    pfx->pbData = NULL;
    if (!PFXExportCertStoreEx(store, pfx, password, NULL, flags)) return FALSE;
    pfx->pbData = HeapAlloc(GetProcessHeap(), 0, pfx->cbData + 64);
    pfx->cbData += 64;
    return PFXExportCertStoreEx(store, pfx, password, NULL, flags);
}

int main(void)
{
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL), back, p12, bare;
    CRYPT_KEY_PROV_INFO info = { 0 };
    BYTE name_buf[128], sig[256];
    CERT_NAME_BLOB name = { sizeof(name_buf), name_buf };
    CRYPT_DATA_BLOB pfx, pfx2, pfx3;
    PCCERT_CONTEXT cert, added, found = NULL;
    WCHAR container[64];
    HCRYPTPROV prov, key_prov;
    HCRYPTKEY key, pub;
    HCRYPTHASH hash;
    DWORD spec, sig_len = sizeof(sig);
    BOOL free_prov;
    int n;

    swprintf(container, 64, L"sg-pfxexport-%lu", GetCurrentProcessId());
    CryptAcquireContextW(&prov, container, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET);
    CryptGenKey(prov, AT_KEYEXCHANGE, 2048 << 16 | CRYPT_EXPORTABLE, &key);
    CryptDestroyKey(key);
    info.pwszContainerName = container;
    info.dwProvType = PROV_RSA_FULL;
    info.dwKeySpec = AT_KEYEXCHANGE;
    CertStrToNameW(X509_ASN_ENCODING, L"CN=SG PFX probe", CERT_X500_NAME_STR, NULL, name_buf, &name.cbData, NULL);
    cert = CertCreateSelfSignCertificate(prov, &name, 0, &info, NULL, NULL, NULL, NULL);
    check(cert != NULL, "a certificate with an exportable key");
    if (!cert) goto done;
    CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_ALWAYS, &added);
    CertSetCertificateContextProperty(added, CERT_KEY_PROV_INFO_PROP_ID, 0, &info);
    CertFreeCertificateContext(added);
    /* the key container is written when its context goes */
    CryptReleaseContext(prov, 0);
    CryptAcquireContextW(&prov, container, NULL, PROV_RSA_FULL, 0);

    check(export_store(store, L"secret", EXPORT_PRIVATE_KEYS | REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY, &pfx),
          "PFXExportCertStoreEx with the key (was a stub)");
    check(PFXIsPFXBlob(&pfx), "it is a PFX");
    check(PFXVerifyPassword(&pfx, L"secret", 0), "PFXVerifyPassword: the right one (was a stub)");
    check(!PFXVerifyPassword(&pfx, L"wrong", 0), "not a wrong one");

    back = PFXImportCertStore(&pfx, L"secret", CRYPT_EXPORTABLE);
    check(back != NULL, "PFXImportCertStore reads it back");
    for (n = 0, found = NULL; back && (found = CertEnumCertificatesInStore(back, found)); n++)
    {
        check(CertCompareCertificate(X509_ASN_ENCODING, found->pCertInfo, cert->pCertInfo), "the same certificate");
        if (CryptAcquireCertificatePrivateKey(found, 0, NULL, &key_prov, &spec, &free_prov))
        {
            static const BYTE data[] = "Stained Glass";
            CryptCreateHash(key_prov, CALG_SHA1, 0, 0, &hash);
            CryptHashData(hash, data, sizeof(data), 0);
            if (!CryptSignHashW(hash, spec, NULL, 0, sig, &sig_len)) printf("      sign %08lx spec %lu\n", GetLastError(), spec);
            else printf("PASS  its key signs\n");
            CryptDestroyHash(hash);
            CryptImportPublicKeyInfo(prov, X509_ASN_ENCODING, &cert->pCertInfo->SubjectPublicKeyInfo, &pub);
            CryptCreateHash(prov, CALG_SHA1, 0, 0, &hash);
            CryptHashData(hash, data, sizeof(data), 0);
            check(CryptVerifySignatureW(hash, sig, sig_len, pub, NULL, 0), "and the original public key verifies it");
            CryptDestroyHash(hash);
            CryptDestroyKey(pub);
            if (free_prov) CryptReleaseContext(key_prov, 0);
        }
        else check(0, "the imported key");
    }
    check(n == 1, "one certificate");
    if (back) CertCloseStore(back, 0);

    check(export_store(store, L"", EXPORT_PRIVATE_KEYS, &pfx2), "exported with no password");
    p12 = CertOpenStore(CERT_STORE_PROV_PKCS12, 0, 0, 0, &pfx2);
    for (n = 0, found = NULL; p12 && (found = CertEnumCertificatesInStore(p12, found)); n++) ;
    check(p12 && n == 1, "CertOpenStore(CERT_STORE_PROV_PKCS12) opens it");
    if (p12) CertCloseStore(p12, 0);

    bare = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);
    CertAddEncodedCertificateToStore(bare, X509_ASN_ENCODING, cert->pbCertEncoded, cert->cbCertEncoded,
                                     CERT_STORE_ADD_ALWAYS, NULL);
    check(!export_store(bare, L"x", EXPORT_PRIVATE_KEYS | REPORT_NO_PRIVATE_KEY, &pfx3),
          "REPORT_NO_PRIVATE_KEY: a certificate without one is refused");
    check(export_store(bare, L"x", 0, &pfx3), "without the flag: the certificate alone");
    CertCloseStore(bare, 0);

done:
    CryptReleaseContext(prov, 0);
    CryptAcquireContextW(&prov, container, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
