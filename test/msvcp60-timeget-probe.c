/* Smoke probe for patches/sg/4539 (fast mode): msvcp60's time_get<unsigned
 * short> (its wide time_get, mapped to the msvcp90 sources' functions) and
 * time_base: construction and get_year/get_date on an empty input (failbit
 * and eofbit). Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
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

struct isit { void *strbuf; unsigned char got; unsigned short val; };

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA(argc > 1 ? argv[1] : "msvcp60");
    void *(THISCALL *ctor)(void *, size_t), *(THISCALL *tb_ctor)(void *, size_t);
    void (THISCALL *dtor)(void *);
    void *(THISCALL *get_year)(const void *, struct isit *, struct isit, struct isit, void *, int *, void *);
    void *(THISCALL *get_date)(const void *, struct isit *, struct isit, struct isit, void *, int *, void *);
    void *(THISCALL *ios_ctor)(void *);
    static char facet[256], tb[64], ios[256];
    struct isit first = {NULL, 0, 0}, last = {NULL, 0, 0}, out;
    int tm[9], err;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ctor = (void *)GetProcAddress(mod, N("??0?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QEAA@_K@Z",
                                         "??0?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QAE@I@Z"));
    get_year = (void *)GetProcAddress(mod, N("?get_year@?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QEBA?AV?$istreambuf_iterator@GU?$char_traits@G@std@@@2@V32@0AEAVios_base@2@AEAHPEAUtm@@@Z",
                                             "?get_year@?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QBE?AV?$istreambuf_iterator@GU?$char_traits@G@std@@@2@V32@0AAVios_base@2@AAHPAUtm@@@Z"));
    get_date = (void *)GetProcAddress(mod, N("?get_date@?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QEBA?AV?$istreambuf_iterator@GU?$char_traits@G@std@@@2@V32@0AEAVios_base@2@AEAHPEAUtm@@@Z",
                                             "?get_date@?$time_get@GV?$istreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@QBE?AV?$istreambuf_iterator@GU?$char_traits@G@std@@@2@V32@0AAVios_base@2@AAHPAUtm@@@Z"));
    tb_ctor = (void *)GetProcAddress(mod, N("??0time_base@std@@QEAA@_K@Z", "??0time_base@std@@QAE@I@Z"));
    dtor = (void *)GetProcAddress(mod, N("??1time_base@std@@UEAA@XZ", "??1time_base@std@@UAE@XZ"));
    ios_ctor = (void *)GetProcAddress(mod, N("??0ios_base@std@@IEAA@XZ", "??0ios_base@std@@IAE@XZ"));
    check(ctor && get_year && get_date && tb_ctor && dtor, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(tb_ctor(tb, 1) == tb && *(void **)tb, "time_base constructor");
    dtor(tb);
    check(ctor(facet, 1) == facet && *(void **)facet, "time_get<unsigned short> constructor");
    memset(ios, 0, sizeof(ios));
    {
        void (THISCALL *ios_init)(void *) = (void *)GetProcAddress(mod, N("?_Init@ios_base@std@@IEAAXXZ", "?_Init@ios_base@std@@IAEXXZ"));
        check(ios_ctor && ios_init, "ios_base exports");
        if (!ios_ctor || !ios_init) { printf("RESULT: FAIL\n"); return 1; }
        ios_ctor(ios);
        ios_init(ios);
    }
    err = 0;
    get_year(facet, &out, first, last, ios, &err, tm);
    check((err & 3) == 3, "get_year on no input: state %#x", err);
    err = 0;
    get_date(facet, &out, first, last, ios, &err, tm);
    check((err & 2) != 0, "get_date on no input: state %#x", err);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
