/* Probe for patches/sg/4510: the _ismbb* / _ismbcblank character tests of the
 * CRT named on the command line (msvcrt, msvcr90, msvcr120, ucrtbase), in the
 * default code page and in code page 932 (with kana single-byte symbols). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef int (__cdecl *fn_is)(unsigned int);
typedef int (__cdecl *fn_is_l)(unsigned int, void *);

static const struct { const char *name; unsigned int c; int cp932; int expect; } tests[] =
{
    {"_ismbbalnum", 'a', 0, 1}, {"_ismbbalnum", '5', 0, 1}, {"_ismbbalnum", '!', 0, 0}, {"_ismbbalnum", 0xb1, 0, 0},
    {"_ismbbalpha", 'Z', 0, 1}, {"_ismbbalpha", '5', 0, 0},
    {"_ismbbpunct", '!', 0, 1}, {"_ismbbpunct", 'a', 0, 0},
    {"_ismbbprint", ' ', 0, 1}, {"_ismbbgraph", ' ', 0, 0}, {"_ismbbgraph", '~', 0, 1}, {"_ismbbprint", '\n', 0, 0},
    {"_ismbbkalnum", 0xb1, 0, 0}, {"_ismbbkpunct", 0xa1, 0, 0}, {"_ismbbkprint", 0xb1, 0, 0},
    {"_ismbbkalnum", 0xb1, 1, 1}, {"_ismbbkalnum", 0xa1, 1, 0}, {"_ismbbkalnum", 'a', 1, 0},
    {"_ismbbkpunct", 0xa1, 1, 1}, {"_ismbbkpunct", 0xa5, 1, 1}, {"_ismbbkpunct", 0xb1, 1, 0},
    {"_ismbbkprint", 0xa1, 1, 1}, {"_ismbbkprint", 0xdf, 1, 1}, {"_ismbbkprint", 'a', 1, 0}, {"_ismbbkprint", 0xe0, 1, 0},
    {"_ismbbalnum", 0xb1, 1, 1}, {"_ismbbalpha", 0xb1, 1, 1}, {"_ismbbpunct", 0xa1, 1, 1}, {"_ismbbpunct", 0xb1, 1, 0},
    {"_ismbbgraph", 0xa1, 1, 1}, {"_ismbbprint", 0xb1, 1, 1}, {"_ismbbalnum", 'a', 1, 1},
    {"_ismbbblank", ' ', 0, 1}, {"_ismbbblank", '\t', 0, 1}, {"_ismbbblank", 'a', 0, 0},
    {"_ismbcblank", ' ', 0, 1}, {"_ismbcblank", 0x8140, 0, 0}, {"_ismbcblank", 0x8140, 1, 1}, {"_ismbcblank", 0x8141, 1, 0},
};

int main(int argc, char **argv)
{
    int (__cdecl *setmbcp)(int);
    HMODULE mod;
    unsigned int i;
    char buf[64];

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    setmbcp = (void *)GetProcAddress(mod, "_setmbcp");
    check(setmbcp != NULL, "_setmbcp");
    for (i = 0; setmbcp && i < ARRAYSIZE(tests); i++)
    {
        fn_is f = (void *)GetProcAddress(mod, tests[i].name);
        fn_is_l fl;

        sprintf(buf, "%s_l", tests[i].name);
        fl = (void *)GetProcAddress(mod, buf);
        if (!f && (!strcmp(tests[i].name, "_ismbbblank") || !strcmp(tests[i].name, "_ismbcblank")) &&
            strcmp(argv[1], "msvcr120") && strcmp(argv[1], "ucrtbase"))
            continue; /* only msvcr120 and later */
        check(f != NULL, "%s export", tests[i].name);
        if (!f) continue;
        setmbcp(tests[i].cp932 ? 932 : 1252);
        check(!!f(tests[i].c) == tests[i].expect, "%s(%#x) cp %d = %d", tests[i].name, tests[i].c,
              tests[i].cp932 ? 932 : 1252, f(tests[i].c));
        if (fl) check(!!fl(tests[i].c, NULL) == tests[i].expect, "%s_l(%#x, NULL)", tests[i].name, tests[i].c);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
