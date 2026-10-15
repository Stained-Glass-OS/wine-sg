/* Probe for patches/sg/5017: BCryptExportKey/BCryptImportKey with
 * BCRYPT_AES_WRAP_KEY_BLOB and a wrapping AES key: RFC 3394 key wrap. The
 * known answers are the RFC's own test vectors (4.1, 4.2, 4.6); the import
 * unwraps them back, and a wrong wrapping key or a damaged blob fails.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

static int failures;
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#endif
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static ULONG unhex(const char *s, UCHAR *out)
{
    ULONG n = 0;
    for (; s[0] && s[1]; s += 2) { unsigned int v; sscanf(s, "%2x", &v); out[n++] = v; }
    return n;
}

int main(void)
{
    static const struct { const char *kek, *data, *wrapped; } tests[] =
    {
        {"000102030405060708090A0B0C0D0E0F", "00112233445566778899AABBCCDDEEFF",
         "1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5"},
        {"000102030405060708090A0B0C0D0E0F1011121314151617", "00112233445566778899AABBCCDDEEFF",
         "96778B25AE6CA435F92B5B97C050AED2468AB8A17AD84E5D"},
        {"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F",
         "00112233445566778899AABBCCDDEEFF000102030405060708090A0B0C0D0E0F",
         "28C9F404C4B810F4CBCCB35CFB87F8263F5786E2D80ED326CBC7F0E71A99F43BFB988B9B7A02DD21"},
    };
    BCRYPT_ALG_HANDLE alg;
    UCHAR kek[32], data[32], want[40], got[64], other_secret[16] = {9};
    BCRYPT_KEY_HANDLE kek_key, data_key, other, back;
    ULONG kek_len, data_len, want_len, size;
    unsigned int i;
    NTSTATUS st;

    st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0);
    check(!st, "open AES %#lx", st);
    BCryptGenerateSymmetricKey(alg, &other, NULL, 0, other_secret, sizeof(other_secret), 0);

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        kek_len = unhex(tests[i].kek, kek);
        data_len = unhex(tests[i].data, data);
        want_len = unhex(tests[i].wrapped, want);
        BCryptGenerateSymmetricKey(alg, &kek_key, NULL, 0, kek, kek_len, 0);
        BCryptGenerateSymmetricKey(alg, &data_key, NULL, 0, data, data_len, 0);

        size = 0;
        st = BCryptExportKey(data_key, kek_key, BCRYPT_AES_WRAP_KEY_BLOB, NULL, 0, &size, 0);
        check(!st && size == want_len, "vector %u: size query %#lx %lu", i, st, size);
        memset(got, 0, sizeof(got));
        st = BCryptExportKey(data_key, kek_key, BCRYPT_AES_WRAP_KEY_BLOB, got, sizeof(got), &size, 0);
        check(!st && size == want_len && !memcmp(got, want, want_len), "vector %u: wrapped as the RFC says (%#lx)", i, st);
        st = BCryptExportKey(data_key, kek_key, BCRYPT_AES_WRAP_KEY_BLOB, got, want_len - 1, &size, 0);
        check(st == STATUS_BUFFER_TOO_SMALL, "vector %u: short buffer %#lx", i, st);

        back = NULL;
        st = BCryptImportKey(alg, kek_key, BCRYPT_AES_WRAP_KEY_BLOB, &back, NULL, 0, want, want_len, 0);
        check(!st && back, "vector %u: import %#lx", i, st);
        if (!st)
        {
            UCHAR blob[64];
            BCRYPT_KEY_DATA_BLOB_HEADER *h = (void *)blob;
            st = BCryptExportKey(back, NULL, BCRYPT_KEY_DATA_BLOB, blob, sizeof(blob), &size, 0);
            check(!st && h->cbKeyData == data_len && !memcmp(h + 1, data, data_len), "vector %u: unwrapped key is the data", i);
            BCryptDestroyKey(back);
        }
        st = BCryptImportKey(alg, other, BCRYPT_AES_WRAP_KEY_BLOB, &back, NULL, 0, want, want_len, 0);
        check(st != 0, "vector %u: the wrong wrapping key fails (%#lx)", i, st);
        if (!st) BCryptDestroyKey(back);
        want[want_len - 1] ^= 1;
        st = BCryptImportKey(alg, kek_key, BCRYPT_AES_WRAP_KEY_BLOB, &back, NULL, 0, want, want_len, 0);
        check(st != 0, "vector %u: a damaged blob fails (%#lx)", i, st);
        if (!st) BCryptDestroyKey(back);
        BCryptDestroyKey(data_key);
        BCryptDestroyKey(kek_key);
    }
    BCryptDestroyKey(other);
    BCryptCloseAlgorithmProvider(alg, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
