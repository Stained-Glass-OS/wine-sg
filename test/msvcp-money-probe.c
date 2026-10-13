/* Smoke probe for patches/sg/4538 (fast mode): the money facets of the DLL
 * named on the command line (msvcp71 .. msvcp90; msvcp60/70 build the same
 * code): moneypunct<char, false> in the "C" locale, _Mpunct::_Makpat for
 * C99's cs_precedes/sep_by_space/sign_posn, money_put into a string stream
 * and money_get from one. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
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

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HMODULE mod;
static int v80;

struct osit { void *container; unsigned char failed; void *strbuf; };  /* msvcp80 has the container */
struct osit_n { unsigned char failed; void *strbuf; };
struct isit { void *strbuf; unsigned char got; char val; };

#define SB_CTOR N("??0?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@H@Z", "??0?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@H@Z")
#define SB_CTOR_STR N("??0?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@H@Z", "??0?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@H@Z")
#define SB_STR N("?str@?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@XZ", "?str@?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@XZ")
#define OS_CTOR N("??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z", "??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QAE@PAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z")
#define STR_CTOR N("??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@PEBD@Z", "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBD@Z")
#define C_STR N("?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAPEBDXZ", "?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEPBDXZ")
#define MP_CTOR N("??0?$moneypunct@D$0A@@std@@QEAA@_K@Z", "??0?$moneypunct@D$0A@@std@@QAE@I@Z")
#define MAKPAT N("?_Makpat@?$_Mpunct@D@std@@AEAAXAEAUpattern@money_base@2@III@Z", "?_Makpat@?$_Mpunct@D@std@@AAEXAAUpattern@money_base@2@III@Z")
#define DECPT N("?decimal_point@?$_Mpunct@D@std@@QEBADXZ", "?decimal_point@?$_Mpunct@D@std@@QBEDXZ")
#define FRAC N("?frac_digits@?$_Mpunct@D@std@@QEBAHXZ", "?frac_digits@?$_Mpunct@D@std@@QBEHXZ")
#define NEGSIGN N("?negative_sign@?$_Mpunct@D@std@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@XZ", "?negative_sign@?$_Mpunct@D@std@@QBE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@XZ")
#define MPUT_CTOR N("??0?$money_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QEAA@_K@Z", "??0?$money_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QAE@I@Z")
#define MPUT N("?put@?$money_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QEBA?AV?$ostreambuf_iterator@DU?$char_traits@D@std@@@2@V32@_NAEAVios_base@2@DO@Z", "?put@?$money_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QBE?AV?$ostreambuf_iterator@DU?$char_traits@D@std@@@2@V32@_NAAVios_base@2@DO@Z")
#define MGET_CTOR N("??0?$money_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QEAA@_K@Z", "??0?$money_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QAE@I@Z")
#define MGET N("?get@?$money_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QEBA?AV?$istreambuf_iterator@DU?$char_traits@D@std@@@2@V32@0_NAEAVios_base@2@AEAHAEAO@Z", "?get@?$money_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QBE?AV?$istreambuf_iterator@DU?$char_traits@D@std@@@2@V32@0_NAAVios_base@2@AAHAAO@Z")

static void *get(const char *name)
{
    void *p = GetProcAddress(mod, name);
    if (!p) printf("missing %s\n", name);
    return p;
}

int main(int argc, char **argv)
{
    static char mp[256], sb[256], os[512], mput[64], mget[64], str[64], ret[64], sb2[256];
    void *(THISCALL *sb_ctor)(void *, int), *(THISCALL *sb_ctor_str)(void *, const void *, int), *(THISCALL *sb_str)(const void *, void *);
    void *(THISCALL *os_ctor)(void *, void *, int, int), *(THISCALL *str_ctor)(void *, const char *);
    const char *(THISCALL *c_str)(const void *);
    void *(THISCALL *mp_ctor)(void *, size_t), *(THISCALL *mput_ctor)(void *, size_t), *(THISCALL *mget_ctor)(void *, size_t);
    void (THISCALL *makpat)(void *, char *, unsigned, unsigned, unsigned);
    char (THISCALL *decpt)(const void *);
    int (THISCALL *frac)(const void *);
    void *(THISCALL *negsign)(const void *, void *);
    char *ios;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    v80 = !strcmp(argv[1], "msvcp80");
    sb_ctor = get(SB_CTOR); sb_ctor_str = get(SB_CTOR_STR); sb_str = get(SB_STR); os_ctor = get(OS_CTOR);
    str_ctor = get(STR_CTOR); c_str = get(C_STR); mp_ctor = get(MP_CTOR); makpat = get(MAKPAT);
    decpt = get(DECPT); frac = get(FRAC); negsign = get(NEGSIGN); mput_ctor = get(MPUT_CTOR); mget_ctor = get(MGET_CTOR);
    check(sb_ctor && sb_ctor_str && sb_str && os_ctor && str_ctor && c_str && mp_ctor && makpat && decpt && frac && negsign &&
          mput_ctor && mget_ctor && GetProcAddress(mod, MPUT) && GetProcAddress(mod, MGET), "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    mp_ctor(mp, 1);
    check(decpt(mp) == '.' && frac(mp) == 0, "moneypunct<char> \"C\": decimal point '%c', %d digits", decpt(mp), frac(mp));
    negsign(mp, ret);
    check(!strcmp(c_str(ret), "-"), "negative sign \"%s\"", c_str(ret));

    {
        /* symbol 2, sign 3, value 4, space 1, none 0 */
        static const struct { unsigned sep, prec, posn; char want[4]; } pats[] =
        {
            {0, 1, 1, {3, 2, 0, 4}}, {1, 1, 1, {3, 2, 1, 4}}, {0, 0, 1, {3, 4, 0, 2}}, {1, 0, 2, {4, 1, 2, 3}},
            {0, 1, 3, {3, 2, 0, 4}}, {0, 1, 4, {2, 3, 0, 4}}, {2, 1, 1, {3, 1, 2, 4}}, {9, 9, 9, {2, 3, 0, 4}},
        };
        for (i = 0; i < ARRAYSIZE(pats); i++)
        {
            char pat[4] = {7, 7, 7, 7};
            makpat(mp, pat, pats[i].sep, pats[i].prec, pats[i].posn);
            check(!memcmp(pat, pats[i].want, 4), "_Makpat(%u, %u, %u) = %d %d %d %d", pats[i].sep, pats[i].prec,
                  pats[i].posn, pat[0], pat[1], pat[2], pat[3]);
        }
    }

    /* money_put into a stream */
    {
        static const struct { double units; const char *want; } puts[] = {{12345, "12345"}, {-500, "-500"}, {0, "0"}};
        for (i = 0; i < ARRAYSIZE(puts); i++)
        {
            memset(sb, 0, sizeof(sb)); memset(os, 0, sizeof(os));
            sb_ctor(sb, 2 /* out */);
            os_ctor(os, sb, 0, 1);
            ios = os + (*(int **)os)[1];
            mput_ctor(mput, 1);
            if (v80)
            {
                struct osit it = {NULL, 0, sb}, out;
                ((void *(THISCALL *)(void *, struct osit *, struct osit, int, void *, char, double))GetProcAddress(mod, MPUT))(mput, &out, it, 0, ios, ' ', puts[i].units);
            }
            else
            {
                struct osit_n it = {0, sb}, out;
                ((void *(THISCALL *)(void *, struct osit_n *, struct osit_n, int, void *, char, double))GetProcAddress(mod, MPUT))(mput, &out, it, 0, ios, ' ', puts[i].units);
            }
            sb_str(sb, ret);
            check(!strcmp(c_str(ret), puts[i].want), "money_put %g: \"%s\"", puts[i].units, c_str(ret));
        }
    }

    /* money_get from a stream */
    {
        static const struct { const char *in; double want; int ok; } gets[] = {{"123", 123, 1}, {"-77 rest", -77, 1}, {"x", 0, 0}};
        for (i = 0; i < ARRAYSIZE(gets); i++)
        {
            struct isit first, last = {NULL, 0, 0}, out;
            double val = -1;
            int err = 0;

            memset(sb2, 0, sizeof(sb2)); memset(os, 0, sizeof(os));
            str_ctor(str, gets[i].in);
            sb_ctor_str(sb2, str, 1 /* in */);
            os_ctor(os, sb2, 0, 1);
            ios = os + (*(int **)os)[1];
            mget_ctor(mget, 1);
            first.strbuf = sb2; first.got = 0; first.val = 0;
            ((void *(THISCALL *)(void *, struct isit *, struct isit, struct isit, int, void *, int *, double *))GetProcAddress(mod, MGET))(mget, &out, first, last, 0, ios, &err, &val);
            check(gets[i].ok ? !(err & 2) && val == gets[i].want : (err & 2) != 0, "money_get \"%s\": %g, state %#x", gets[i].in, val, err);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
