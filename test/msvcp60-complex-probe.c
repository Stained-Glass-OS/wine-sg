/* Smoke probe for patches/sg/4540 (fast mode): msvcp60's complex<float>
 * and complex<double> exports (VC6 names, mapped to the msvcp90 sources'
 * functions, and the template compound assignments): construction, abs,
 * operator-=, _Ctr<double>::sqrt, polar. Lines "ok"/"FAIL", last "RESULT". */
#include <windows.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN64
#define THISCALL
#define N(a64, a32) a64
#else
#define THISCALL __attribute__((thiscall))
#define N(a64, a32) a32
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { float re, im; } cf;
typedef struct { double re, im; } cd;

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA(argc > 1 ? argv[1] : "msvcp60");
    cf *(THISCALL *ctor)(cf *, const float *, const float *);
    float (__cdecl *fabs_c)(const cf *);
    cf *(__cdecl *sub)(cf *, const cf *);
    cd *(__cdecl *sub_d)(cd *, const cd *);
    double (__cdecl *dsqrt)(double);
    cd *(__cdecl *polar)(cd *, const double *, const double *);
    float re = 3, im = 4;
    double r = 2, t = 0;
    cf a, b = {1, 1};
    cd x = {5, 7}, y = {2, 3}, p;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ctor = (void *)GetProcAddress(mod, N("??0?$_Complex_base@M@std@@QEAA@AEBM0@Z", "??0?$_Complex_base@M@std@@QAE@ABM0@Z"));
    fabs_c = (void *)GetProcAddress(mod, N("?abs@std@@YAMAEBV?$complex@M@1@@Z", "?abs@std@@YAMABV?$complex@M@1@@Z"));
    sub = (void *)GetProcAddress(mod, N("??$?ZMM@std@@YAAEAV?$complex@M@0@AEAV10@AEBV10@@Z", "??$?ZMM@std@@YAAAV?$complex@M@0@AAV10@ABV10@@Z"));
    sub_d = (void *)GetProcAddress(mod, N("??Zstd@@YAAEAV?$complex@N@0@AEAV10@AEBV10@@Z", "??Zstd@@YAAAV?$complex@N@0@AAV10@ABV10@@Z"));
    dsqrt = (void *)GetProcAddress(mod, "?sqrt@?$_Ctr@N@std@@SANN@Z");
    polar = (void *)GetProcAddress(mod, N("?polar@std@@YA?AV?$complex@N@1@AEBN0@Z", "?polar@std@@YA?AV?$complex@N@1@ABN0@Z"));
    check(ctor && fabs_c && sub && sub_d && dsqrt && polar, "exports %p %p %p %p %p %p", ctor, fabs_c, sub, sub_d, dsqrt, polar);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(ctor(&a, &re, &im) == &a && a.re == 3 && a.im == 4, "_Complex_base<float>(3, 4)");
    check(fabs(fabs_c(&a) - 5) < 1e-6, "abs(3+4i) = %g", fabs_c(&a));
    check(sub(&a, &b) == &a && a.re == 2 && a.im == 3, "operator-=: %g %g", a.re, a.im);
    check(sub_d(&x, &y) == &x && x.re == 3 && x.im == 4, "operator-= (double): %g %g", x.re, x.im);
    check(dsqrt(16) == 4, "_Ctr<double>::sqrt(16)");
    check(polar(&p, &r, &t) == &p && p.re == 2 && p.im == 0, "polar(2, 0): %g %g", p.re, p.im);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
