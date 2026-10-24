/* aesecb-probe.c (wine-sg 6091): AES-ECB through bcrypt, the FIPS-197 AES-128 example, one block and three. */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

static void hex(const char *key, const UCHAR *buf, ULONG len, NTSTATUS status)
{
    ULONG i;
    printf("%s=", key);
    if (status) printf("error %#lx", status);
    else for (i = 0; i < len; i++) printf("%02x", buf[i]);
    printf("\n");
}

int main(void)
{
    UCHAR key_bytes[16], plain[48], out[48], back[48];
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key;
    ULONG len = 0, i;
    NTSTATUS status;

    for (i = 0; i < 16; i++) key_bytes[i] = i;
    for (i = 0; i < 48; i++) plain[i] = (i % 16) * 0x11;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0);
    BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (UCHAR *)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
    BCryptGenerateSymmetricKey(alg, &key, NULL, 0, key_bytes, 16, 0);
    status = BCryptEncrypt(key, plain, 16, NULL, NULL, 0, out, 16, &len, 0);
    hex("ecb_one", out, len, status);
    status = BCryptEncrypt(key, plain, 48, NULL, NULL, 0, out, 48, &len, 0);
    hex("ecb_three", out, len, status);
    status = BCryptDecrypt(key, out, 48, NULL, NULL, 0, back, 48, &len, 0);
    printf("ecb_back=%#lx %d\n", status, len == 48 && !memcmp(back, plain, 48));
    BCryptDestroyKey(key);
    printf("done=1\n");
    return 0;
}
