/* gdiplus (patches/sg/2663): with the page unit UnitDisplay the page scale does not change what a string measures. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

typedef int Status;
enum { Ok = 0 };
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
typedef struct { float X, Y, Width, Height; } RectF;
typedef struct { INT First, Length; } CharRange;

#define PixelFormat16bppRGB555 0x21005
#define PixelFormat16bppRGB565 0x21006
#define PixelFormat16bppARGB1555 0x61007
#define PixelFormat24bppRGB 0x21808
#define PixelFormat32bppARGB 0x26200A

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
FN(Status, GdipCreateFontFamilyFromName, (const WCHAR *, void *, void **));
FN(Status, GdipCreateFont, (void *, float, int, int, void **));
FN(Status, GdipCreateStringFormat, (int, WORD, void **));
FN(Status, GdipMeasureString, (void *, const WCHAR *, int, void *, const RectF *, void *, RectF *, int *, int *));
FN(Status, GdipSetPageUnit, (void *, int));
FN(Status, GdipSetPageScale, (void *, float));

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;
    void *family = NULL, *graphics = NULL, *font = NULL, *fmt = NULL;
    HDC hdc = CreateCompatibleDC(0);
    RectF rc, b1, b2, b3;
    int unit;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipCreateFromHDC); LOAD(GdipCreateFontFamilyFromName); LOAD(GdipCreateFont);
    LOAD(GdipCreateStringFormat); LOAD(GdipMeasureString); LOAD(GdipSetPageUnit); LOAD(GdipSetPageScale);
    if (!gp || pGdiplusStartup(&token, &in, NULL) != 0) { printf("FAIL  start\nRESULT: FAIL\n"); return 1; }
    pGdipCreateFontFamilyFromName(L"Tahoma", NULL, &family);
    pGdipCreateFromHDC(hdc, &graphics);
    pGdipCreateStringFormat(0, 0, &fmt);
    check(family && graphics && fmt, "objects");
    if (!family || !graphics || !fmt) goto done;

    for (unit = 3; unit <= 5; unit++)
    {
        pGdipCreateFont(family, unit == 3 ? 20.0f : unit == 4 ? 0.3f : 2.0f, 0, unit, &font);
        pGdipSetPageUnit(graphics, 1);
        pGdipSetPageScale(graphics, 1.0f);
        memset(&rc, 0, sizeof(rc)); memset(&b1, 0, sizeof(b1));
        pGdipMeasureString(graphics, L"Sample", 6, font, &rc, fmt, &b1, NULL, NULL);
        pGdipSetPageScale(graphics, 2.0f);
        memset(&b2, 0, sizeof(b2));
        pGdipMeasureString(graphics, L"Sample", 6, font, &rc, fmt, &b2, NULL, NULL);
        pGdipSetPageScale(graphics, 0.5f);
        memset(&b3, 0, sizeof(b3));
        pGdipMeasureString(graphics, L"Sample", 6, font, &rc, fmt, &b3, NULL, NULL);
        check(b1.Height > 0 && fabs(b1.Height - b2.Height) < 0.5f && fabs(b1.Height - b3.Height) < 0.5f,
              "font unit %d: height %.2f at scale 1, %.2f at 2, %.2f at 0.5", unit, b1.Height, b2.Height, b3.Height);
        check(fabs(b1.Width - b2.Width) < 0.5f && fabs(b1.Width - b3.Width) < 0.5f,
              "font unit %d: width %.2f at scale 1, %.2f at 2, %.2f at 0.5", unit, b1.Width, b2.Width, b3.Width);
    }
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
