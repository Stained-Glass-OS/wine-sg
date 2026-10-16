/* Probe for patches/sg/4543: ucrtbase's corecrt_math.h helpers _dscale,
 * _dexp, _dunscale, _dpoly, _dlog, _dsin with their float (_fd) and long
 * double (_ld, which is double) forms and _o_ forms, against tables.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <math.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HMODULE mod;
static void *get(const char *n) { void *p = GetProcAddress(mod, n); if (!p) printf("missing %s\n", n); return p; }
static int approx(double a, double b) { return a == b || fabs(a - b) <= 1e-14 * fabs(b); }

int main(int argc, char **argv)
{
    short (__cdecl *dscale)(double *, long), (__cdecl *dexp_)(double *, double, long), (__cdecl *dunscale)(short *, double *);
    short (__cdecl *fdscale)(float *, long), (__cdecl *fdunscale)(short *, float *);
    double (__cdecl *dpoly)(double, const double *, int), (__cdecl *dlog_)(double, int), (__cdecl *dsin_)(double, unsigned int);
    float (__cdecl *fdsin)(float, unsigned int);
    unsigned int i;

    if (!(mod = LoadLibraryA("ucrtbase.dll"))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    dscale = get("_dscale"); dexp_ = get("_dexp"); dunscale = get("_dunscale"); dpoly = get("_dpoly");
    dlog_ = get("_dlog"); dsin_ = get("_dsin"); fdscale = get("_fdscale"); fdunscale = get("_fdunscale"); fdsin = get("_fdsin");
    {
        static const char *more[] = {"_fdexp", "_fdpoly", "_fdlog", "_ldscale", "_ldexp", "_ldunscale", "_ldpoly", "_ldlog", "_ldsin",
            "_o__dscale", "_o__dsin", "_o__dpoly", "_o__dunscale", "_o__dlog", "_o__fdexp", "_o__ldexp", "_o__fdsin"};
        for (i = 0; i < ARRAYSIZE(more); i++) check(GetProcAddress(mod, more[i]) != NULL, "%s exported", more[i]);
    }
    if (!dscale || !dexp_ || !dunscale || !dpoly || !dlog_ || !dsin_ || !fdscale || !fdunscale || !fdsin) { printf("RESULT: FAIL\n"); return 1; }

    {
        static const struct { double x; long e; double want; short cls; } t[] =
        {
            {1.5, 3, 12, -1}, {1e308, 10, INFINITY, 1}, {0, 5, 0, 0}, {3, -1, 1.5, -1}, {1e-300, -60, 1e-300 * 0x1p-60, -2},
            {1, -2000, 0, 0},
        };
        for (i = 0; i < ARRAYSIZE(t); i++)
        {
            double x = t[i].x;
            short c = dscale(&x, t[i].e);
            check(approx(x, t[i].want) && c == t[i].cls, "_dscale(%g, %ld) = %g, class %d", t[i].x, t[i].e, x, c);
        }
    }
    {
        static const struct { double x, y; long e; double want; short cls; } t[] =
        {
            {0, 3, 2, 12, -1}, {1, 1, 0, 2.718281828459045, -1}, {0.6931471805599453 * 10, 1, -10, 1, -1}, {-INFINITY, 5, 0, 0, 0},
            {2, 0, 7, 0, 0}, {1000, 1, 0, INFINITY, 1},
        };
        for (i = 0; i < ARRAYSIZE(t); i++)
        {
            double x = t[i].x;
            short c = dexp_(&x, t[i].y, t[i].e);
            check((x == t[i].want || fabs(x - t[i].want) <= 1e-12 * fabs(t[i].want)) && c == t[i].cls, "_dexp(%g, %g, %ld) = %.17g, class %d", t[i].x, t[i].y, t[i].e, x, c);
        }
    }
    {
        static const struct { double x, frac; short e, cls; } t[] =
        {
            {8, 0.5, 4, -1}, {-3, -0.75, 2, -1}, {0, 0, 0, 0}, {INFINITY, INFINITY, 0, 1}, {0x1p-1060, 0.5, -1059, -1},
        };
        for (i = 0; i < ARRAYSIZE(t); i++)
        {
            double x = t[i].x;
            short e = 99, c = dunscale(&e, &x);
            check(x == t[i].frac && e == t[i].e && c == t[i].cls, "_dunscale(%g) = %g * 2^%d, class %d", t[i].x, x, e, c);
        }
        {
            float f = 8;
            short e = 0, c = fdunscale(&e, &f);
            check(f == 0.5f && e == 4 && c == -1, "_fdunscale(8)");
            f = 1.5f;
            check(fdscale(&f, 2) == -1 && f == 6, "_fdscale(1.5, 2)");
        }
    }
    {
        static const double tab[] = {1, 2, 3};
        check(dpoly(2, tab, 2) == 11 && dpoly(5, tab, 0) == 1, "_dpoly: %g", dpoly(2, tab, 2));
        check(approx(dlog_(100, 1), 2) && approx(dlog_(M_E, 0), 1), "_dlog");
        check(approx(dsin_(0.5, 0), sin(0.5)) && approx(dsin_(0.5, 1), cos(0.5)) && approx(dsin_(0.5, 2), -sin(0.5)) &&
              approx(dsin_(0.5, 3), -cos(0.5)) && approx(dsin_(0.5, 5), cos(0.5)), "_dsin quadrants");
        check(fabsf(fdsin(0.5f, 1) - cosf(0.5f)) < 1e-6f, "_fdsin");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
