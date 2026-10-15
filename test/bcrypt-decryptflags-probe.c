/* Probe for patches/sg/5016: BCryptDecrypt with a symmetric key takes only
 * BCRYPT_BLOCK_PADDING ("can be zero or the following value"); other flags
 * fail STATUS_INVALID_PARAMETER, like BCryptEncrypt. Zero and block padding
 * round-trip.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { ULONG flags; NTSTATUS want; } tests[] =
    {
        {0, 0}, {BCRYPT_BLOCK_PADDING, 0},
        {0x2, STATUS_INVALID_PARAMETER}, {0x4, STATUS_INVALID_PARAMETER}, {0x8, STATUS_INVALID_PARAMETER},
        {BCRYPT_BLOCK_PADDING | 0x100, STATUS_INVALID_PARAMETER}, {0x80000000, STATUS_INVALID_PARAMETER},
    };
    static UCHAR secret[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key;
    UCHAR plain[32], enc[64], dec[64], iv[16];
    ULONG len, dlen;
    unsigned int i;
    NTSTATUS st;

    for (i = 0; i < sizeof(plain); i++) plain[i] = i * 7;
    st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0);
    check(!st, "open AES %#lx", st);
    BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (UCHAR *)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    st = BCryptGenerateSymmetricKey(alg, &key, NULL, 0, secret, sizeof(secret), 0);
    check(!st, "key %#lx", st);

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        ULONG eflags = tests[i].flags & BCRYPT_BLOCK_PADDING;
        memset(iv, 0, sizeof(iv));
        st = BCryptEncrypt(key, plain, sizeof(plain), NULL, iv, sizeof(iv), enc, sizeof(enc), &len, eflags);
        memset(iv, 0, sizeof(iv));
        memset(dec, 0, sizeof(dec));
        st = BCryptDecrypt(key, enc, len, NULL, iv, sizeof(iv), dec, sizeof(dec), &dlen, tests[i].flags);
        if (!tests[i].want)
            check(!st && dlen == sizeof(plain) && !memcmp(dec, plain, sizeof(plain)), "flags %#lx round trip: %#lx len %lu",
                  tests[i].flags, st, dlen);
        else
            check(st == tests[i].want, "flags %#lx: %#lx (want %#lx)", tests[i].flags, st, tests[i].want);
    }
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
