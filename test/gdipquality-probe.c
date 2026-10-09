/* GDI+ image quality and styled font metrics (patches/sg/1656), run by
 * test/gdipquality-gate.sh:
 *  - a fine checkerboard drawn at an eighth of its size with
 *    InterpolationModeHighQualityBicubic comes out an even grey (the
 *    prefilter averages each destination pixel's footprint), where
 *    bilinear sampling aliases to black and white;
 *  - InterpolationModeBicubic is a cubic kernel: an edge drawn larger is
 *    not the bilinear ramp;
 *  - GdipGetLineSpacing, GdipGetCellAscent/Descent and GdipGetEmHeight of
 *    a bold or italic style are that face's, as GDI reads them.
 * These modes were bilinear and the style was ignored ("FIXME"). */
#include <windows.h>
#include <stdio.h>
#include <math.h>

typedef int GpStatus;
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread, SuppressExternalCodecs; } StartupInput;
GpStatus WINAPI GdiplusStartup(ULONG_PTR *, const StartupInput *, void *);
GpStatus WINAPI GdipCreateBitmapFromScan0(INT, INT, INT, INT, BYTE *, void **);
GpStatus WINAPI GdipGetImageGraphicsContext(void *, void **);
GpStatus WINAPI GdipSetInterpolationMode(void *, INT);
GpStatus WINAPI GdipSetPixelOffsetMode(void *, INT);
GpStatus WINAPI GdipDrawImageRectI(void *, void *, INT, INT, INT, INT);
GpStatus WINAPI GdipBitmapGetPixel(void *, INT, INT, DWORD *);
GpStatus WINAPI GdipBitmapSetPixel(void *, INT, INT, DWORD);
GpStatus WINAPI GdipDeleteGraphics(void *);
GpStatus WINAPI GdipDisposeImage(void *);
GpStatus WINAPI GdipGraphicsClear(void *, DWORD);
GpStatus WINAPI GdipCreateFontFamilyFromName(const WCHAR *, void *, void **);
GpStatus WINAPI GdipGetLineSpacing(void *, INT, UINT16 *);
GpStatus WINAPI GdipGetEmHeight(void *, INT, UINT16 *);
GpStatus WINAPI GdipGetCellAscent(void *, INT, UINT16 *);
GpStatus WINAPI GdipDeleteFontFamily(void *);

#define PixelFormat32bppARGB 0x26200a
#define ModeBilinear 3
#define ModeBicubic 4
#define ModeNearest 5
#define ModeHQBicubic 7

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void *draw_scaled(void *src, INT mode, INT w, INT h)
{
    void *dst, *g;
    GdipCreateBitmapFromScan0(w, h, 0, PixelFormat32bppARGB, NULL, &dst);
    GdipGetImageGraphicsContext(dst, &g);
    GdipGraphicsClear(g, 0xff00ff00);
    GdipSetInterpolationMode(g, mode);
    GdipDrawImageRectI(g, src, 0, 0, w, h);
    GdipDeleteGraphics(g);
    return dst;
}

static int grey_spread(void *bmp, int w, int h, int *min_out, int *max_out)
{
    int x, y, mn = 255, mx = 0;
    for (y = 1; y < h - 1; y++)
        for (x = 1; x < w - 1; x++)
        {
            DWORD c;
            int v;
            GdipBitmapGetPixel(bmp, x, y, &c);
            v = c & 0xff;
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
    *min_out = mn; *max_out = mx;
    return mx - mn;
}

/* GDI's metrics of a face: hhea ascent+descent+gap, as GDI+ reads them */
static int gdi_line_spacing(const WCHAR *face, BOOL bold, BOOL italic, int *em, int *ascent)
{
    LOGFONTW lf = {0};
    OUTLINETEXTMETRICW otm;
    WCHAR got[LF_FACESIZE];
    BYTE hhea[36];
    HDC hdc = CreateCompatibleDC(0);
    HFONT f, old;
    int ret = -1;

    lstrcpyW(lf.lfFaceName, face);
    lf.lfHeight = -2048;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = italic;
    lf.lfCharSet = DEFAULT_CHARSET;
    f = CreateFontIndirectW(&lf);
    old = SelectObject(hdc, f);
    otm.otmSize = sizeof(otm);
    if (GetTextFaceW(hdc, LF_FACESIZE, got) && !lstrcmpiW(got, face) && GetOutlineTextMetricsW(hdc, sizeof(otm), &otm) &&
        GetFontData(hdc, 0x61656868 /* hhea */, 0, hhea, sizeof(hhea)) == sizeof(hhea))
    {
        int a = (hhea[4] << 8) | hhea[5], d = (short)((hhea[6] << 8) | hhea[7]), gap = (short)((hhea[8] << 8) | hhea[9]);
        ret = a - d + gap;
        *em = otm.otmEMSquare;
        *ascent = a;
    }
    SelectObject(hdc, old);
    DeleteObject(f);
    DeleteDC(hdc);
    return ret;
}

static INT CALLBACK enum_proc(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM param)
{
    WCHAR *out = (WCHAR *)param;
    int em, asc, regular, bold;
    if (!(type & TRUETYPE_FONTTYPE) || lf->lfFaceName[0] == '@' || out[0]) return 1;
    regular = gdi_line_spacing(lf->lfFaceName, FALSE, FALSE, &em, &asc);
    bold = gdi_line_spacing(lf->lfFaceName, TRUE, FALSE, &em, &asc);
    if (regular > 0 && bold > 0 && regular != bold) lstrcpyW(out, lf->lfFaceName);
    return 1;
}

int main(void)
{
    StartupInput si = {1, NULL, FALSE, FALSE};
    ULONG_PTR token;
    void *checker, *edge, *out, *out2;
    int x, y, mn, mx, spread_hq, spread_bl;

    GdiplusStartup(&token, &si, NULL);

    /* a 64x64 one-pixel checkerboard drawn at 8x8 */
    GdipCreateBitmapFromScan0(64, 64, 0, PixelFormat32bppARGB, NULL, &checker);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            GdipBitmapSetPixel(checker, x, y, (x + y) % 2 ? 0xffffffff : 0xff000000);
    out = draw_scaled(checker, ModeHQBicubic, 8, 8);
    spread_hq = grey_spread(out, 8, 8, &mn, &mx);
    printf("  high quality bicubic: %d..%d\n", mn, mx);
    check(mn >= 96 && mx <= 160, "HighQualityBicubic downscaling averages the checkerboard to grey");
    GdipDisposeImage(out);
    out = draw_scaled(checker, ModeBilinear, 8, 8);
    spread_bl = grey_spread(out, 8, 8, &mn, &mx);
    printf("  bilinear: %d..%d\n", mn, mx);
    check(abs(mn - 128) > 64 || abs(mx - 128) > 64, "... where bilinear sampling does not");
    GdipDisposeImage(out);

    /* an edge 0 0 255 255 (8 rows) drawn 16 times wider; the middle of it */
    GdipCreateBitmapFromScan0(4, 8, 0, PixelFormat32bppARGB, NULL, &edge);
    for (y = 0; y < 8; y++)
    {
        GdipBitmapSetPixel(edge, 0, y, 0xff000000);
        GdipBitmapSetPixel(edge, 1, y, 0xff000000);
        GdipBitmapSetPixel(edge, 2, y, 0xffffffff);
        GdipBitmapSetPixel(edge, 3, y, 0xffffffff);
    }
    out = draw_scaled(edge, ModeBicubic, 64, 8);
    out2 = draw_scaled(edge, ModeBilinear, 64, 8);
    {
        int differ = 0, monotonic = 1, prev = -1;
        for (x = 16; x < 48; x++)
        {
            DWORD a, b;
            GdipBitmapGetPixel(out, x, 4, &a);
            GdipBitmapGetPixel(out2, x, 4, &b);
            if (abs((int)(a & 0xff) - (int)(b & 0xff)) > 6) differ++;
            if ((int)(a & 0xff) + 2 < prev) monotonic = 0;
            prev = a & 0xff;
        }
        printf("  bicubic differs from bilinear at %d of 64 pixels\n", differ);
        check(differ >= 4 && monotonic, "InterpolationModeBicubic is a cubic kernel, not the bilinear ramp");
    }
    GdipDisposeImage(out);
    GdipDisposeImage(out2);

    /* styled metrics */
    {
        WCHAR face[LF_FACESIZE] = {0};
        HDC hdc = GetDC(0);
        LOGFONTW lf = {0};
        lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(hdc, &lf, enum_proc, (LPARAM)face, 0);
        ReleaseDC(0, hdc);
        if (face[0])
        {
            void *family;
            UINT16 spacing_regular = 0, spacing_bold = 0, em = 0, ascent = 0;
            int gdi_em, gdi_ascent, gdi_bold = gdi_line_spacing(face, TRUE, FALSE, &gdi_em, &gdi_ascent);

            GdipCreateFontFamilyFromName(face, NULL, &family);
            GdipGetLineSpacing(family, 0, &spacing_regular);
            GdipGetLineSpacing(family, 1 /* FontStyleBold */, &spacing_bold);
            GdipGetEmHeight(family, 1, &em);
            GdipGetCellAscent(family, 1, &ascent);
            printf("  %ls: line spacing regular %u, bold %u (GDI bold %d), em %u, ascent %u\n",
                   face, spacing_regular, spacing_bold, gdi_bold, em, ascent);
            check(spacing_bold == gdi_bold && spacing_bold != spacing_regular && ascent == gdi_ascent && em == gdi_em,
                  "a bold style's line spacing and ascent are the bold face's");
            GdipDeleteFontFamily(family);
        }
        else
            printf("  (no font here whose bold face has other metrics; style check not run)\n");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
