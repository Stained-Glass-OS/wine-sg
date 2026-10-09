/* The software key storage provider (patches/sg/1679), run by
 * test/ncryptksp-gate.sh: named keys made, kept, opened again, enumerated
 * and deleted; RSA decryption and raw RSA; ECDSA keys kept; ECDH secret
 * agreement and key derivation; AES keys; export policy; the providers
 * there are not; a certificate's CNG key through
 * CryptAcquireCertificatePrivateKey. These were stubs. */
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <stdio.h>

#ifndef MS_PLATFORM_CRYPTO_PROVIDER
#define MS_PLATFORM_CRYPTO_PROVIDER L"Microsoft Platform Crypto Provider"
#endif
#ifndef CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG
#define CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG 0x00010000
#endif
#ifndef CERT_NCRYPT_KEY_SPEC
#define CERT_NCRYPT_KEY_SPEC 0xFFFFFFFF
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const WCHAR KEYNAME[] = L"SG probe key";

int main(void)
{
    NCRYPT_PROV_HANDLE prov, other;
    NCRYPT_KEY_HANDLE key, key2, ec1, ec2;
    NCRYPT_SECRET_HANDLE s1, s2;
    NCryptProviderName *providers = NULL;
    NCryptAlgorithmName *algs = NULL;
    NCryptKeyName *name = NULL;
    BCRYPT_PKCS1_PADDING_INFO pad = { BCRYPT_SHA256_ALGORITHM };
    BYTE hash[32], sig[512], cipher[512], plain[512], d1[64], d2[64], blob[2048];
    DWORD count, size, size2, policy, i;
    void *state = NULL;
    SECURITY_STATUS ret;
    WCHAR buf[256];
    BOOL found;

    for (i = 0; i < sizeof(hash); i++) hash[i] = (BYTE)i;

    check(!NCryptOpenStorageProvider(&prov, NULL, 0), "the default provider");
    check(!NCryptOpenStorageProvider(&other, MS_KEY_STORAGE_PROVIDER, 0), "the software provider by name");
    NCryptFreeObject(other);
    ret = NCryptOpenStorageProvider(&other, MS_PLATFORM_CRYPTO_PROVIDER, 0);
    check(ret == NTE_DEVICE_NOT_FOUND, "no TPM: NTE_DEVICE_NOT_FOUND");
    check(!NCryptGetProperty(prov, NCRYPT_NAME_PROPERTY, (BYTE *)buf, sizeof(buf), &size, 0) &&
          !lstrcmpW(buf, MS_KEY_STORAGE_PROVIDER), "the provider's name");
    check(!NCryptEnumStorageProviders(&count, &providers, 0) && count >= 1 &&
          !lstrcmpW(providers[0].pszName, MS_KEY_STORAGE_PROVIDER), "NCryptEnumStorageProviders");
    NCryptFreeBuffer(providers);
    found = FALSE;
    if (!NCryptEnumAlgorithms(prov, NCRYPT_SIGNATURE_OPERATION, &count, &algs, 0))
        for (i = 0; i < count; i++) if (!lstrcmpW(algs[i].pszName, L"ECDSA_P256")) found = TRUE;
    check(found, "NCryptEnumAlgorithms: ECDSA_P256 signs");
    NCryptFreeBuffer(algs);

    /* a kept RSA key */
    key = 0;
    NCryptOpenKey(prov, &key, KEYNAME, 0, 0);
    if (key) NCryptDeleteKey(key, 0);
    check(!NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, KEYNAME, AT_KEYEXCHANGE, 0),
          "a named RSA key");
    size = 2048;
    NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&size, sizeof(size), 0);
    check(!NCryptFinalizeKey(key, 0), "finalized (and kept)");
    check(!NCryptGetProperty(key, NCRYPT_NAME_PROPERTY, (BYTE *)buf, sizeof(buf), &size, 0) && !lstrcmpW(buf, KEYNAME),
          "its name");
    check(!NCryptSignHash(key, &pad, hash, sizeof(hash), sig, sizeof(sig), &size, NCRYPT_PAD_PKCS1_FLAG | NCRYPT_SILENT_FLAG)
          && size == 256, "signs (silently)");
    check(!NCryptEncrypt(key, hash, sizeof(hash), NULL, cipher, sizeof(cipher), &size2, NCRYPT_PAD_PKCS1_FLAG),
          "encrypts");
    check(!NCryptDecrypt(key, cipher, size2, NULL, plain, sizeof(plain), &size2, NCRYPT_PAD_PKCS1_FLAG) &&
          size2 == sizeof(hash) && !memcmp(plain, hash, sizeof(hash)), "NCryptDecrypt gives it back");
    check(NCryptExportKey(key, 0, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, blob, sizeof(blob), &size2, 0) == NTE_NOT_SUPPORTED,
          "its private part does not leave (export policy 0)");
    check(!NCryptExportKey(key, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, blob, sizeof(blob), &size2, 0), "its public part does");
    check(NCryptIsKeyHandle(key) && !NCryptIsKeyHandle(prov), "NCryptIsKeyHandle");
    NCryptFreeObject(key);

    ret = NCryptCreatePersistedKey(prov, &key2, BCRYPT_RSA_ALGORITHM, KEYNAME, 0, 0);
    check(ret == NTE_EXISTS, "the name again: NTE_EXISTS");

    key = 0;
    check(!NCryptOpenKey(prov, &key, KEYNAME, 0, 0) && key, "NCryptOpenKey: the kept key");
    check(!NCryptVerifySignature(key, &pad, hash, sizeof(hash), sig, size, NCRYPT_PAD_PKCS1_FLAG),
          "verifies what it signed before");
    check(!NCryptGetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&size, sizeof(size), &size2, 0) && size == 2048,
          "its length kept");
    size2 = 0;
    check(!NCryptGetProperty(key, NCRYPT_LAST_MODIFIED_PROPERTY, NULL, 0, &size2, 0) && size2 == sizeof(FILETIME),
          "its last modified time");

    found = FALSE;
    while (!NCryptEnumKeys(prov, NULL, &name, &state, 0))
    {
        if (!lstrcmpW(name->pszName, KEYNAME) && !lstrcmpW(name->pszAlgid, L"RSA")) found = TRUE;
        NCryptFreeBuffer(name);
    }
    NCryptFreeBuffer(state);
    check(found, "NCryptEnumKeys lists it");

    check(!NCryptDeleteKey(key, 0), "NCryptDeleteKey");
    ret = NCryptOpenKey(prov, &key, KEYNAME, 0, 0);
    check(ret == NTE_BAD_KEYSET, "gone: NTE_BAD_KEYSET");

    /* a kept ECDSA key, exportable */
    check(!NCryptCreatePersistedKey(prov, &key, L"ECDSA_P256", L"SG probe ec", 0, NCRYPT_OVERWRITE_KEY_FLAG), "an ECDSA_P256 key");
    policy = NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG;
    NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, (BYTE *)&policy, sizeof(policy), 0);
    check(!NCryptFinalizeKey(key, 0), "finalized");
    check(!NCryptSignHash(key, NULL, hash, sizeof(hash), sig, sizeof(sig), &size, 0) && size == 64, "signs");
    NCryptFreeObject(key);
    key = 0;
    check(!NCryptOpenKey(prov, &key, L"SG probe ec", 0, 0) &&
          !NCryptVerifySignature(key, NULL, hash, sizeof(hash), sig, size, 0), "opened again, it verifies");
    check(!NCryptExportKey(key, 0, BCRYPT_ECCPRIVATE_BLOB, NULL, blob, sizeof(blob), &size2, 0),
          "exportable: its private part leaves");
    NCryptDeleteKey(key, 0);

    /* ECDH */
    NCryptCreatePersistedKey(prov, &ec1, L"ECDH_P256", NULL, 0, 0);
    NCryptCreatePersistedKey(prov, &ec2, L"ECDH_P256", NULL, 0, 0);
    NCryptFinalizeKey(ec1, 0);
    NCryptFinalizeKey(ec2, 0);
    check(!NCryptSecretAgreement(ec1, ec2, &s1, 0) && !NCryptSecretAgreement(ec2, ec1, &s2, 0), "NCryptSecretAgreement");
    check(!NCryptDeriveKey(s1, BCRYPT_KDF_HASH, NULL, d1, sizeof(d1), &size, 0) &&
          !NCryptDeriveKey(s2, BCRYPT_KDF_HASH, NULL, d2, sizeof(d2), &size2, 0) && size == size2 && size &&
          !memcmp(d1, d2, size), "NCryptDeriveKey: both sides agree");
    NCryptFreeObject(s1);
    NCryptFreeObject(s2);
    NCryptFreeObject(ec1);
    NCryptFreeObject(ec2);

    /* AES */
    check(!NCryptCreatePersistedKey(prov, &key, BCRYPT_AES_ALGORITHM, L"SG probe aes", 0, NCRYPT_OVERWRITE_KEY_FLAG) &&
          !NCryptFinalizeKey(key, 0), "a kept AES key");
    NCryptFreeObject(key);
    key = 0;
    check(!NCryptOpenKey(prov, &key, L"SG probe aes", 0, 0) &&
          !NCryptGetProperty(key, NCRYPT_ALGORITHM_PROPERTY, (BYTE *)buf, sizeof(buf), &size, 0) && !lstrcmpW(buf, L"AES"),
          "opened again");
    NCryptDeleteKey(key, 0);

    /* a machine key */
    check(!NCryptCreatePersistedKey(prov, &key, L"ECDSA_P384", L"SG probe machine", 0,
                                    NCRYPT_MACHINE_KEY_FLAG | NCRYPT_OVERWRITE_KEY_FLAG) && !NCryptFinalizeKey(key, 0),
          "a machine key");
    NCryptFreeObject(key);
    key = 0;
    ret = NCryptOpenKey(prov, &key, L"SG probe machine", 0, 0);
    check(ret == NTE_BAD_KEYSET, "not among the user's");
    key = 0;
    check(!NCryptOpenKey(prov, &key, L"SG probe machine", 0, NCRYPT_MACHINE_KEY_FLAG), "among the machine's");
    if (key) NCryptDeleteKey(key, 0);

    /* a certificate whose key is a CNG one */
    {
        CERT_NAME_BLOB subject = { 0 };
        CRYPT_KEY_PROV_INFO info = { 0 };
        PCCERT_CONTEXT cert;
        HCRYPTPROV_OR_NCRYPT_KEY_HANDLE handle = 0;
        DWORD spec = 0;
        BOOL free_it = FALSE;
        BYTE name_buf[128];

        NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, L"SG probe cert key", 0, NCRYPT_OVERWRITE_KEY_FLAG);
        NCryptFinalizeKey(key, 0);
        NCryptFreeObject(key);
        subject.cbData = sizeof(name_buf);
        subject.pbData = name_buf;
        CertStrToNameW(X509_ASN_ENCODING, L"CN=SG probe", CERT_X500_NAME_STR, NULL, name_buf, &subject.cbData, NULL);
        cert = CertCreateSelfSignCertificate(0, &subject, 0, NULL, NULL, NULL, NULL, NULL);
        check(cert != NULL, "a certificate");
        if (cert)
        {
            info.pwszContainerName = (WCHAR *)L"SG probe cert key";
            info.pwszProvName = (WCHAR *)MS_KEY_STORAGE_PROVIDER;
            info.dwProvType = 0;
            CertSetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, 0, &info);
            check(CryptAcquireCertificatePrivateKey(cert, CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG, NULL, &handle, &spec, &free_it) &&
                  spec == CERT_NCRYPT_KEY_SPEC && NCryptIsKeyHandle(handle) && free_it,
                  "CryptAcquireCertificatePrivateKey: the CNG key");
            if (handle && spec == CERT_NCRYPT_KEY_SPEC) NCryptFreeObject(handle);
            CertFreeCertificateContext(cert);
        }
        key = 0;
        NCryptOpenKey(prov, &key, L"SG probe cert key", 0, 0);
        if (key) NCryptDeleteKey(key, 0);
    }

    NCryptFreeObject(prov);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
