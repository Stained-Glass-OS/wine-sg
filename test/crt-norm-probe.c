/* Probe for patches/sg/4522: norm, normf and norml of <complex.h> (msvcr120,
 * ucrtbase): the squared magnitude. The structures are passed as MSVC passes
 * them: _Dcomplex (16 bytes) by reference on x64 and on the stack on i386,
 * _Fcomplex (8 bytes) in a register on x64.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <math.h>

typedef struct { double v[2]; } dcomplex;
typedef struct { float v[2]; } fcomplex;

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(int argc, char **argv)
{
    static const struct { double r, i, norm; } tests[] =
    {
        {3, 4, 25}, {0, 0, 0}, {-1, 0, 1}, {0, -2, 4}, {1.5, -2.5, 8.5}, {1e150, 1e150, 2e300}, {1e-200, 0, 0},
    };
    double (__cdecl *p_norm)(dcomplex), (__cdecl *p_norml)(dcomplex);
    float (__cdecl *p_normf)(fcomplex);
    HMODULE mod;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_norm = (void *)GetProcAddress(mod, "norm");
    p_normf = (void *)GetProcAddress(mod, "normf");
    p_norml = (void *)GetProcAddress(mod, "norml");
    check(p_norm && p_normf && p_norml, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        dcomplex d = {{tests[i].r, tests[i].i}};
        fcomplex f = {{(float)tests[i].r, (float)tests[i].i}};
        double n = p_norm(d), nl = p_norml(d);
        float nf = p_normf(f);
        double want_f = (double)f.v[0] * f.v[0] + (double)f.v[1] * f.v[1];

        check(n == tests[i].norm || fabs(n - tests[i].norm) <= 1e-12 * tests[i].norm, "norm(%g, %g) = %g", tests[i].r, tests[i].i, n);
        check(nl == n, "norml(%g, %g) = %g", tests[i].r, tests[i].i, nl);
        check(isinf(want_f) ? isinf(nf) : fabs(nf - want_f) <= 1e-6 * want_f, "normf(%g, %g) = %g", tests[i].r, tests[i].i, nf);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
