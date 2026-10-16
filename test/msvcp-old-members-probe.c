/* Smoke probe for patches/sg/4536 (fast mode): in msvcp60 .. msvcp90 (as
 * each exports them), _Ctraits<float/double/long double>::_Cosh, _Sinh,
 * _Exp, _Infv, _Nanv and _Isinf, the plain _Cosh/_Sinh/_FCosh/_FSinh, the
 * assignments of data-less classes, _UShinit, _Mtxinit/_Mtxlock and _Once.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef _WIN64
#define THISCALL
#else
#define THISCALL __attribute__((thiscall))
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG once_calls;
static void __cdecl once_fn(void) { once_calls++; }

int main(int argc, char **argv)
{
    HMODULE mod;
    unsigned int found = 0;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }

    {
        double (__cdecl *dcosh)(double, double) = (void *)GetProcAddress(mod, "?_Cosh@?$_Ctraits@N@std@@SANNN@Z");
        double (__cdecl *dsinh)(double, double) = (void *)GetProcAddress(mod, "?_Sinh@?$_Ctraits@N@std@@SANNN@Z");
        float (__cdecl *fcosh)(float, float) = (void *)GetProcAddress(mod, "?_Cosh@?$_Ctraits@M@std@@SAMMM@Z");
        double (__cdecl *dinf)(double) = (void *)GetProcAddress(mod, "?_Infv@?$_Ctraits@N@std@@SANN@Z");
        double (__cdecl *dnan)(double) = (void *)GetProcAddress(mod, "?_Nanv@?$_Ctraits@N@std@@SANN@Z");
        float (__cdecl *fnan)(float) = (void *)GetProcAddress(mod, "?_Nanv@?$_Ctraits@M@std@@SAMM@Z");
        unsigned char (__cdecl *disinf)(double) = (void *)GetProcAddress(mod, "?_Isinf@?$_Ctraits@N@std@@SA_NN@Z");
        double (__cdecl *pcosh)(double, double) = (void *)GetProcAddress(mod, "_Cosh");

        if (dcosh && dsinh && fcosh && dinf && dnan && fnan && disinf)
        {
            found++;
            check(fabs(dcosh(1.0, 2.0) - 2.0 * cosh(1.0)) < 1e-14 && fabs(dsinh(0.5, 3.0) - 3.0 * sinh(0.5)) < 1e-14,
                  "_Ctraits<double>::_Cosh/_Sinh");
            check(fabsf(fcosh(1.0f, 1.0f) - (float)cosh(1.0)) < 1e-6f, "_Ctraits<float>::_Cosh");
            check(isinf(dinf(0)) && dinf(0) > 0 && isnan(dnan(0)) && isnan(fnan(0)), "_Infv, _Nanv");
            check((disinf(INFINITY) & 1) && !(disinf(1.0) & 1) && !(disinf(NAN) & 1), "_Isinf");
        }
        if (pcosh)
        {
            found++;
            check(fabs(pcosh(0.0, 5.0) - 5.0) < 1e-15, "_Cosh(0, 5)");
        }
    }
    {
        void *(THISCALL *assign)(void *, const void *) = (void *)GetProcAddress(mod, sizeof(void *) == 8 ?
                "??4?$char_traits@D@std@@QEAAAEAU01@AEBU01@@Z" : "??4?$char_traits@D@std@@QAEAAU01@ABU01@@Z");
        void *(THISCALL *ush)(void *) = (void *)GetProcAddress(mod, sizeof(void *) == 8 ? "??0_UShinit@std@@QEAA@XZ" : "??0_UShinit@std@@QAE@XZ");
        void (THISCALL *ush_dtor)(void *) = (void *)GetProcAddress(mod, sizeof(void *) == 8 ? "??1_UShinit@std@@QEAA@XZ" : "??1_UShinit@std@@QAE@XZ");
        char a[8], b[8];

        if (assign) { found++; check(assign(a, b) == a, "char_traits assignment"); }
        if (ush && ush_dtor) { found++; check(ush(a) == a, "_UShinit"); ush_dtor(a); }
    }
    {
        void (__cdecl *init)(void *) = (void *)GetProcAddress(mod, "_Mtxinit");
        void (__cdecl *lock)(void *) = (void *)GetProcAddress(mod, "_Mtxlock");
        void (__cdecl *unlock)(void *) = (void *)GetProcAddress(mod, "_Mtxunlock");
        void (__cdecl *dst)(void *) = (void *)GetProcAddress(mod, "_Mtxdst");
        void (__cdecl *once)(LONG *, void (__cdecl *)(void)) = (void *)GetProcAddress(mod, "_Once");
        CRITICAL_SECTION cs;
        LONG flag = 0;

        if (init && lock && unlock && dst)
        {
            found++;
            init(&cs); lock(&cs); lock(&cs);
            check(cs.RecursionCount == 2, "_Mtxlock recursive (%ld)", cs.RecursionCount);
            unlock(&cs); unlock(&cs); dst(&cs);
        }
        if (once)
        {
            found++;
            once(&flag, once_fn); once(&flag, once_fn);
            check(once_calls == 1, "_Once");
        }
    }
    check(found > 0, "%u groups present", found);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
