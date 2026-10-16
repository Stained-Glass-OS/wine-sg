/* Probe for patches/sg/4509: time_get<unsigned short> and time_base of the
 * msvcp DLL named on the command line (msvcp90, msvcp140, msvcp_win). */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN64
#define THISCALL
#define N(a64, a32) a64
#else
#define THISCALL __attribute__((thiscall))
#define N(a64, a32) a32
#endif

#define TG "?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@"
#define TGW "?$time_get@_WV?$istreambuf_iterator@_WU?$char_traits@_W@std@@@std@@@std@@"
#define SB "?$basic_streambuf@GU?$char_traits@G@std@@@std@@"
#define IOS "?$basic_ios@GU?$char_traits@G@std@@@std@@"

struct iter { void *strbuf; unsigned char got; unsigned short val; };
typedef void *(THISCALL *fn_p)(void *);
typedef void *(THISCALL *fn_refs)(void *, size_t);
typedef void (THISCALL *fn_setg)(void *, unsigned short *, unsigned short *, unsigned short *);
typedef void (THISCALL *fn_init)(void *, void *, int);
typedef struct iter *(THISCALL *fn_get)(const void *, struct iter *, struct iter, struct iter, void *, int *, struct tm *);
typedef int (THISCALL *fn_order)(const void *);
typedef size_t (__cdecl *fn_getcat)(const void **, const void *);
typedef const void *(__cdecl *fn_classic)(void);

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    static unsigned char sb[256], ios[1024], tb[64];
    static unsigned short text[] = {'2', '0', '2', '5', ' ', 0};
    fn_p sb_ctor, ios_ctor, tb_ctor, tb_dtor;
    fn_refs tb_ctor_refs;
    fn_setg setg;
    fn_init init;
    fn_get get_year;
    fn_order date_order, date_order_w;
    fn_getcat getcat, getcat_w;
    fn_classic classic;
    const void *facet = NULL, *facet_w = NULL, *vtable, *tb_vtable;
    struct iter first, last, ret;
    struct tm tm;
    HMODULE mod;
    int state = 0;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
#define GET(v, a64, a32) v = (void *)GetProcAddress(mod, N(a64, a32)); check(v != NULL, "export %s", #v)
    GET(sb_ctor, "??0" SB "IEAA@XZ", "??0" SB "IAE@XZ");
    GET(setg, "?setg@" SB "IEAAXPEAG00@Z", "?setg@" SB "IAEXPAG00@Z");
    GET(ios_ctor, "??0" IOS "IEAA@XZ", "??0" IOS "IAE@XZ");
    GET(init, "?init@" IOS "IEAAXPEAV?$basic_streambuf@GU?$char_traits@G@std@@@2@_N@Z", "?init@" IOS "IAEXPAV?$basic_streambuf@GU?$char_traits@G@std@@@2@_N@Z");
    GET(get_year, "?get_year@" TG "QEBA?AV?$istreambuf_iterator@GU?$char_traits@G@std@@@2@V32@0AEAVios_base@2@AEAHPEAUtm@@@Z",
                  "?get_year@" TG "QBE?AV?$istreambuf_iterator@GU?$char_traits@G@std@@@2@V32@0AAVios_base@2@AAHPAUtm@@@Z");
    GET(date_order, "?date_order@" TG "QEBA?AW4dateorder@time_base@2@XZ", "?date_order@" TG "QBE?AW4dateorder@time_base@2@XZ");
    GET(date_order_w, "?date_order@" TGW "QEBA?AW4dateorder@time_base@2@XZ", "?date_order@" TGW "QBE?AW4dateorder@time_base@2@XZ");
    GET(getcat, "?_Getcat@" TG "SA_KPEAPEBVfacet@locale@2@PEBV42@@Z", "?_Getcat@" TG "SAIPAPBVfacet@locale@2@PBV42@@Z");
    GET(getcat_w, "?_Getcat@" TGW "SA_KPEAPEBVfacet@locale@2@PEBV42@@Z", "?_Getcat@" TGW "SAIPAPBVfacet@locale@2@PBV42@@Z");
    GET(classic, "?classic@locale@std@@SAAEBV12@XZ", "?classic@locale@std@@SAABV12@XZ");
    GET(tb_ctor_refs, "??0time_base@std@@QEAA@_K@Z", "??0time_base@std@@QAE@I@Z");
    GET(tb_ctor, "??_Ftime_base@std@@QEAAXXZ", "??_Ftime_base@std@@QAEXXZ");
    GET(tb_dtor, "??1time_base@std@@UEAA@XZ", "??1time_base@std@@UAE@XZ");
    vtable = GetProcAddress(mod, "??_7" TG "6B@");
    tb_vtable = GetProcAddress(mod, "??_7time_base@std@@6B@");
    check(vtable && tb_vtable, "vtables exported");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(getcat(&facet, classic()) == 5 /* LC_TIME */ && facet, "_Getcat makes a facet");
    getcat_w(&facet_w, classic());
    check(*(const void **)facet == vtable && *(const void **)facet_w != vtable, "the facet has the unsigned short vtable");
    check(date_order(facet) == date_order_w(facet_w), "date_order as the wchar_t facet: %d", date_order(facet));

    sb_ctor(sb);
    setg(sb, text, text, text + 5);
    memset(ios, 0, sizeof(ios));
    ios_ctor(ios);
    init(ios, sb, 0);
    first.strbuf = sb; first.got = 0; first.val = 0;
    last.strbuf = NULL; last.got = 0; last.val = 0;
    memset(&tm, 0, sizeof(tm));
    get_year(facet, &ret, first, last, ios, &state, &tm);
    check(tm.tm_year == 125 && !(state & 2 /* failbit */), "get_year(\"2025\"): tm_year %d, state %#x", tm.tm_year, state);

    memset(tb, 0, sizeof(tb));
    check(tb_ctor_refs(tb, 1) == tb && *(const void **)tb == tb_vtable, "time_base(refs) has its vtable");
    tb_dtor(tb);
    check(tb_ctor(tb) == tb && *(const void **)tb == tb_vtable, "time_base()");
    tb_dtor(tb);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
