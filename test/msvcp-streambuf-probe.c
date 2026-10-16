/* Probe for patches/sg/4500: basic_streambuf copy constructor and assignment,
 * basic_ios::set_rdbuf, through the exports of the msvcp DLL named on the
 * command line (msvcp100, msvcp110, msvcp120, msvcp140, msvcp_win). Each check
 * prints "ok" or "FAIL"; the last line is "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define SB(n, a64, a32) n a64
#else
#define THISCALL __attribute__((thiscall))
#define SB(n, a64, a32) n a32
#endif

typedef void *(THISCALL *fn_p)(void *);
typedef void *(THISCALL *fn_pp)(void *, const void *);
typedef void (THISCALL *fn_set3)(void *, char *, char *, char *);
typedef void (THISCALL *fn_init)(void *, void *, int);
typedef void (THISCALL *fn_state)(void *, unsigned int);
typedef int (THISCALL *fn_int)(void *);

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define CHARSB "?$basic_streambuf@DU?$char_traits@D@std@@@std@@"
#define CHARIOS "?$basic_ios@DU?$char_traits@D@std@@@std@@"

static const struct { const char *name; const char *what; } streambuf_types[] =
{
#ifdef _WIN64
    {"??0?$basic_streambuf@_WU?$char_traits@_W@std@@@std@@IEAA@XZ", "??0?$basic_streambuf@_WU?$char_traits@_W@std@@@std@@IEAA@AEBV01@@Z"},
    {"??0?$basic_streambuf@GU?$char_traits@G@std@@@std@@IEAA@XZ", "??0?$basic_streambuf@GU?$char_traits@G@std@@@std@@IEAA@AEBV01@@Z"},
#else
    {"??0?$basic_streambuf@_WU?$char_traits@_W@std@@@std@@IAE@XZ", "??0?$basic_streambuf@_WU?$char_traits@_W@std@@@std@@IAE@ABV01@@Z"},
    {"??0?$basic_streambuf@GU?$char_traits@G@std@@@std@@IAE@XZ", "??0?$basic_streambuf@GU?$char_traits@G@std@@@std@@IAE@ABV01@@Z"},
#endif
};

int main(int argc, char **argv)
{
    static char buf[64], other[64];
    static unsigned char __attribute__((aligned(16))) a[512], b[512], c[512], w1[512], w2[512], ios[1024];
    fn_p ctor, dtor, eback, gptr, egptr, pbase, pptr, epptr, ios_ctor, ios_dtor, rdbuf_get;
    fn_pp copy, assign, set_rdbuf, rdbuf_set;
    fn_set3 setg, setp;
    fn_init init;
    fn_state setstate;
    fn_int rdstate;
    HMODULE mod;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1])))
    {
        printf("FAIL cannot load %s\nRESULT: FAIL\n", argc < 2 ? "(none)" : argv[1]);
        return 1;
    }
#define GET(var, n64, n32) var = (void *)GetProcAddress(mod, SB(, n64, n32)); check(var != NULL, "export %s", #var)
    GET(ctor, "??0" CHARSB "IEAA@XZ", "??0" CHARSB "IAE@XZ");
    GET(dtor, "??1" CHARSB "UEAA@XZ", "??1" CHARSB "UAE@XZ");
    GET(copy, "??0" CHARSB "IEAA@AEBV01@@Z", "??0" CHARSB "IAE@ABV01@@Z");
    GET(assign, "??4" CHARSB "IEAAAEAV01@AEBV01@@Z", "??4" CHARSB "IAEAAV01@ABV01@@Z");
    GET(setg, "?setg@" CHARSB "IEAAXPEAD00@Z", "?setg@" CHARSB "IAEXPAD00@Z");
    GET(setp, "?setp@" CHARSB "IEAAXPEAD00@Z", "?setp@" CHARSB "IAEXPAD00@Z");
    GET(eback, "?eback@" CHARSB "IEBAPEADXZ", "?eback@" CHARSB "IBEPADXZ");
    GET(gptr, "?gptr@" CHARSB "IEBAPEADXZ", "?gptr@" CHARSB "IBEPADXZ");
    GET(egptr, "?egptr@" CHARSB "IEBAPEADXZ", "?egptr@" CHARSB "IBEPADXZ");
    GET(pbase, "?pbase@" CHARSB "IEBAPEADXZ", "?pbase@" CHARSB "IBEPADXZ");
    GET(pptr, "?pptr@" CHARSB "IEBAPEADXZ", "?pptr@" CHARSB "IBEPADXZ");
    GET(epptr, "?epptr@" CHARSB "IEBAPEADXZ", "?epptr@" CHARSB "IBEPADXZ");
    GET(ios_ctor, "??0" CHARIOS "IEAA@XZ", "??0" CHARIOS "IAE@XZ");
    GET(ios_dtor, "??1" CHARIOS "UEAA@XZ", "??1" CHARIOS "UAE@XZ");
    GET(init, "?init@" CHARIOS "IEAAXPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@_N@Z",
              "?init@" CHARIOS "IAEXPAV?$basic_streambuf@DU?$char_traits@D@std@@@2@_N@Z");
    GET(setstate, "?setstate@" CHARIOS "QEAAXI@Z", "?setstate@" CHARIOS "QAEXI@Z");
    GET(rdstate, "?rdstate@ios_base@std@@QEBAHXZ", "?rdstate@ios_base@std@@QBEHXZ");
    GET(rdbuf_get, "?rdbuf@" CHARIOS "QEBAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@XZ",
                   "?rdbuf@" CHARIOS "QBEPAV?$basic_streambuf@DU?$char_traits@D@std@@@2@XZ");
    GET(rdbuf_set, "?rdbuf@" CHARIOS "QEAAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@PEAV32@@Z",
                   "?rdbuf@" CHARIOS "QAEPAV?$basic_streambuf@DU?$char_traits@D@std@@@2@PAV32@@Z");
    GET(set_rdbuf, "?set_rdbuf@" CHARIOS "QEAAXPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@@Z",
                   "?set_rdbuf@" CHARIOS "QAEXPAV?$basic_streambuf@DU?$char_traits@D@std@@@2@@Z");
    if (failures)
    {
        printf("RESULT: FAIL\n");
        return 1;
    }

    /* copy constructor: same six pointers, same vtable, its own fields */
    ctor(a);
    setg(a, buf, buf + 2, buf + 10);
    setp(a, buf + 20, buf + 25, buf + 40);
    check(copy(b, a) == b, "copy constructor returns this");
    check(*(void **)b == *(void **)a, "copy has the streambuf vtable");
    {
        static const struct { const char *name; fn_p get; char *expect; } table[] =
        {
            {"eback", NULL, buf}, {"gptr", NULL, buf + 2}, {"egptr", NULL, buf + 10},
            {"pbase", NULL, buf + 20}, {"pptr", NULL, buf + 25}, {"epptr", NULL, buf + 40},
        };
        fn_p getters[6] = {eback, gptr, egptr, pbase, pptr, epptr};

        for (i = 0; i < 6; i++)
            check(getters[i](b) == table[i].expect, "copy %s: %p (expected %p)", table[i].name,
                  getters[i](b), table[i].expect);
        setg(a, other, other, other + 1);
        check(eback(b) == buf && gptr(b) == buf + 2, "the copy keeps its pointers when the original changes");

        /* assignment */
        ctor(c);
        check(eback(c) == NULL && pptr(c) == NULL, "a new streambuf is empty");
        setg(a, buf, buf + 2, buf + 10);
        check(assign(c, a) == c, "assignment returns this");
        for (i = 0; i < 6; i++)
            check(getters[i](c) == table[i].expect, "assigned %s: %p (expected %p)", table[i].name,
                  getters[i](c), table[i].expect);
        check(assign(c, c) == c && gptr(c) == buf + 2, "self assignment");
    }

    /* the wchar_t and unsigned short streambufs keep their own vtables */
    for (i = 0; i < ARRAYSIZE(streambuf_types); i++)
    {
        fn_p wctor = (void *)GetProcAddress(mod, streambuf_types[i].name);
        fn_pp wcopy = (void *)GetProcAddress(mod, streambuf_types[i].what);

        check(wctor && wcopy, "exports of %s", streambuf_types[i].what);
        if (!wctor || !wcopy) continue;
        wctor(w1);
        wcopy(w2, w1);
        check(*(void **)w2 == *(void **)w1, "copy %u has the vtable of its type", i);
    }

    /* set_rdbuf leaves the state alone, rdbuf(sb) clears it */
    memset(ios, 0, sizeof(ios));
    ios_ctor(ios);
    init(ios, a, 0);
    setstate(ios, 4 /* badbit */);
    check(rdstate(ios) & 4, "badbit set: %#x", rdstate(ios));
    set_rdbuf(ios, b);
    check(rdbuf_get(ios) == b, "set_rdbuf sets the buffer");
    check(rdstate(ios) & 4, "set_rdbuf keeps the state: %#x", rdstate(ios));
    check(rdbuf_set(ios, c) == b && rdstate(ios) == 0, "rdbuf(sb) returns the old one and clears the state");
    ios_dtor(ios);

    dtor(a); dtor(b); dtor(c);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
