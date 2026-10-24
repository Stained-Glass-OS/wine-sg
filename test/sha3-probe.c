/* sha3-probe.c (wine-sg 6087): bcrypt's SHA3-256/384/512, plain and HMAC, by name and by pseudo-handle.
 * Prints KEY=hex lines; test/sha3-gate.sh has the FIPS 202 values (Python hashlib's). */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

static const WCHAR *names[3] = { L"SHA3-256", L"SHA3-384", L"SHA3-512" };
static const ULONG_PTR handles[3] = { 0x3a1, 0x3b1, 0x3c1 }, hmac_handles[3] = { 0x3d1, 0x3e1, 0x3f1 };
static const int bits[3] = { 256, 384, 512 };

static void put(const char *key, int n, const UCHAR *buf, ULONG len, NTSTATUS status)
{
    ULONG i;
    printf("%s_%d=", key, n);
    if (status) printf("error %#lx", status);
    else for (i = 0; i < len; i++) printf("%02x", buf[i]);
    printf("\n");
}

int main(void)
{
    static UCHAR long_input[1000];
    UCHAR out[64];
    ULONG len, size, block;
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE hash;
    NTSTATUS status;
    int i;
    ULONG off;

    memset(long_input, 'a', sizeof(long_input));
    for (i = 0; i < 3; i++)
    {
        len = bits[i] / 8;
        status = BCryptOpenAlgorithmProvider(&alg, names[i], NULL, 0);
        printf("open_%d=%#lx\n", bits[i], status);
        if (status) continue;
        size = 0;
        BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (UCHAR *)&size, sizeof(size), &block, 0);
        printf("length_%d=%lu\n", bits[i], size);
        BCryptGetProperty(alg, BCRYPT_HASH_BLOCK_LENGTH, (UCHAR *)&block, sizeof(block), &size, 0);
        printf("block_%d=%lu\n", bits[i], block);
        status = BCryptHash(alg, NULL, 0, (UCHAR *)"abc", 3, out, len);
        put("abc", bits[i], out, len, status);
        /* by pieces that straddle the sponge's rate */
        status = BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0);
        for (off = 0; !status && off < sizeof(long_input); off += 137)
            status = BCryptHashData(hash, long_input + off, min(137, sizeof(long_input) - off), 0);
        if (!status) status = BCryptFinishHash(hash, out, len, 0);
        put("long", bits[i], out, len, status);
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(alg, 0);

        status = BCryptHash((BCRYPT_ALG_HANDLE)handles[i], NULL, 0, (UCHAR *)"abc", 3, out, len);
        put("pseudo", bits[i], out, len, status);
        status = BCryptOpenAlgorithmProvider(&alg, names[i], NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (!status) status = BCryptHash(alg, (UCHAR *)"key", 3, (UCHAR *)"The quick brown fox", 19, out, len);
        put("hmac", bits[i], out, len, status);
        if (!status) BCryptCloseAlgorithmProvider(alg, 0);
        status = BCryptHash((BCRYPT_ALG_HANDLE)hmac_handles[i], (UCHAR *)"key", 3, (UCHAR *)"The quick brown fox", 19,
                            out, len);
        put("hmac_pseudo", bits[i], out, len, status);
    }
    printf("done=1\n");
    return 0;
}
