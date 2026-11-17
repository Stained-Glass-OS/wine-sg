/* des3ecb-probe.c: 3DES in ECB mode through BCrypt (wine-sg 7819), for test/des3ecb-gate.sh.
 * Known answer from NIST SP 800-67's example: key 0123456789ABCDEF 23456789ABCDEF01 456789ABCDEF0123,
 * "The qufck brown fox jump" -> A826FD8CE53B855F CCE21C8112256FE6 68D5C05DD9B6B900 */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

static const UCHAR key[24] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef, 0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0x01,
                              0x45,0x67,0x89,0xab,0xcd,0xef,0x01,0x23};
static const UCHAR expect[24] = {0xa8,0x26,0xfd,0x8c,0xe5,0x3b,0x85,0x5f, 0xcc,0xe2,0x1c,0x81,0x12,0x25,0x6f,0xe6,
                                 0x68,0xd5,0xc0,0x5d,0xd9,0xb6,0xb9,0x00};

int main(void)
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE k = NULL;
    UCHAR plain[24], out[32], back[32], same[16], same_out[16];
    WCHAR mode[64];
    ULONG len = 0;
    NTSTATUS st;

    memcpy(plain, "The qufck brown fox jump", 24);
    st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_3DES_ALGORITHM, NULL, 0);
    printf("Open=%#lx\n", st);
    st = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (UCHAR *)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
    printf("SetMode=%#lx\n", st);
    mode[0] = 0;
    BCryptGetProperty(alg, BCRYPT_CHAINING_MODE, (UCHAR *)mode, sizeof(mode), &len, 0);
    printf("Mode=%ls\n", mode);
    st = BCryptGenerateSymmetricKey(alg, &k, NULL, 0, (UCHAR *)key, sizeof(key), 0);
    printf("Key=%#lx\n", st);
    if (st) return 1;
    st = BCryptEncrypt(k, plain, 24, NULL, NULL, 0, out, sizeof(out), &len, 0);
    printf("Encrypt=%#lx %lu %d\n", st, len, !st && len == 24 && !memcmp(out, expect, 24));
    st = BCryptDecrypt(k, out, 24, NULL, NULL, 0, back, sizeof(back), &len, 0);
    printf("Decrypt=%#lx %d\n", st, !st && len == 24 && !memcmp(back, plain, 24));
    /* ECB: two equal blocks encrypt alike */
    memcpy(same, "SAMEBLKSSAMEBLKS", 16);
    st = BCryptEncrypt(k, same, 16, NULL, NULL, 0, same_out, sizeof(same_out), &len, 0);
    printf("Equal=%#lx %d\n", st, !st && !memcmp(same_out, same_out + 8, 8));
    /* no vector in ECB */
    st = BCryptEncrypt(k, plain, 8, NULL, (UCHAR *)key, 8, out, sizeof(out), &len, 0);
    printf("WithIV=%#lx\n", st);
    BCryptDestroyKey(k);
    BCryptCloseAlgorithmProvider(alg, 0);
    return 0;
}
