/* gdiplus: the box GdipMeasureDriverString gives for a matrix is the box of the transformed
 * rectangle of the text, as the Wine tests record from Windows (patches/sg/2634). The flat
 * API is reached through GetProcAddress. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

typedef int Status;
enum { Ok = 0 };
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
typedef struct { float X, Y, Width, Height; } RectF;
typedef struct { float X, Y; } PointF;

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

#define FN(ret, name, args) static ret (WINAPI *p##name) args
FN(Status, GdiplusStartup, (ULONG_PTR *, const StartupInput *, void *));
FN(Status, GdipCreateFromHDC, (HDC, void **));
FN(Status, GdipDeleteGraphics, (void *));
FN(Status, GdipCreateFontFamilyFromName, (const WCHAR *, void *, void **));
FN(Status, GdipCreateFont, (void *, float, int, int, void **));
FN(Status, GdipMeasureDriverString, (void *, const WORD *, int, void *, const PointF *, int, void *, RectF *));
FN(Status, GdipCreateMatrix2, (float, float, float, float, float, float, void **));
FN(Status, GdipRotateMatrix, (void *, float, int));
FN(Status, GdipDeleteMatrix, (void *));

static int near_(float a, float b, float tol) { return fabs(a - b) <= tol; }

int main(void)
{
    static const WCHAR str[] = L"Wine";
    static const struct { float m11, m12, m21, m22, dx, dy, angle; const char *name; } tests[] =
    {
        { 1, 0, 0, 1, 0, 0, 0, "identity" },
        { 2, 0, 0, 3, 0, 0, 0, "scale 2x3" },
        { 1, 0, 0, 1, 10, 20, 0, "translate" },
        { 1, 0, 0, 1, 0, 0, 90, "rotate 90" },
        { 2, 0, 0, 3, 0, 0, 45, "scale + rotate 45" },
        { 1, 0.5f, -0.25f, 1, 5, -7, 0, "shear" },
    };
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;
    void *family = NULL, *font = NULL, *graphics = NULL;
    HDC hdc = CreateCompatibleDC(0);
    PointF pos[4] = { {0, 0}, {20, 0}, {40, 0}, {60, 0} };
    RectF base, box;
    unsigned int i, k;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipCreateFromHDC); LOAD(GdipDeleteGraphics); LOAD(GdipCreateFontFamilyFromName);
    LOAD(GdipCreateFont); LOAD(GdipMeasureDriverString); LOAD(GdipCreateMatrix2); LOAD(GdipRotateMatrix); LOAD(GdipDeleteMatrix);
    if (!gp || !pGdiplusStartup || pGdiplusStartup(&token, &in, NULL) != Ok)
    {
        printf("FAIL  gdiplus did not start\nRESULT: FAIL\n");
        return 1;
    }
    pGdipCreateFontFamilyFromName(L"Tahoma", NULL, &family);
    pGdipCreateFont(family, 100.0f, 0, 2, &font);
    pGdipCreateFromHDC(hdc, &graphics);
    if (!font || !graphics) { printf("FAIL  no font\nRESULT: FAIL\n"); return 1; }

    check(pGdipMeasureDriverString(graphics, str, 4, font, pos, 1 /* CmapLookup */, NULL, &base) == Ok &&
          base.Width > 0 && base.Height > 0, "untransformed box %g,%g %gx%g", base.X, base.Y, base.Width, base.Height);

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        void *matrix = NULL;
        float xs[4], ys[4], minx, maxx, miny, maxy;
        float cx[4] = { base.X, base.X + base.Width, base.X + base.Width, base.X };
        float cy[4] = { base.Y, base.Y, base.Y + base.Height, base.Y + base.Height };
        Status st;

        pGdipCreateMatrix2(tests[i].m11, tests[i].m12, tests[i].m21, tests[i].m22, tests[i].dx, tests[i].dy, &matrix);
        if (tests[i].angle) pGdipRotateMatrix(matrix, tests[i].angle, 1 /* append */);
        {
            /* the matrix read back as the transformation of the corners: use the flat matrix elements */
            float el[6];
            static Status (WINAPI *pGdipGetMatrixElements)(void *, float *);
            if (!pGdipGetMatrixElements) pGdipGetMatrixElements = (void *)GetProcAddress(gp, "GdipGetMatrixElements");
            pGdipGetMatrixElements(matrix, el);
            for (k = 0; k < 4; k++)
            {
                xs[k] = cx[k] * el[0] + cy[k] * el[2] + el[4];
                ys[k] = cx[k] * el[1] + cy[k] * el[3] + el[5];
            }
        }
        minx = maxx = xs[0]; miny = maxy = ys[0];
        for (k = 1; k < 4; k++)
        {
            if (xs[k] < minx) minx = xs[k];
            if (xs[k] > maxx) maxx = xs[k];
            if (ys[k] < miny) miny = ys[k];
            if (ys[k] > maxy) maxy = ys[k];
        }
        st = pGdipMeasureDriverString(graphics, str, 4, font, pos, 1, matrix, &box);
        /* the glyphs are measured at the size the matrix makes them: hinting moves it a little */
        check(st == Ok && near_(box.X, minx, 2.0f) && near_(box.Y, miny, 2.0f) &&
              near_(box.Width, maxx - minx, 0.02f * (maxx - minx) + 1.0f) &&
              near_(box.Height, maxy - miny, 0.02f * (maxy - miny) + 1.0f),
              "%s: box %g,%g %gx%g (want %g,%g %gx%g)", tests[i].name, box.X, box.Y, box.Width, box.Height,
              minx, miny, maxx - minx, maxy - miny);
        pGdipDeleteMatrix(matrix);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
