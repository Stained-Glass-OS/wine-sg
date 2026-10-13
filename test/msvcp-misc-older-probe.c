/* Smoke probe for patches/sg/4559 (fast mode), in the DLL named on the
 * command line: __Wcrtomb_lk against _Wcrtomb with C and code page 1252
 * conversion data (msvcp60 .. msvcp120), and in msvcp110 the empty
 * _Add_vtordisp1/_Add_vtordisp2 members and details::_GetCurrentThreadId.
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

int main(int argc, char **argv)
{
    static const WCHAR chars[] = {'A', 0xe9, 0x20ac, 0x3042};
    int (__cdecl *p_wcrtomb)(char *, WCHAR, void *, const void *), (__cdecl *p_lk)(char *, WCHAR, void *, const void *);
    HMODULE mod;
    unsigned int i, k;
    int ver;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ver = atoi(argv[1] + 5);
    p_wcrtomb = (void *)GetProcAddress(mod, "_Wcrtomb");
    p_lk = (void *)GetProcAddress(mod, "__Wcrtomb_lk");
    check(p_wcrtomb && p_lk, "_Wcrtomb, __Wcrtomb_lk exported");
    for (k = 0; p_wcrtomb && p_lk && k < 2; k++)
    {
        DWORD cvt[16] = {0};

        if (k) cvt[ver >= 110 ? 0 : 1] = 1252;  /* the page follows an LCID before msvcp110 */
        for (i = 0; i < ARRAYSIZE(chars); i++)
        {
            char a[8] = {0}, b[8] = {0};
            int sa = 0, sb = 0, ra = p_wcrtomb(a, chars[i], &sa, cvt), rb = p_lk(b, chars[i], &sb, cvt);

            check(ra == rb && !memcmp(a, b, sizeof(a)), "__Wcrtomb_lk(%#x, page %d) = %d %#x", chars[i], k ? 1252 : 0, rb, (BYTE)b[0]);
        }
    }

    if (ver == 110)
    {
        static const char *const names[] =
        {
            N("?_Add_vtordisp1@?$basic_ios@DU?$char_traits@D@std@@@std@@UEAAXXZ", "?_Add_vtordisp1@?$basic_ios@DU?$char_traits@D@std@@@std@@UAEXXZ"),
            N("?_Add_vtordisp2@?$basic_ostream@_WU?$char_traits@_W@std@@@std@@UEAAXXZ", "?_Add_vtordisp2@?$basic_ostream@_WU?$char_traits@_W@std@@@std@@UAEXXZ"),
            N("?_Add_vtordisp1@?$basic_istream@GU?$char_traits@G@std@@@std@@UEAAXXZ", "?_Add_vtordisp1@?$basic_istream@GU?$char_traits@G@std@@@std@@UAEXXZ"),
        };
        DWORD (__cdecl *p_tid)(void) = (void *)GetProcAddress(mod, "?_GetCurrentThreadId@details@Concurrency@@YAKXZ");
        char obj[64];

        for (i = 0; i < ARRAYSIZE(names); i++)
        {
            void (THISCALL *f)(void *) = (void *)GetProcAddress(mod, names[i]);

            memset(obj, 0x5a, sizeof(obj));
            if (f) f(obj);
            check(f && obj[0] == 0x5a && obj[63] == 0x5a, "%s: does nothing", names[i]);
        }
        check(p_tid && p_tid() == GetCurrentThreadId(), "_GetCurrentThreadId");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
