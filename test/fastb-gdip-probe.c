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

    {
        GpBitmap *bmp = NULL;
        BYTE pal_buf[sizeof(ColorPalette) + 3 * sizeof(ARGB)];
        ColorPalette *custom = (ColorPalette *)pal_buf;
        ARGB px = 0;
        PixelFormat fmt = 0;
        BitmapData bd;
        GpRect rc = { 0, 0, 2, 2 };

        GdipCreateBitmapFromScan0(8, 8, 0, PixelFormat32bppARGB, NULL, &bmp);
        GdipBitmapSetPixel(bmp, 0, 0, 0xffff0000);
        GdipBitmapSetPixel(bmp, 1, 0, 0xff10c010);
        GdipBitmapSetPixel(bmp, 2, 0, 0xff0000f0);
        GdipBitmapSetPixel(bmp, 3, 0, 0xffe0e0e0);
        GdipBitmapSetPixel(bmp, 4, 0, 0xff202020);

        st = GdipBitmapConvertFormat(bmp, PixelFormat8bppIndexed, DitherTypeNone, PaletteTypeFixedHalftone8, NULL, 0.0f);
        check(st == Ok, "convert to 8bppIndexed halftone8");
        GdipGetImagePixelFormat((GpImage *)bmp, &fmt);
        check(fmt == PixelFormat8bppIndexed, "the bitmap now has the new format");
        GdipBitmapGetPixel(bmp, 0, 0, &px);
        check(px == 0xffff0000, "red stays red");
        GdipBitmapGetPixel(bmp, 1, 0, &px);
        check(px == 0xff00ff00, "greenish goes to the green corner");
        GdipBitmapGetPixel(bmp, 2, 0, &px);
        check(px == 0xff0000ff, "blueish goes to blue");
        GdipBitmapGetPixel(bmp, 3, 0, &px);
        check(px == 0xffffffff, "light grey goes to white");
        GdipBitmapGetPixel(bmp, 4, 0, &px);
        check(px == 0xff000000, "dark grey goes to black");

        st = GdipBitmapConvertFormat(bmp, PixelFormat24bppRGB, DitherTypeNone, PaletteTypeOptimal, NULL, 0.0f);
        GdipGetImagePixelFormat((GpImage *)bmp, &fmt);
        check(st == Ok && fmt == PixelFormat24bppRGB, "back to 24bppRGB");
        GdipBitmapGetPixel(bmp, 0, 0, &px);
        check(px == 0xffff0000, "pixels survive the round trip");

        st = GdipBitmapConvertFormat(bmp, PixelFormat1bppIndexed, DitherTypeNone, PaletteTypeFixedBW, NULL, 0.0f);
        check(st == Ok, "to 1bppIndexed black and white");
        GdipBitmapGetPixel(bmp, 3, 0, &px);
        check(px == 0xffffffff, "light pixel is white");
        GdipBitmapGetPixel(bmp, 4, 0, &px);
        check(px == 0xff000000, "dark pixel is black");

        custom->Flags = 0; custom->Count = 3;
        custom->Entries[0] = 0xff112233; custom->Entries[1] = 0xff445566; custom->Entries[2] = 0xff778899;
        GdipBitmapSetPixel(bmp, 5, 0, 0xff102030);
        st = GdipBitmapConvertFormat(bmp, PixelFormat4bppIndexed, DitherTypeErrorDiffusion, PaletteTypeCustom, custom, 0.0f);
        check(st == Ok, "to 4bppIndexed with a custom palette");
        GdipBitmapGetPixel(bmp, 5, 0, &px);
        check(px == 0xff112233 || px == 0xffffffff || px == 0xff000000 || px == 0xff445566 || px == 0xff778899,
                "the pixel is a palette colour");

        st = GdipBitmapConvertFormat(bmp, PixelFormat8bppIndexed, DitherTypeNone, PaletteTypeCustom, NULL, 0.0f);
        check(st == InvalidParameter, "a custom palette is required");
        st = GdipBitmapConvertFormat(NULL, PixelFormat24bppRGB, DitherTypeNone, PaletteTypeOptimal, NULL, 0.0f);
        check(st == InvalidParameter, "NULL bitmap");
        GdipBitmapLockBits(bmp, &rc, ImageLockModeRead, PixelFormat4bppIndexed, &bd);
        st = GdipBitmapConvertFormat(bmp, PixelFormat24bppRGB, DitherTypeNone, PaletteTypeOptimal, NULL, 0.0f);
        check(st == WrongState, "a locked bitmap cannot be converted");
        GdipBitmapUnlockBits(bmp, &bd);
        GdipDisposeImage((GpImage *)bmp);
    }

    {
        HDC hdc = GetDC(NULL);
        UINT sizes[2];
        int k;

        for (k = 0; k < 2; k++)
        {
            GpMetafile *mf = NULL;
            GpGraphics *g = NULL;
            HENHMETAFILE hemf = NULL;
            BYTE data[100];

            memset(data, 0x5a, sizeof(data));
            st = GdipRecordMetafile(hdc, EmfTypeEmfPlusOnly, NULL, MetafileFrameUnitPixel, NULL, &mf);
            GdipGetImageGraphicsContext((GpImage *)mf, &g);
            if (k == 1)
            {
                st = GdipComment(g, sizeof(data), data);
                check(st == Ok, "GdipComment records into a metafile");
                st = GdipComment(g, 5, NULL);
                check(st == InvalidParameter, "GdipComment needs data");
            }
            GdipDeleteGraphics(g);
            GdipGetHemfFromMetafile(mf, &hemf);
            sizes[k] = hemf ? GetEnhMetaFileBits(hemf, 0, NULL) : 0;
            if (hemf) DeleteEnhMetaFile(hemf);
            GdipDisposeImage((GpImage *)mf);
        }
        check(sizes[0] && sizes[1] >= sizes[0] + 100, "the comment adds its bytes to the metafile");
        {
            GpBitmap *bm = NULL;
            GpGraphics *g = NULL;

            GdipCreateBitmapFromScan0(2, 2, 0, PixelFormat32bppARGB, NULL, &bm);
            GdipGetImageGraphicsContext((GpImage *)bm, &g);
            st = GdipComment(g, 4, (BYTE *)"abcd");
            check(st == Ok, "GdipComment on a bitmap graphics is accepted");
            GdipDeleteGraphics(g);
            GdipDisposeImage((GpImage *)bm);
        }
        ReleaseDC(NULL, hdc);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
