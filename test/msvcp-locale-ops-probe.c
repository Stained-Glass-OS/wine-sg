/* Probe for patches/sg/4502: std::locale operator=, == and != of the msvcp DLL
 * named on the command line (msvcp60 .. msvcp90 export them). */
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

typedef void *(THISCALL *fn_cstr)(void *, const char *, int);
typedef void *(THISCALL *fn_pp)(void *, const void *);
typedef unsigned char (THISCALL *fn_cmp)(const void *, const void *);
typedef void (THISCALL *fn_p)(void *);
typedef const void *(__cdecl *fn_classic)(void);

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    void *a, *b, *d, *e;   /* a std::locale is one pointer to its implementation */
    fn_cstr ctor_cstr;
    fn_pp copy, assign;
    fn_cmp equal, not_equal;
    fn_p ctor, dtor;
    fn_classic classic;
    const void *c;
    HMODULE mod;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ctor_cstr = (void *)GetProcAddress(mod, N("??0locale@std@@QEAA@PEBDH@Z", "??0locale@std@@QAE@PBDH@Z"));
    ctor = (void *)GetProcAddress(mod, N("??0locale@std@@QEAA@XZ", "??0locale@std@@QAE@XZ"));
    copy = (void *)GetProcAddress(mod, N("??0locale@std@@QEAA@AEBV01@@Z", "??0locale@std@@QAE@ABV01@@Z"));
    dtor = (void *)GetProcAddress(mod, N("??1locale@std@@QEAA@XZ", "??1locale@std@@QAE@XZ"));
    assign = (void *)GetProcAddress(mod, N("??4locale@std@@QEAAAEAV01@AEBV01@@Z", "??4locale@std@@QAEAAV01@ABV01@@Z"));
    equal = (void *)GetProcAddress(mod, N("??8locale@std@@QEBA_NAEBV01@@Z", "??8locale@std@@QBE_NABV01@@Z"));
    not_equal = (void *)GetProcAddress(mod, N("??9locale@std@@QEBA_NAEBV01@@Z", "??9locale@std@@QBE_NABV01@@Z"));
    classic = (void *)GetProcAddress(mod, N("?classic@locale@std@@SAAEBV12@XZ", "?classic@locale@std@@SAABV12@XZ"));
    check(ctor_cstr && ctor && copy && dtor && assign && equal && not_equal && classic, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    c = classic();
    ctor_cstr(&a, "C", 0x3f);
    copy(&b, &a);
    ctor(&d);
    ctor_cstr(&e, "C", 0x3f);

    {
        static const struct { const char *what; int lhs, rhs, equal; } table[] =
        {
            {"a copy", 0, 1, 1},
            {"another \"C\" locale", 0, 4, 1},
            {"the classic locale", 0, 2, 1},
            {"itself", 0, 0, 1},
        };
        const void *objs[5] = {&a, &b, c, &d, &e};
        unsigned int i;

        for (i = 0; i < ARRAYSIZE(table); i++)
        {
            check(!!equal(objs[table[i].lhs], objs[table[i].rhs]) == table[i].equal, "== %s", table[i].what);
            check(!!not_equal(objs[table[i].lhs], objs[table[i].rhs]) == !table[i].equal, "!= %s", table[i].what);
        }
    }

    check(a != e, "two \"C\" locales have their own implementations");
    check(assign(&d, &a) == &d, "assignment returns this");
    check(d == a, "assignment shares the implementation");
    check(assign(&d, &d) == &d && d == a, "self assignment");
    check(assign(&e, &e) == &e, "self assignment of another");

    dtor(&a); dtor(&b); dtor(&d); dtor(&e);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
