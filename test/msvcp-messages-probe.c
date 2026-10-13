/* Smoke probe for patches/sg/4537 (fast mode): the messages facet of the DLL
 * named on the command line (msvcp60 .. msvcp90): construction, open (no
 * catalogs: -1), get (the default string), close, _Getcat, and the facet's
 * vtable and id exports. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
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

int main(int argc, char **argv)
{
    HMODULE mod;
    void *(THISCALL *ctor)(void *, size_t);
    void (THISCALL *dtor)(void *);
    int (THISCALL *open)(const void *, const void *, const void *);
    void *(THISCALL *get)(const void *, void *, int, int, int, const void *);
    void (THISCALL *close)(const void *, int);
    void *(THISCALL *str_ctor)(void *, const char *);
    const char *(THISCALL *c_str)(const void *);
    void *vtbl, *id;
    char facet[64], name[64], dflt[64], ret[64];
    const char *s;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ctor = (void *)GetProcAddress(mod, N("??0?$messages@D@std@@QEAA@_K@Z", "??0?$messages@D@std@@QAE@I@Z"));
    dtor = (void *)GetProcAddress(mod, N("??1?$messages@D@std@@MEAA@XZ", "??1?$messages@D@std@@MAE@XZ"));
    if (!dtor) dtor = (void *)GetProcAddress(mod, N("??1?$messages@D@std@@UEAA@XZ", "??1?$messages@D@std@@UAE@XZ"));
    open = (void *)GetProcAddress(mod, N("?open@?$messages@D@std@@QEBAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@AEBVlocale@2@@Z",
                                         "?open@?$messages@D@std@@QBEHABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@ABVlocale@2@@Z"));
    get = (void *)GetProcAddress(mod, N("?get@?$messages@D@std@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@HHHAEBV32@@Z",
                                        "?get@?$messages@D@std@@QBE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@HHHABV32@@Z"));
    close = (void *)GetProcAddress(mod, N("?close@?$messages@D@std@@QEBAXH@Z", "?close@?$messages@D@std@@QBEXH@Z"));
    str_ctor = (void *)GetProcAddress(mod, N("??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@PEBD@Z",
                                             "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBD@Z"));
    c_str = (void *)GetProcAddress(mod, N("?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAPEBDXZ",
                                          "?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEPBDXZ"));
    vtbl = GetProcAddress(mod, "??_7?$messages@D@std@@6B@");
    id = GetProcAddress(mod, "?id@?$messages@D@std@@2V0locale@2@A");
    check(ctor && dtor && open && get && close && str_ctor && c_str && vtbl && id, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    memset(facet, 0, sizeof(facet));
    check(ctor(facet, 1) == facet && *(void **)facet == vtbl, "constructor sets the vtable");
    str_ctor(name, "catalog");
    check(open(facet, name, NULL) == -1, "open: no catalogs");
    str_ctor(dflt, "default text");
    memset(ret, 0, sizeof(ret));
    get(facet, ret, -1, 1, 2, dflt);
    s = c_str(ret);
    check(s && !strcmp(s, "default text"), "get returns the default: %s", s ? s : "(null)");
    close(facet, -1);
    dtor(facet);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
