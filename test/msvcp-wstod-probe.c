/* Probe for patches/sg/4549: msvcp140's (and msvcp_win's) wide string to
 * floating point conversions _WStod, _WStodx, _WStof, _WStofx, _WStold and
 * _WStoldx (the value times ten to the given power, the end of the number,
 * the error code of the "x" forms with errno kept), against the narrow
 * _Stod family, and __Wcrtomb_lk against _Wcrtomb with C and code page 1252
 * conversion data. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

/* msvcp110 and later */
typedef struct { unsigned page; int mb_max, unk; BYTE isleadbyte[32]; } cvtvec;

int main(int argc, char **argv)
{
    static const struct { const WCHAR *s; long exp; double want; int end, err; } tests[] =
    {
        {L"1.5", 0, 1.5, 3, 0},
        {L"  -2.25xyz", 0, -2.25, 7, 0},
        {L"12", 2, 1200, 2, 0},
        {L"125", -2, 1.25, 3, 0},
        {L"0x10", 0, 16, 4, 0},
        {L"abc", 0, 0, 0, 0},
        {L"", 3, 0, 0, 0},
        {L"1e999", 0, HUGE_VAL, 5, ERANGE},
        {L"-1e-999", 0, -0.0, 7, ERANGE},
        {L"inf", 0, HUGE_VAL, 3, 0},
    };
    double (__cdecl *p_wstod)(const WCHAR *, WCHAR **, long), (__cdecl *p_wstold)(const WCHAR *, WCHAR **, long);
    double (__cdecl *p_wstodx)(const WCHAR *, WCHAR **, long, int *), (__cdecl *p_wstoldx)(const WCHAR *, WCHAR **, long, int *);
    float (__cdecl *p_wstof)(const WCHAR *, WCHAR **, long), (__cdecl *p_wstofx)(const WCHAR *, WCHAR **, long, int *);
    double (__cdecl *p_stod)(const char *, char **, long);
    int (__cdecl *p_wcrtomb)(char *, WCHAR, void *, const cvtvec *), (__cdecl *p_wcrtomb_lk)(char *, WCHAR, void *, const cvtvec *);
    int *(__cdecl *p_errno)(void);
    HMODULE mod, crt;
    unsigned int i, k;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    crt = LoadLibraryA("ucrtbase.dll");
    p_errno = (void *)GetProcAddress(crt, "_errno");
#define GET(v, n) v = (void *)GetProcAddress(mod, n); check(v != NULL, "%s exported", n)
    GET(p_wstod, "_WStod"); GET(p_wstodx, "_WStodx"); GET(p_wstof, "_WStof"); GET(p_wstofx, "_WStofx");
    GET(p_wstold, "_WStold"); GET(p_wstoldx, "_WStoldx"); GET(p_stod, "_Stod");
    GET(p_wcrtomb, "_Wcrtomb"); GET(p_wcrtomb_lk, "__Wcrtomb_lk");
    if (failures || !p_errno) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        char narrow[32];
        WCHAR *end;
        volatile double d, nd;  /* rounds an x87 return value to double */
        volatile float f;
        int err;

        WideCharToMultiByte(CP_ACP, 0, tests[i].s, -1, narrow, sizeof(narrow), NULL, NULL);
        for (k = 0; k < 2; k++)
        {
            double (*fn)(const WCHAR *, WCHAR **, long) = k ? p_wstold : p_wstod;
            const char *name = k ? "_WStold" : "_WStod";

            end = NULL;
            d = fn(tests[i].s, &end, tests[i].exp);
            nd = p_stod(narrow, NULL, tests[i].exp);
            check(d == tests[i].want && end == tests[i].s + tests[i].end && d == nd,
                  "%s(\"%ls\", %ld) = %g, end %d", name, tests[i].s, tests[i].exp, d, (int)(end - tests[i].s));
        }
        for (k = 0; k < 2; k++)
        {
            double (*fn)(const WCHAR *, WCHAR **, long, int *) = k ? p_wstoldx : p_wstodx;
            const char *name = k ? "_WStoldx" : "_WStodx";

            *p_errno() = 77; err = -1; end = NULL;
            d = fn(tests[i].s, &end, tests[i].exp, &err);
            check(d == tests[i].want && end == tests[i].s + tests[i].end && err == tests[i].err &&
                  *p_errno() == (tests[i].err ? tests[i].err : 77),
                  "%s(\"%ls\") = %g, err %d, errno %d", name, tests[i].s, d, err, *p_errno());
        }
        if (fabs(tests[i].want) < 1e30 || isinf(tests[i].want))
        {
            f = p_wstof(tests[i].s, &end, tests[i].exp);
            check(f == (float)tests[i].want && end == tests[i].s + tests[i].end, "_WStof(\"%ls\") = %g", tests[i].s, f);
            *p_errno() = 77; err = -1;
            f = p_wstofx(tests[i].s, &end, tests[i].exp, &err);
            check(f == (float)tests[i].want && err == tests[i].err, "_WStofx(\"%ls\") = %g, err %d", tests[i].s, f, err);
        }
    }

    /* __Wcrtomb_lk against _Wcrtomb */
    {
        static const WCHAR chars[] = {'A', 0, 0xe9, 0xff, 0x100, 0x20ac, 0x3042};
        cvtvec cvt[2];

        memset(cvt, 0, sizeof(cvt));
        cvt[1].page = 1252; cvt[1].mb_max = 1;
        for (k = 0; k < 2; k++)
            for (i = 0; i < ARRAYSIZE(chars); i++)
            {
                char a[8] = {0}, b[8] = {0};
                int state_a = 0, state_b = 0, ra, rb, ea, eb;

                *p_errno() = 0;
                ra = p_wcrtomb(a, chars[i], &state_a, &cvt[k]);
                ea = *p_errno(); *p_errno() = 0;
                rb = p_wcrtomb_lk(b, chars[i], &state_b, &cvt[k]);
                eb = *p_errno();
                check(ra == rb && ea == eb && !memcmp(a, b, sizeof(a)), "__Wcrtomb_lk(%#x, page %u) = %d %#x (%d %#x)",
                      chars[i], cvt[k].page, rb, (BYTE)b[0], ra, (BYTE)a[0]);
                if (k && chars[i] == 0x20ac) check(rb == 1 && (BYTE)b[0] == 0x80, "euro sign in 1252");
                if (!k && chars[i] == 0x20ac) check(rb == -1 && eb == EILSEQ, "euro sign in C: %d %d", rb, eb);
            }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
