/* ncrypt: NCryptExportKey NCRYPT_PKCS8_PRIVATE_KEY_BLOB (patch 3721). Prints "P8 <label> <hex of DER>" and
 * "PUB <label> <hex>" lines; the gate feeds them to the host's openssl (key consistency, same public key). */
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void hex(const char *tag, const char *label, const BYTE *b, DWORD n)
{
    DWORD i;
    printf("%s %s ", tag, label);
    for (i = 0; i < n; i++) printf("%02X", b[i]);
    printf("\n");
}

static NCRYPT_KEY_HANDLE make_key(NCRYPT_PROV_HANDLE prov, const WCHAR *alg, DWORD policy, DWORD bits)
{
    NCRYPT_KEY_HANDLE key = 0;
    SECURITY_STATUS st = NCryptCreatePersistedKey(prov, &key, alg, NULL, 0, 0);
    if (st) { printf("FAIL create %ls %#lx\n", alg, st); f++; return 0; }
    if (bits) NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&bits, 4, 0);
    NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, (BYTE *)&policy, 4, 0);
    st = NCryptFinalizeKey(key, 0);
    if (st) { printf("FAIL finalize %ls %#lx\n", alg, st); f++; NCryptFreeObject(key); return 0; }
    return key;
}

static void try_key(NCRYPT_PROV_HANDLE prov, const WCHAR *alg, const char *label, DWORD bits, const WCHAR *pubtype)
{
    NCRYPT_KEY_HANDLE key = make_key(prov, alg, NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG | NCRYPT_ALLOW_EXPORT_FLAG, bits), nokey, wrap;
    BYTE *der, small[8], *pub;
    DWORD len = 0, len2 = 0;
    SECURITY_STATUS st;
    NCryptBuffer buf = { 4, NCRYPTBUFFER_PKCS_KEY_NAME, (void *)L"x" };
    NCryptBufferDesc desc = { NCRYPTBUFFER_VERSION, 1, &buf };

    if (!key) return;
    st = NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, NULL, NULL, 0, &len, 0);
    CHECK(st == 0 && len > 30, "%s size query %#lx %lu", label, st, len);
    der = malloc(len ? len : 1);
    st = NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, NULL, small, sizeof(small), &len2, 0);
    CHECK(st == NTE_BUFFER_TOO_SMALL && len2 == len, "%s small buffer %#lx %lu", label, st, len2);
    st = NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, NULL, der, len, &len2, 0);
    CHECK(st == 0 && len2 == len && der[0] == 0x30, "%s export %#lx", label, st);
    if (!st) hex("P8", label, der, len2);
    free(der);

    st = NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, &desc, NULL, 0, &len2, 0);
    if (!wcscmp(alg, BCRYPT_RSA_ALGORITHM))
        /* RSA PKCS8 export with parameters: superseded by 5621 at integration
         * (documented: NCRYPTBUFFER_PKCS_* parameters select the encrypted
         * PKCS8 form, which 5621 implements for RSA), so it now succeeds. */
        CHECK(st == 0 && len2 > 0, "%s parameters are accepted (5621) %#lx", label, st);
    else
        CHECK(st == NTE_NOT_SUPPORTED, "%s parameters are refused %#lx", label, st);
    st = NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, NULL, NULL, 0, &len2, 0x80);
    CHECK(st == NTE_BAD_FLAGS, "%s bad flags %#lx", label, st);

    st = NCryptExportKey(key, 0, pubtype, NULL, NULL, 0, &len, 0);
    pub = malloc(len);
    st = NCryptExportKey(key, 0, pubtype, NULL, pub, len, &len, 0);
    CHECK(st == 0, "%s public export %#lx", label, st);
    if (!st) hex("PUB", label, pub, len);
    free(pub);

    /* wrapped for another key */
    wrap = make_key(prov, BCRYPT_RSA_ALGORITHM, 0, 2048);
    if (wrap)
    {
        st = NCryptExportKey(key, wrap, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, NULL, NULL, 0, &len2, 0);
        CHECK(st == 0 && len2 > len, "%s wrapped size %#lx %lu", label, st, len2);
        NCryptFreeObject(wrap);
    }
    NCryptFreeObject(key);

    /* the policy that does not allow plaintext export */
    nokey = make_key(prov, alg, NCRYPT_ALLOW_EXPORT_FLAG, bits);
    if (nokey)
    {
        st = NCryptExportKey(nokey, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, NULL, NULL, 0, &len2, 0);
        CHECK(st == NTE_NOT_SUPPORTED, "%s plaintext export without the policy %#lx", label, st);
        NCryptFreeObject(nokey);
    }
}

int main(void)
{
    NCRYPT_PROV_HANDLE prov = 0;
    NCRYPT_KEY_HANDLE k;
    DWORD len;
    SECURITY_STATUS st = NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0);

    if (st) { printf("FAIL provider %#lx\nRESULT: FAIL\n", st); return 1; }
    try_key(prov, BCRYPT_RSA_ALGORITHM, "rsa2048", 2048, BCRYPT_RSAPUBLIC_BLOB);
    try_key(prov, BCRYPT_RSA_ALGORITHM, "rsa1024", 1024, BCRYPT_RSAPUBLIC_BLOB);
    try_key(prov, BCRYPT_ECDSA_P256_ALGORITHM, "p256", 0, BCRYPT_ECCPUBLIC_BLOB);
    try_key(prov, BCRYPT_ECDSA_P384_ALGORITHM, "p384", 0, BCRYPT_ECCPUBLIC_BLOB);
    try_key(prov, BCRYPT_ECDH_P256_ALGORITHM, "ecdh256", 0, BCRYPT_ECCPUBLIC_BLOB);
    k = make_key(prov, BCRYPT_DSA_ALGORITHM, NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG, 1024);
    if (k)
    {
        st = NCryptExportKey(k, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, NULL, NULL, 0, &len, 0);
        CHECK(st == NTE_NOT_SUPPORTED, "DSA is not exported as PKCS #8 here: %#lx", st);
        NCryptFreeObject(k);
    }
    NCryptFreeObject(prov);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
