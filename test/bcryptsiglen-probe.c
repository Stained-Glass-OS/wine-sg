/* bcryptsiglen gate probe (wine-sg 8405): BCryptGetProperty(key, BCRYPT_SIGNATURE_LENGTH) of
 * signing keys, and that it is the size BCryptSignHash gives. Prints KEY=1 per check, then done=1. */
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <bcrypt.h>
#include <stdio.h>

static ULONG siglen(BCRYPT_KEY_HANDLE key, NTSTATUS *st)
{
    ULONG len = 0, got = 0;
    *st = BCryptGetProperty(key, BCRYPT_SIGNATURE_LENGTH, (UCHAR *)&len, sizeof(len), &got, 0);
    return *st || got != sizeof(len) ? 0 : len;
}

static ULONG signed_size(BCRYPT_KEY_HANDLE key, BOOL rsa)
{
    BCRYPT_PKCS1_PADDING_INFO pad = { BCRYPT_SHA256_ALGORITHM };
    UCHAR hash[32] = {1, 2, 3}, sig[1024];
    ULONG got = 0;
    if (BCryptSignHash(key, rsa ? &pad : NULL, hash, sizeof(hash), sig, sizeof(sig), &got, rsa ? BCRYPT_PAD_PKCS1 : 0)) return 0;
    return got;
}

static void keycheck(const char *name, const WCHAR *alg, ULONG bits, ULONG want, BOOL rsa)
{
    BCRYPT_ALG_HANDLE h;
    BCRYPT_KEY_HANDLE key;
    NTSTATUS st = 0;
    ULONG len = 0, sz = 0, small = 0;
    if (!BCryptOpenAlgorithmProvider(&h, alg, NULL, 0) && !BCryptGenerateKeyPair(h, &key, bits, 0) &&
        !BCryptFinalizeKeyPair(key, 0))
    {
        len = siglen(key, &st);
        sz = signed_size(key, rsa);
        /* a buffer too small for the DWORD */
        if (BCryptGetProperty(key, BCRYPT_SIGNATURE_LENGTH, (UCHAR *)&small, 2, &small, 0) == STATUS_BUFFER_TOO_SMALL) small = 1;
        BCryptDestroyKey(key);
    }
    printf("%s=%d (len %lu status %08lx signed %lu small %lu)\n", name, len == want && sz == want && small == 1,
           len, (unsigned long)st, sz, small);
}

int main(void)
{
    BCRYPT_ALG_HANDLE h;
    BCRYPT_KEY_HANDLE key;
    UCHAR secret[16] = {0};
    NTSTATUS st = 0;

    keycheck("rsa2048", BCRYPT_RSA_ALGORITHM, 2048, 256, TRUE);
    keycheck("rsa1024", BCRYPT_RSA_ALGORITHM, 1024, 128, TRUE);
    keycheck("ecdsa256", BCRYPT_ECDSA_P256_ALGORITHM, 256, 64, FALSE);
    keycheck("ecdsa384", BCRYPT_ECDSA_P384_ALGORITHM, 384, 96, FALSE);
    /* a symmetric key has no signatures */
    if (!BCryptOpenAlgorithmProvider(&h, BCRYPT_AES_ALGORITHM, NULL, 0) &&
        !BCryptGenerateSymmetricKey(h, &key, NULL, 0, secret, sizeof(secret), 0))
    {
        siglen(key, &st);
        BCryptDestroyKey(key);
    }
    printf("aes=%d (status %08lx)\n", st == STATUS_NOT_SUPPORTED, (unsigned long)st);
    printf("done=1\n");
    return 0;
}
