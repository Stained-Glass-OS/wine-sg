/* gdiplus, fast mode (patch 3502): image attribute threshold and
 * SetToIdentity, the cached-background hint, GetCustomLineCapStrokeCaps and
 * RotateLineTransform. */
#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
#include <math.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static ARGB draw_with(GpImageAttributes *attr, ARGB src)
{
    GpBitmap *in, *out;
    GpGraphics *g;
    ARGB result = 0;

    GdipCreateBitmapFromScan0(1, 1, 0, PixelFormat32bppARGB, NULL, &in);
    GdipCreateBitmapFromScan0(1, 1, 0, PixelFormat32bppARGB, NULL, &out);
    GdipBitmapSetPixel(in, 0, 0, src);
    GdipGetImageGraphicsContext((GpImage *)out, &g);
    GdipSetInterpolationMode(g, InterpolationModeNearestNeighbor);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipGraphicsClear(g, 0x00000000);
    GdipDrawImageRectRectI(g, (GpImage *)in, 0, 0, 1, 1, 0, 0, 1, 1, UnitPixel, attr, NULL, NULL);
    GdipBitmapGetPixel(out, 0, 0, &result);
    GdipDeleteGraphics(g);
    GdipDisposeImage((GpImage *)in);
    GdipDisposeImage((GpImage *)out);
    return result;
}

int main(void)
{
    GdiplusStartupInput input = { 1 };
    ULONG_PTR token;
    GpImageAttributes *attr;
    GpStatus st;
    ARGB plain, result;
    ColorMatrix zero_red = {{
        {0, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}}};

    GdiplusStartup(&token, &input, NULL);
    GdipCreateImageAttributes(&attr);

    plain = draw_with(NULL, 0xff80c040);
    check(plain == 0xff80c040, "plain draw keeps the colour");

    st = GdipSetImageAttributesCachedBackground(attr, TRUE);
    check(st == Ok, "cached background is accepted");
    st = GdipSetImageAttributesCachedBackground(NULL, TRUE);
    check(st == InvalidParameter, "cached background needs attributes");

    st = GdipSetImageAttributesThreshold(attr, ColorAdjustTypeBitmap, TRUE, 0.5f);
    check(st == Ok, "threshold is accepted");
    result = draw_with(attr, 0xff80c040);
    check(result == 0xffffff00, "threshold 0.5 makes every channel full or empty");
    st = GdipSetImageAttributesThreshold(attr, ColorAdjustTypeBitmap, FALSE, 0.5f);
    result = draw_with(attr, 0xff80c040);
    check(result == plain, "threshold off");
    st = GdipSetImageAttributesThreshold(attr, ColorAdjustTypeCount, TRUE, 0.5f);
    check(st == InvalidParameter, "threshold type check");

    GdipSetImageAttributesColorMatrix(attr, ColorAdjustTypeDefault, TRUE, &zero_red, NULL, ColorMatrixFlagsDefault);
    result = draw_with(attr, 0xff80c040);
    check((result & 0x00ff0000) == 0, "default colour matrix removes red");
    st = GdipSetImageAttributesToIdentity(attr, ColorAdjustTypeBitmap);
    check(st == Ok, "SetToIdentity");
    result = draw_with(attr, 0xff80c040);
    check(result == plain, "bitmaps follow their own identity matrix, not the default one");
    check(GdipSetImageAttributesToIdentity(attr, ColorAdjustTypeCount) == InvalidParameter, "SetToIdentity type check");
    GdipDisposeImageAttributes(attr);

    {
        GpCustomLineCap *cap;
        GpPath *path;
        GpLineCap start = LineCapFlat, end = LineCapFlat;

        GdipCreatePath(FillModeAlternate, &path);
        GdipAddPathLine(path, 0, 0, 1, 1);
        GdipCreateCustomLineCap(NULL, path, LineCapFlat, 0, &cap);
        GdipSetCustomLineCapStrokeCaps(cap, LineCapRound, LineCapTriangle);
        st = GdipGetCustomLineCapStrokeCaps(cap, &start, &end);
        check(st == Ok && start == LineCapRound && end == LineCapTriangle, "stroke caps round trip");
        check(GdipGetCustomLineCapStrokeCaps(cap, NULL, &end) == InvalidParameter, "stroke caps null");
        GdipDeleteCustomLineCap(cap);
        GdipDeletePath(path);
    }

    {
        GpLineGradient *brush;
        GpMatrix *m;
        float e[6];
        GpRectF rc = {0, 0, 10, 10};

        GdipCreateLineBrushFromRect(&rc, 0xff0000ff, 0xffff0000, LinearGradientModeHorizontal, WrapModeTile, &brush);
        st = GdipRotateLineTransform(brush, 90.0f, MatrixOrderPrepend);
        check(st == Ok, "RotateLineTransform");
        GdipCreateMatrix(&m);
        GdipGetLineTransform(brush, m);
        GdipGetMatrixElements(m, e);
        check(fabsf(e[0]) < 1e-4f && fabsf(e[1] - 1.0f) < 1e-4f && fabsf(e[2] + 1.0f) < 1e-4f && fabsf(e[3]) < 1e-4f,
              "line gradient transform is rotated");
        GdipDeleteMatrix(m);
        GdipDeleteBrush((GpBrush *)brush);
    }

    GdiplusShutdown(token);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
