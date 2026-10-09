/* Crypto odds and ends (patches/sg/1681), run by test/cryptrest-gate.sh:
 * raw RSA (BCRYPT_PAD_NONE, NCRYPT_NO_PADDING_FLAG) both ways;
 * BCryptGenRandom with the RNG pseudo-handle; CertFindChainInStore by
 * issuer, with a key, a usage and a callback. These were stubs. */
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <stdio.h>

#ifndef BCRYPT_RSA_ALG_HANDLE
#define BCRYPT_RSA_ALG_HANDLE ((BCRYPT_ALG_HANDLE)0x000000e1)
#define BCRYPT_RNG_ALG_HANDLE ((BCRYPT_ALG_HANDLE)0x00000081)
#endif

/* the whole structure, as the SDK has it (mingw's stops at pvFindArg) */
typedef struct
{
    DWORD cbSize;
    LPCSTR pszUsageIdentifier;
    DWORD dwKeySpec;
    DWORD dwAcquirePrivateKeyFlags;
    DWORD cIssuer;
    CERT_NAME_BLOB *rgIssuer;
    BOOL (WINAPI *pfnFindCallback)(PCCERT_CONTEXT, void *);
    void *pvFindArg;
    DWORD *pdwIssuerChainIndex;
    DWORD *pdwIssuerElementIndex;
} FIND_PARA;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int callback_calls, callback_answer = TRUE;
static BOOL WINAPI find_callback(PCCERT_CONTEXT cert, void *arg)
{
    callback_calls++;
    return callback_answer;
}

int main(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    BCRYPT_KEY_HANDLE bkey;
    BYTE plain[256], cipher[256], back[256], rnd[32] = {0};
    DWORD size, i;
    NTSTATUS st;

    /* raw RSA */
    NCryptOpenStorageProvider(&prov, NULL, 0);
    NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, NULL, 0, 0);
    size = 2048;
    NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&size, sizeof(size), 0);
    NCryptFinalizeKey(key, 0);
    for (i = 0; i < 256; i++) plain[i] = (BYTE)(i * 7 + 1);
    plain[0] = 0x12;
    check(!NCryptEncrypt(key, plain, 256, NULL, NULL, 0, &size, NCRYPT_NO_PADDING_FLAG) && size == 256,
          "raw RSA: the size, a block");
    check(!NCryptEncrypt(key, plain, 256, NULL, cipher, sizeof(cipher), &size, NCRYPT_NO_PADDING_FLAG) && size == 256 &&
          memcmp(cipher, plain, 256), "raw RSA encrypts");
    check(!NCryptDecrypt(key, cipher, 256, NULL, back, sizeof(back), &size, NCRYPT_NO_PADDING_FLAG) && size == 256 &&
          !memcmp(back, plain, 256), "and decrypts back");
    check(NCryptEncrypt(key, plain, 16, NULL, cipher, sizeof(cipher), &size, NCRYPT_NO_PADDING_FLAG) == NTE_INVALID_PARAMETER,
          "a part block: NTE_INVALID_PARAMETER");
    NCryptFreeObject(key);
    NCryptFreeObject(prov);

    /* the same through BCrypt */
    st = BCryptGenerateKeyPair(BCRYPT_RSA_ALG_HANDLE, &bkey, 1024, 0);
    if (!st) st = BCryptFinalizeKeyPair(bkey, 0);
    check(!st && !BCryptEncrypt(bkey, plain, 128, NULL, NULL, 0, cipher, sizeof(cipher), &size, BCRYPT_PAD_NONE) &&
          size == 128 && !BCryptDecrypt(bkey, cipher, 128, NULL, NULL, 0, back, sizeof(back), &size, BCRYPT_PAD_NONE) &&
          !memcmp(back, plain, 128), "BCRYPT_PAD_NONE both ways");
    BCryptDestroyKey(bkey);

    /* the RNG pseudo-handle */
    check(!BCryptGenRandom(BCRYPT_RNG_ALG_HANDLE, rnd, sizeof(rnd), 0), "BCryptGenRandom(BCRYPT_RNG_ALG_HANDLE)");
    for (i = 0, size = 0; i < sizeof(rnd); i++) size |= rnd[i];
    check(size != 0, "random bytes");
    check(BCryptGenRandom(BCRYPT_RNG_ALG_HANDLE, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == STATUS_INVALID_PARAMETER,
          "a handle and the system's generator: STATUS_INVALID_PARAMETER");

    /* chains by issuer */
    {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);
        CERT_NAME_BLOB subject = { 0 }, other = { 0 };
        BYTE name_buf[128], other_buf[128];
        PCCERT_CONTEXT cert, added = NULL;
        PCCERT_CHAIN_CONTEXT chain;
        FIND_PARA para = { sizeof(para) };
        DWORD chain_index = 99, element_index = 99;
        WCHAR container[64];
        HCRYPTPROV hprov;
        CRYPT_KEY_PROV_INFO info = { 0 };

        subject.cbData = sizeof(name_buf);
        subject.pbData = name_buf;
        CertStrToNameW(X509_ASN_ENCODING, L"CN=SG chain probe", CERT_X500_NAME_STR, NULL, name_buf, &subject.cbData, NULL);
        other.cbData = sizeof(other_buf);
        other.pbData = other_buf;
        CertStrToNameW(X509_ASN_ENCODING, L"CN=Somebody else", CERT_X500_NAME_STR, NULL, other_buf, &other.cbData, NULL);
        swprintf(container, 64, L"sg-chain-probe-%lu", GetCurrentProcessId());
        CryptAcquireContextW(&hprov, container, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET);
        {
            HCRYPTKEY k;
            CryptGenKey(hprov, AT_KEYEXCHANGE, 1024 << 16, &k);
            CryptDestroyKey(k);
        }
        info.pwszContainerName = container;
        info.dwProvType = PROV_RSA_FULL;
        info.dwKeySpec = AT_KEYEXCHANGE;
        cert = CertCreateSelfSignCertificate(hprov, &subject, 0, &info, NULL, NULL, NULL, NULL);
        check(cert != NULL, "a self-signed certificate with a key");
        if (cert)
        {
            CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_ALWAYS, &added);
            if (added) CertSetCertificateContextProperty(added, CERT_KEY_PROV_INFO_PROP_ID, 0, &info);
            para.cIssuer = 1;
            para.rgIssuer = &subject;
            para.pdwIssuerChainIndex = &chain_index;
            para.pdwIssuerElementIndex = &element_index;
            chain = CertFindChainInStore(store, X509_ASN_ENCODING, 0, CERT_CHAIN_FIND_BY_ISSUER, &para, NULL);
            check(chain && chain->rgpChain[0]->rgpElement[0]->pCertContext &&
                  CertCompareCertificate(X509_ASN_ENCODING, chain->rgpChain[0]->rgpElement[0]->pCertContext->pCertInfo,
                                         cert->pCertInfo), "CertFindChainInStore: the chain of the issuer's certificate");
            check(chain_index == 0 && element_index == 0, "and where the issuer is in it");
            chain = CertFindChainInStore(store, X509_ASN_ENCODING, 0, CERT_CHAIN_FIND_BY_ISSUER, &para, chain);
            check(!chain && GetLastError() == CRYPT_E_NOT_FOUND, "no more: CRYPT_E_NOT_FOUND");
            para.rgIssuer = &other;
            chain = CertFindChainInStore(store, X509_ASN_ENCODING, 0, CERT_CHAIN_FIND_BY_ISSUER, &para, NULL);
            check(!chain, "another issuer: none");
            para.rgIssuer = &subject;
            para.pfnFindCallback = find_callback;
            callback_answer = FALSE;
            chain = CertFindChainInStore(store, X509_ASN_ENCODING, 0, CERT_CHAIN_FIND_BY_ISSUER, &para, NULL);
            check(!chain && callback_calls == 1, "the callback turns it down");
            para.pfnFindCallback = NULL;
            para.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
            chain = CertFindChainInStore(store, X509_ASN_ENCODING, 0, CERT_CHAIN_FIND_BY_ISSUER, &para, NULL);
            check(chain != NULL, "for client authentication (no usage listed: all)");
            if (chain) CertFreeCertificateChain(chain);
            para.pszUsageIdentifier = NULL;
            /* without its key it is not found, unless no key is asked for */
            CertSetCertificateContextProperty(added, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL);
            chain = CertFindChainInStore(store, X509_ASN_ENCODING, 0, CERT_CHAIN_FIND_BY_ISSUER, &para, NULL);
            check(!chain, "no private key: none");
            chain = CertFindChainInStore(store, X509_ASN_ENCODING, CERT_CHAIN_FIND_BY_ISSUER_NO_KEY_FLAG,
                                         CERT_CHAIN_FIND_BY_ISSUER, &para, NULL);
            check(chain != NULL, "CERT_CHAIN_FIND_BY_ISSUER_NO_KEY_FLAG: found");
            if (chain) CertFreeCertificateChain(chain);
            CertFreeCertificateContext(added);
            CertFreeCertificateContext(cert);
        }
        CertCloseStore(store, 0);
        CryptReleaseContext(hprov, 0);
        CryptAcquireContextW(&hprov, container, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
