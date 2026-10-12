/* Probe for patches/sg/4503: codecvt<char16_t, char, _Mbstatet> and
 * codecvt<char32_t, char, _Mbstatet> (UTF-8 <-> UTF-16 / UTF-32) of the DLL
 * named on the command line (msvcp140, msvcp_win). Table driven; lines
 * "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define N(a64, a32) a64
#else
#define THISCALL __attribute__((thiscall))
#define N(a64, a32) a32
#endif

enum { OK = 0, PARTIAL = 1, ERR = 2, NOCONV = 3 };
enum { consume_header = 4, generate_header = 2 };

typedef struct { int wchar; unsigned short byte, state; } mbstate;
typedef void *(THISCALL *fn_ctor_mode)(void *, const void *, ULONG, int, size_t);
typedef void *(THISCALL *fn_ctor_refs)(void *, size_t);
typedef void (THISCALL *fn_p)(void *);
typedef int (THISCALL *fn_int)(const void *);
typedef int (THISCALL *fn_in)(const void *, mbstate *, const char *, const char *, const char **, void *, void *, void **);
typedef int (THISCALL *fn_out)(const void *, mbstate *, const void *, const void *, const void **, char *, char *, char **);
typedef int (THISCALL *fn_len)(const void *, mbstate *, const char *, const char *, size_t);
typedef int (THISCALL *fn_unshift)(const void *, mbstate *, char *, char *, char **);
typedef void *(THISCALL *fn_locinfo)(void *, const char *);

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

struct facet_funcs
{
    const char *name;
    unsigned int size;   /* sizeof the internal character */
    fn_ctor_mode ctor_mode;
    fn_ctor_refs ctor_refs;
    fn_p dtor;
    fn_in in;
    fn_out out;
    fn_len length;
    fn_unshift unshift;
};

static HMODULE mod;
static fn_int encoding, max_length, always_noconv;
static unsigned char locinfo[1024];

static void *get(const char *n64, const char *n32)
{
    void *p = (void *)GetProcAddress(mod, N(n64, n32));
    if (!p) { printf("FAIL export %s\n", N(n64, n32)); failures++; }
    return p;
}

#define CV(t) "?$codecvt@" t "DU_Mbstatet@@@std@@"
static void load(struct facet_funcs *f, const char *t)
{
    char a[256], b[256];
#define G(field, s64, s32) sprintf(a, s64, t); sprintf(b, s32, t); f->field = get(a, b)
    G(ctor_mode, "??0?$codecvt@%sDU_Mbstatet@@@std@@QEAA@AEBV_Locinfo@1@KW4_Codecvt_mode@1@_K@Z",
                 "??0?$codecvt@%sDU_Mbstatet@@@std@@QAE@ABV_Locinfo@1@KW4_Codecvt_mode@1@I@Z");
    G(ctor_refs, "??0?$codecvt@%sDU_Mbstatet@@@std@@QEAA@_K@Z", "??0?$codecvt@%sDU_Mbstatet@@@std@@QAE@I@Z");
    G(dtor, "??1?$codecvt@%sDU_Mbstatet@@@std@@MEAA@XZ", "??1?$codecvt@%sDU_Mbstatet@@@std@@MAE@XZ");
    if (!strcmp(t, "_S"))
    {
        G(in, "?in@?$codecvt@%sDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEBD1AEAPEBDPEA_S3AEAPEA_S@Z",
              "?in@?$codecvt@%sDU_Mbstatet@@@std@@QBEHAAU_Mbstatet@@PBD1AAPBDPA_S3AAPA_S@Z");
        G(out, "?out@?$codecvt@%sDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEB_S1AEAPEB_SPEAD3AEAPEAD@Z",
               "?out@?$codecvt@%sDU_Mbstatet@@@std@@QBEHAAU_Mbstatet@@PB_S1AAPB_SPAD3AAPAD@Z");
    }
    else
    {
        G(in, "?in@?$codecvt@%sDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEBD1AEAPEBDPEA_U3AEAPEA_U@Z",
              "?in@?$codecvt@%sDU_Mbstatet@@@std@@QBEHAAU_Mbstatet@@PBD1AAPBDPA_U3AAPA_U@Z");
        G(out, "?out@?$codecvt@%sDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEB_U1AEAPEB_UPEAD3AEAPEAD@Z",
               "?out@?$codecvt@%sDU_Mbstatet@@@std@@QBEHAAU_Mbstatet@@PB_U1AAPB_UPAD3AAPAD@Z");
    }
    G(length, "?length@?$codecvt@%sDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEBD1_K@Z",
              "?length@?$codecvt@%sDU_Mbstatet@@@std@@QBEHAAU_Mbstatet@@PBD1I@Z");
    G(unshift, "?unshift@?$codecvt@%sDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEAD1AEAPEAD@Z",
               "?unshift@?$codecvt@%sDU_Mbstatet@@@std@@QBEHAAU_Mbstatet@@PAD1AAPAD@Z");
#undef G
}

static const struct
{
    const char *what;
    ULONG max_code; int mode;
    const char *src; int src_len;
    unsigned int out_room;
    int ret, used;                 /* result, bytes consumed */
    unsigned int expect16[6], expect32[6], expect_count16, expect_count32;
} in_tests[] =
{
    {"ASCII, 2, 3 and 4 byte characters", 0x10ffff, 0, "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", 10, 8, OK, 10,
        {0x41, 0xe9, 0x20ac, 0xd83d, 0xde00}, {0x41, 0xe9, 0x20ac, 0x1f600}, 5, 4},
    {"a byte order mark is consumed", 0x10ffff, consume_header, "\xef\xbb\xbf" "A", 4, 8, OK, 4, {0x41}, {0x41}, 1, 1},
    {"a byte order mark is a character without consume_header", 0x10ffff, 0, "\xef\xbb\xbf" "A", 4, 8, OK, 4,
        {0xfeff, 0x41}, {0xfeff, 0x41}, 2, 2},
    {"an incomplete sequence is partial", 0x10ffff, 0, "A\xe2\x82", 3, 8, PARTIAL, 1, {0x41}, {0x41}, 1, 1},
    {"an overlong form is an error", 0x10ffff, 0, "\xc0\x80", 2, 8, ERR, 0, {0}, {0}, 0, 0},
    {"an encoded surrogate is an error", 0x10ffff, 0, "\xed\xa0\x80", 3, 8, ERR, 0, {0}, {0}, 0, 0},
    {"a stray continuation byte is an error", 0x10ffff, 0, "A\x80", 2, 8, ERR, 1, {0x41}, {0x41}, 1, 1},
    {"past the maximum code", 0xff, 0, "\xc3\xa9\xe2\x82\xac", 5, 8, ERR, 2, {0xe9}, {0xe9}, 1, 1},
    {"no room for the second half of a pair", 0x10ffff, 0, "\xf0\x9f\x98\x80", 4, 1, PARTIAL, 0, {0}, {0x1f600}, 0, 1},
};

static void test_facet(struct facet_funcs *f)
{
    static unsigned char obj[64];
    unsigned int buf[16], i, j;
    void *to_next;
    const char *from_next;
    char bytes[32], *byte_next;
    const void *wfrom_next;
    mbstate state;
    int ret;

    for (i = 0; i < ARRAYSIZE(in_tests); i++)
    {
        unsigned int count = f->size == 2 ? in_tests[i].expect_count16 : in_tests[i].expect_count32;
        const unsigned int *expect = f->size == 2 ? in_tests[i].expect16 : in_tests[i].expect32;
        int want_ret = in_tests[i].ret, want_used = in_tests[i].used, same = 1;

        if (f->size == 4 && i == ARRAYSIZE(in_tests) - 1) { want_ret = OK; want_used = 4; }  /* one char32_t fits */
        f->ctor_mode(obj, locinfo, in_tests[i].max_code, in_tests[i].mode, 0);
        memset(&state, 0, sizeof(state));
        memset(buf, 0xcc, sizeof(buf));
        ret = f->in(obj, &state, in_tests[i].src, in_tests[i].src + in_tests[i].src_len, &from_next,
                buf, (char *)buf + in_tests[i].out_room * f->size, &to_next);
        if (((char *)to_next - (char *)buf) / f->size != count) same = 0;
        for (j = 0; same && j < count; j++)
            if ((f->size == 2 ? ((unsigned short *)buf)[j] : buf[j]) != expect[j]) same = 0;
        check(ret == want_ret && from_next - in_tests[i].src == want_used && same,
              "%s in: %s (ret %d used %d, %u out)", f->name, in_tests[i].what, ret,
              (int)(from_next - in_tests[i].src), (unsigned)(((char *)to_next - (char *)buf) / f->size));
        f->dtor(obj);
    }

    /* out */
    f->ctor_mode(obj, locinfo, 0x10ffff, 0, 0);
    {
        static const unsigned short w16[] = {0x41, 0xe9, 0x20ac, 0xd83d, 0xde00};
        static const unsigned int w32[] = {0x41, 0xe9, 0x20ac, 0x1f600};
        const void *src = f->size == 2 ? (const void *)w16 : (const void *)w32;
        unsigned int n = f->size == 2 ? 5 : 4;

        memset(&state, 0, sizeof(state));
        ret = f->out(obj, &state, src, (const char *)src + n * f->size, &wfrom_next, bytes, bytes + sizeof(bytes), &byte_next);
        check(ret == OK && byte_next - bytes == 10 && !memcmp(bytes, "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", 10),
              "%s out: all characters (ret %d, %d bytes)", f->name, ret, (int)(byte_next - bytes));
        memset(&state, 0, sizeof(state));
        ret = f->out(obj, &state, src, (const char *)src + n * f->size, &wfrom_next, bytes, bytes + 5, &byte_next);
        check(ret == PARTIAL && byte_next - bytes == 3 && (const char *)wfrom_next - (const char *)src == 2 * f->size,
              "%s out: no room for the euro sign (ret %d, %d bytes)", f->name, ret, (int)(byte_next - bytes));
    }
    if (f->size == 2)
    {
        static const unsigned short high_end[] = {0x41, 0xd83d}, low[] = {0xde00, 0x41};

        memset(&state, 0, sizeof(state));
        ret = f->out(obj, &state, high_end, high_end + 2, &wfrom_next, bytes, bytes + sizeof(bytes), &byte_next);
        check(ret == PARTIAL && wfrom_next == high_end + 1 && byte_next - bytes == 1,
              "char16 out: a high surrogate at the end waits for the rest (ret %d)", ret);
        memset(&state, 0, sizeof(state));
        ret = f->out(obj, &state, low, low + 2, &wfrom_next, bytes, bytes + sizeof(bytes), &byte_next);
        check(ret == ERR && wfrom_next == low, "char16 out: a lone low surrogate is an error (ret %d)", ret);
    }
    else
    {
        static const unsigned int big[] = {0x110000};

        memset(&state, 0, sizeof(state));
        ret = f->out(obj, &state, big, big + 1, &wfrom_next, bytes, bytes + sizeof(bytes), &byte_next);
        check(ret == ERR, "char32 out: past U+10FFFF is an error (ret %d)", ret);
    }
    f->dtor(obj);

    /* the maximum code limits out too */
    f->ctor_mode(obj, locinfo, 0x7f, 0, 0);
    {
        static const unsigned short e16[] = {0x41, 0xe9};
        static const unsigned int e32[] = {0x41, 0xe9};
        const void *src = f->size == 2 ? (const void *)e16 : (const void *)e32;

        memset(&state, 0, sizeof(state));
        ret = f->out(obj, &state, src, (const char *)src + 2 * f->size, &wfrom_next, bytes, bytes + sizeof(bytes), &byte_next);
        check(ret == ERR && byte_next - bytes == 1, "%s out: past the maximum code (ret %d)", f->name, ret);
    }
    f->dtor(obj);

    /* generate_header writes the byte order mark once */
    f->ctor_mode(obj, locinfo, 0x10ffff, generate_header, 0);
    {
        static const unsigned short a16[] = {0x41};
        static const unsigned int a32[] = {0x41};
        const void *src = f->size == 2 ? (const void *)a16 : (const void *)a32;

        memset(&state, 0, sizeof(state));
        ret = f->out(obj, &state, src, (const char *)src + f->size, &wfrom_next, bytes, bytes + sizeof(bytes), &byte_next);
        check(ret == OK && byte_next - bytes == 4 && !memcmp(bytes, "\xef\xbb\xbf" "A", 4), "%s out: header first", f->name);
        ret = f->out(obj, &state, src, (const char *)src + f->size, &wfrom_next, bytes, bytes + sizeof(bytes), &byte_next);
        check(ret == OK && byte_next - bytes == 1 && bytes[0] == 'A', "%s out: header only once", f->name);
    }
    f->dtor(obj);

    /* length, the base class queries, unshift; the default facet consumes a header */
    f->ctor_refs(obj, 0);
    memset(&state, 0, sizeof(state));
    ret = f->length(obj, &state, "A\xc3\xa9\xf0\x9f\x98\x80" "B", "A\xc3\xa9\xf0\x9f\x98\x80" "B" + 8, 3);
    check(ret == (f->size == 2 ? 3 : 7), "%s length of 3 characters: %d", f->name, ret);
    check(encoding(obj) == 0, "%s encoding: %d", f->name, encoding(obj));
    check(!always_noconv(obj), "%s always_noconv", f->name);
    check(max_length(obj) >= 4, "%s max_length: %d", f->name, max_length(obj));
    memset(&state, 0, sizeof(state));
    ret = f->unshift(obj, &state, bytes, bytes + 4, &byte_next);
    check(ret == NOCONV && byte_next == bytes, "%s unshift: %d", f->name, ret);
    memset(&state, 0, sizeof(state));
    memset(buf, 0, sizeof(buf));
    ret = f->in(obj, &state, "\xef\xbb\xbf" "A", "\xef\xbb\xbf" "A" + 4, &from_next, buf, (char *)buf + 4 * f->size, &to_next);
    check(ret == OK && ((char *)to_next - (char *)buf) == f->size, "%s: the default facet consumes a header", f->name);
    f->dtor(obj);
}

int main(int argc, char **argv)
{
    struct facet_funcs f16 = {"char16", 2}, f32 = {"char32", 4};
    fn_locinfo locinfo_ctor;
    fn_p locinfo_dtor;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    load(&f16, "_S");
    load(&f32, "_U");
    encoding = get("?encoding@codecvt_base@std@@QEBAHXZ", "?encoding@codecvt_base@std@@QBEHXZ");
    max_length = get("?max_length@codecvt_base@std@@QEBAHXZ", "?max_length@codecvt_base@std@@QBEHXZ");
    always_noconv = get("?always_noconv@codecvt_base@std@@QEBA_NXZ", "?always_noconv@codecvt_base@std@@QBE_NXZ");
    locinfo_ctor = get("??0_Locinfo@std@@QEAA@PEBD@Z", "??0_Locinfo@std@@QAE@PBD@Z");
    locinfo_dtor = get("??1_Locinfo@std@@QEAA@XZ", "??1_Locinfo@std@@QAE@XZ");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    locinfo_ctor(locinfo, "C");
    test_facet(&f16);
    test_facet(&f32);
    locinfo_dtor(locinfo);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
