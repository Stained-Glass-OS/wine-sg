/* Smoke probe for patches/sg/4557 (fast mode), in the DLL named on the
 * command line (msvcp60 .. msvcp80): the older forms of the numpunct
 * constructors and _Init (no "use default names" flag), of
 * time_get::_Getcat (no locale) and msvcp70..80's
 * basic_ostream<wchar_t>(_Uninitialized). The facets built answer like
 * the "C" locale's. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HMODULE mod;
static void *get(const char *name) { return GetProcAddress(mod, name); }

int main(int argc, char **argv)
{
    static const char *const types[] = {"D", "_W", "G"};
    static char locinfo[4096];
    void *(THISCALL *locinfo_ctor)(void *, const char *);
    unsigned int t;
    int is60;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    is60 = !strcmp(argv[1], "msvcp60");
    locinfo_ctor = get(N("??0_Locinfo@std@@QEAA@PEBD@Z", "??0_Locinfo@std@@QAE@PBD@Z"));
    check(locinfo_ctor != NULL, "_Locinfo exported");
    if (!locinfo_ctor) { printf("RESULT: FAIL\n"); return 1; }
    locinfo_ctor(locinfo, "C");

    for (t = 0; t < 3; t++)
    {
        const char *T = types[t];
        char name[256];
        void *(THISCALL *ctor)(void *, void *, size_t), *(THISCALL *ctor_name)(void *, const char *, size_t);
        void (THISCALL *init)(void *, void *);
        int (THISCALL *point)(void *), (THISCALL *sep)(void *);
        unsigned int (__cdecl *getcat)(const void **);
        static char facet[3][3][512];

        if (is60 && t == 1) continue;  /* no wchar_t in msvcp60 */
        sprintf(name, N("??0?$numpunct@%s@std@@QEAA@AEBV_Locinfo@1@_K@Z", "??0?$numpunct@%s@std@@QAE@ABV_Locinfo@1@I@Z"), T);
        ctor = get(name);
        sprintf(name, N("?_Init@?$numpunct@%s@std@@IEAAXAEBV_Locinfo@2@@Z", "?_Init@?$numpunct@%s@std@@IAEXABV_Locinfo@2@@Z"), T);
        init = get(name);
        sprintf(name, N("?decimal_point@?$numpunct@%s@std@@QEBA%sXZ", "?decimal_point@?$numpunct@%s@std@@QBE%sXZ"), T, T);
        point = get(name);
        sprintf(name, N("?thousands_sep@?$numpunct@%s@std@@QEBA%sXZ", "?thousands_sep@?$numpunct@%s@std@@QBE%sXZ"), T, T);
        sep = get(name);
        sprintf(name, N("??0?$numpunct@%s@std@@IEAA@PEBD_K@Z", "??0?$numpunct@%s@std@@IAE@PBDI@Z"), T);
        ctor_name = get(name);
        check(ctor && init && point && sep, "%s: numpunct exports %p %p %p %p", T, ctor, init, point, sep);
        if (ctor && init && point && sep)
        {
            check(ctor(facet[t][0], locinfo, 1) == facet[t][0], "%s: numpunct(_Locinfo, refs)", T);
            /* the "C" thousands separator comes from localeconv: not checked */
            check((point(facet[t][0]) & 0xffff) == '.', "%s: decimal point '%c', separator %#x",
                  T, point(facet[t][0]) & 0xff, sep(facet[t][0]) & 0xffff);
            init(facet[t][0], locinfo);
            check((point(facet[t][0]) & 0xffff) == '.', "%s: _Init(_Locinfo)", T);
            if (ctor_name)
            {
                check(ctor_name(facet[t][1], "C", 1) == facet[t][1] && (point(facet[t][1]) & 0xffff) == '.',
                      "%s: numpunct(const char *, refs)", T);
            }
        }

        sprintf(name, "?_Getcat@?$time_get@%sV?$istreambuf_iterator@%sU?$char_traits@%s@std@@@std@@@std@@%s", T, T, T,
                N("SA_KPEAPEBVfacet@locale@2@@Z", "SAIPAPBVfacet@locale@2@@Z"));
        getcat = get(name);
        if (!is60 || getcat)
        {
            const void *f = NULL;

            check(getcat != NULL, "%s: time_get::_Getcat(facet **) exported", T);
            if (getcat)
            {
                check(getcat(NULL) == LC_TIME, "%s: _Getcat(NULL) = LC_TIME", T);
                check(getcat(&f) == LC_TIME && f != NULL, "%s: _Getcat builds a facet %p", T, f);
            }
        }
    }

    if (!is60)
    {
        void *(THISCALL *octor)(void *, int, int) = get(N("??0?$basic_ostream@_WU?$char_traits@_W@std@@@std@@QEAA@W4_Uninitialized@1@@Z",
                                                          "??0?$basic_ostream@_WU?$char_traits@_W@std@@@std@@QAE@W4_Uninitialized@1@@Z"));
        static char os[1024];

        check(octor != NULL, "basic_ostream<wchar_t>(_Uninitialized) exported");
        if (octor) check(octor(os, 0, 1) == os, "basic_ostream<wchar_t>(_Uninitialized)");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
