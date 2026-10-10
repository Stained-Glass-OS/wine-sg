/* Probe for patches/sg/2443: bcrypt behaviours recorded from Windows: AES KEY_LENGTH is read-only, GCM with block padding,
 * a CFB size query, a hash used up by BCryptDeriveKeyCapi, a private DSA export from a public key. */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

#ifndef STATUS_NOT_SUPPORTED
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#endif
#ifndef STATUS_INVALID_BUFFER_SIZE
#define STATUS_INVALID_BUFFER_SIZE ((NTSTATUS)0xc0000206)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#endif
#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#endif

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }
static void checkst(const char *name, NTSTATUS got, NTSTATUS want)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%s (%08lx, want %08lx)", name, (unsigned long)got, (unsigned long)want);
    check(buf, got == want);
}

int main(void)
{
    BCRYPT_ALG_HANDLE aes = NULL, sha1 = NULL, dsa = NULL;
    BCRYPT_KEY_HANDLE key = NULL, dkey = NULL, dpub = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    UCHAR secret[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}, iv[16] = {0}, data[48] = {0}, out[64], nonce[12] = {0}, tag[16];
    UCHAR derived[20];
    ULONG size, keylen = 512;
    NTSTATUS st;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ai;

    st = BCryptOpenAlgorithmProvider(&aes, BCRYPT_AES_ALGORITHM, NULL, 0);
    checkst("open AES", st, 0);
    st = BCryptSetProperty(aes, BCRYPT_KEY_LENGTH, (UCHAR *)&keylen, sizeof(keylen), 0);
    checkst("AES KEY_LENGTH cannot be set", st, STATUS_NOT_SUPPORTED);

    /* GCM with block padding: the size is worked out as if it padded, then refused */
    st = BCryptSetProperty(aes, BCRYPT_CHAINING_MODE, (UCHAR *)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    checkst("GCM mode", st, 0);
    st = BCryptGenerateSymmetricKey(aes, &key, NULL, 0, secret, sizeof(secret), 0);
    checkst("GCM key", st, 0);
    BCRYPT_INIT_AUTH_MODE_INFO(ai);
    ai.pbNonce = nonce; ai.cbNonce = sizeof(nonce);
    ai.pbTag = tag; ai.cbTag = sizeof(tag);
    st = BCryptEncrypt(key, data, 32, &ai, iv, 16, out, 32, &size, BCRYPT_BLOCK_PADDING);
    checkst("GCM padding, output too small for the padded size", st, STATUS_BUFFER_TOO_SMALL);
    st = BCryptEncrypt(key, data, 32, &ai, iv, 16, out, 48, &size, BCRYPT_BLOCK_PADDING);
    checkst("GCM padding, output big enough: still invalid", st, STATUS_INVALID_PARAMETER);
    st = BCryptEncrypt(key, data, 32, &ai, NULL, 0, out, 32, &size, 0);
    checkst("GCM without padding works", st, 0);
    BCryptDestroyKey(key); key = NULL;

    /* size queries */
    st = BCryptSetProperty(aes, BCRYPT_CHAINING_MODE, (UCHAR *)BCRYPT_CHAIN_MODE_CFB, sizeof(BCRYPT_CHAIN_MODE_CFB), 0);
    checkst("CFB mode", st, 0);
    st = BCryptGenerateSymmetricKey(aes, &key, NULL, 0, secret, sizeof(secret), 0);
    size = 0;
    st = BCryptEncrypt(key, data, 17, NULL, iv, 16, NULL, 0, &size, 0);
    checkst("CFB size query for 17 bytes", st, 0);
    check("size is 17", size == 17);
    BCryptDestroyKey(key); key = NULL;
    st = BCryptSetProperty(aes, BCRYPT_CHAINING_MODE, (UCHAR *)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    st = BCryptGenerateSymmetricKey(aes, &key, NULL, 0, secret, sizeof(secret), 0);
    size = 0;
    st = BCryptEncrypt(key, data, 17, NULL, iv, 16, NULL, 0, &size, 0);
    checkst("CBC size query for 17 bytes stays invalid", st, STATUS_INVALID_BUFFER_SIZE);
    BCryptDestroyKey(key); key = NULL;
    BCryptCloseAlgorithmProvider(aes, 0);

    /* BCryptDeriveKeyCapi uses the hash up */
    st = BCryptOpenAlgorithmProvider(&sha1, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    st = BCryptCreateHash(sha1, &hash, NULL, 0, NULL, 0, 0);
    st = BCryptHashData(hash, (UCHAR *)"abc", 3, 0);
    st = BCryptDeriveKeyCapi(hash, NULL, derived, 20, 0);
    checkst("DeriveKeyCapi", st, 0);
    st = BCryptDeriveKeyCapi(hash, NULL, derived, 20, 0);
    checkst("again: invalid handle", st, STATUS_INVALID_HANDLE);
    st = BCryptHashData(hash, NULL, 0, 0);
    checkst("HashData: invalid handle", st, STATUS_INVALID_HANDLE);
    st = BCryptFinishHash(hash, derived, 20, 0);
    checkst("FinishHash: invalid handle", st, STATUS_INVALID_HANDLE);
    st = BCryptDestroyHash(hash);
    checkst("destroying it works", st, 0);
    BCryptCloseAlgorithmProvider(sha1, 0);

    /* a DSA key with only the public half has no private blob */
    st = BCryptOpenAlgorithmProvider(&dsa, BCRYPT_DSA_ALGORITHM, NULL, 0);
    checkst("open DSA", st, 0);
    st = BCryptGenerateKeyPair(dsa, &dkey, 512, 0);
    if (!st) st = BCryptFinalizeKeyPair(dkey, 0);
    if (st) printf("      (DSA generation: %08lx -- the DSA checks are skipped)\n", (unsigned long)st);
    else
    {
        UCHAR blob[1024];
        ULONG blen = 0;
        st = BCryptExportKey(dkey, NULL, BCRYPT_DSA_PUBLIC_BLOB, blob, sizeof(blob), &blen, 0);
        checkst("export the public blob", st, 0);
        st = BCryptImportKeyPair(dsa, NULL, BCRYPT_DSA_PUBLIC_BLOB, &dpub, blob, blen, 0);
        checkst("import it as a public key", st, 0);
        st = BCryptExportKey(dpub, NULL, BCRYPT_DSA_PRIVATE_BLOB, blob, sizeof(blob), &blen, 0);
        checkst("private export from the public key", st, STATUS_INVALID_PARAMETER);
        BCryptDestroyKey(dpub);
        BCryptDestroyKey(dkey);
    }
    BCryptCloseAlgorithmProvider(dsa, 0);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
