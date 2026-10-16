/* Smoke probe for patches/sg/4542 (fast mode): msvcp60's argument-less
 * facet _Getcat functions return their locale categories (table).
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <locale.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef _WIN64
#define K "_K"
#else
#define K "I"
#endif

int main(int argc, char **argv)
{
    static const struct { const char *name; size_t cat; } tests[] =
    {
        {"?_Getcat@facet@locale@std@@SA" K "XZ", (size_t)-1},
        {"?_Getcat@?$collate@D@std@@SA" K "XZ", LC_COLLATE},
        {"?_Getcat@?$ctype@G@std@@SA" K "XZ", LC_CTYPE},
        {"?_Getcat@?$codecvt@DDH@std@@SA" K "XZ", LC_CTYPE},
        {"?_Getcat@?$numpunct@D@std@@SA" K "XZ", LC_NUMERIC},
        {"?_Getcat@?$num_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@SA" K "XZ", LC_NUMERIC},
        {"?_Getcat@?$num_put@GV?$ostreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@SA" K "XZ", LC_NUMERIC},
        {"?_Getcat@?$time_get@DV?$istreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@SA" K "XZ", LC_TIME},
        {"?_Getcat@?$time_put@GV?$ostreambuf_iterator@GU?$char_traits@G@std@@@std@@@std@@SA" K "XZ", LC_TIME},
    };
    HMODULE mod = LoadLibraryA(argc > 1 ? argv[1] : "msvcp60");
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        size_t (__cdecl *f)(void) = (void *)GetProcAddress(mod, tests[i].name);
        check(f && f() == tests[i].cat, "%s = %Id", tests[i].name, f ? (SSIZE_T)f() : -99);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
