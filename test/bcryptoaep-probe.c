/* RSA-OAEP in bcrypt (patches/sg/2411), run by test/bcryptoaep-gate.sh. Padding is done
 * by bcrypt itself over the raw RSA operation, so SHA-1 and a NULL label work. The probe
 * checks the padding against RFC 8017 7.1 on its own: it opens an OAEP ciphertext with the
 * raw private operation (BCRYPT_PAD_NONE) and takes the block apart with its own MGF1, and
 * it builds a block with its own MGF1, closes it with the raw public operation, and has
 * BCryptDecrypt open it. Round trips over a table of hashes, labels and sizes come on top. */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xc0000001)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#endif
#ifndef STATUS_NOT_SUPPORTED
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const char *desc(const char *a, const char *b)
{
    static char buf[256];
    snprintf(buf, sizeof(buf), "%s: %s", a, b);
    return buf;
}

static int hash_of(const WCHAR *alg, const BYTE *data, ULONG len, BYTE *out, ULONG *outlen)
{
    BCRYPT_ALG_HANDLE a;
    BCRYPT_HASH_HANDLE h;
    ULONG n, got;
    int ok = 0;
    static const BYTE none[1];

    if (BCryptOpenAlgorithmProvider(&a, alg, NULL, 0)) return 0;
    if (!BCryptGetProperty(a, BCRYPT_HASH_LENGTH, (UCHAR *)&n, sizeof(n), &got, 0) &&
        !BCryptCreateHash(a, &h, NULL, 0, NULL, 0, 0))
    {
        ok = !BCryptHashData(h, (UCHAR *)(len ? data : none), len, 0) && !BCryptFinishHash(h, out, n, 0);
        BCryptDestroyHash(h);
        *outlen = n;
    }
    BCryptCloseAlgorithmProvider(a, 0);
    return ok;
}

/* xor MGF1(seed, seedlen) over buf[0..len) */
static int mgf1_xor(const WCHAR *alg, const BYTE *seed, ULONG seedlen, BYTE *buf, ULONG len)
{
    BYTE *tmp = malloc(seedlen + 4), d[64];
    ULONG i = 0, c, n, dl;
    int ok = 1;

    memcpy(tmp, seed, seedlen);
    for (c = 0; i < len && ok; c++)
    {
        tmp[seedlen] = c >> 24; tmp[seedlen + 1] = c >> 16; tmp[seedlen + 2] = c >> 8; tmp[seedlen + 3] = c;
        ok = hash_of(alg, tmp, seedlen + 4, d, &dl);
        for (n = 0; ok && n < dl && i < len; n++, i++) buf[i] ^= d[n];
    }
    free(tmp);
    return ok;
}

struct row
{
    const WCHAR *alg;
    const char *name;
    const char *label;     /* NULL: no label */
    ULONG cblabel;
    ULONG msglen;          /* ULONG_MAX: the most the key takes */
    ULONG bits;
};

static const struct row rows[] =
{
    { BCRYPT_SHA1_ALGORITHM,   "SHA1",   NULL,      0, 0,          1024 },
    { BCRYPT_SHA1_ALGORITHM,   "SHA1",   NULL,      0, 20,         1024 },
    { BCRYPT_SHA1_ALGORITHM,   "SHA1",   "SgLabel", 7, 33,         1024 },
    { BCRYPT_SHA1_ALGORITHM,   "SHA1",   "",        0, 1,          1024 },
    { BCRYPT_SHA1_ALGORITHM,   "SHA1",   NULL,      0, 0xffffffff, 1024 },
    { BCRYPT_SHA256_ALGORITHM, "SHA256", NULL,      0, 32,         2048 },
    { BCRYPT_SHA256_ALGORITHM, "SHA256", "lab",     3, 0xffffffff, 2048 },
    { BCRYPT_SHA256_ALGORITHM, "SHA256", "lab",     3, 5,          1024 },
    { BCRYPT_SHA384_ALGORITHM, "SHA384", NULL,      0, 17,         2048 },
    { BCRYPT_SHA512_ALGORITHM, "SHA512", NULL,      0, 100,        2048 },
    { BCRYPT_SHA512_ALGORITHM, "SHA512", "x",       1, 0xffffffff, 2048 },
    { BCRYPT_MD5_ALGORITHM,    "MD5",    NULL,      0, 40,         1024 },
};

static BCRYPT_KEY_HANDLE make_key(BCRYPT_ALG_HANDLE rsa, ULONG bits)
{
    BCRYPT_KEY_HANDLE k;
    if (BCryptGenerateKeyPair(rsa, &k, bits, 0) || BCryptFinalizeKeyPair(k, 0)) return NULL;
    return k;
}

int main(void)
{
    BCRYPT_ALG_HANDLE rsa;
    BCRYPT_KEY_HANDLE key1024, key2048, key;
    BCRYPT_OAEP_PADDING_INFO pad;
    BYTE msg[300], enc[256], dec[300], em[256], ref[256];
    ULONG i, k, hl, n, m, got, got2;
    NTSTATUS st;
    char what[160];

    for (i = 0; i < sizeof(msg); i++) msg[i] = (BYTE)(i * 13 + 5);
    check(!BCryptOpenAlgorithmProvider(&rsa, BCRYPT_RSA_ALGORITHM, NULL, 0), "RSA provider");
    key1024 = make_key(rsa, 1024);
    key2048 = make_key(rsa, 2048);
    check(key1024 && key2048, "RSA keys of 1024 and 2048 bits");

    for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
    {
        const struct row *r = &rows[i];
        BYTE lhash[64], seed[64], *db;
        ULONG dbl, p, bad;
        ULONG hlen = 0;

        key = r->bits == 1024 ? key1024 : key2048;
        k = r->bits / 8;
        if (!hash_of(r->alg, (BYTE *)"", 0, lhash, &hlen)) { check(0, "hash"); continue; }
        hl = hlen;
        m = r->msglen == 0xffffffff ? k - 2 * hl - 2 : r->msglen;
        if (m > sizeof(msg)) m = sizeof(msg);
        pad.pszAlgId = r->alg; pad.pbLabel = (UCHAR *)r->label; pad.cbLabel = r->cblabel;
        sprintf(what, "%s label=%s len=%lu bits=%lu", r->name, r->label ? (r->cblabel ? r->label : "empty") : "NULL", m, r->bits);

        got = 0;
        st = BCryptEncrypt(key, msg, m, &pad, NULL, 0, NULL, 0, &got, BCRYPT_PAD_OAEP);
        check(!st && got == k, desc("size query", what));
        memset(enc, 0, sizeof(enc));
        st = BCryptEncrypt(key, msg, m, &pad, NULL, 0, enc, k, &got, BCRYPT_PAD_OAEP);
        check(!st && got == k, desc("encrypt", what));
        if (st) continue;

        /* round trip */
        memset(dec, 0, sizeof(dec));
        got2 = 0;
        st = BCryptDecrypt(key, enc, k, &pad, NULL, 0, NULL, 0, &got2, BCRYPT_PAD_OAEP);
        check(!st && got2 == m, desc("decrypt size", what));
        st = BCryptDecrypt(key, enc, k, &pad, NULL, 0, dec, sizeof(dec), &got2, BCRYPT_PAD_OAEP);
        check(!st && got2 == m && !memcmp(dec, msg, m), desc("round trip", what));

        /* the block, taken apart by the probe: RFC 8017 7.1.1 */
        memset(ref, 0, sizeof(ref));
        st = BCryptDecrypt(key, enc, k, NULL, NULL, 0, ref, k, &got2, BCRYPT_PAD_NONE);
        check(!st && got2 == k && ref[0] == 0, desc("raw block starts with 0", what));
        if (!st)
        {
            BYTE lh[64];
            ULONG lhl;

            memcpy(seed, ref + 1, hl);
            db = ref + 1 + hl;
            dbl = k - hl - 1;
            bad = !mgf1_xor(r->alg, db, dbl, seed, hl) || !mgf1_xor(r->alg, seed, hl, db, dbl);
            hash_of(r->alg, (BYTE *)(r->label ? r->label : ""), r->cblabel, lh, &lhl);
            bad |= memcmp(db, lh, hl) != 0;
            for (p = hl; p < dbl && db[p] == 0; p++) ;
            bad |= p >= dbl || db[p] != 1 || dbl - p - 1 != m || memcmp(db + p + 1, msg, m);
            check(!bad, desc("block is a valid OAEP block (MGF1, label hash, 0x01, message)", what));
        }

        /* the block, built by the probe, opened by BCryptDecrypt */
        {
            BYTE lh[64];
            ULONG lhl;

            memset(em, 0, sizeof(em));
            for (n = 0; n < hl; n++) em[1 + n] = (BYTE)(0x30 + n * 3 + i);
            db = em + 1 + hl;
            dbl = k - hl - 1;
            hash_of(r->alg, (BYTE *)(r->label ? r->label : ""), r->cblabel, lh, &lhl);
            memcpy(db, lh, hl);
            db[dbl - m - 1] = 1;
            memcpy(db + dbl - m, msg, m);
            mgf1_xor(r->alg, em + 1, hl, db, dbl);
            mgf1_xor(r->alg, db, dbl, em + 1, hl);
            memset(enc, 0, sizeof(enc));
            st = BCryptEncrypt(key, em, k, NULL, NULL, 0, enc, k, &got, BCRYPT_PAD_NONE);
            memset(dec, 0, sizeof(dec));
            n = 0;
            if (!st) st = BCryptDecrypt(key, enc, k, &pad, NULL, 0, dec, sizeof(dec), &n, BCRYPT_PAD_OAEP);
            check(!st && n == m && !memcmp(dec, msg, m), desc("a block built by the probe opens", what));

            /* wrong label / wrong hash must not open it */
            if (!st)
            {
                BCRYPT_OAEP_PADDING_INFO wl = pad;
                BCRYPT_OAEP_PADDING_INFO wh = pad;

                wl.pbLabel = (UCHAR *)(r->label ? "zz" : "wrong"); wl.cbLabel = r->label ? 2 : 5;
                if (r->label && r->cblabel == 2 && !memcmp(r->label, "zz", 2)) wl.cbLabel = 1;
                st = BCryptDecrypt(key, enc, k, &wl, NULL, 0, dec, sizeof(dec), &n, BCRYPT_PAD_OAEP);
                check(st != 0, desc("a different label does not open it", what));
                wh.pszAlgId = r->alg == BCRYPT_SHA256_ALGORITHM ? BCRYPT_SHA512_ALGORITHM : BCRYPT_SHA256_ALGORITHM;
                st = BCryptDecrypt(key, enc, k, &wh, NULL, 0, dec, sizeof(dec), &n, BCRYPT_PAD_OAEP);
                check(st != 0, desc("a different hash does not open it", what));
            }
        }
    }

    /* randomness */
    {
        BYTE a[256], b[256];

        pad.pszAlgId = BCRYPT_SHA1_ALGORITHM; pad.pbLabel = NULL; pad.cbLabel = 0;
        BCryptEncrypt(key1024, msg, 10, &pad, NULL, 0, a, 128, &got, BCRYPT_PAD_OAEP);
        BCryptEncrypt(key1024, msg, 10, &pad, NULL, 0, b, 128, &got, BCRYPT_PAD_OAEP);
        check(memcmp(a, b, 128) != 0, "two encryptions of one message differ");
    }

    /* errors */
    pad.pszAlgId = BCRYPT_SHA1_ALGORITHM; pad.pbLabel = NULL; pad.cbLabel = 0;
    st = BCryptEncrypt(key1024, msg, 128 - 2 * 20 - 2 + 1, &pad, NULL, 0, enc, 128, &got, BCRYPT_PAD_OAEP);
    check(st == STATUS_INVALID_PARAMETER, "a message one byte too long: STATUS_INVALID_PARAMETER");
    st = BCryptEncrypt(key1024, msg, 10, &pad, NULL, 0, enc, 127, &got, BCRYPT_PAD_OAEP);
    check(st == STATUS_BUFFER_TOO_SMALL && got == 128, "a short output buffer: STATUS_BUFFER_TOO_SMALL and the size");
    st = BCryptEncrypt(key1024, msg, 10, NULL, NULL, 0, enc, 128, &got, BCRYPT_PAD_OAEP);
    check(st == STATUS_INVALID_PARAMETER, "no padding info: STATUS_INVALID_PARAMETER");
    pad.pbLabel = NULL; pad.cbLabel = 4;
    st = BCryptEncrypt(key1024, msg, 10, &pad, NULL, 0, enc, 128, &got, BCRYPT_PAD_OAEP);
    check(st == STATUS_INVALID_PARAMETER, "a label length without a label: STATUS_INVALID_PARAMETER");
    pad.cbLabel = 0; pad.pszAlgId = L"NOSUCHHASH";
    st = BCryptEncrypt(key1024, msg, 10, &pad, NULL, 0, enc, 128, &got, BCRYPT_PAD_OAEP);
    check(st == STATUS_NOT_SUPPORTED, "an unknown hash: STATUS_NOT_SUPPORTED");
    pad.pszAlgId = BCRYPT_SHA512_ALGORITHM;
    st = BCryptEncrypt(key1024, msg, 0, &pad, NULL, 0, enc, 128, &got, BCRYPT_PAD_OAEP);
    check(st == STATUS_INVALID_PARAMETER, "a key too small for the hash (SHA-512 in 1024 bits)");

    /* damaged ciphertexts */
    pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
    BCryptEncrypt(key1024, msg, 10, &pad, NULL, 0, enc, 128, &got, BCRYPT_PAD_OAEP);
    st = BCryptDecrypt(key1024, enc, 127, &pad, NULL, 0, dec, sizeof(dec), &got, BCRYPT_PAD_OAEP);
    check(st != 0, "a ciphertext one byte short does not decrypt");
    enc[50] ^= 1;
    st = BCryptDecrypt(key1024, enc, 128, &pad, NULL, 0, dec, sizeof(dec), &got, BCRYPT_PAD_OAEP);
    check(st != 0, "a flipped bit does not decrypt");
    st = BCryptDecrypt(key1024, enc, 128, &pad, NULL, 0, dec, sizeof(dec), &got, BCRYPT_PAD_OAEP);
    check(st != 0, "(again, with an output buffer)");
    BCryptEncrypt(key1024, msg, 10, &pad, NULL, 0, enc, 128, &got, BCRYPT_PAD_OAEP);
    st = BCryptDecrypt(key1024, enc, 128, &pad, NULL, 0, dec, 5, &got, BCRYPT_PAD_OAEP);
    check(st == STATUS_BUFFER_TOO_SMALL && got == 10, "a short plaintext buffer: STATUS_BUFFER_TOO_SMALL and the size");
    st = BCryptDecrypt(key2048, enc, 128, &pad, NULL, 0, dec, sizeof(dec), &got, BCRYPT_PAD_OAEP);
    check(st != 0, "another key does not decrypt");

    BCryptDestroyKey(key1024);
    BCryptDestroyKey(key2048);
    BCryptCloseAlgorithmProvider(rsa, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
