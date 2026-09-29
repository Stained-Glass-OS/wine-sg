/* msvcp's _Sinh / _Cosh (patches/sg/0509): what std::complex's sin, cos,
 * sinh, cosh and tan call in Microsoft's C++ library. Prints one line a check. */
#include <windows.h>
#include <math.h>
#include <stdio.h>

typedef double (__cdecl *dd_fn)(double, double);
typedef float (__cdecl *ff_fn)(float, float);

static int close_to(double a, double b) { return fabs(a - b) <= 1e-12 * fmax(1.0, fabs(b)); }

int main(void)
{
    static const char *dlls[] = { "msvcp140.dll", "msvcp120.dll", "msvcp110.dll", "msvcp100.dll" };
    int i;
    for (i = 0; i < 4; i++) {
        HMODULE m = LoadLibraryA(dlls[i]);
        dd_fn s = m ? (dd_fn)GetProcAddress(m, "_Sinh") : NULL, c = m ? (dd_fn)GetProcAddress(m, "_Cosh") : NULL;
        dd_fn ls = m ? (dd_fn)GetProcAddress(m, "_LSinh") : NULL, lc = m ? (dd_fn)GetProcAddress(m, "_LCosh") : NULL;
        ff_fn fs = m ? (ff_fn)GetProcAddress(m, "_FSinh") : NULL, fc = m ? (ff_fn)GetProcAddress(m, "_FCosh") : NULL;
        if (!s || !c || !ls || !lc || !fs || !fc) { printf("%s missing\n", dlls[i]); continue; }
        printf("%s values %d %d %d %d %d %d\n", dlls[i],
               close_to(s(1.0, 2.0), 2 * sinh(1.0)), close_to(c(0.5, 3.0), 3 * cosh(0.5)),
               close_to(ls(-2.0, 1.0), sinh(-2.0)), close_to(lc(2.0, -1.0), -cosh(2.0)),
               fabs(fs(1.0f, 2.0f) - 2 * sinhf(1.0f)) < 1e-5, fabs(fc(1.0f, 1.0f) - coshf(1.0f)) < 1e-5);
        printf("%s edges %d %d %d %d\n", dlls[i], s(3.0, 0.0) == 0.0, !!isnan(c(NAN, 1.0)),
               isfinite(s(710.0, 1e-300)) && s(710.0, 1e-300) > 0, s(-710.0, 1.0) == -INFINITY || s(-710.0, 1.0) < -1e300);
    }
    return 0;
}
