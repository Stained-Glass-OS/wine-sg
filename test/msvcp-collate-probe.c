/* Probe for patches/sg/4504: collate<char>/<wchar_t>::transform, num_get
 * _Hexdig and locale::_Locimp::_Clocptr_func of the msvcp DLL named on the
 * command line (msvcp60 .. msvcp90). Table driven. */
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

typedef void *(THISCALL *fn_ctor)(void *, size_t);
typedef void (THISCALL *fn_p)(void *);
typedef void *(THISCALL *fn_transform)(const void *, void *, const void *, const void *);
typedef int (THISCALL *fn_compare)(const void *, const void *, const void *, const void *, const void *);
typedef int (THISCALL *fn_hexdig)(void *, int, int, int, int);
typedef void **(__cdecl *fn_clocptr)(void);
typedef const void *(__cdecl *fn_classic)(void);

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

/* basic_string of msvcp60 .. msvcp90: allocator pointer (70+: and a 16 byte
 * buffer union), size, reserved */
struct string_v6 { void *alloc; void *ptr; size_t size, res; };
struct string_v7 { void *alloc; union { char buf[16]; void *ptr; } data; size_t size, res; };

static const void *str_data(const void *s, int v6, unsigned int elem)
{
    if (v6) return ((const struct string_v6 *)s)->ptr;
    return ((const struct string_v7 *)s)->res >= 16 / elem ? ((const struct string_v7 *)s)->data.ptr
                                                          : ((const struct string_v7 *)s)->data.buf;
}
static size_t str_size(const void *s, int v6)
{
    return v6 ? ((const struct string_v6 *)s)->size : ((const struct string_v7 *)s)->size;
}

static int sign(int x) { return x < 0 ? -1 : x > 0; }

int main(int argc, char **argv)
{
    static const char *pairs[][2] = {{"abc", "abd"}, {"b", "a"}, {"same", "same"}, {"", "x"}, {"long string past sixteen", "long string past sixteem"}};
    static const struct { char dig; int value; } hex[] =
    {
        {'0', 0}, {'9', 9}, {'a', 10}, {'f', 15}, {'A', 10}, {'F', 15}, {'g', -1}, {'G', -1}, {'/', -1}, {':', -1},
    };
    unsigned char coll[256], coll_w[256], s1[64], s2[64];
    fn_ctor ctor, ctor_w;
    fn_p dtor;
    fn_transform transform, transform_w;
    fn_compare compare;
    fn_hexdig hexdig, hexdig_w;
    fn_clocptr clocptr;
    fn_classic classic;
    HMODULE mod;
    unsigned int i;
    int v6;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    v6 = !strcmp(argv[1], "msvcp60");
#define GET(v, a64, a32) v = (void *)GetProcAddress(mod, N(a64, a32)); check(v != NULL, "export %s", #v)
    GET(ctor, "??0?$collate@D@std@@QEAA@_K@Z", "??0?$collate@D@std@@QAE@I@Z");
    dtor = (void *)GetProcAddress(mod, N("??1?$collate@D@std@@MEAA@XZ", "??1?$collate@D@std@@MAE@XZ"));
    if (!dtor) dtor = (void *)GetProcAddress(mod, N("??1?$collate@D@std@@UEAA@XZ", "??1?$collate@D@std@@UAE@XZ"));
    check(dtor != NULL, "export dtor");
    GET(transform, "?transform@?$collate@D@std@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@PEBD0@Z",
                   "?transform@?$collate@D@std@@QBE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@PBD0@Z");
    GET(compare, "?compare@?$collate@D@std@@QEBAHPEBD000@Z", "?compare@?$collate@D@std@@QBEHPBD000@Z");
    /* msvcp90 only */
    hexdig = (void *)GetProcAddress(mod, N("?_Hexdig@?$num_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@AEBAHDDDD@Z",
                "?_Hexdig@?$num_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@ABEHDDDD@Z"));
    hexdig_w = (void *)GetProcAddress(mod, N("?_Hexdig@?$num_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@AEBAHGGGG@Z",
                  "?_Hexdig@?$num_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@ABEHGGGG@Z"));
    GET(ctor_w, "??0?$collate@G@std@@QEAA@_K@Z", "??0?$collate@G@std@@QAE@I@Z");
    GET(transform_w, "?transform@?$collate@G@std@@QEBA?AV?$basic_string@GU?$char_traits@G@std@@V?$allocator@G@2@@2@PEBG0@Z",
                     "?transform@?$collate@G@std@@QBE?AV?$basic_string@GU?$char_traits@G@std@@V?$allocator@G@2@@2@PBG0@Z");
    GET(classic, "?classic@locale@std@@SAAEBV12@XZ", "?classic@locale@std@@SAABV12@XZ");
    clocptr = (void *)GetProcAddress(mod, N("?_Clocptr_func@_Locimp@locale@std@@CAAEAPEAV123@XZ", "?_Clocptr_func@_Locimp@locale@std@@CAAAPAV123@XZ"));
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    /* sort keys order as the strings compare */
    ctor(coll, 0);
    for (i = 0; i < ARRAYSIZE(pairs); i++)
    {
        const char *a = pairs[i][0], *b = pairs[i][1];
        int cmp = compare(coll, a, a + strlen(a), b, b + strlen(b)), keycmp;
        size_t la, lb;

        memset(s1, 0, sizeof(s1)); memset(s2, 0, sizeof(s2));
        transform(coll, s1, a, a + strlen(a));
        transform(coll, s2, b, b + strlen(b));
        la = str_size(s1, v6); lb = str_size(s2, v6);
        keycmp = memcmp(str_data(s1, v6, 1), str_data(s2, v6, 1), la < lb ? la : lb);
        if (!keycmp) keycmp = (la > lb) - (la < lb);
        check(sign(keycmp) == sign(cmp) && (!*a || la), "transform(\"%s\") vs (\"%s\"): key order %d, compare %d, key sizes %u %u",
              a, b, sign(keycmp), cmp, (unsigned)la, (unsigned)lb);
    }
    dtor(coll);

    /* the unsigned short facet: a key for each string, equal strings equal keys */
    ctor_w(coll_w, 0);
    {
        static const unsigned short w1[] = {'a','b','c'}, w2[] = {'a','b','c'}, w3[] = {'a','b','d'};
        unsigned short k1[64], k3[64];
        size_t l1, l2, l3;

        memset(s1, 0, sizeof(s1));
        transform_w(coll_w, s1, w1, w1 + 3);
        l1 = str_size(s1, v6);
        check(l1 > 0, "wide transform gives a key (%u)", (unsigned)l1);
        memcpy(k1, str_data(s1, v6, 2), (l1 < 64 ? l1 : 64) * 2);
        transform_w(coll_w, s2, w2, w2 + 3);
        l2 = str_size(s2, v6);
        check(l1 == l2 && !memcmp(k1, str_data(s2, v6, 2), l1 * 2), "equal wide strings have equal keys");
        transform_w(coll_w, s2, w3, w3 + 3);
        l3 = str_size(s2, v6);
        memcpy(k3, str_data(s2, v6, 2), (l3 < 64 ? l3 : 64) * 2);
        check(l3 != l1 || memcmp(k1, k3, l1 * 2), "different wide strings have different keys");
    }

    /* _Hexdig with the facet's '0', 'a' and 'A' */
    for (i = 0; hexdig && hexdig_w && i < ARRAYSIZE(hex); i++)
    {
        int r = hexdig(NULL, hex[i].dig, '0', 'a', 'A'), rw = hexdig_w(NULL, hex[i].dig, '0', 'a', 'A');
        check(r == hex[i].value && rw == hex[i].value, "_Hexdig('%c') = %d / %d (expected %d)", hex[i].dig, r, rw, hex[i].value);
    }

    if (clocptr)
    {
        void **p = clocptr();
        check(p && *p == *(void *const *)classic(), "_Clocptr_func gives the classic locale's implementation");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
