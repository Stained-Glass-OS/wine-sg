/* cabinet's Compression API with XPRESS (patch 3306), run by test/fastxpress-gate.sh */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define COMPRESS_ALGORITHM_XPRESS 3
#define COMPRESS_RAW (1 << 29)
typedef HANDLE COMPRESSOR_HANDLE;
typedef BOOL (WINAPI *CREATE)(DWORD, void *, COMPRESSOR_HANDLE *);
typedef BOOL (WINAPI *COMPRESS)(COMPRESSOR_HANDLE, const void *, SIZE_T, void *, SIZE_T, SIZE_T *);
typedef BOOL (WINAPI *DECOMPRESS)(COMPRESSOR_HANDLE, const void *, SIZE_T, void *, SIZE_T, SIZE_T *);
typedef BOOL (WINAPI *CLOSE)(COMPRESSOR_HANDLE);
typedef BOOL (WINAPI *SETINFO)(COMPRESSOR_HANDLE, int, const void *, SIZE_T);

static CREATE create_c, create_d;
static COMPRESS compress_f;
static DECOMPRESS decompress_f;
static CLOSE close_c, close_d;
static SETINFO set_c;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static void hex(const BYTE *p, SIZE_T n, char *out)
{
    SIZE_T i;
    out[0] = 0;
    for (i = 0; i < n && i < 60; ++i) sprintf(out + strlen(out), "%02x", p[i]);
}

/* compress and decompress, raw or framed */
static int roundtrip(const BYTE *data, SIZE_T size, BOOL raw, SIZE_T block, SIZE_T *csize)
{
    COMPRESSOR_HANDLE c = NULL, d = NULL;
    BYTE *cbuf, *dbuf;
    SIZE_T need = 0, got = 0, back = 0;
    DWORD alg = COMPRESS_ALGORITHM_XPRESS | (raw ? COMPRESS_RAW : 0);
    int ok = 0;

    if (!create_c(alg, NULL, &c) || !create_d(alg, NULL, &d)) return 0;
    if (block)
    {
        DWORD b = block;
        set_c(c, 1, &b, sizeof(b));
    }
    compress_f(c, data, size, NULL, 0, &need);
    cbuf = malloc(need + 16);
    dbuf = malloc(size + 16);
    if (compress_f(c, data, size, cbuf, need, &got) && got <= need)
    {
        if (csize) *csize = got;
        memset(dbuf, 0xa5, size + 16);
        ok = decompress_f(d, cbuf, got, dbuf, size + 16, &back) && back == size && !memcmp(dbuf, data, size);
    }
    free(cbuf);
    free(dbuf);
    close_c(c);
    close_d(d);
    return ok;
}

static void vector(const char *name, const char *text, SIZE_T text_len, const BYTE *expect, SIZE_T expect_len)
{
    COMPRESSOR_HANDLE c = NULL, d = NULL;
    BYTE out[512], back[512];
    SIZE_T got = 0, n = 0;
    char h1[200], h2[200];

    create_c(COMPRESS_ALGORITHM_XPRESS | COMPRESS_RAW, NULL, &c);
    create_d(COMPRESS_ALGORITHM_XPRESS | COMPRESS_RAW, NULL, &d);
    compress_f(c, text, text_len, out, sizeof(out), &got);
    hex(out, got, h1);
    hex(expect, expect_len, h2);
    CHECKF(got == expect_len && !memcmp(out, expect, expect_len), "%s compresses to the known stream (%s)", name, h1);
    memset(back, 0, sizeof(back));
    CHECKF(decompress_f(d, expect, expect_len, back, sizeof(back), &n) && n == text_len && !memcmp(back, text, text_len),
            "%s: the known stream decompresses (%s)", name, h2);
    close_c(c);
    close_d(d);
}

int main(void)
{
    HMODULE cab = LoadLibraryA("cabinet.dll");
    static const BYTE v_alpha[] = { 0x3f,0,0,0, 'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z' };
    static const BYTE v_abc100[] = { 0xff,0xff,0xff,0x1f, 'a','b','c', 0x17,0x00, 0x0f, 0xff, 0x26,0x01 };
    static const BYTE v_ab[] = { 0xff,0xff,0xff,0x3f, 'a','b', 0x0f,0x00, 0x02 };
    static const BYTE v_nib2[] = { 0xff,0xff,0xff,0x21, 'a','b', 0x0f,0x00, 0x42, 'X','Y', 'c','d', 0x0f,0x00 };
    char abc[301], ab[15], nib2[33];
    BYTE *buf, *buf2;
    SIZE_T i, csize, n;
    int ok, k;

    create_c = (CREATE)GetProcAddress(cab, "CreateCompressor");
    create_d = (CREATE)GetProcAddress(cab, "CreateDecompressor");
    compress_f = (COMPRESS)GetProcAddress(cab, "Compress");
    decompress_f = (DECOMPRESS)GetProcAddress(cab, "Decompress");
    close_c = (CLOSE)GetProcAddress(cab, "CloseCompressor");
    close_d = (CLOSE)GetProcAddress(cab, "CloseDecompressor");
    set_c = (SETINFO)GetProcAddress(cab, "SetCompressorInformation");
    if (!create_c || !compress_f) { check(0, "cabinet exports the Compression API"); return 1; }

    {
        COMPRESSOR_HANDLE h = NULL;
        BOOL r = create_c(COMPRESS_ALGORITHM_XPRESS, NULL, &h);
        CHECKF(r && h, "an XPRESS compressor can be created (%lu)", GetLastError());
        if (r) close_c(h);
        r = create_c(4, NULL, &h);
        CHECKF(!r && GetLastError() == ERROR_NOT_SUPPORTED, "XPRESS_HUFF is still not supported (%lu)", GetLastError());
    }

    vector("a run of 26 different letters", "abcdefghijklmnopqrstuvwxyz", 26, v_alpha, sizeof(v_alpha));
    for (i = 0; i < 100; ++i) memcpy(abc + 3 * i, "abc", 3);
    abc[300] = 0;
    vector("abc 100 times", abc, 300, v_abc100, sizeof(v_abc100));
    memcpy(ab, "abababababababab", 14); ab[14] = 0;
    vector("ab 7 times (a 4-bit length)", ab, 14, v_ab, sizeof(v_ab));
    memcpy(nib2, "ababababababab", 14);
    memcpy(nib2 + 14, "XYcdcdcdcdcdcdcdcd", 18);
    vector("two 4-bit lengths share a byte", nib2, 32, v_nib2, sizeof(v_nib2));

    /* round trips, raw and framed, of every size class of match length */
    buf = malloc(400000);
    for (k = 0; k < 2; ++k)
    {
        BOOL raw = k == 0;
        const char *kind = raw ? "raw" : "framed";

        ok = roundtrip((const BYTE *)"", 0, raw, 0, NULL);
        CHECKF(ok, "%s: empty input", kind);
        ok = roundtrip((const BYTE *)"x", 1, raw, 0, NULL);
        CHECKF(ok, "%s: one byte", kind);
        ok = roundtrip((const BYTE *)"abcabcabc", 9, raw, 0, NULL);
        CHECKF(ok, "%s: a short repeat", kind);

        for (n = 3; n <= 40; ++n)
        {
            memset(buf, 'q', 1);
            memset(buf + 1, 'z', n);        /* a run: the match is n - 1 long */
            if (!roundtrip(buf, n + 1, raw, 0, NULL)) break;
        }
        CHECKF(n == 41, "%s: runs of every length up to 40 (stopped at %Iu)", kind, n);

        {
            static const SIZE_T lens[] = { 270, 277, 278, 279, 280, 281, 300, 1000, 65534, 65535, 65536, 65537, 65539, 70000, 200000 };
            for (i = 0; i < sizeof(lens) / sizeof(lens[0]); ++i)
            {
                memset(buf, 0, lens[i]);
                if (!roundtrip(buf, lens[i], raw, 0, &csize)) break;
            }
            CHECKF(i == sizeof(lens) / sizeof(lens[0]), "%s: long runs of zeros (stopped at index %Iu)", kind, i);
        }

        /* text and structured data */
        for (i = 0; i < 100000; ++i) buf[i] = "the quick brown fox jumps over the lazy dog 0123456789 "[(i * 7 + (i >> 5)) % 56];
        ok = roundtrip(buf, 100000, raw, 0, &csize);
        CHECKF(ok && csize < 60000, "%s: patterned text compresses (%Iu of 100000)", kind, csize);

        /* data that does not compress */
        srand(7);
        for (i = 0; i < 90000; ++i) buf[i] = rand() >> 3;
        ok = roundtrip(buf, 90000, raw, 0, &csize);
        CHECKF(ok, "%s: random data (%Iu bytes)", kind, csize);
        CHECKF(raw || csize <= 90000 + 24 + 4 * 2 + 16, "%s: random data is stored in chunks, not expanded much (%Iu)", kind, csize);
    }

    /* matches farther back than the 8 KiB window are not used */
    srand(11);
    for (i = 0; i < 10000; ++i) buf[i] = rand() >> 2;
    memcpy(buf + 10000, buf, 10000);
    ok = roundtrip(buf, 20000, TRUE, 0, &csize);
    CHECKF(ok, "raw: a repeat beyond the window (%Iu bytes)", csize);
    ok = roundtrip(buf, 20000, FALSE, 0, &csize);
    CHECKF(ok, "framed: a repeat beyond the window (%Iu bytes)", csize);

    /* a bigger block */
    for (i = 0; i < 300000; ++i) buf[i] = (i * i >> 9) & 0x1f;
    ok = roundtrip(buf, 300000, FALSE, 131072, &csize);
    CHECKF(ok, "framed with a 128 KiB block (%Iu bytes)", csize);

    /* corrupt streams are refused, not read out of bounds */
    {
        COMPRESSOR_HANDLE d = NULL;
        BYTE bad[64], out[64];
        SIZE_T got = 0;
        BOOL r;

        create_d(COMPRESS_ALGORITHM_XPRESS | COMPRESS_RAW, NULL, &d);
        memcpy(bad, v_abc100, sizeof(v_abc100));
        r = decompress_f(d, bad, sizeof(v_abc100) - 1, out, sizeof(out), &got);
        CHECKF(!r || got <= sizeof(out), "a stream cut in a length (%d, %lu)", r, GetLastError());
        r = decompress_f(d, v_abc100, sizeof(v_abc100), out, 20, &got);
        CHECKF(!r, "an output buffer too small is refused (%lu)", GetLastError());
        memcpy(bad, v_ab, sizeof(v_ab));
        bad[6] = 0xff; bad[7] = 0xff;     /* a match before the start */
        r = decompress_f(d, bad, sizeof(v_ab), out, sizeof(out), &got);
        CHECKF(!r, "a match that reaches before the start is refused (%lu)", GetLastError());
        close_d(d);
    }

    /* a framed buffer of another algorithm is refused */
    {
        COMPRESSOR_HANDLE c = NULL, d = NULL;
        BYTE out[256], back[64];
        SIZE_T got = 0, n2 = 0;
        BOOL r;

        create_c(2, NULL, &c);              /* MSZIP */
        compress_f(c, "hello hello hello", 17, out, sizeof(out), &got);
        create_d(COMPRESS_ALGORITHM_XPRESS, NULL, &d);
        r = decompress_f(d, out, got, back, sizeof(back), &n2);
        CHECKF(!r && GetLastError() == ERROR_BAD_COMPRESSION_BUFFER, "an MSZIP buffer is not XPRESS (%lu)", GetLastError());
        close_c(c);
        close_d(d);
    }

    free(buf);
    (void)buf2;
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
