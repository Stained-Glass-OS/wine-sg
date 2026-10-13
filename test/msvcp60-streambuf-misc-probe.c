/* Smoke probe for patches/sg/4567 (fast mode): msvcp60's pubseekpos
 * overloads with the mode as a short (against the int ones, a table of
 * positions on a string buffer) and basic_streambuf<unsigned short>::getloc.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { long off; __int64 pos; int state; } fpos_int;

#define SBD "?$basic_streambuf@DU?$char_traits@D@std@@@std@@"
#define STRD "?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@"

int main(void)
{
    static const __int64 positions[] = {0, 3, 5, 2, 9, -1};
    HMODULE mod = LoadLibraryA("msvcp60.dll");
    void *(THISCALL *str_ctor)(void *, const char *, void *), *(THISCALL *sb_ctor)(void *, void *, int);
    void *(THISCALL *sbg_ctor)(void *, int), *(THISCALL *getloc)(void *, void *), (THISCALL *loc_dtor)(void *);
#ifdef _WIN64
    fpos_int *(THISCALL *seek_i)(void *, fpos_int *, fpos_int *, int), *(THISCALL *seek_s)(void *, fpos_int *, fpos_int *, short);
#else
    fpos_int *(THISCALL *seek_i)(void *, fpos_int *, fpos_int, int), *(THISCALL *seek_s)(void *, fpos_int *, fpos_int, short);
#endif
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    str_ctor = (void *)GetProcAddress(mod, N("??0" STRD "std@@QEAA@PEBDAEBV?$allocator@D@1@@Z", "??0" STRD "std@@QAE@PBDABV?$allocator@D@1@@Z"));
    sb_ctor = (void *)GetProcAddress(mod, N("??0?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@AEBV" STRD "1@H@Z",
                                            "??0?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@ABV" STRD "1@H@Z"));
    seek_i = (void *)GetProcAddress(mod, N("?pubseekpos@" SBD "QEAA?AV?$fpos@H@2@V32@H@Z", "?pubseekpos@" SBD "QAE?AV?$fpos@H@2@V32@H@Z"));
    seek_s = (void *)GetProcAddress(mod, N("?pubseekpos@" SBD "QEAA?AV?$fpos@H@2@V32@F@Z", "?pubseekpos@" SBD "QAE?AV?$fpos@H@2@V32@F@Z"));
    sbg_ctor = (void *)GetProcAddress(mod, N("??0?$basic_stringbuf@GU?$char_traits@G@std@@V?$allocator@G@2@@std@@QEAA@H@Z",
                                             "??0?$basic_stringbuf@GU?$char_traits@G@std@@V?$allocator@G@2@@std@@QAE@H@Z"));
    getloc = (void *)GetProcAddress(mod, N("?getloc@?$basic_streambuf@GU?$char_traits@G@std@@@std@@QEAA?AVlocale@2@XZ",
                                           "?getloc@?$basic_streambuf@GU?$char_traits@G@std@@@std@@QAE?AVlocale@2@XZ"));
    loc_dtor = (void *)GetProcAddress(mod, N("??1locale@std@@QEAA@XZ", "??1locale@std@@QAE@XZ"));
    check(str_ctor && sb_ctor && seek_i && seek_s && sbg_ctor && getloc && loc_dtor, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    {
        static char str[64], sb[512];
        char alloc = 0;

        str_ctor(str, "hello", &alloc);
        sb_ctor(sb, str, 3 /* in|out */);
        for (i = 0; i < ARRAYSIZE(positions); i++)
        {
            fpos_int p = {0, positions[i], 0}, a, b;

            memset(&a, 0x55, sizeof(a)); memset(&b, 0xaa, sizeof(b));
#ifdef _WIN64
            seek_i(sb, &a, &p, 1);
            seek_s(sb, &b, &p, 1);
#else
            seek_i(sb, &a, p, 1);
            seek_s(sb, &b, p, 1);
#endif
            check(a.off == b.off && a.pos == b.pos && a.state == b.state, "pubseekpos(%I64d, in): short form %ld %I64d, int form %ld %I64d",
                  positions[i], b.off, b.pos, a.off, a.pos);
        }
    }
    {
        static char sb[512], loc[64];

        sbg_ctor(sb, 3);
        check(getloc(sb, loc) == loc && *(void **)loc != NULL, "basic_streambuf<unsigned short>::getloc");
        loc_dtor(loc);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
