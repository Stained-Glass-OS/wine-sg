/* bcryptkdf-probe.c (wine-sg 6088): the key derivation providers -- PBKDF2, HKDF, SP800_56A_CONCAT,
 * SP800_108_CTR_HMAC -- through BCryptGenerateSymmetricKey + BCryptKeyDerivation, and BCryptDeriveKey's
 * SP800_56A_CONCAT over an ECDH P-256 agreement. Prints KEY=hex lines; the gate computes the expected ones. */
#define NTDDI_VERSION 0x0A000006
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

#ifndef BCRYPT_HKDF_HASH_ALGORITHM
#define BCRYPT_HKDF_HASH_ALGORITHM L"HkdfHashAlgorithm"
#endif
#ifndef BCRYPT_HKDF_SALT_AND_FINALIZE
#define BCRYPT_HKDF_SALT_AND_FINALIZE L"HkdfSaltAndFinalize"
#endif
#ifndef BCRYPT_PBKDF2_ALG_HANDLE
#define BCRYPT_PBKDF2_ALG_HANDLE ((BCRYPT_ALG_HANDLE)0x00000331)
#endif

static void hex(const char *key, const UCHAR *buf, ULONG len, NTSTATUS status)
{
    ULONG i;
    printf("%s=", key);
    if (status) printf("error %#lx", status);
    else for (i = 0; i < len; i++) printf("%02x", buf[i]);
    printf("\n");
}

static NTSTATUS derive(const WCHAR *alg_name, const UCHAR *secret, ULONG secret_len, BCryptBuffer *params,
                       ULONG count, UCHAR *out, ULONG out_len, const WCHAR *hkdf_hash, const UCHAR *hkdf_salt,
                       ULONG hkdf_salt_len)
{
    BCryptBufferDesc desc = { BCRYPTBUFFER_VERSION, count, params };
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key;
    ULONG done = 0;
    NTSTATUS status;

    if ((status = BCryptOpenAlgorithmProvider(&alg, alg_name, NULL, 0))) return status;
    if (!(status = BCryptGenerateSymmetricKey(alg, &key, NULL, 0, (UCHAR *)secret, secret_len, 0)))
    {
        if (hkdf_hash)
        {
            status = BCryptSetProperty(key, BCRYPT_HKDF_HASH_ALGORITHM, (UCHAR *)hkdf_hash,
                                       (wcslen(hkdf_hash) + 1) * sizeof(WCHAR), 0);
            if (!status) status = BCryptSetProperty(key, BCRYPT_HKDF_SALT_AND_FINALIZE, (UCHAR *)hkdf_salt, hkdf_salt_len, 0);
        }
        if (!status) status = BCryptKeyDerivation(key, count ? &desc : NULL, out, out_len, &done, 0);
        if (!status && done != out_len) status = 0xdead;
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return status;
}

int main(void)
{
    static const WCHAR sha256[] = L"SHA256", sha512[] = L"SHA512", sha3[] = L"SHA3-256";
    ULONGLONG iterations = 4096;
    UCHAR out[96], raw[64], raw2[64], d1[48], d2[48];
    BCryptBuffer p[8];
    NTSTATUS status;
    BCRYPT_ALG_HANDLE ecdh;
    BCRYPT_KEY_HANDLE a, b, pub_a, pub_b;
    BCRYPT_SECRET_HANDLE sa, sb;
    UCHAR blob[200];
    ULONG len, len2;

    /* PBKDF2-HMAC-SHA256, RFC 7914's/8018's style vector */
    p[0] = (BCryptBuffer){ sizeof(sha256), KDF_HASH_ALGORITHM, (void *)sha256 };
    p[1] = (BCryptBuffer){ 4, KDF_SALT, (void *)"salt" };
    p[2] = (BCryptBuffer){ sizeof(iterations), KDF_ITERATION_COUNT, &iterations };
    status = derive(L"PBKDF2", (const UCHAR *)"password", 8, p, 3, out, 40, NULL, NULL, 0);
    hex("pbkdf2", out, 40, status);
    p[0] = (BCryptBuffer){ sizeof(sha3), KDF_HASH_ALGORITHM, (void *)sha3 };
    status = derive(L"PBKDF2", (const UCHAR *)"password", 8, p, 3, out, 32, NULL, NULL, 0);
    hex("pbkdf2_sha3", out, 32, status);

    /* HKDF-SHA256, RFC 5869 test case 1 */
    {
        UCHAR ikm[22], salt[13], info[10];
        int i;
        memset(ikm, 0x0b, sizeof(ikm));
        for (i = 0; i < 13; i++) salt[i] = i;
        for (i = 0; i < 10; i++) info[i] = 0xf0 + i;
        p[0] = (BCryptBuffer){ sizeof(info), KDF_HKDF_INFO, info };
        status = derive(L"HKDF", ikm, sizeof(ikm), p, 1, out, 42, sha256, salt, sizeof(salt));
        hex("hkdf", out, 42, status);
    }

    /* SP 800-56A concatenation KDF */
    p[0] = (BCryptBuffer){ sizeof(sha512), KDF_HASH_ALGORITHM, (void *)sha512 };
    p[1] = (BCryptBuffer){ 3, KDF_ALGORITHMID, (void *)"AES" };
    p[2] = (BCryptBuffer){ 5, KDF_PARTYUINFO, (void *)"Alice" };
    p[3] = (BCryptBuffer){ 3, KDF_PARTYVINFO, (void *)"Bob" };
    status = derive(L"SP800_56A_CONCAT", (const UCHAR *)"shared secret Z", 15, p, 4, out, 80, NULL, NULL, 0);
    hex("concat", out, 80, status);

    /* SP 800-108 counter mode, HMAC-SHA256 */
    p[0] = (BCryptBuffer){ sizeof(sha256), KDF_HASH_ALGORITHM, (void *)sha256 };
    p[1] = (BCryptBuffer){ 5, KDF_LABEL, (void *)"label" };
    p[2] = (BCryptBuffer){ 7, KDF_CONTEXT, (void *)"context" };
    status = derive(L"SP800_108_CTR_HMAC", (const UCHAR *)"key derivation key", 18, p, 3, out, 48, NULL, NULL, 0);
    hex("sp800108", out, 48, status);

    /* by pseudo-handle */
    {
        BCRYPT_KEY_HANDLE key;
        BCryptBufferDesc desc = { BCRYPTBUFFER_VERSION, 3, p };
        ULONG done;
        p[0] = (BCryptBuffer){ sizeof(sha256), KDF_HASH_ALGORITHM, (void *)sha256 };
        p[1] = (BCryptBuffer){ 4, KDF_SALT, (void *)"salt" };
        p[2] = (BCryptBuffer){ sizeof(iterations), KDF_ITERATION_COUNT, &iterations };
        status = BCryptGenerateSymmetricKey(BCRYPT_PBKDF2_ALG_HANDLE, &key, NULL, 0, (UCHAR *)"password", 8, 0);
        if (!status) status = BCryptKeyDerivation(key, &desc, out, 40, &done, 0);
        hex("pbkdf2_pseudo", out, 40, status);
        if (!status) BCryptDestroyKey(key);
    }

    /* an unknown hash, and HKDF without its salt step */
    p[0] = (BCryptBuffer){ sizeof(L"MD7"), KDF_HASH_ALGORITHM, (void *)L"MD7" };
    printf("bad_hash=%#lx\n", derive(L"PBKDF2", (const UCHAR *)"pw", 2, p, 1, out, 16, NULL, NULL, 0));
    printf("hkdf_unfinalized=%#lx\n", derive(L"HKDF", (const UCHAR *)"pw", 2, NULL, 0, out, 16, NULL, NULL, 0));

    /* ECDH P-256 agreement, then BCryptDeriveKey(SP800_56A_CONCAT) on both sides */
    BCryptOpenAlgorithmProvider(&ecdh, BCRYPT_ECDH_P256_ALGORITHM, NULL, 0);
    BCryptGenerateKeyPair(ecdh, &a, 256, 0); BCryptFinalizeKeyPair(a, 0);
    BCryptGenerateKeyPair(ecdh, &b, 256, 0); BCryptFinalizeKeyPair(b, 0);
    BCryptExportKey(a, NULL, BCRYPT_ECCPUBLIC_BLOB, blob, sizeof(blob), &len, 0);
    BCryptImportKeyPair(ecdh, NULL, BCRYPT_ECCPUBLIC_BLOB, &pub_a, blob, len, 0);
    BCryptExportKey(b, NULL, BCRYPT_ECCPUBLIC_BLOB, blob, sizeof(blob), &len, 0);
    BCryptImportKeyPair(ecdh, NULL, BCRYPT_ECCPUBLIC_BLOB, &pub_b, blob, len, 0);
    BCryptSecretAgreement(a, pub_b, &sa, 0);
    BCryptSecretAgreement(b, pub_a, &sb, 0);
    {
        BCryptBufferDesc desc = { BCRYPTBUFFER_VERSION, 4, p };
        p[0] = (BCryptBuffer){ sizeof(sha256), KDF_HASH_ALGORITHM, (void *)sha256 };
        p[1] = (BCryptBuffer){ 3, KDF_ALGORITHMID, (void *)"AES" };
        p[2] = (BCryptBuffer){ 5, KDF_PARTYUINFO, (void *)"Alice" };
        p[3] = (BCryptBuffer){ 3, KDF_PARTYVINFO, (void *)"Bob" };
        status = BCryptDeriveKey(sa, BCRYPT_KDF_SP80056A_CONCAT, &desc, d1, 48, &len, 0);
        printf("ecdh_concat=%#lx len %lu\n", status, len);
        status = BCryptDeriveKey(sb, BCRYPT_KDF_SP80056A_CONCAT, &desc, d2, 48, &len2, 0);
        printf("ecdh_both=%d\n", !status && !memcmp(d1, d2, 48));
        BCryptDeriveKey(sa, BCRYPT_KDF_RAW_SECRET, NULL, raw, sizeof(raw), &len, 0);
        hex("ecdh_raw", raw, len, 0);
        hex("ecdh_kdf", d1, 48, 0);
        (void)raw2;
    }
    printf("done=1\n");
    return 0;
}
