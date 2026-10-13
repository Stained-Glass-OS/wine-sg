/* Smoke probe for patches/sg/4508: exports wired to existing implementations.
 *   probe.exe forwards        msvcm80/msvcm90 entries resolve to msvcp80/msvcp90's functions
 *   probe.exe complex         msvcp70 complex<double> operators, msvcp71 _LCosh
 *   probe.exe cpperror DLL    msvcp110/msvcp120 _Throw_Cpp_error throws system_error(EBUSY) */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef _WIN64
#define N(a64, a32) a64
#else
#define N(a64, a32) a32
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

struct cd { double r, i; };
struct cf { float r, i; };
struct cond { int value; const void *category; };
struct system_error { const void *vtable; char *name; int do_free; struct cond code; };

static LONG WINAPI handler(EXCEPTION_POINTERS *ep)
{
    const struct system_error *se;

    if (ep->ExceptionRecord->ExceptionCode != 0xe06d7363) return EXCEPTION_CONTINUE_SEARCH;
    se = (const struct system_error *)ep->ExceptionRecord->ExceptionInformation[1];
    check(se && se->code.value == 16 /* EBUSY */ && se->code.category, "_Throw_Cpp_error(0) threw code %d", se ? se->code.value : -1);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(!!failures);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "forwards"))
    {
        static const struct { const char *m, *p, *name; } fw[] =
        {
            {"msvcm80.dll", "msvcp80.dll", N("?_Cerr_func@std@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@1@XZ", "?_Cerr_func@std@@YAAAV?$basic_ostream@DU?$char_traits@D@std@@@1@XZ")},
            {"msvcm80.dll", "msvcp80.dll", N("?classic@locale@std@@SAAEBV12@XZ", "?classic@locale@std@@SAABV12@XZ")},
            {"msvcm90.dll", "msvcp90.dll", N("?_Cout_func@std@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@1@XZ", "?_Cout_func@std@@YAAAV?$basic_ostream@DU?$char_traits@D@std@@@1@XZ")},
            {"msvcm90.dll", "msvcp90.dll", N("?_Init_cnt_func@Init@ios_base@std@@CAAEAHXZ", "?_Init_cnt_func@Init@ios_base@std@@CAAAHXZ")},
        };
        const void *(__cdecl *classic)(void);
        unsigned int i;

        for (i = 0; i < ARRAYSIZE(fw); i++)
        {
            void *a = GetProcAddress(LoadLibraryA(fw[i].m), fw[i].name), *b = GetProcAddress(LoadLibraryA(fw[i].p), fw[i].name);
            check(a && a == b, "%s %.40s -> %s", fw[i].m, fw[i].name, fw[i].p);
        }
        classic = (void *)GetProcAddress(LoadLibraryA("msvcm80.dll"), N("?classic@locale@std@@SAAEBV12@XZ", "?classic@locale@std@@SAABV12@XZ"));
        check(classic && classic() != NULL, "msvcm80 classic() gives a locale");
    }
    else if (!strcmp(argv[1], "complex"))
    {
        HMODULE p70 = LoadLibraryA("msvcp70.dll"), p71 = LoadLibraryA("msvcp71.dll");
        struct cd *(__cdecl *addd)(struct cd *, const struct cd *, const struct cd *) =
                (void *)GetProcAddress(p70, N("??Hstd@@YA?AV?$complex@N@0@AEBV10@0@Z", "??Hstd@@YA?AV?$complex@N@0@ABV10@0@Z"));
        struct cd *(__cdecl *muld)(struct cd *, const struct cd *, const struct cd *) =
                (void *)GetProcAddress(p70, N("??Dstd@@YA?AV?$complex@N@0@AEBV10@0@Z", "??Dstd@@YA?AV?$complex@N@0@ABV10@0@Z"));
        unsigned char (__cdecl *eqd)(const struct cd *, const struct cd *) =
                (void *)GetProcAddress(p70, N("??8std@@YA_NAEBV?$complex@N@0@0@Z", "??8std@@YA_NABV?$complex@N@0@0@Z"));
        double (__cdecl *cosh_)(double, double) = (void *)GetProcAddress(p71, "_LCosh");
        struct cd a = {1, 2}, b = {3, 4}, r;

        check(addd && muld && eqd && cosh_, "exports");
        if (!failures)
        {
            addd(&r, &a, &b);
            check(r.r == 4 && r.i == 6, "(1+2i)+(3+4i) = %g%+gi", r.r, r.i);
            muld(&r, &a, &b);
            check(r.r == -5 && r.i == 10, "(1+2i)*(3+4i) = %g%+gi", r.r, r.i);
            check(eqd(&a, &a) && !eqd(&a, &b), "==");
            check(fabs(cosh_(1.0, 2.0) - 2.0 * cosh(1.0)) < 1e-12, "_LCosh(1, 2) = %g", cosh_(1.0, 2.0));
        }
    }
    else if (!strcmp(argv[1], "cpperror") && argc >= 3)
    {
        void (__cdecl *throw_cpp)(int) = (void *)GetProcAddress(LoadLibraryA(argv[2]), "?_Throw_Cpp_error@std@@YAXH@Z");

        check(throw_cpp != NULL, "%s export", argv[2]);
        if (throw_cpp)
        {
            AddVectoredExceptionHandler(1, handler);
            throw_cpp(0);
            check(0, "nothing thrown");
        }
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
