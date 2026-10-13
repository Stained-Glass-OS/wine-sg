/* Probe for patches/sg/4530: _Yarn<wchar_t> and _Yarn<unsigned short>
 * (msvcp140, msvcp_win), _Facet_base constructors, destructor and assignment
 * (msvcp110 and later), _UShinit and the assignments of the data-less
 * classes _Iosb<int>, _Init_locks and _Crt_new_delete, in the DLL named on
 * the command line. Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
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

struct yarn { WCHAR *str; WCHAR null_str; };
typedef void *(THISCALL *fn1)(void *);
typedef void *(THISCALL *fn2)(void *, const void *);
static HMODULE mod;

static void *get(const char *a64, const char *a32)
{
    void *p = GetProcAddress(mod, N(a64, a32));
    check(p != NULL, "%s exported", N(a64, a32));
    return p;
}

static void test_yarn(char t)
{
    const char *tn = t == 'G' ? "G" : "_W";
    char n[8][128];
    fn1 ctor, dtor, c_str, empty;
    fn2 ctor_str, copy, assign, assign_str;
    struct yarn a, b, c;
    const WCHAR *s;

#define M(i, f64, f32) sprintf(n[i], N(f64, f32), tn, tn)
    M(0, "??0?$_Yarn@%s@std@@QEAA@XZ", "??0?$_Yarn@%s@std@@QAE@XZ");
    M(1, "??1?$_Yarn@%s@std@@QEAA@XZ", "??1?$_Yarn@%s@std@@QAE@XZ");
    M(2, "?c_str@?$_Yarn@%s@std@@QEBAPEB%sXZ", "?c_str@?$_Yarn@%s@std@@QBEPB%sXZ");
    M(3, "?empty@?$_Yarn@%s@std@@QEBA_NXZ", "?empty@?$_Yarn@%s@std@@QBE_NXZ");
    M(4, "??0?$_Yarn@%s@std@@QEAA@PEB%s@Z", "??0?$_Yarn@%s@std@@QAE@PB%s@Z");
    M(5, "??0?$_Yarn@%s@std@@QEAA@AEBV01@@Z", "??0?$_Yarn@%s@std@@QAE@ABV01@@Z");
    M(6, "??4?$_Yarn@%s@std@@QEAAAEAV01@AEBV01@@Z", "??4?$_Yarn@%s@std@@QAEAAV01@ABV01@@Z");
    M(7, "??4?$_Yarn@%s@std@@QEAAAEAV01@PEB%s@Z", "??4?$_Yarn@%s@std@@QAEAAV01@PB%s@Z");
    ctor = get(n[0], n[0]); dtor = get(n[1], n[1]); c_str = get(n[2], n[2]); empty = get(n[3], n[3]);
    ctor_str = get(n[4], n[4]); copy = get(n[5], n[5]); assign = get(n[6], n[6]); assign_str = get(n[7], n[7]);
    if (!ctor || !dtor || !c_str || !empty || !ctor_str || !copy || !assign || !assign_str) return;

    ctor(&a);
    s = c_str(&a);
    check(s && !*s && ((unsigned char)(ULONG_PTR)empty(&a) & 1), "_Yarn<%c>: default is empty", t);
    ctor_str(&b, L"abc");
    s = c_str(&b);
    check(!wcscmp(s, L"abc") && !((unsigned char)(ULONG_PTR)empty(&b) & 1), "_Yarn<%c>(\"abc\")", t);
    copy(&c, &b);
    check(c.str && c.str != b.str && !wcscmp(c_str(&c), L"abc"), "_Yarn<%c> copy has its own buffer", t);
    assign_str(&b, L"longer string");
    check(!wcscmp(c_str(&c), L"abc") && !wcscmp(c_str(&b), L"longer string"), "_Yarn<%c> assign string", t);
    check(assign(&a, &b) == &a && a.str != b.str && !wcscmp(c_str(&a), L"longer string"), "_Yarn<%c> assign", t);
    assign(&a, &a);
    check(!wcscmp(c_str(&a), L"longer string"), "_Yarn<%c> self assign", t);
    dtor(&a); dtor(&b); dtor(&c);
}

int main(int argc, char **argv)
{
    int v140, v110;
    char obj[64];

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    v140 = !strcmp(argv[1], "msvcp140") || !strcmp(argv[1], "msvcp_win");
    v110 = v140 || !strcmp(argv[1], "msvcp110") || !strcmp(argv[1], "msvcp120");

    if (v140)
    {
        test_yarn('G');
        test_yarn('_');
    }
    if (v110)
    {
        fn2 ctor = get("??0_Facet_base@std@@QEAA@XZ", "??0_Facet_base@std@@QAE@XZ");
        fn2 copy = get("??0_Facet_base@std@@QEAA@AEBV01@@Z", "??0_Facet_base@std@@QAE@ABV01@@Z");
        fn1 dtor = get("??1_Facet_base@std@@UEAA@XZ", "??1_Facet_base@std@@UAE@XZ");
        fn2 assign = get("??4_Facet_base@std@@QEAAAEAV01@AEBV01@@Z", "??4_Facet_base@std@@QAEAAV01@ABV01@@Z");
        void *vtbl = GetProcAddress(mod, "??_7_Facet_base@std@@6B@");
        void *a[2], *b[2];

        if (ctor && copy && dtor && assign)
        {
            memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
            check(ctor(a, NULL) == a && a[0] == vtbl, "_Facet_base(): vtable %p (%p)", a[0], vtbl);
            check(copy(b, a) == b && b[0] == vtbl, "_Facet_base(const&)");
            b[0] = (void *)1;
            check(assign(b, a) == b && b[0] == (void *)1, "operator= leaves the vtable");
            dtor(a);
        }
    }
    {
        fn1 ctor = get("??0_UShinit@std@@QEAA@XZ", "??0_UShinit@std@@QAE@XZ");
        fn1 dtor = get("??1_UShinit@std@@QEAA@XZ", "??1_UShinit@std@@QAE@XZ");
        fn2 assign = get("??4_UShinit@std@@QEAAAEAV01@AEBV01@@Z", "??4_UShinit@std@@QAEAAV01@ABV01@@Z");
        fn2 iosb = get("??4?$_Iosb@H@std@@QEAAAEAV01@AEBV01@@Z", "??4?$_Iosb@H@std@@QAEAAV01@ABV01@@Z");
        fn2 locks = get("??4_Init_locks@std@@QEAAAEAV01@AEBV01@@Z", "??4_Init_locks@std@@QAEAAV01@ABV01@@Z");

        if (ctor && dtor && assign)
        {
            check(ctor(obj) == obj && assign(obj, obj + 8) == obj, "_UShinit");
            dtor(obj);
        }
        if (iosb && locks) check(iosb(obj, obj + 8) == obj && locks(obj, obj + 8) == obj, "_Iosb, _Init_locks assignment");
        if (v140)
        {
            fn2 crt = get("??4_Crt_new_delete@std@@QEAAAEAU01@AEBU01@@Z", "??4_Crt_new_delete@std@@QAEAAU01@ABU01@@Z");
            if (crt) check(crt(obj, obj + 8) == obj, "_Crt_new_delete assignment");
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
