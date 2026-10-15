/* Probe for patches/sg/4553, in the DLL named on the command line:
 * ctype<char>::classic_table (msvcp100 .. msvcp140, msvcp_win), the "C"
 * locale's character classes, against a table and ctype<char>'s own
 * protected classic_table where it is exported; and
 * _Locinfo::_W_Gettnames (msvcp110 and later), the time names of the
 * "C" locale with the wide day and month names, beside _Gettnames.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

#define UP 0x1
#define LO 0x2
#define DI 0x4
#define SP 0x8
#define PU 0x10
#define CN 0x20
#define BL 0x40
#define HX 0x80

/* whether the time names at p hold a pointer to the wide or narrow string s */
static int has_name(void *p, const void *s, int wide)
{
    void **slot = p;
    unsigned int i;

    for (i = 0; i < 200; i++)
    {
        if (IsBadReadPtr(slot[i], 16)) continue;
        if (wide ? !wcscmp(slot[i], s) : !strcmp(slot[i], s)) return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { unsigned char c; short bits; } classes[] =
    {
        {'A', UP | HX}, {'Z', UP}, {'a', LO | HX}, {'z', LO}, {'0', DI | HX}, {'9', DI | HX}, {' ', SP | BL},
        {'\t', SP | CN | BL}, {'\n', SP | CN}, {'!', PU}, {'~', PU}, {0, CN}, {0x7f, CN}, {0x80, 0}, {0xe9, 0}, {'G', UP},
    };
    const short *(__cdecl *p_table)(void), *(__cdecl *p_prot)(void);
    HMODULE mod;
    int ver;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ver = !strcmp(argv[1], "msvcp_win") ? 140 : atoi(argv[1] + 5);

    p_table = (void *)GetProcAddress(mod, N("?classic_table@?$ctype@D@std@@SAPEBFXZ", "?classic_table@?$ctype@D@std@@SAPBFXZ"));
    p_prot = (void *)GetProcAddress(mod, N("?classic_table@?$ctype@D@std@@KAPEBFXZ", "?classic_table@?$ctype@D@std@@KAPBFXZ"));
    check(p_table != NULL, "classic_table exported");
    if (p_table)
    {
        const short *t = p_table();

        check(t != NULL && (!p_prot || p_prot() == t), "classic_table %p", t);
        for (i = 0; t && i < ARRAYSIZE(classes); i++)
            /* the blank class of a tab is left out: not checked against Windows */
            check((t[classes[i].c] & 0xff & ~BL) == (classes[i].bits & ~BL), "class of %#x: %#x", classes[i].c, t[classes[i].c]);
        if (t) check(t[' '] & BL, "space is blank");
    }

    if (ver >= 110)
    {
        void *(THISCALL *locinfo_ctor)(void *, const char *) = (void *)GetProcAddress(mod,
                N("??0_Locinfo@std@@QEAA@PEBD@Z", "??0_Locinfo@std@@QAE@PBD@Z"));
        void *(THISCALL *gettnames)(void *, void **) = (void *)GetProcAddress(mod,
                N("?_Gettnames@_Locinfo@std@@QEBA?AV_Timevec@2@XZ", "?_Gettnames@_Locinfo@std@@QBE?AV_Timevec@2@XZ"));
        void *(THISCALL *w_gettnames)(void *, void **) = (void *)GetProcAddress(mod,
                N("?_W_Gettnames@_Locinfo@std@@QEBA?AV_Timevec@2@XZ", "?_W_Gettnames@_Locinfo@std@@QBE?AV_Timevec@2@XZ"));
        static char locinfo[4096];
        void *tv = NULL, *tv2 = NULL;

        check(locinfo_ctor && gettnames && w_gettnames, "_Locinfo, _Gettnames, _W_Gettnames exported");
        if (locinfo_ctor && gettnames && w_gettnames)
        {
            static const WCHAR *const wide[] = {L"Sunday", L"Sat", L"January", L"Dec", L"AM", L"PM"};
            static const char *const narrow[] = {"Sunday", "Saturday", "January", "December"};

            locinfo_ctor(locinfo, "C");
            check(w_gettnames(locinfo, &tv) == &tv && tv != NULL, "_W_Gettnames returns the time names %p", tv);
            gettnames(locinfo, &tv2);
            check(tv2 && tv2 != tv, "_Gettnames, a copy of its own");
            for (i = 0; tv && i < ARRAYSIZE(wide); i++)
                check(has_name(tv, wide[i], 1), "_W_Gettnames has %ls", wide[i]);
            for (i = 0; tv && i < ARRAYSIZE(narrow); i++)
                check(has_name(tv, narrow[i], 0), "_W_Gettnames has %s", narrow[i]);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
