/* RtlCompressBuffer(LZNT1) really compresses (patches/sg/2212).  It used to
 * store every chunk uncompressed.  The probe checks the stream format with a
 * decoder of its own (written from the LZNT1 description, independent of
 * ntdll's), sizes, and round trips through RtlDecompressBuffer. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef LONG NTSTATUS;
#define STATUS_SUCCESS 0
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023)

static NTSTATUS (WINAPI *pRtlGetCompressionWorkSpaceSize)(USHORT, ULONG *, ULONG *);
static NTSTATUS (WINAPI *pRtlCompressBuffer)(USHORT, UCHAR *, ULONG, UCHAR *, ULONG, ULONG, ULONG *, void *);
static NTSTATUS (WINAPI *pRtlDecompressBuffer)(USHORT, UCHAR *, ULONG, UCHAR *, ULONG, ULONG *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* an independent LZNT1 decoder; returns the decoded size, or -1 for a malformed stream */
static long decode(const UCHAR *in, ULONG in_len, UCHAR *out, ULONG out_max, int *saw_compressed, int *saw_raw)
{
    ULONG ip = 0, op = 0;

    while (ip + 2 <= in_len)
    {
        unsigned hdr = in[ip] | (in[ip + 1] << 8);
        ULONG size = (hdr & 0xfff) + 1, chunk_start = op, end;

        if (!hdr) break;
        if ((hdr & 0x7000) != 0x3000) return -1;
        ip += 2;
        if (ip + size > in_len) return -1;
        end = ip + size;
        if (!(hdr & 0x8000))
        {
            *saw_raw = 1;
            if (op + size > out_max) return -1;
            memcpy(out + op, in + ip, size);
            op += size;
            ip = end;
            continue;
        }
        *saw_compressed = 1;
        while (ip < end)
        {
            unsigned flags = in[ip++], bit;

            for (bit = 0; bit < 8 && ip < end; bit++)
            {
                if (!(flags & (1u << bit)))
                {
                    if (op >= out_max) return -1;
                    out[op++] = in[ip++];
                }
                else
                {
                    unsigned code, pos = op - chunk_start, db = 4, lb, len, disp, k;

                    if (ip + 2 > end) return -1;
                    code = in[ip] | (in[ip + 1] << 8);
                    ip += 2;
                    while ((1u << db) < pos && db < 12) db++;   /* ceil(log2(pos)), at least 4 */
                    lb = 16 - db;
                    len = (code & ((1u << lb) - 1)) + 3;
                    disp = (code >> lb) + 1;
                    if (disp > pos || op + len > out_max) return -1;
                    for (k = 0; k < len; k++, op++) out[op] = out[op - disp];
                }
            }
        }
    }
    return op;
}

static void fill(UCHAR *p, ULONG n, int kind)
{
    ULONG i, seed = 12345u + kind;
    const char *text = "the quick brown fox jumps over the lazy dog. ";

    for (i = 0; i < n; i++)
    {
        seed = seed * 1103515245u + 12345u;
        switch (kind)
        {
        case 0: p[i] = 0; break;                                       /* zeros */
        case 1: p[i] = text[i % strlen(text)]; break;                  /* repeating text */
        case 2: p[i] = (seed >> 16) & 0xff; break;                     /* noise */
        case 3: p[i] = ((i / 700) % 2) ? (seed >> 16) & 0xff : text[i % 9]; break;   /* mixed */
        default: p[i] = (i * 7) & 0xff; break;                         /* ramp */
        }
    }
}

int main(void)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    static const ULONG sizes[] = { 1, 2, 3, 4, 17, 100, 4095, 4096, 4097, 8192, 10000, 70000 };
    static const char *names[] = { "zeros", "text", "noise", "mixed", "ramp" };
    ULONG cws = 0, dws = 0, final_size, i, k;
    UCHAR *src, *dst, *back, *ws;
    int bad_round = 0, bad_stream = 0, kind;

    pRtlGetCompressionWorkSpaceSize = (void *)GetProcAddress(nt, "RtlGetCompressionWorkSpaceSize");
    pRtlCompressBuffer = (void *)GetProcAddress(nt, "RtlCompressBuffer");
    pRtlDecompressBuffer = (void *)GetProcAddress(nt, "RtlDecompressBuffer");
    pRtlGetCompressionWorkSpaceSize(2, &cws, &dws);   /* COMPRESSION_FORMAT_LZNT1 */
    ws = malloc(cws + 16);
    src = malloc(80000);
    dst = malloc(120000);
    back = malloc(80000);

    for (kind = 0; kind < 5; kind++)
    {
        for (k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++)
        {
            ULONG n = sizes[k];
            int comp = 0, raw = 0;
            long dec;
            NTSTATUS st;

            fill(src, n, kind);
            memset(dst, 0x5a, 120000);
            final_size = 0xdeadbeef;
            st = pRtlCompressBuffer(2, src, n, dst, 120000, 4096, &final_size, ws);
            if (st != STATUS_SUCCESS || final_size == 0xdeadbeef) { bad_stream++; continue; }
            dec = decode(dst, final_size, back, 80000, &comp, &raw);
            if (dec != (long)n || memcmp(back, src, n)) { bad_stream++; printf("      %s %lu: own decoder gave %ld\n", names[kind], (unsigned long)n, dec); }
            {
                ULONG got = 0;
                memset(back, 0, 80000);
                st = pRtlDecompressBuffer(2, back, 80000, dst, final_size, &got);
                if (st != STATUS_SUCCESS || got != n || memcmp(back, src, n)) { bad_round++; printf("      %s %lu: RtlDecompressBuffer %08lx got %lu\n", names[kind], (unsigned long)n, (unsigned long)st, (unsigned long)got); }
            }
            if (kind == 0 && n >= 100)
                if (!(final_size < n / 8 + 16 && comp)) { bad_stream++; printf("      zeros %lu only shrank to %lu\n", (unsigned long)n, (unsigned long)final_size); }
            if (kind == 1 && n >= 4096)
                if (!(final_size < n / 2 && comp)) { bad_stream++; printf("      text %lu only shrank to %lu\n", (unsigned long)n, (unsigned long)final_size); }
            if (kind == 2)
                if (!(final_size == n + 2 * ((n + 4095) / 4096) && raw && !comp)) { bad_stream++; printf("      noise %lu became %lu\n", (unsigned long)n, (unsigned long)final_size); }
        }
    }
    check(!bad_stream, "every stream parses with the probe's own decoder, and zeros/text shrink, noise is stored raw");
    check(!bad_round, "every stream round-trips through RtlDecompressBuffer");

    /* output space: the exact size works, one byte less is STATUS_BUFFER_TOO_SMALL */
    fill(src, 10000, 1);
    final_size = 0;
    check(pRtlCompressBuffer(2, src, 10000, dst, 120000, 4096, &final_size, ws) == STATUS_SUCCESS && final_size > 0, "compress text");
    {
        ULONG need = final_size, f2 = 0xdeadbeef;
        NTSTATUS st;

        memset(back, 0, 80000);
        st = pRtlCompressBuffer(2, src, 10000, back, need, 4096, &f2, ws);
        check(st == STATUS_SUCCESS && f2 == need && !memcmp(back, dst, need), "the exact output size is enough, and gives the same stream");
        st = pRtlCompressBuffer(2, src, 10000, back, need - 1, 4096, &f2, ws);
        check(st == STATUS_BUFFER_TOO_SMALL, "one byte less is STATUS_BUFFER_TOO_SMALL");
        st = pRtlCompressBuffer(2, src, 10000, back, 1, 4096, &f2, ws);
        check(st == STATUS_BUFFER_TOO_SMALL, "a one byte output is STATUS_BUFFER_TOO_SMALL");
    }

    /* a long run uses the longest matches: 4096 zeros need only a few bytes */
    memset(src, 0, 4096);
    final_size = 0;
    check(pRtlCompressBuffer(2, src, 4096, dst, 120000, 4096, &final_size, ws) == STATUS_SUCCESS && final_size < 40, "4096 zero bytes compress to a few bytes");

    (void)i;
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
