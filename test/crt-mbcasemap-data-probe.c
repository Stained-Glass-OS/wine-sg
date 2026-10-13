/* Probe for patches/sg/4561: ucrtbase's _mbcasemap data export, the case
 * map of the process' multibyte code page, against __p__mbcasemap() at
 * start and after _setmbcp to each code page of a table.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    static const int pages[] = {1252, 932, 1251, 437, -3 /* _MB_CP_SBCS */, 936, 1252};
    HMODULE mod = LoadLibraryA(argc > 1 ? argv[1] : "ucrtbase");
    unsigned char *data, *(__cdecl *p_casemap)(void);
    int (__cdecl *p_setmbcp)(int), (__cdecl *p_getmbcp)(void);
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    data = (void *)GetProcAddress(mod, "_mbcasemap");
    p_casemap = (void *)GetProcAddress(mod, "__p__mbcasemap");
    p_setmbcp = (void *)GetProcAddress(mod, "_setmbcp");
    p_getmbcp = (void *)GetProcAddress(mod, "_getmbcp");
    check(data && p_casemap && p_setmbcp && p_getmbcp, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(!memcmp(data, p_casemap(), 256), "at start (code page %d)", p_getmbcp());
    for (i = 0; i < ARRAYSIZE(pages); i++)
    {
        int r = p_setmbcp(pages[i]);

        check(!r && !memcmp(data, p_casemap(), 256), "_setmbcp(%d) = %d: _mbcasemap follows ('a' -> %#x)",
              pages[i], r, data['a']);
    }
    check(data['a'] == 'A' && data['Z'] == 'z', "1252 case map");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
