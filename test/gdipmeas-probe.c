/* gdiplus: what the Wine tests record from Windows about measuring a string (patches/sg/2633):
 * a measured string is a line higher by an eighth of the font's size than the region of its
 * characters, the alignments place the box in the layout rectangle, and the nearest colour of
 * a 16 bit surface is the colour with the low bits of each channel dropped. The flat API is
 * reached through GetProcAddress. */
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
FN(Status, GdipDeleteGraphics, (void *));
FN(Status, GdipCreateFontFamilyFromName, (const WCHAR *, void *, void **));
FN(Status, GdipCreateFont, (void *, float, int, int, void **));
FN(Status, GdipCreateStringFormat, (int, WORD, void **));
FN(Status, GdipSetStringFormatAlign, (void *, int));
FN(Status, GdipSetStringFormatLineAlign, (void *, int));
FN(Status, GdipSetStringFormatMeasurableCharacterRanges, (void *, int, const CharRange *));
FN(Status, GdipMeasureString, (void *, const WCHAR *, int, void *, const RectF *, void *, RectF *, int *, int *));
FN(Status, GdipCreateRegion, (void **));
FN(Status, GdipMeasureCharacterRanges, (void *, const WCHAR *, int, void *, const RectF *, void *, int, void **));
FN(Status, GdipGetRegionBounds, (void *, void *, RectF *));
FN(Status, GdipCreateBitmapFromScan0, (int, int, int, int, BYTE *, void **));
FN(Status, GdipGetImageGraphicsContext, (void *, void **));
FN(Status, GdipDisposeImage, (void *));
FN(Status, GdipGetNearestColor, (void *, DWORD *));

static int near_(float a, float b, float tol) { return fabs(a - b) <= tol; }

static void test_measure(void)
{
    static const WCHAR str[] = L"A01";
    static const float sizes[] = { 13.0f, 20.0f, 50.0f };
    unsigned int i;
    void *family = NULL, *graphics = NULL;
    HDC hdc = CreateCompatibleDC(0);
    Status st;

    st = pGdipCreateFontFamilyFromName(L"Tahoma", NULL, &family);
    check(st == Ok && family, "family (%d)", st);
    st = pGdipCreateFromHDC(hdc, &graphics);
    check(st == Ok && graphics, "graphics (%d)", st);
    if (!family || !graphics) return;

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        float size = sizes[i], margin_x = size / 6.0f, margin_y = size / 8.0f;
        void *font = NULL, *fmt = NULL, *region = NULL;
        RectF rect, bounds, rb, b2;
        CharRange range = { 0, 3 };
        float w, h, wr, hr;
        int glyphs = 0, lines = 0;
        int mode;

        pGdipCreateFont(family, size, 0, 2 /* UnitPixel */, &font);
        pGdipCreateStringFormat(0x4000 /* NoWrap */, 0, &fmt);
        pGdipSetStringFormatMeasurableCharacterRanges(fmt, 1, &range);
        pGdipCreateRegion(&region);

        rect = (RectF){ 0, 0, 0, 0 };
        st = pGdipMeasureString(graphics, str, -1, font, &rect, fmt, &bounds, &glyphs, &lines);
        w = bounds.Width; h = bounds.Height;
        check(st == Ok && glyphs == 3 && lines == 1 && w > 0 && h > 0, "%g: measured (%d, %d, %d)", size, st, glyphs, lines);

        rect = (RectF){ 5, 5, 32000, 32000 };
        pGdipMeasureCharacterRanges(graphics, str, -1, font, &rect, fmt, 1, &region);
        pGdipGetRegionBounds(region, graphics, &rb);
        wr = rb.Width; hr = rb.Height;
        check(near_(h - hr, margin_y, 0.6), "%g: the measured line is higher by an eighth (%g - %g = %g, want %g)", size, h, hr, h - hr, margin_y);
        check(near_(w - wr, margin_x * 2, 1.0), "%g: and wider by two sixths (%g)", size, w - wr);

        /* a rectangle lower than the line cuts it */
        rect = (RectF){ 0, 0, 0, h / 2 };
        pGdipMeasureString(graphics, str, -1, font, &rect, fmt, &b2, NULL, NULL);
        check(near_(b2.Height, h / 2, 0.01), "%g: cut by the rectangle (%g, want %g)", size, b2.Height, h / 2);

        /* alignments: 1 centre, 2 far */
        for (mode = 1; mode <= 2; mode++)
        {
            float fx = mode == 1 ? 0.5f : 1.0f;

            pGdipSetStringFormatAlign(fmt, mode);
            pGdipSetStringFormatLineAlign(fmt, mode);

            rect = (RectF){ 5, 5, w * 2, h * 2 };
            pGdipMeasureString(graphics, str, -1, font, &rect, fmt, &b2, NULL, NULL);
            check(near_(b2.X, 5 + w * fx, 0.1) && near_(b2.Y, 5 + h * fx, 0.1) && near_(b2.Width, w, 0.1) && near_(b2.Height, h, 0.1),
                  "%g: mode %d string box %g,%g %gx%g (want %g,%g)", size, mode, b2.X, b2.Y, b2.Width, b2.Height, 5 + w * fx, 5 + h * fx);

            rect = (RectF){ 5, 5, 0, 0 };
            pGdipMeasureString(graphics, str, -1, font, &rect, fmt, &b2, NULL, NULL);
            check(near_(b2.X, 5 - w * (mode == 1 ? 0.5f : 1.0f) + (mode == 1 ? 0 : w), 0.1) || mode == 2, "%g: mode %d empty rect x %g", size, mode, b2.X);
            check(near_(b2.Y, 5 - h * (mode == 1 ? 0.5f : 0.0f), 0.1) || mode == 2, "%g: mode %d empty rect y %g", size, mode, b2.Y);

            rect = (RectF){ 5, 5, wr * 2, hr * 2 };
            pGdipMeasureCharacterRanges(graphics, str, -1, font, &rect, fmt, 1, &region);
            pGdipGetRegionBounds(region, graphics, &rb);
            check(near_(rb.X, 5 + wr * fx, 1.5) && near_(rb.Y, 5 + hr * fx, 1.0) && near_(rb.Width, wr, 1.0) && near_(rb.Height, hr, 1.0),
                  "%g: mode %d region %g,%g %gx%g (want %g,%g)", size, mode, rb.X, rb.Y, rb.Width, rb.Height, 5 + wr * fx, 5 + hr * fx);
        }
    }
    pGdipDeleteGraphics(graphics);
}

static void test_nearest(void)
{
    static const struct { int format; DWORD want; const char *name; } tests[] =
    {
        { PixelFormat32bppARGB, 0xdeadbeef, "32bppARGB" },
        { PixelFormat24bppRGB, 0xdeadbeef, "24bppRGB" },
        { PixelFormat16bppRGB565, 0xffa8bce8, "565" },
        { PixelFormat16bppRGB555, 0xffa8b8e8, "555" },
        { PixelFormat16bppARGB1555, 0xffa8b8e8, "1555 opaque" },
    };
    unsigned int i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        void *bmp = NULL, *graphics = NULL;
        DWORD color = 0xdeadbeef;
        Status st;

        pGdipCreateBitmapFromScan0(10, 10, 0, tests[i].format, NULL, &bmp);
        st = pGdipGetImageGraphicsContext(bmp, &graphics);
        if (st != Ok) { check(0, "%s: graphics (%d)", tests[i].name, st); continue; }
        st = pGdipGetNearestColor(graphics, &color);
        check(st == Ok && color == tests[i].want, "%s: %#lx (want %#lx)", tests[i].name, color, tests[i].want);
        if (tests[i].format == PixelFormat16bppARGB1555)
        {
            color = 0x40adbeef;
            pGdipGetNearestColor(graphics, &color);
            check(color == 0x00a8b8e8, "1555 transparent: %#lx", color);
        }
        pGdipDeleteGraphics(graphics);
        pGdipDisposeImage(bmp);
    }
}

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipCreateFromHDC); LOAD(GdipDeleteGraphics); LOAD(GdipCreateFontFamilyFromName);
    LOAD(GdipCreateFont); LOAD(GdipCreateStringFormat); LOAD(GdipSetStringFormatAlign); LOAD(GdipSetStringFormatLineAlign);
    LOAD(GdipSetStringFormatMeasurableCharacterRanges); LOAD(GdipMeasureString); LOAD(GdipCreateRegion);
    LOAD(GdipMeasureCharacterRanges); LOAD(GdipGetRegionBounds); LOAD(GdipCreateBitmapFromScan0);
    LOAD(GdipGetImageGraphicsContext); LOAD(GdipDisposeImage); LOAD(GdipGetNearestColor);
    if (!gp || !pGdiplusStartup || pGdiplusStartup(&token, &in, NULL) != Ok)
    {
        printf("FAIL  gdiplus did not start\nRESULT: FAIL\n");
        return 1;
    }
    test_measure();
    test_nearest();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
