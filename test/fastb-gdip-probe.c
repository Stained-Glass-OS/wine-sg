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

    {
        static const GUID matrix_guid = {0x718f2615, 0x7933, 0x40e3, {0xa5, 0x11, 0x5f, 0x68, 0xfe, 0x14, 0xdd, 0x74}};
        static const GUID lut_guid = {0xa7ce72a9, 0x0f7f, 0x40d7, {0xb3, 0xcc, 0xd0, 0xc0, 0x2d, 0x5c, 0x32, 0x12}};
        static const GUID blur_guid = {0x633c80a4, 0x1843, 0x482b, {0x9e, 0xf2, 0xbe, 0x28, 0x34, 0xc5, 0xfd, 0xd4}};
        static const GUID bc_guid = {0xd3a1dbe1, 0x8ec4, 0x4c17, {0x9f, 0x4c, 0xea, 0x97, 0xad, 0x1c, 0x34, 0x3d}};
        static const GUID red_guid = {0x74d29d05, 0x69a4, 0x4266, {0x95, 0x49, 0x3c, 0xc5, 0x28, 0x36, 0xb6, 0x32}};
        HMODULE gp = GetModuleHandleA("gdiplus.dll");
        GpStatus (WINAPI *create)(GUID, void **) = (void *)GetProcAddress(gp, "GdipCreateEffect");
        GpStatus (WINAPI *delete)(void *) = (void *)GetProcAddress(gp, "GdipDeleteEffect");
        GpStatus (WINAPI *set)(void *, const void *, UINT) = (void *)GetProcAddress(gp, "GdipSetEffectParameters");
        GpStatus (WINAPI *get)(void *, UINT *, void *) = (void *)GetProcAddress(gp, "GdipGetEffectParameters");
        GpStatus (WINAPI *apply)(GpBitmap *, void *, RECT *, BOOL, void **, INT *) = (void *)GetProcAddress(gp, "GdipBitmapApplyEffect");
        GpStatus (WINAPI *create_apply)(GpBitmap **, INT, void *, RECT *, RECT *, GpBitmap **, BOOL, void **, INT *) =
                (void *)GetProcAddress(gp, "GdipBitmapCreateApplyEffect");
        GpStatus (WINAPI *draw_fx)(GpGraphics *, GpImage *, GpRectF *, GpMatrix *, void *, GpImageAttributes *, GpUnit) =
                (void *)GetProcAddress(gp, "GdipDrawImageFX");
        void *fx = NULL;
        GpBitmap *bm = NULL, *out = NULL;
        ARGB px = 0;
        ColorMatrix cm;
        UINT size = 4;
        BYTE lut[1024];
        struct { int b, c; } bcp = { 50, 0 };
        struct { float r; BOOL e; } blp = { 4.0f, FALSE };
        RECT roi = { 0, 0, 2, 2 };
        int i, j;

        check(create && delete && set && get && apply && create_apply && draw_fx, "effect functions are exported");
        st = create(matrix_guid, &fx);
        check(st == Ok && fx, "create a colour matrix effect");
        memset(&cm, 0, sizeof(cm));
        for (i = 0; i < 5; i++) cm.m[i][i] = 1.0f;
        cm.m[0][0] = 0.0f; cm.m[0][2] = 1.0f;  /* red goes into blue */
        cm.m[2][2] = 0.0f; cm.m[2][0] = 1.0f;  /* blue goes into red */
        st = set(fx, &cm, sizeof(cm));
        check(st == Ok, "set the matrix");
        st = set(fx, &cm, sizeof(cm) - 4);
        check(st == InvalidParameter, "wrong parameter size");
        size = sizeof(cm); memset(&cm, 0, sizeof(cm));
        st = get(fx, &size, &cm);
        check(st == Ok && size == sizeof(cm) && cm.m[0][2] == 1.0f, "the parameters are kept");
        size = 4;
        st = get(fx, &size, &cm);
        check(st == InvalidParameter, "buffer too small");

        GdipCreateBitmapFromScan0(4, 4, 0, PixelFormat32bppARGB, NULL, &bm);
        GdipBitmapSetPixel(bm, 0, 0, 0xffff2010);
        GdipBitmapSetPixel(bm, 3, 3, 0xffff2010);
        st = apply(bm, fx, NULL, FALSE, NULL, NULL);
        check(st == Ok, "ApplyEffect with a colour matrix");
        GdipBitmapGetPixel(bm, 0, 0, &px);
        check(px == 0xff1020ff, "red and blue swapped");
        delete(fx);

        /* a region only */
        GdipBitmapSetPixel(bm, 0, 0, 0xffff2010);
        create(matrix_guid, &fx);
        cm.m[0][0] = 0.0f;
        memset(&cm, 0, sizeof(cm));
        for (i = 0; i < 5; i++) cm.m[i][i] = 1.0f;
        cm.m[0][0] = 0.0f; cm.m[0][2] = 1.0f; cm.m[2][2] = 0.0f; cm.m[2][0] = 1.0f;
        set(fx, &cm, sizeof(cm));
        GdipBitmapSetPixel(bm, 3, 3, 0xffff2010);
        st = apply(bm, fx, &roi, FALSE, NULL, NULL);
        GdipBitmapGetPixel(bm, 0, 0, &px);
        check(st == Ok && px == 0xff1020ff, "inside the region it applies");
        GdipBitmapGetPixel(bm, 3, 3, &px);
        check(px == 0xffff2010, "outside the region it does not");

        st = create_apply(&bm, 1, fx, &roi, NULL, &out, FALSE, NULL, NULL);
        check(st == Ok && out, "CreateApplyEffect");
        if (out)
        {
            UINT w = 0;
            GdipGetImageWidth((GpImage *)out, &w);
            check(w == 2, "the output is the region");
            GdipDisposeImage((GpImage *)out);
        }
        delete(fx);

        create(lut_guid, &fx);
        for (i = 0; i < 4; i++) for (j = 0; j < 256; j++) lut[i * 256 + j] = i == 1 ? 255 - j : j; /* G inverted */
        st = set(fx, lut, sizeof(lut));
        GdipBitmapSetPixel(bm, 1, 1, 0xff0a2030);
        apply(bm, fx, NULL, FALSE, NULL, NULL);
        GdipBitmapGetPixel(bm, 1, 1, &px);
        check(st == Ok && px == 0xff0adf30, "a lookup table inverts green");
        delete(fx);

        create(bc_guid, &fx);
        st = set(fx, &bcp, sizeof(bcp));
        check(st == Ok, "brightness and contrast set");
        bcp.b = 300;
        st = set(fx, &bcp, sizeof(bcp));
        check(st == InvalidParameter, "brightness out of range");
        bcp.b = 50;
        set(fx, &bcp, sizeof(bcp));
        GdipBitmapSetPixel(bm, 1, 1, 0xff102030);
        apply(bm, fx, NULL, FALSE, NULL, NULL);
        GdipBitmapGetPixel(bm, 1, 1, &px);
        check(px == 0xff425262, "brightness +50 adds 50 at neutral contrast");
        delete(fx);

        create(blur_guid, &fx);
        set(fx, &blp, sizeof(blp));
        {
            GpBitmap *b2 = NULL;
            ARGB centre = 0, near_px = 0;
            GdipCreateBitmapFromScan0(9, 9, 0, PixelFormat32bppARGB, NULL, &b2);
            for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) GdipBitmapSetPixel(b2, i, j, 0xff000000);
            GdipBitmapSetPixel(b2, 4, 4, 0xffffffff);
            st = apply(b2, fx, NULL, FALSE, NULL, NULL);
            GdipBitmapGetPixel(b2, 4, 4, &centre);
            GdipBitmapGetPixel(b2, 5, 4, &near_px);
            check(st == Ok && (centre & 0xff) < 0xff && (near_px & 0xff) > 0, "blur spreads a bright pixel");
            GdipDisposeImage((GpImage *)b2);
        }
        delete(fx);

        create(red_guid, &fx);
        {
            RECT area = { 0, 0, 1, 1 };
            struct { UINT n; RECT *a; } rp = { 1, &area };
            UINT sz = sizeof(rp);
            struct { UINT n; RECT *a; } back = {0};

            st = set(fx, &rp, sizeof(rp));
            check(st == Ok, "red eye areas set");
            area.right = 99; /* the effect keeps its own copy */
            st = get(fx, &sz, &back);
            check(st == Ok && back.n == 1 && back.a && back.a[0].right == 1, "red eye areas are copied");
            free(back.a);
            GdipBitmapSetPixel(bm, 0, 0, 0xffe02020);
            apply(bm, fx, NULL, FALSE, NULL, NULL);
            GdipBitmapGetPixel(bm, 0, 0, &px);
            check(((px >> 16) & 0xff) <= 0x30, "red eye reduced");
        }
        delete(fx);

        {
            GpBitmap *dst = NULL;
            GpGraphics *g = NULL;

            create(matrix_guid, &fx);
            set(fx, &cm, sizeof(cm));
            GdipBitmapSetPixel(bm, 2, 2, 0xffff2010);
            GdipCreateBitmapFromScan0(4, 4, 0, PixelFormat32bppARGB, NULL, &dst);
            GdipGetImageGraphicsContext((GpImage *)dst, &g);
            GdipSetInterpolationMode(g, InterpolationModeNearestNeighbor);
            GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
            st = draw_fx(g, (GpImage *)bm, NULL, NULL, fx, NULL, UnitPixel);
            check(st == Ok, "DrawImageFX with an effect");
            GdipBitmapGetPixel(dst, 2, 2, &px);
            check(px == 0xff1020ff, "the drawn image went through the effect");
            GdipBitmapGetPixel(bm, 2, 2, &px);
            check(px == 0xffff2010, "and the source is unchanged");
            GdipDeleteGraphics(g);
            GdipDisposeImage((GpImage *)dst);
            delete(fx);
        }
        GdipDisposeImage((GpImage *)bm);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
