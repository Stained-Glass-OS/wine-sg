/* XPRESS (COMPRESSION_FORMAT_XPRESS and XPRESS_HUFF) through RtlCompressBuffer, RtlDecompressBuffer and
 * RtlDecompressFragment.  The decompressors are checked with the examples of the open specification
 * [MS-XCA] (section 3), the Plain LZ77 compressor must produce those examples byte for byte, and
 * every kind of input round-trips in both formats.  They used to fail with STATUS_UNSUPPORTED_COMPRESSION. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORMAT_XPRESS 3
#define FORMAT_XPRESS_HUFF 4
#define ENGINE_MAXIMUM 0x100

typedef LONG (WINAPI *compress_fn)(USHORT, UCHAR *, ULONG, UCHAR *, ULONG, ULONG, ULONG *, void *);
typedef LONG (WINAPI *decompress_fn)(USHORT, UCHAR *, ULONG, UCHAR *, ULONG, ULONG *);
typedef LONG (WINAPI *fragment_fn)(USHORT, UCHAR *, ULONG, UCHAR *, ULONG, ULONG, ULONG *, void *);
typedef LONG (WINAPI *workspace_fn)(USHORT, ULONG *, ULONG *);

static compress_fn pCompress;
static decompress_fn pDecompress;
static fragment_fn pFragment;
static workspace_fn pWorkSpace;
static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const UCHAR plain_alphabet[] =
{
    0x3f, 0x00, 0x00, 0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
    0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
};
static const UCHAR plain_abc[] = {0xff, 0xff, 0xff, 0x1f, 0x61, 0x62, 0x63, 0x17, 0x00, 0x0f, 0xff, 0x26, 0x01};

static const UCHAR huff_alphabet[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x50, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x45, 0x44, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xd8, 0x52, 0x3e, 0xd7, 0x94, 0x11, 0x5b, 0xe9, 0x19, 0x5f, 0xf9, 0xd6, 0x7c, 0xdf, 0x8d, 0x04,
    0x00, 0x00, 0x00, 0x00,
};
static const UCHAR huff_abc[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xa8, 0xdc, 0x00, 0x00, 0xff, 0x26, 0x01,
};

static UCHAR abc300[300];
static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";

static const char *name_of(int format) { return (format & 0xff) == FORMAT_XPRESS ? "XPRESS" : "XPRESS_HUFF"; }

static void check_decode(int format, const UCHAR *packed, ULONG packed_size, const UCHAR *expect, ULONG expect_size,
                         const char *what)
{
    UCHAR *out = calloc(1, expect_size + 64);
    ULONG final = 0xdeadbeef;
    LONG status = pDecompress(format, out, expect_size + 64, (UCHAR *)packed, packed_size, &final);
    char text[160];

    sprintf(text, "%s decodes %s (status %#lx, %lu bytes)", name_of(format), what, status, final);
    check(!status && final == expect_size && !memcmp(out, expect, expect_size), text);
    free(out);
}

static ULONG rng = 12345;
static UCHAR next_random(void) { rng = rng * 1103515245 + 12345; return rng >> 16; }

static void roundtrip(int format, const UCHAR *data, ULONG size, const char *what)
{
    ULONG bound = size * 2 + 4096, packed_size = 0, final = 0xdeadbeef;
    UCHAR *packed = malloc(bound), *back = malloc(size + 64);
    void *workspace = calloc(1, 4096);
    LONG status;
    char text[200];

    status = pCompress(format, (UCHAR *)data, size, packed, bound, 4096, &packed_size, workspace);
    sprintf(text, "%s compresses %s (status %#lx, %lu -> %lu)", name_of(format), what, status, size, packed_size);
    check(!status, text);
    if (!status)
    {
        memset(back, 0xcc, size + 64);
        status = pDecompress(format, back, size + 64, packed, packed_size, &final);
        sprintf(text, "%s round trip of %s (status %#lx, %lu bytes)", name_of(format), what, status, final);
        check(!status && final == size && !memcmp(back, data, size), text);
        /* the exact size of buffer is enough */
        memset(back, 0xcc, size + 64);
        final = 0xdeadbeef;
        status = pDecompress(format, back, size, packed, packed_size, &final);
        sprintf(text, "%s round trip of %s into a buffer of exactly its size", name_of(format), what);
        check(!status && final == size && !memcmp(back, data, size), text);
        /* a fragment: from the middle */
        if (size > 100)
        {
            ULONG off = size / 3, want = size / 2;
            memset(back, 0xcc, size + 64);
            final = 0xdeadbeef;
            status = pFragment(format, back, want, packed, packed_size, off, &final, workspace);
            sprintf(text, "%s fragment of %s at %lu (status %#lx, %lu bytes)", name_of(format), what, off, status, final);
            check(!status && final == want && !memcmp(back, data + off, want), text);
        }
        /* a compressed buffer one byte short is refused */
        if (packed_size > 8)
        {
            ULONG short_final = 0;
            status = pCompress(format, (UCHAR *)data, size, packed, packed_size - 1, 4096, &short_final, workspace);
            sprintf(text, "%s of %s into a buffer one byte short is STATUS_BUFFER_TOO_SMALL", name_of(format), what);
            check(status == (LONG)0xC0000023, text);
        }
    }
    free(workspace);
    free(packed);
    free(back);
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    static UCHAR big[300000], text[200000], mixed[150000], random_data[100000], zeros[100000], one_run[70000];
    ULONG compress_ws, decompress_ws, packed_size = 0, final;
    static UCHAR packed[600], out[400];
    void *workspace = calloc(1, 4096);
    LONG status;
    unsigned int i, f;

    pCompress = (compress_fn)GetProcAddress(ntdll, "RtlCompressBuffer");
    pDecompress = (decompress_fn)GetProcAddress(ntdll, "RtlDecompressBuffer");
    pFragment = (fragment_fn)GetProcAddress(ntdll, "RtlDecompressFragment");
    pWorkSpace = (workspace_fn)GetProcAddress(ntdll, "RtlGetCompressionWorkSpaceSize");

    for (i = 0; i < 300; i++) abc300[i] = "abc"[i % 3];

    /* the examples of the specification */
    check_decode(FORMAT_XPRESS, plain_alphabet, sizeof(plain_alphabet), (const UCHAR *)alphabet, 26, "the alphabet example");
    check_decode(FORMAT_XPRESS, plain_abc, sizeof(plain_abc), abc300, 300, "the 300 byte example");
    check_decode(FORMAT_XPRESS_HUFF, huff_alphabet, sizeof(huff_alphabet), (const UCHAR *)alphabet, 26, "the alphabet example");
    check_decode(FORMAT_XPRESS_HUFF, huff_abc, sizeof(huff_abc), abc300, 300, "the 300 byte example");

    /* and the Plain LZ77 encoder produces them */
    memset(packed, 0, sizeof(packed));
    status = pCompress(FORMAT_XPRESS, (UCHAR *)alphabet, 26, packed, sizeof(packed), 4096, &packed_size, workspace);
    check(!status && packed_size == sizeof(plain_alphabet) && !memcmp(packed, plain_alphabet, packed_size),
          "XPRESS compresses the alphabet to the specification's bytes");
    status = pCompress(FORMAT_XPRESS, abc300, 300, packed, sizeof(packed), 4096, &packed_size, workspace);
    check(!status && packed_size == sizeof(plain_abc) && !memcmp(packed, plain_abc, packed_size),
          "XPRESS compresses the 300 byte example to the specification's bytes");
    /* the Huffman encoder's choice of codes is its own: what it makes must decode to the example */
    status = pCompress(FORMAT_XPRESS_HUFF, abc300, 300, packed, sizeof(packed), 4096, &packed_size, workspace);
    check(!status && packed_size >= 256 && packed_size < 300, "XPRESS_HUFF compresses the 300 byte example");

    /* inputs */
    for (i = 0; i < sizeof(big); i++) big[i] = "abc"[i % 3];
    for (i = 0; i < sizeof(text); i++) text[i] = "the quick brown fox jumps over the lazy dog. "[(i * 7 + i / 45) % 45];
    for (i = 0; i < sizeof(mixed); i++) mixed[i] = (i / 997) % 3 ? next_random() : "xyzzy"[i % 5];
    for (i = 0; i < sizeof(random_data); i++) random_data[i] = next_random();
    memset(one_run, 'q', sizeof(one_run));
    for (f = FORMAT_XPRESS; f <= FORMAT_XPRESS_HUFF; f++)
    {
        roundtrip(f, (const UCHAR *)"", 0, "nothing");
        roundtrip(f, (const UCHAR *)"a", 1, "one byte");
        roundtrip(f, (const UCHAR *)"ab", 2, "two bytes");
        roundtrip(f, (const UCHAR *)alphabet, 26, "the alphabet");
        roundtrip(f, abc300, 300, "300 bytes of a repeat");
        roundtrip(f, zeros, sizeof(zeros), "100000 zero bytes");
        roundtrip(f, one_run, sizeof(one_run), "70000 equal bytes");
        roundtrip(f, big, sizeof(big), "300000 bytes of a repeat");
        roundtrip(f, text, sizeof(text), "200000 bytes of text");
        roundtrip(f, mixed, sizeof(mixed), "mixed random and repeated bytes");
        roundtrip(f, random_data, sizeof(random_data), "random bytes");
        roundtrip(f | ENGINE_MAXIMUM, text, sizeof(text), "text with the maximum engine");
    }

    /* the compressed text is smaller than the text */
    {
        UCHAR *dst = malloc(sizeof(text) * 2);
        pCompress(FORMAT_XPRESS, text, sizeof(text), dst, sizeof(text) * 2, 4096, &packed_size, workspace);
        check(packed_size < sizeof(text) / 2, "XPRESS shrinks repeated text");
        pCompress(FORMAT_XPRESS_HUFF, text, sizeof(text), dst, sizeof(text) * 2, 4096, &packed_size, workspace);
        check(packed_size < sizeof(text) / 2, "XPRESS_HUFF shrinks repeated text");
        free(dst);
    }

    /* damaged streams */
    {
        static const UCHAR no_history[] = {0xff, 0xff, 0xff, 0xff, 0x07, 0x00};  /* a match before any data */
        static const UCHAR cut_match[] = {0xff, 0xff, 0xff, 0xff, 0x61, 0x62, 0x63, 0x07};  /* half a match word */
        UCHAR damaged[400];

        status = pDecompress(FORMAT_XPRESS, out, sizeof(out), (UCHAR *)no_history, sizeof(no_history), &final);
        check(status == (LONG)0xC0000242, "XPRESS: a match with nothing before it is STATUS_BAD_COMPRESSION_BUFFER");
        status = pDecompress(FORMAT_XPRESS, out, sizeof(out), (UCHAR *)cut_match, sizeof(cut_match), &final);
        check(status == (LONG)0xC0000242, "XPRESS: half a match word is STATUS_BAD_COMPRESSION_BUFFER");
        memcpy(damaged, huff_alphabet, sizeof(huff_alphabet));
        memset(damaged, 0, 256);
        status = pDecompress(FORMAT_XPRESS_HUFF, out, sizeof(out), damaged, sizeof(huff_alphabet), &final);
        check(status == (LONG)0xC0000242, "XPRESS_HUFF: a table without codes is STATUS_BAD_COMPRESSION_BUFFER");
        memcpy(damaged, huff_alphabet, sizeof(huff_alphabet));
        damaged[0x61 / 2] = 0x11;  /* two codes of one bit where there were more */
        status = pDecompress(FORMAT_XPRESS_HUFF, out, sizeof(out), damaged, sizeof(huff_alphabet), &final);
        check(status == (LONG)0xC0000242, "XPRESS_HUFF: an incomplete code is STATUS_BAD_COMPRESSION_BUFFER");
    }

    /* work space sizes */
    for (f = FORMAT_XPRESS; f <= FORMAT_XPRESS_HUFF; f++)
    {
        compress_ws = decompress_ws = 0;
        status = pWorkSpace(f, &compress_ws, &decompress_ws);
        check(!status && compress_ws && decompress_ws, "the work space sizes are non-zero");
        status = pWorkSpace(f | ENGINE_MAXIMUM, &compress_ws, &decompress_ws);
        check(!status, "the work space sizes with the maximum engine");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
