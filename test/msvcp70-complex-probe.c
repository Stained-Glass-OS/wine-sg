/* Smoke probe for patches/sg/4541 (fast mode): msvcp70's VC7.0-named
 * complex exports (32-bit; Wine's 64-bit msvcp70 does not export them),
 * mapped to the msvcp90 sources' functions: construction, abs, sqrt, polar.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN64
int main(void) { printf("ok   no 64-bit names to test\nRESULT: PASS\n"); return 0; }
#else
#define THISCALL __attribute__((thiscall))

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { float re, im; } cf;
typedef struct { double re, im; } cd;

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA(argc > 1 ? argv[1] : "msvcp70");
    cf *(THISCALL *ctor)(cf *, const float *, const float *);
    double (__cdecl *abs_d)(const cd *);
    cd *(__cdecl *sqrt_d)(cd *, const cd *);
    cd *(__cdecl *polar)(cd *, const double *);
    float re = 3, im = 4;
    double r = 2;
    cf a;
    cd x = {3, 4}, y = {-4, 0}, out;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ctor = (void *)GetProcAddress(mod, "??0?$_Complex_base@M@std@@QAE@ABM0@Z");
    abs_d = (void *)GetProcAddress(mod, "?abs@std@@YANABV?$complex@N@1@@Z");
    sqrt_d = (void *)GetProcAddress(mod, "?sqrt@std@@YA?AV?$complex@N@1@ABV21@@Z");
    polar = (void *)GetProcAddress(mod, "?polar@std@@YA?AV?$complex@N@1@ABN@Z");
    check(ctor && abs_d && sqrt_d && polar, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    check(ctor(&a, &re, &im) == &a && a.re == 3 && a.im == 4, "_Complex_base<float>(3, 4)");
    check(fabs(abs_d(&x) - 5) < 1e-12, "abs(3+4i)");
    check(sqrt_d(&out, &y) == &out && fabs(out.re) < 1e-12 && fabs(out.im - 2) < 1e-12, "sqrt(-4) = %g%+gi", out.re, out.im);
    check(polar(&out, &r) == &out && out.re == 2 && out.im == 0, "polar(2)");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
#endif
