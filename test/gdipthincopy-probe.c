/* GDI+ thin lines drawn with CompositingModeSourceCopy (patches/sg/6120): only the pixels a line
 * covers are copied. Wine copied the line's whole output rectangle, transparent where the line is
 * not, so the second diagonal of a cross erased the first: PyCharm's (JetBrains Runtime) close
 * button showed one stroke of its X. As PyCharm draws it: a 10x10 32bppRGB bitmap, SourceCopy,
 * cleared black, a 1-pixel white pen, SmoothingModeNone, PixelOffsetModeHalf.
 *   probe MODE   (MODE 1 SourceCopy, 0 SourceOver); prints the 10 rows and DIAG/ANTI/BACK lines */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
typedef int GpStatus;
typedef void GpBitmap, GpGraphics, GpPen, GpBrush;
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL a, b; } StartupInput;
GpStatus WINAPI GdiplusStartup(ULONG_PTR *, const StartupInput *, void *);
GpStatus WINAPI GdipCreateBitmapFromScan0(INT, INT, INT, INT, BYTE *, GpBitmap **);
GpStatus WINAPI GdipGetImageGraphicsContext(GpBitmap *, GpGraphics **);
GpStatus WINAPI GdipSetCompositingMode(GpGraphics *, int);
GpStatus WINAPI GdipSetSmoothingMode(GpGraphics *, int);
GpStatus WINAPI GdipSetPixelOffsetMode(GpGraphics *, int);
GpStatus WINAPI GdipGraphicsClear(GpGraphics *, DWORD);
GpStatus WINAPI GdipCreateSolidFill(DWORD, GpBrush **);
GpStatus WINAPI GdipCreatePen2(GpBrush *, float, int, GpPen **);
GpStatus WINAPI GdipDrawLine(GpGraphics *, GpPen *, float, float, float, float);
GpStatus WINAPI GdipDeleteGraphics(GpGraphics *);
GpStatus WINAPI GdipBitmapGetPixel(GpBitmap *, INT, INT, DWORD *);

int main(int argc, char **argv)
{
    StartupInput in = { 1 };
    ULONG_PTR token;
    GpBitmap *bm; GpGraphics *g; GpBrush *brush; GpPen *pen;
    int x, y, diag = 0, anti = 0, back = 1, mode = argc > 1 ? atoi(argv[1]) : 1;
    char lit[10][10];

    GdiplusStartup(&token, &in, NULL);
    GdipCreateBitmapFromScan0(10, 10, 0, 0x21808 /* PixelFormat32bppRGB */, NULL, &bm);
    GdipGetImageGraphicsContext(bm, &g);
    GdipSetCompositingMode(g, mode);
    GdipSetSmoothingMode(g, 5);
    GdipSetPixelOffsetMode(g, 4 /* Half */);
    GdipGraphicsClear(g, 0xff000000);
    GdipSetSmoothingMode(g, 3 /* None */);
    GdipCreateSolidFill(0xffffffff, &brush);
    GdipCreatePen2(brush, 1.0f, 0 /* UnitWorld */, &pen);
    GdipDrawLine(g, pen, 0.35f, 0.35f, 9.65f, 9.65f);
    GdipDrawLine(g, pen, 9.65f, 0.35f, 0.35f, 9.65f);
    GdipDeleteGraphics(g);
    for (y = 0; y < 10; y++)
    {
        for (x = 0; x < 10; x++)
        {
            DWORD c = 0;
            GdipBitmapGetPixel(bm, x, y, &c);
            lit[y][x] = (c & 0xff) > 0x80;
            putchar(lit[y][x] ? '#' : '.');
            if (!lit[y][x] && (c & 0xffffff)) back = 0;   /* not the black background */
        }
        putchar('\n');
    }
    for (x = 1; x < 9; x++)
    {
        diag += lit[x][x];
        anti += lit[x][9 - x] || lit[x][10 - x];
    }
    for (y = 0; y < 10; y++)
        for (x = 0; x < 10; x++)
            if (lit[y][x] && x != y && x + y != 9 && x + y != 10) back = 0;
    printf("DIAG %d\nANTI %d\nBACK %d\n", diag >= 7, anti >= 7, back);
    return 0;
}
