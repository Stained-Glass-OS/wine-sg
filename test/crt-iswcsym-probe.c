/* Probe for patches/sg/4521: _iswcsym_l and _iswcsymf_l (msvcr80 and later,
 * ucrtbase) against a table, and ucrtbase's _o___pwctype_func and
 * _o__ismbclower against their plain forms.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    static const struct { unsigned short c; int sym, symf; } tests[] =
    {
        {'a', 1, 1}, {'Z', 1, 1}, {'_', 1, 1}, {'0', 1, 0}, {'9', 1, 0}, {' ', 0, 0}, {'-', 0, 0}, {'$', 0, 0},
        {0xe9, 1, 1}, {0x3b1, 1, 1}, {0x660, 1, 0}, {0x2014, 0, 0}, {0x4e00, 1, 1}, {0, 0, 0},
    };
    int (__cdecl *p_sym)(unsigned short, void *), (__cdecl *p_symf)(unsigned short, void *);
    HMODULE mod;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_sym = (void *)GetProcAddress(mod, "_iswcsym_l");
    p_symf = (void *)GetProcAddress(mod, "_iswcsymf_l");
    check(p_sym && p_symf, "exports");
    if (!p_sym || !p_symf) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        int s = p_sym(tests[i].c, NULL), f = p_symf(tests[i].c, NULL);
        check(!s == !tests[i].sym && !f == !tests[i].symf, "U+%04x: _iswcsym_l %d, _iswcsymf_l %d", tests[i].c, s, f);
    }

    if (!strcmp(argv[1], "ucrtbase"))
    {
        const unsigned short *(__cdecl *p_pw)(void) = (void *)GetProcAddress(mod, "__pwctype_func");
        const unsigned short *(__cdecl *p_opw)(void) = (void *)GetProcAddress(mod, "_o___pwctype_func");
        int (__cdecl *p_low)(unsigned int) = (void *)GetProcAddress(mod, "_ismbclower");
        int (__cdecl *p_olow)(unsigned int) = (void *)GetProcAddress(mod, "_o__ismbclower");

        check(p_opw && p_olow && p_opw() == p_pw() && p_opw()['a'] == p_pw()['a'], "_o___pwctype_func");
        check(p_olow && !p_olow('a') == !p_low('a') && p_olow('a') && !p_olow('A'), "_o__ismbclower");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
