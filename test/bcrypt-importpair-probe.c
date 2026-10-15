/* Probe for patches/sg/5015: BCryptImportKeyPair ignores hImportKey ("This
 * parameter is not currently used and should be NULL"): a public key blob
 * imports with or without one, and the imported key verifies a signature.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key, other, imported;
    UCHAR blob[512], hash[32], sig[128];
    ULONG size, sig_len;
    unsigned int i;
    NTSTATUS st;

    st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, NULL, 0);
    check(!st, "open ECDSA_P256 %#lx", st);
    if (st) { printf("RESULT: FAIL\n"); return 1; }
    BCryptGenerateKeyPair(alg, &key, 256, 0);
    BCryptFinalizeKeyPair(key, 0);
    BCryptGenerateKeyPair(alg, &other, 256, 0);
    BCryptFinalizeKeyPair(other, 0);
    st = BCryptExportKey(key, NULL, BCRYPT_ECCPUBLIC_BLOB, blob, sizeof(blob), &size, 0);
    check(!st, "export the public key %#lx", st);
    for (i = 0; i < sizeof(hash); i++) hash[i] = i;
    st = BCryptSignHash(key, NULL, hash, sizeof(hash), sig, sizeof(sig), &sig_len, 0);
    check(!st, "sign %#lx", st);

    for (i = 0; i < 2; i++)
    {
        imported = NULL;
        st = BCryptImportKeyPair(alg, i ? other : NULL, BCRYPT_ECCPUBLIC_BLOB, &imported, blob, size, 0);
        check(!st && imported, "import with %s import key: %#lx", i ? "an" : "no", st);
        if (st) continue;
        st = BCryptVerifySignature(imported, NULL, hash, sizeof(hash), sig, sig_len, 0);
        check(!st, "the imported key (%s import key) verifies: %#lx", i ? "an" : "no", st);
        BCryptDestroyKey(imported);
    }
    BCryptDestroyKey(other);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
