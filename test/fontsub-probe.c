/* fontsub probe: subsets a generated TrueType font with CreateFontPackage */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *(__cdecl *allocfn)(size_t);
typedef void *(__cdecl *reallocfn)(void *, size_t);
typedef void (__cdecl *freefn)(void *);
typedef ULONG (__cdecl *cfpfn)(const unsigned char *, ULONG, unsigned char **, ULONG *, ULONG *, unsigned short,
    unsigned short, unsigned short, unsigned short, unsigned short, unsigned short, const unsigned short *,
    unsigned short, allocfn, reallocfn, freefn, void *);

static void *cb_alloc(size_t n) { return malloc(n); }
static void *cb_realloc(void *p, size_t n) { return realloc(p, n); }
static void cb_free(void *p) { free(p); }

static void p16(unsigned char *p, unsigned v) { p[0] = v >> 8; p[1] = v; }
static void p32(unsigned char *p, unsigned v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static unsigned g16(const unsigned char *p) { return (p[0] << 8) | p[1]; }
static unsigned g32(const unsigned char *p) { return ((unsigned)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static unsigned checksum(const unsigned char *d, unsigned len)
{
    unsigned sum = 0, i;
    for (i = 0; i < len; i += 4)
    {
        unsigned char t[4] = {0};
        memcpy(t, d + i, len - i < 4 ? len - i : 4);
        sum += g32(t);
    }
    return sum;
}

struct tbl { const char *tag; unsigned char *data; unsigned len; };

/* glyph 0..4: .notdef, A, B, C (composite of A), D; 32 bytes each, short loca */
static unsigned build_font(unsigned char *out, unsigned base)
{
    struct tbl t[11];
    unsigned char cmap[48], head[54], hhea[36], maxp[32], hmtx[20], loca[12], glyf[5 * 32], name[64], post[32], os2[78];
    unsigned n = 0, i, pos;
    const char *family = "SgSub";

    memset(cmap, 0, sizeof(cmap)); memset(head, 0, sizeof(head)); memset(hhea, 0, sizeof(hhea));
    memset(maxp, 0, sizeof(maxp)); memset(hmtx, 0, sizeof(hmtx)); memset(glyf, 0, sizeof(glyf));
    memset(name, 0, sizeof(name)); memset(post, 0, sizeof(post)); memset(os2, 0, sizeof(os2));
    p16(os2, 1); p16(os2 + 4, 400); p16(os2 + 6, 5); p16(os2 + 58, 0x40); p16(os2 + 60, 0x41); p16(os2 + 62, 0x44);
    p16(os2 + 64, 800); p16(os2 + 66, (unsigned short)-200); p16(os2 + 68, 0); p16(os2 + 70, 800); p16(os2 + 72, 200); p32(os2 + 42, 1);

    /* cmap: one (3,1) format 4 subtable, A..D -> 1..4 */
    p16(cmap + 2, 1); p16(cmap + 4, 3); p16(cmap + 6, 1); p32(cmap + 8, 12);
    {
        unsigned char *s = cmap + 12;
        p16(s, 4); p16(s + 2, 32); p16(s + 6, 4); p16(s + 8, 4); p16(s + 10, 1); p16(s + 12, 0);
        p16(s + 14, 0x44); p16(s + 16, 0xffff); p16(s + 18, 0);
        p16(s + 20, 0x41); p16(s + 22, 0xffff);
        p16(s + 24, (1 - 0x41) & 0xffff); p16(s + 26, 1);
        p16(s + 28, 0); p16(s + 30, 0);
    }
    p32(head, 0x00010000); p32(head + 12, 0x5f0f3cf5); p16(head + 18, 1000);
    p16(head + 36, 0); p16(head + 38, 0); p16(head + 40, 100); p16(head + 42, 100);
    p16(head + 46, 8); p16(head + 48, 2); p16(head + 50, 0);
    p32(hhea, 0x00010000); p16(hhea + 4, 800); p16(hhea + 34, 5);
    p32(maxp, 0x00010000); p16(maxp + 4, 5); p16(maxp + 6, 3); p16(maxp + 8, 1); p16(maxp + 28, 1);
    for (i = 0; i < 5; i++) p16(hmtx + i * 4, 500);
    for (i = 0; i < 5; i++)
    {
        unsigned char *g = glyf + i * 32;
        if (i == 3)
        {
            p16(g, 0xffff); p16(g + 4, 10); p16(g + 8, 110); p16(g + 10, 0x0003); p16(g + 12, 1);
            p16(g + 14, 10); p16(g + 16, 0);
        }
        else
        {
            p16(g, 1); p16(g + 4, 0); p16(g + 6, 0); p16(g + 8, 100); p16(g + 10, 100);
            p16(g + 10, 100); p16(g + 12, 2); p16(g + 14, 0);
            g[16] = g[17] = g[18] = 1;
            p16(g + 19, 0); p16(g + 21, 100); p16(g + 23, 0);
            p16(g + 25, 0); p16(g + 27, 100); p16(g + 29, 0);
        }
    }
    for (i = 0; i <= 5; i++) p16(loca + i * 2, i * 16);  /* short format: offsets / 2 */
    /* name: (3,1,0x409) ids 1, 2, 4, 6 */
    p16(name + 2, 4); p16(name + 4, 6 + 4 * 12);
    {
        static const unsigned ids[4] = { 1, 2, 4, 6 };
        unsigned char *rec = name + 6, *str = name + 6 + 4 * 12;
        for (i = 0; i < 4; i++, rec += 12)
        {
            p16(rec, 3); p16(rec + 2, 1); p16(rec + 4, 0x409); p16(rec + 6, ids[i]); p16(rec + 8, 10); p16(rec + 10, 0);
        }
        for (i = 0; i < 5; i++) { str[i * 2] = 0; str[i * 2 + 1] = family[i]; }
    }
    p32(post, 0x00030000);

    t[n++] = (struct tbl){ "OS/2", os2, sizeof(os2) };
    t[n++] = (struct tbl){ "cmap", cmap, sizeof(cmap) };
    t[n++] = (struct tbl){ "glyf", glyf, sizeof(glyf) };
    t[n++] = (struct tbl){ "head", head, sizeof(head) };
    t[n++] = (struct tbl){ "hhea", hhea, sizeof(hhea) };
    t[n++] = (struct tbl){ "hmtx", hmtx, sizeof(hmtx) };
    t[n++] = (struct tbl){ "loca", loca, sizeof(loca) };
    t[n++] = (struct tbl){ "maxp", maxp, sizeof(maxp) };
    t[n++] = (struct tbl){ "name", name, 6 + 4 * 12 + 10 };
    t[n++] = (struct tbl){ "post", post, sizeof(post) };
    memset(out, 0, 4096);
    p32(out, 0x00010000); p16(out + 4, n); p16(out + 6, 128); p16(out + 8, 3); p16(out + 10, n * 16 - 128);
    /* tags must be sorted: cmap glyf head hhea hmtx loca maxp name post */
    pos = base + 12 + n * 16;
    for (i = 0; i < n; i++)
    {
        unsigned char *rec = out + 12 + i * 16;
        unsigned len = t[i].len;
        memcpy(rec, t[i].tag, 4);
        memcpy(out + (pos - base), t[i].data, len);
        p32(rec + 4, checksum(t[i].data, len)); p32(rec + 8, pos); p32(rec + 12, len);
        pos += (len + 3) & ~3u;
    }
    return pos - base;
}

static int parse(const unsigned char *f, unsigned size, unsigned *num_glyphs, unsigned *loca_fmt, const unsigned char **loca,
                 const unsigned char **glyf, int *sorted)
{
    unsigned n, i;
    DWORD prev = 0;
    *sorted = 1;
    if (size < 12 || g32(f) != 0x00010000) return 0;
    n = g16(f + 4);
    for (i = 0; i < n; i++)
    {
        const unsigned char *rec = f + 12 + i * 16;
        DWORD tag = g32(rec);
        if (tag <= prev) *sorted = 0;
        prev = tag;
        if (!memcmp(rec, "maxp", 4)) *num_glyphs = g16(f + g32(rec + 8) + 4);
        if (!memcmp(rec, "head", 4)) *loca_fmt = g16(f + g32(rec + 8) + 50);
        if (!memcmp(rec, "loca", 4)) *loca = f + g32(rec + 8);
        if (!memcmp(rec, "glyf", 4)) *glyf = f + g32(rec + 8);
    }
    return 1;
}

static void report(const char *tag, ULONG ret, const unsigned char *f, ULONG size)
{
    unsigned num = 0, fmt = 9, i;
    const unsigned char *loca = NULL, *glyf = NULL;
    int sorted = 0;
    HANDLE h;
    DWORD count = 0;

    printf("%s ret=%lu", tag, ret);
    if (ret || !f) { printf("\n"); return; }
    if (!parse(f, size, &num, &fmt, &loca, &glyf, &sorted)) { printf(" badfont\n"); return; }
    printf(" sum=%s sorted=%d glyphs=%u loca=%u kept:", checksum(f, size) == 0xb1b0afba ? "ok" : "bad", sorted, num, fmt);
    for (i = 0; i < num && loca; i++)
    {
        unsigned a = fmt ? g32(loca + i * 4) : g16(loca + i * 2) * 2, b = fmt ? g32(loca + i * 4 + 4) : g16(loca + i * 2 + 2) * 2;
        if (b > a) printf(" %u", i);
    }
    h = AddFontMemResourceEx((void *)f, size, NULL, &count);
    printf(" loadable=%d\n", h != NULL);
    if (h) RemoveFontMemResourceEx(h);
}

int main(void)
{
    HMODULE mod = LoadLibraryA("fontsub.dll");
    cfpfn create = (cfpfn)GetProcAddress(mod, "CreateFontPackage");
    unsigned char font[4096], ttc[8192], *out;
    unsigned char *copy;
    unsigned size = build_font(font, 0), tsize;
    ULONG alloc = 0, written = 0, ret;
    unsigned short chars[2];

    {
        DWORD cnt = 0;
        HANDLE h = AddFontMemResourceEx(font, size, NULL, &cnt);
        printf("fixture loadable=%d\n", h != NULL);
        if (h) RemoveFontMemResourceEx(h);
    }
    chars[0] = 'C';
    out = NULL;
    ret = create(font, size, &out, &alloc, &written, 1, 0, 0, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    report("char C", ret, out, written);
    if (!ret) printf("sizes %d\n", alloc == written && written < size + 16);
    free(out); out = NULL;

    chars[0] = 3;
    ret = create(font, size, &out, &alloc, &written, 1 | 8, 0, 0, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    report("glyph 3", ret, out, written);
    free(out); out = NULL;

    chars[0] = 4;
    ret = create(font, size, &out, &alloc, &written, 1 | 8, 0, 0, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    report("glyph 4", ret, out, written);
    free(out); out = NULL;

    chars[0] = 'B'; chars[1] = 'D';
    ret = create(font, size, &out, &alloc, &written, 1, 0, 0, 0, 3, 0xffff, chars, 2, cb_alloc, cb_realloc, cb_free, NULL);
    report("chars B D any-encoding", ret, out, written);
    free(out); out = NULL;

    ret = create(font, size, &out, &alloc, &written, 1, 0, 0, 0, 1, 0, chars, 2, cb_alloc, cb_realloc, cb_free, NULL);
    printf("no such cmap ret=%lu\n", ret);

    ret = create(font, size, &out, &alloc, &written, 0, 0, 0, 0, 3, 1, chars, 0, cb_alloc, cb_realloc, cb_free, NULL);
    printf("no subset flag ret=%lu same=%d\n", ret, !ret && written == size && !memcmp(out, font, size));
    free(out); out = NULL;

    ret = create(font, size, &out, &alloc, &written, 1, 0, 1, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    printf("package format ret=%lu\n", ret);
    ret = create(font, size / 2, &out, &alloc, &written, 1, 0, 0, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    printf("truncated ret=%lu\n", ret);
    ret = create((unsigned char *)"garbage-garbage", 15, &out, &alloc, &written, 1, 0, 0, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    printf("garbage ret=%lu\n", ret);

    /* a TTC of two copies; the second face is subset */
    memset(ttc, 0, sizeof(ttc));
    memcpy(ttc, "ttcf", 4); p32(ttc + 4, 0x00010000); p32(ttc + 8, 2); p32(ttc + 12, 20); p32(ttc + 16, 20 + size);
    copy = malloc(4096);
    build_font(copy, 20); memcpy(ttc + 20, copy, size);
    build_font(copy, 20 + size); memcpy(ttc + 20 + size, copy, size);
    free(copy);
    tsize = 20 + size * 2;
    chars[0] = 'D';
    ret = create(ttc, tsize, &out, &alloc, &written, 1 | 4, 1, 0, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    report("ttc face 1 char D", ret, out, written);
    free(out); out = NULL;
    ret = create(ttc, tsize, &out, &alloc, &written, 1 | 4, 2, 0, 0, 3, 1, chars, 1, cb_alloc, cb_realloc, cb_free, NULL);
    printf("ttc bad index ret=%lu\n", ret);
    return 0;
}
