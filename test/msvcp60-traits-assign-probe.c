/* Smoke probe for patches/sg/4566 (fast mode): msvcp60's
 * char_traits<char/unsigned short>::assign(ptr, count, const char &) (the
 * VC 6 form with the character by reference): count copies, nothing
 * after them, the pointer returned. Table driven; lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define N(a, b) a
#else
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const size_t counts[] = {0, 1, 7, 64};
    HMODULE mod = LoadLibraryA("msvcp60.dll");
    char *(__cdecl *assign_c)(char *, size_t, const char *);
    unsigned short *(__cdecl *assign_s)(unsigned short *, size_t, const unsigned short *);
    unsigned int i, k;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    assign_c = (void *)GetProcAddress(mod, N("?assign@?$char_traits@D@std@@SAPEADPEAD_KAEBD@Z", "?assign@?$char_traits@D@std@@SAPADPADIABD@Z"));
    assign_s = (void *)GetProcAddress(mod, N("?assign@?$char_traits@G@std@@SAPEAGPEAG_KAEBG@Z", "?assign@?$char_traits@G@std@@SAPAGPAGIABG@Z"));
    check(assign_c && assign_s, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    for (i = 0; i < ARRAYSIZE(counts); i++)
    {
        char c[80], ch = 'q';
        unsigned short s[80], sh = 0x4242;
        int ok_c = 1, ok_s = 1;

        memset(c, '.', sizeof(c)); memset(s, 0, sizeof(s));
        check(assign_c(c, counts[i], &ch) == c, "char: returns the pointer");
        check(assign_s(s, counts[i], &sh) == s, "unsigned short: returns the pointer");
        for (k = 0; k < 80; k++)
        {
            if (c[k] != (k < counts[i] ? 'q' : '.')) ok_c = 0;
            if (s[k] != (k < counts[i] ? 0x4242 : 0)) ok_s = 0;
        }
        check(ok_c && ok_s, "count %Iu: %d %d", counts[i], ok_c, ok_s);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
