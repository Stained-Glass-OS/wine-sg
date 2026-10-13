/* Probe for patches/sg/5026: BCryptGetProperty/BCryptSetProperty fail with
 * STATUS_NOT_SUPPORTED ("The named property ... is not supported") for a
 * property or chaining mode the object does not have, on algorithm, hash and
 * key handles; known properties still answer.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef STATUS_NOT_SUPPORTED
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#endif

int main(void)
{
    static const WCHAR *algs[] = {L"AES", L"3DES", L"RC4", L"RSA", L"DSA", L"SHA256", L"MD5", L"ECDSA_P256", L"RNG"};
    static UCHAR secret[24] = {1, 2, 3};
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key;
    BCRYPT_HASH_HANDLE hash;
    UCHAR buf[256];
    ULONG size, len;
    unsigned int i;
    NTSTATUS st;

    for (i = 0; i < ARRAYSIZE(algs); i++)
    {
        st = BCryptOpenAlgorithmProvider(&alg, algs[i], NULL, 0);
        if (st) { check(0, "%ls opens: %#lx", algs[i], st); continue; }
        st = BCryptGetProperty(alg, L"SgNoSuchProperty", buf, sizeof(buf), &size, 0);
        check(st == STATUS_NOT_SUPPORTED, "%ls algorithm, get an unknown property: %#lx", algs[i], st);
        st = BCryptSetProperty(alg, L"SgNoSuchProperty", buf, 4, 0);
        check(st == STATUS_NOT_SUPPORTED, "%ls algorithm, set an unknown property: %#lx", algs[i], st);
        BCryptCloseAlgorithmProvider(alg, 0);
    }

    BCryptOpenAlgorithmProvider(&alg, L"AES", NULL, 0);
    st = BCryptGetProperty(alg, BCRYPT_BLOCK_LENGTH, (UCHAR *)&len, sizeof(len), &size, 0);
    check(!st && len == 16, "AES block length still answers: %#lx %lu", st, len);
    st = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (UCHAR *)L"SgNoSuchMode", sizeof(L"SgNoSuchMode"), 0);
    check(st == STATUS_NOT_SUPPORTED, "AES algorithm, an unknown chaining mode: %#lx", st);
    BCryptGenerateSymmetricKey(alg, &key, NULL, 0, secret, 16, 0);
    st = BCryptGetProperty(key, L"SgNoSuchProperty", buf, sizeof(buf), &size, 0);
    check(st == STATUS_NOT_SUPPORTED, "AES key, get an unknown property: %#lx", st);
    st = BCryptSetProperty(key, BCRYPT_CHAINING_MODE, (UCHAR *)L"SgNoSuchMode", sizeof(L"SgNoSuchMode"), 0);
    check(st == STATUS_NOT_SUPPORTED, "AES key, an unknown chaining mode: %#lx", st);
    st = BCryptSetProperty(key, L"SgNoSuchProperty", buf, 4, 0);
    check(st == STATUS_NOT_SUPPORTED, "AES key, set an unknown property: %#lx", st);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);

    BCryptOpenAlgorithmProvider(&alg, L"RC4", NULL, 0);
    st = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (UCHAR *)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    check(st == STATUS_NOT_SUPPORTED, "RC4 algorithm, CBC chaining: %#lx", st);
    BCryptCloseAlgorithmProvider(alg, 0);

    BCryptOpenAlgorithmProvider(&alg, L"SHA256", NULL, 0);
    st = BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0);
    check(!st, "SHA256 hash: %#lx", st);
    st = BCryptGetProperty(hash, L"SgNoSuchProperty", buf, sizeof(buf), &size, 0);
    check(st == STATUS_NOT_SUPPORTED, "SHA256 hash, get an unknown property: %#lx", st);
    st = BCryptGetProperty(hash, BCRYPT_HASH_LENGTH, (UCHAR *)&len, sizeof(len), &size, 0);
    check(!st && len == 32, "SHA256 hash length still answers: %#lx %lu", st, len);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    BCryptOpenAlgorithmProvider(&alg, L"RSA", NULL, 0);
    BCryptGenerateKeyPair(alg, &key, 512, 0);
    BCryptFinalizeKeyPair(key, 0);
    st = BCryptGetProperty(key, L"SgNoSuchProperty", buf, sizeof(buf), &size, 0);
    check(st == STATUS_NOT_SUPPORTED, "RSA key, get an unknown property: %#lx", st);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
