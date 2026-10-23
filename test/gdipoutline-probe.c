/* gdipoutline-probe: GdipWindingModeOutline and GdipSetImageAttributesOutputChannel
 * (Stained Glass OS wine-sg 6505). Prints name=value lines; see test/gdipoutline-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <math.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;
typedef struct { float X, Y; } PointF;

static HMODULE g;
#define FN(name) static FARPROC p_##name
FN(GdiplusStartup); FN(GdipCreatePath); FN(GdipAddPathPolygon); FN(GdipStartPathFigure); FN(GdipWindingModeOutline);
FN(GdipGetPointCount); FN(GdipGetPathTypes); FN(GdipCreateBitmapFromScan0); FN(GdipGetImageGraphicsContext);
FN(GdipGraphicsClear); FN(GdipCreateSolidFill); FN(GdipFillPath); FN(GdipBitmapGetPixel); FN(GdipSetPathFillMode);
FN(GdipCreateImageAttributes); FN(GdipSetImageAttributesOutputChannel); FN(GdipDrawImageRectRectI);
FN(GdipBitmapSetPixel); FN(GdipDeleteGraphics); FN(GdipDisposeImage); FN(GdipDeletePath); FN(GdipSetSmoothingMode);
#define CALL(n, sig, ...) ((GpStatus (WINAPI *) sig) p_##n)(__VA_ARGS__)

static void *path_of(const PointF *const *figs, const int *counts, int n)
{
    void *path = NULL;
    int i;
    CALL(GdipCreatePath, (int, void **), 1 /* winding */, &path);
    for (i = 0; i < n; i++)
    {
        CALL(GdipStartPathFigure, (void *), path);
        CALL(GdipAddPathPolygon, (void *, const PointF *, int), path, figs[i], counts[i]);
    }
    return path;
}

static int figures(void *path, int *count)
{
    BYTE types[256];
    int i, n = 0;
    CALL(GdipGetPointCount, (void *, int *), path, count);
    if (*count > 256) return -1;
    CALL(GdipGetPathTypes, (void *, BYTE *, int), path, types, *count);
    for (i = 0; i < *count; i++) if ((types[i] & 7) == 0) n++;
    return n;
}

/* the path filled (winding) onto a white 40x40 bitmap: is X,Y black? */
static int filled(void *path, int x, int y)
{
    void *bmp = NULL, *gfx = NULL, *brush = NULL;
    DWORD c = 0;
    CALL(GdipCreateBitmapFromScan0, (INT, INT, INT, int, BYTE *, void **), 40, 40, 0, 0x26200a, NULL, &bmp);
    CALL(GdipGetImageGraphicsContext, (void *, void **), bmp, &gfx);
    CALL(GdipGraphicsClear, (void *, DWORD), gfx, 0xffffffff);
    CALL(GdipCreateSolidFill, (DWORD, void **), 0xff000000, &brush);
    CALL(GdipFillPath, (void *, void *, void *), gfx, brush, path);
    CALL(GdipBitmapGetPixel, (void *, INT, INT, DWORD *), bmp, x, y, &c);
    CALL(GdipDeleteGraphics, (void *), gfx);
    CALL(GdipDisposeImage, (void *), bmp);
    return (c & 0xffffff) == 0;
}

int main(void)
{
    ULONG_PTR token; StartupInput in = {1};
    int count, n;
    GpStatus st;

    g = LoadLibraryA("gdiplus.dll");
#define L(n) p_##n = GetProcAddress(g, #n)
    L(GdiplusStartup); L(GdipCreatePath); L(GdipAddPathPolygon); L(GdipStartPathFigure); L(GdipWindingModeOutline);
    L(GdipGetPointCount); L(GdipGetPathTypes); L(GdipCreateBitmapFromScan0); L(GdipGetImageGraphicsContext);
    L(GdipGraphicsClear); L(GdipCreateSolidFill); L(GdipFillPath); L(GdipBitmapGetPixel); L(GdipSetPathFillMode);
    L(GdipCreateImageAttributes); L(GdipSetImageAttributesOutputChannel); L(GdipDrawImageRectRectI);
    L(GdipBitmapSetPixel); L(GdipDeleteGraphics); L(GdipDisposeImage); L(GdipDeletePath); L(GdipSetSmoothingMode);
    if (CALL(GdiplusStartup, (ULONG_PTR *, StartupInput *, void *), &token, &in, NULL)) { printf("no_gdiplus=1\n"); return 1; }

    /* two overlapping squares: their union, 8 corners, one figure */
    {
        static const PointF a[] = { {0, 0}, {10, 0}, {10, 10}, {0, 10} }, b[] = { {5, 5}, {15, 5}, {15, 15}, {5, 15} };
        const PointF *figs[] = { a, b }; int counts[] = { 4, 4 };
        void *path = path_of(figs, counts, 2);
        st = CALL(GdipWindingModeOutline, (void *, void *, float), path, NULL, 0.25f);
        n = figures(path, &count);
        printf("union=%d status %d points %d figures %d\n", st == 0 && count == 8 && n == 1, st, count, n);
        printf("union_area=%d\n", filled(path, 7, 7) && filled(path, 12, 12) && filled(path, 2, 2) && !filled(path, 12, 2)
               && !filled(path, 2, 12));
        CALL(GdipDeletePath, (void *), path);
    }
    /* a pentagram: winding fills its middle; the outline is its 10-pointed rim */
    {
        PointF star[5];
        const PointF *figs[] = { star }; int counts[] = { 5 }, i;
        void *path;
        for (i = 0; i < 5; i++)
        {
            double a = -M_PI / 2 + i * 4 * M_PI / 5;
            star[i].X = 20 + 15 * cos(a); star[i].Y = 20 + 15 * sin(a);
        }
        path = path_of(figs, counts, 1);
        st = CALL(GdipWindingModeOutline, (void *, void *, float), path, NULL, 0.25f);
        n = figures(path, &count);
        printf("star=%d status %d points %d figures %d\n", st == 0 && count == 10 && n == 1, st, count, n);
        printf("star_middle=%d\n", filled(path, 20, 20));
        CALL(GdipDeletePath, (void *), path);
    }
    /* a square with a hole turned the other way: two figures, the hole stays empty */
    {
        static const PointF outer[] = { {0, 0}, {30, 0}, {30, 30}, {0, 30} }, hole[] = { {10, 10}, {10, 20}, {20, 20}, {20, 10} };
        const PointF *figs[] = { outer, hole }; int counts[] = { 4, 4 };
        void *path = path_of(figs, counts, 2);
        st = CALL(GdipWindingModeOutline, (void *, void *, float), path, NULL, 0.25f);
        n = figures(path, &count);
        printf("hole=%d status %d points %d figures %d\n", st == 0 && count == 8 && n == 2, st, count, n);
        printf("hole_area=%d\n", filled(path, 5, 5) && !filled(path, 15, 15));
        CALL(GdipDeletePath, (void *), path);
    }

    /* output channels of a red picture: no cyan (white), all magenta and yellow (black), no black ink */
    {
        void *src = NULL, *dst = NULL, *gfx = NULL, *attr = NULL;
        static const struct { const char *name; int channel; DWORD want; } ch[] = {
            { "c", 0, 0xffffff }, { "m", 1, 0x000000 }, { "y", 2, 0x000000 }, { "k", 3, 0xffffff } };
        int i, x, y;
        DWORD c = 0;
        CALL(GdipCreateBitmapFromScan0, (INT, INT, INT, int, BYTE *, void **), 4, 4, 0, 0x26200a, NULL, &src);
        for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) CALL(GdipBitmapSetPixel, (void *, INT, INT, DWORD), src, x, y, 0xffff0000);
        for (i = 0; i < 4; i++)
        {
            CALL(GdipCreateBitmapFromScan0, (INT, INT, INT, int, BYTE *, void **), 4, 4, 0, 0x26200a, NULL, &dst);
            CALL(GdipGetImageGraphicsContext, (void *, void **), dst, &gfx);
            CALL(GdipCreateImageAttributes, (void **), &attr);
            st = CALL(GdipSetImageAttributesOutputChannel, (void *, int, BOOL, int), attr, 0, TRUE, ch[i].channel);
            CALL(GdipDrawImageRectRectI, (void *, void *, INT, INT, INT, INT, INT, INT, INT, INT, int, void *, void *, void *),
                 gfx, src, 0, 0, 4, 4, 0, 0, 4, 4, 2 /* UnitPixel */, attr, NULL, NULL);
            CALL(GdipBitmapGetPixel, (void *, INT, INT, DWORD *), dst, 2, 2, &c);
            printf("channel_%s=%d status %d got %06lx\n", ch[i].name, st == 0 && (c & 0xffffff) == ch[i].want, st, c & 0xffffff);
            CALL(GdipDeleteGraphics, (void *), gfx);
            CALL(GdipDisposeImage, (void *), dst);
        }
        st = CALL(GdipSetImageAttributesOutputChannel, (void *, int, BOOL, int), attr, 0, TRUE, 4);
        printf("channel_bad=%d status %d\n", st == 2 /* InvalidParameter */, st);
    }
    printf("done=1\n");
    return 0;
}
