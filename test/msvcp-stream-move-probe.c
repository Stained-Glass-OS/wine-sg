/* Probe for patches/sg/4535: the move constructors and move assignments of
 * basic_istream, basic_ostream and basic_iostream and basic_ios::move, for
 * char, wchar_t and unsigned short, in the DLL named on the command line
 * (msvcp100 .. msvcp140, msvcp_win). The moved-to stream takes the state
 * and tie but no stream buffer; the source keeps its buffer and loses its
 * tie; move assignment swaps all but the buffers.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define X64 1
#else
#define THISCALL __attribute__((thiscall))
#define X64 0
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HMODULE mod;
static const char *T;   /* "D", "_W", "G" */
static int v100;

static void *get(const char *fmt)
{
    char name[512];
    void *p;
    /* %1 = type */
    char tmp[512], *o = name;
    const char *f;
    for (f = fmt; *f; f++)
    {
        if (*f == '%' && f[1] == 'T') { strcpy(o, T); o += strlen(T); f++; }
        else *o++ = *f;
    }
    *o = 0;
    strcpy(tmp, name);
    p = GetProcAddress(mod, name);
    if (!p) printf("missing %s\n", tmp);
    return p;
}

static char *ios_of(void *stream)
{
    const int *vbtable = *(const int **)stream;
    return (char *)stream + vbtable[1];
}

typedef void *(THISCALL *ctor3)(void *, void *, int);
typedef void *(THISCALL *ctor4)(void *, void *, int, int);
typedef void *(THISCALL *fn1)(void *);
typedef void *(THISCALL *fn2)(void *, void *);
typedef int (THISCALL *getflags)(void *);
typedef int (THISCALL *setflags)(void *, int);

static void test_type(void)
{
    static char a[1024], b[1024], c[1024], d[1024];
    fn2 rdbuf_set, tie_set, ios_move;
    fn1 rdbuf_get, tie_get;
    getflags flags_get;
    setflags flags_set;
    void *ctor, *move_ctor, *assign;
    const char *cls[3] = {"istream", "ostream", "iostream"};
    int i;

    rdbuf_set = get(X64 ? "?rdbuf@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QEAAPEAV?$basic_streambuf@%TU?$char_traits@%T@std@@@2@PEAV32@@Z"
                        : "?rdbuf@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QAEPAV?$basic_streambuf@%TU?$char_traits@%T@std@@@2@PAV32@@Z");
    rdbuf_get = get(X64 ? "?rdbuf@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QEBAPEAV?$basic_streambuf@%TU?$char_traits@%T@std@@@2@XZ"
                        : "?rdbuf@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QBEPAV?$basic_streambuf@%TU?$char_traits@%T@std@@@2@XZ");
    tie_set = get(X64 ? "?tie@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QEAAPEAV?$basic_ostream@%TU?$char_traits@%T@std@@@2@PEAV32@@Z"
                      : "?tie@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QAEPAV?$basic_ostream@%TU?$char_traits@%T@std@@@2@PAV32@@Z");
    tie_get = get(X64 ? "?tie@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QEBAPEAV?$basic_ostream@%TU?$char_traits@%T@std@@@2@XZ"
                      : "?tie@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QBEPAV?$basic_ostream@%TU?$char_traits@%T@std@@@2@XZ");
    flags_get = (void *)GetProcAddress(mod, X64 ? "?flags@ios_base@std@@QEBAHXZ" : "?flags@ios_base@std@@QBEHXZ");
    flags_set = (void *)GetProcAddress(mod, X64 ? "?flags@ios_base@std@@QEAAHH@Z" : "?flags@ios_base@std@@QAEHH@Z");
    /* msvcp100 has only the rvalue form */
    ios_move = v100 ? get(X64 ? "?move@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QEAAX$$QEAV12@@Z" : "?move@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QAEX$$QAV12@@Z")
                    : get(X64 ? "?move@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QEAAXAEAV12@@Z" : "?move@?$basic_ios@%TU?$char_traits@%T@std@@@std@@QAEXAAV12@@Z");
    check(rdbuf_set && rdbuf_get && tie_set && tie_get && flags_get && flags_set && ios_move, "%s: accessors", T);
    if (!rdbuf_set || !rdbuf_get || !tie_set || !tie_get || !flags_get || !flags_set || !ios_move) return;

    for (i = 0; i < 3; i++)
    {
        char fmt[512];

        if (i == 0) ctor = get(X64 ? "??0?$basic_istream@%TU?$char_traits@%T@std@@@std@@QEAA@PEAV?$basic_streambuf@%TU?$char_traits@%T@std@@@1@_N@Z"
                                   : "??0?$basic_istream@%TU?$char_traits@%T@std@@@std@@QAE@PAV?$basic_streambuf@%TU?$char_traits@%T@std@@@1@_N@Z");
        else if (i == 1) ctor = get(X64 ? "??0?$basic_ostream@%TU?$char_traits@%T@std@@@std@@QEAA@PEAV?$basic_streambuf@%TU?$char_traits@%T@std@@@1@_N@Z"
                                        : "??0?$basic_ostream@%TU?$char_traits@%T@std@@@std@@QAE@PAV?$basic_streambuf@%TU?$char_traits@%T@std@@@1@_N@Z");
        else ctor = get(X64 ? "??0?$basic_iostream@%TU?$char_traits@%T@std@@@std@@QEAA@PEAV?$basic_streambuf@%TU?$char_traits@%T@std@@@1@@Z"
                            : "??0?$basic_iostream@%TU?$char_traits@%T@std@@@std@@QAE@PAV?$basic_streambuf@%TU?$char_traits@%T@std@@@1@@Z");
        sprintf(fmt, X64 ? "??0?$basic_%s@%%TU?$char_traits@%%T@std@@@std@@%cEAA@$$QEAV01@@Z" : "??0?$basic_%s@%%TU?$char_traits@%%T@std@@@std@@%cAE@$$QAV01@@Z",
                cls[i], v100 ? 'Q' : 'I');
        move_ctor = get(fmt);
        sprintf(fmt, X64 ? "??4?$basic_%s@%%TU?$char_traits@%%T@std@@@std@@%cEAAAEAV01@$$QEAV01@@Z" : "??4?$basic_%s@%%TU?$char_traits@%%T@std@@@std@@%cAEAAV01@$$QAV01@@Z",
                cls[i], v100 ? 'Q' : 'I');
        assign = get(fmt);
        check(ctor && move_ctor && assign, "%s %s: exports", T, cls[i]);
        if (!ctor || !move_ctor || !assign) continue;

        memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
        if (i == 2) ((ctor3)ctor)(a, NULL, 1); else ((ctor4)ctor)(a, NULL, 0, 1);
        rdbuf_set(ios_of(a), (void *)0x1110);
        tie_set(ios_of(a), (void *)0x2220);
        flags_set(ios_of(a), 0x800);
        ((ctor3)move_ctor)(b, a, 1);
        check(!rdbuf_get(ios_of(b)) && tie_get(ios_of(b)) == (void *)0x2220 && flags_get(ios_of(b)) == 0x800,
              "%s %s move: the new one has the tie and flags, no buffer", T, cls[i]);
        check(rdbuf_get(ios_of(a)) == (void *)0x1110 && !tie_get(ios_of(a)), "%s %s move: the source keeps its buffer, loses its tie", T, cls[i]);

        memset(c, 0, sizeof(c)); memset(d, 0, sizeof(d));
        if (i == 2) { ((ctor3)ctor)(c, NULL, 1); ((ctor3)ctor)(d, NULL, 1); }
        else { ((ctor4)ctor)(c, NULL, 0, 1); ((ctor4)ctor)(d, NULL, 0, 1); }
        rdbuf_set(ios_of(c), (void *)0x3330); flags_set(ios_of(c), 0x200);
        rdbuf_set(ios_of(d), (void *)0x4440); flags_set(ios_of(d), 0x400);
        check(((fn2)assign)(d, c) == d && flags_get(ios_of(d)) == 0x200 && flags_get(ios_of(c)) == 0x400 &&
              rdbuf_get(ios_of(d)) == (void *)0x4440 && rdbuf_get(ios_of(c)) == (void *)0x3330,
              "%s %s move assignment swaps all but the buffers", T, cls[i]);
    }
    /* basic_ios::move by itself */
    {
        char *ia = ios_of(a), *ib = ios_of(b);
        tie_set(ib, (void *)0x5550);
        flags_set(ib, 0x100);
        rdbuf_set(ia, (void *)0x6660);
        tie_set(ia, (void *)0x7770);  /* swapped into ib, which must end without a tie */
        ios_move(ia, ib);
        check(!rdbuf_get(ia) && tie_get(ia) == (void *)0x5550 && flags_get(ia) == 0x100 && !tie_get(ib), "%s basic_ios::move", T);
    }
}

int main(int argc, char **argv)
{
    static const char *types[] = {"D", "_W", "G"};
    int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    v100 = !strcmp(argv[1], "msvcp100");
    for (i = 0; i < 3; i++)
    {
        T = types[i];
        test_type();
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
