/* Probe for patches/sg/4517: ucrtbase's mbrtoc16, mbrtoc32, c16rtomb and
 * c32rtomb (UTF-8 whatever the locale) and their _o_ forms.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { unsigned long w; unsigned short b, s; } state_t;  /* UCRT _Mbstatet */
#define ILSEQ ((size_t)-1)
#define PARTIAL ((size_t)-2)
#define PENDING ((size_t)-3)

int main(int argc, char **argv)
{
    /* one string at a time from the initial state: the results of each call
     * of mbrtoc32 (with n bytes) and of mbrtoc16 */
    static const struct { const char *str; size_t n; size_t ret; unsigned int c32; unsigned short c16[2]; } dec[] =
    {
        {"A", 1, 1, 'A', {'A'}},
        {"", 1, 0, 0, {0}},
        {"\xc3\xa9", 2, 2, 0xe9, {0xe9}},
        {"\xe2\x82\xac", 3, 3, 0x20ac, {0x20ac}},
        {"\xef\xbf\xbd", 3, 3, 0xfffd, {0xfffd}},
        {"\xf0\x9f\x98\x80", 4, 4, 0x1f600, {0xd83d, 0xde00}},
        {"\xf4\x8f\xbf\xbf", 4, 4, 0x10ffff, {0xdbff, 0xdfff}},
        {"\xc3\xa9", 1, PARTIAL, 0, {0}},
        {"\xe2\x82\xac", 2, PARTIAL, 0, {0}},
        {"A", 0, PARTIAL, 0, {0}},
        {"\x80", 1, ILSEQ, 0, {0}},
        {"\xc0\x80", 2, ILSEQ, 0, {0}},
        {"\xc1\xbf", 2, ILSEQ, 0, {0}},
        {"\xe0\x80\x80", 3, ILSEQ, 0, {0}},
        {"\xed\xa0\x80", 3, ILSEQ, 0, {0}},
        {"\xf0\x80\x80\x80", 4, ILSEQ, 0, {0}},
        {"\xf4\x90\x80\x80", 4, ILSEQ, 0, {0}},
        {"\xf8\x88\x80\x80\x80", 5, ILSEQ, 0, {0}},
        {"\xc3" "A", 2, ILSEQ, 0, {0}},
    };
    static const struct { unsigned int c; size_t ret; const char *out; } enc32[] =
    {
        {'A', 1, "A"}, {0, 1, ""}, {0xe9, 2, "\xc3\xa9"}, {0x7ff, 2, "\xdf\xbf"}, {0x800, 3, "\xe0\xa0\x80"},
        {0x20ac, 3, "\xe2\x82\xac"}, {0xffff, 3, "\xef\xbf\xbf"}, {0x10000, 4, "\xf0\x90\x80\x80"},
        {0x1f600, 4, "\xf0\x9f\x98\x80"}, {0x10ffff, 4, "\xf4\x8f\xbf\xbf"},
        {0xd800, ILSEQ, NULL}, {0xdfff, ILSEQ, NULL}, {0x110000, ILSEQ, NULL},
    };
    /* sequences of char16_t through one state */
    static const struct { unsigned short c[3]; unsigned int count; size_t ret[3]; const char *out; } enc16[] =
    {
        {{'A'}, 1, {1}, "A"},
        {{0xe9}, 1, {2}, "\xc3\xa9"},
        {{0x20ac}, 1, {3}, "\xe2\x82\xac"},
        {{0xd83d, 0xde00}, 2, {0, 4}, "\xf0\x9f\x98\x80"},
        {{0xdbff, 0xdfff}, 2, {0, 4}, "\xf4\x8f\xbf\xbf"},
        {{0xde00}, 1, {ILSEQ}, ""},
        {{0xd83d, 'A'}, 2, {0, ILSEQ}, ""},
        {{0xd83d, 0xd83d}, 2, {0, ILSEQ}, ""},
    };
    size_t (__cdecl *p_mbrtoc32)(unsigned int *, const char *, size_t, state_t *);
    size_t (__cdecl *p_mbrtoc16)(unsigned short *, const char *, size_t, state_t *);
    size_t (__cdecl *p_c32rtomb)(char *, unsigned int, state_t *);
    size_t (__cdecl *p_c16rtomb)(char *, unsigned short, state_t *);
    int *(__cdecl *p_errno)(void);
    unsigned int i, j, pass;
    HMODULE mod;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_errno = (void *)GetProcAddress(mod, "_errno");

    for (pass = 0; pass < 2; pass++)
    {
        const char *pre = pass ? "_o_" : "";
        char name[32];

#define GET(v, n) sprintf(name, "%s%s", pre, n); v = (void *)GetProcAddress(mod, name); check(v != NULL, "%s exported", name)
        GET(p_mbrtoc32, "mbrtoc32"); GET(p_mbrtoc16, "mbrtoc16");
        if (!pass) { GET(p_c32rtomb, "c32rtomb"); GET(p_c16rtomb, "c16rtomb"); }
        if (failures) { printf("RESULT: FAIL\n"); return 1; }

        for (i = 0; i < ARRAYSIZE(dec); i++)
        {
            state_t st = {0};
            unsigned int c32 = 0xbad;
            unsigned short c16 = 0xbad;
            size_t r;

            *p_errno() = 0;
            r = p_mbrtoc32(&c32, dec[i].str, dec[i].n, &st);
            check(r == dec[i].ret && (r >= PARTIAL || c32 == dec[i].c32) && (r != ILSEQ || *p_errno() == EILSEQ),
                  "%smbrtoc32 case %u: %Id %#x", pre, i, r, c32);

            memset(&st, 0, sizeof(st));
            r = p_mbrtoc16(&c16, dec[i].str, dec[i].n, &st);
            check(r == dec[i].ret && (r >= PARTIAL || c16 == dec[i].c16[0]), "%smbrtoc16 case %u: %Id %#x", pre, i, r, c16);
            if (dec[i].c16[1])
            {
                r = p_mbrtoc16(&c16, "Z", 1, &st);
                check(r == PENDING && c16 == dec[i].c16[1], "%smbrtoc16 case %u low surrogate: %Id %#x", pre, i, r, c16);
                r = p_mbrtoc16(&c16, "Z", 1, &st);
                check(r == 1 && c16 == 'Z', "%smbrtoc16 case %u after the pair: %Id %#x", pre, i, r, c16);
            }
        }

        /* a four byte character one byte at a time */
        {
            static const char s[] = "\xf0\x9f\x98\x80";
            static const size_t want[] = {PARTIAL, PARTIAL, PARTIAL, 1};
            state_t st = {0};
            unsigned int c32 = 0;
            unsigned short c16 = 0;

            for (j = 0; j < 4; j++)
            {
                size_t r = p_mbrtoc32(&c32, s + j, 1, &st);
                check(r == want[j] && (j < 3 || c32 == 0x1f600), "%smbrtoc32 byte %u: %Id %#x", pre, j, r, c32);
            }
            memset(&st, 0, sizeof(st));
            for (j = 0; j < 4; j++)
            {
                size_t r = p_mbrtoc16(&c16, s + j, 1, &st);
                check(r == want[j] && (j < 3 || c16 == 0xd83d), "%smbrtoc16 byte %u: %Id %#x", pre, j, r, c16);
            }
        }
        /* a NULL string resets the state */
        {
            state_t st = {0};
            unsigned int c32 = 0;
            size_t r = p_mbrtoc32(&c32, "\xe2\x82", 2, &st);

            check(r == PARTIAL, "%smbrtoc32 partial", pre);
            r = p_mbrtoc32(NULL, NULL, 0, &st);
            /* the same as mbrtoc32(NULL, "", 1, state): an error with a partial character */
            check(r == 0 || r == ILSEQ, "%smbrtoc32(NULL string) %Id", pre, r);
            r = p_mbrtoc32(&c32, "B", 1, &st);
            check(r == 1 && c32 == 'B', "%smbrtoc32 after reset: %Id %#x", pre, r, c32);
            r = p_mbrtoc32(&c32, "\xc3", 1, NULL);  /* the internal state */
            r = p_mbrtoc32(&c32, "\xa9", 1, NULL);
            check(r == 1 && c32 == 0xe9, "%smbrtoc32 internal state: %Id %#x", pre, r, c32);
        }
    }

    for (i = 0; i < ARRAYSIZE(enc32); i++)
    {
        char buf[8] = {0};
        state_t st = {0};
        size_t r;

        *p_errno() = 0;
        r = p_c32rtomb(buf, enc32[i].c, &st);
        check(r == enc32[i].ret && (r == ILSEQ ? *p_errno() == EILSEQ : !memcmp(buf, enc32[i].out, r)),
              "c32rtomb(%#x): %Id", enc32[i].c, r);
    }
    check(p_c32rtomb(NULL, 0x20ac, NULL) == 1, "c32rtomb(NULL)");

    for (i = 0; i < ARRAYSIZE(enc16); i++)
    {
        char buf[16] = {0}, *p = buf;
        state_t st = {0};
        int ok = 1;

        for (j = 0; j < enc16[i].count; j++)
        {
            size_t r = p_c16rtomb(p, enc16[i].c[j], &st);
            if (r != enc16[i].ret[j]) ok = 0;
            if (r != ILSEQ) p += r;
        }
        check(ok && (size_t)(p - buf) == strlen(enc16[i].out) && !memcmp(buf, enc16[i].out, p - buf),
              "c16rtomb sequence %u: %Iu bytes", i, (size_t)(p - buf));
    }
    {
        state_t st = {0};
        char buf[8];
        size_t r;

        p_c16rtomb(buf, 0xd83d, &st);
        r = p_c16rtomb(NULL, 0, &st);
        check(r == 1, "c16rtomb(NULL) %Id", r);
        r = p_c16rtomb(buf, 'A', &st);
        check(r == 1 && buf[0] == 'A', "c16rtomb after reset: %Id", r);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
