/* gdipwiden-probe.c: test/gdipwiden-gate.sh (wine-sg 9503). GDI+ wide pens drawn by the widener: compound
 * lines (bands with gaps), inset alignment (a closed figure's stroke inside it, either orientation), round
 * joins, miters cut at the miter limit (LineJoinMiter), dash caps, dash offsets and pen transforms. Each is drawn onto a
 * 32 bpp bitmap and checked at pixels inside and outside the shape Windows draws; the compound line is
 * also drawn through an HDC (the GDI pen path, which cannot draw it).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;
typedef struct { float X, Y; } PointF;

static GpStatus (WINAPI *pGdiplusStartup)(ULONG_PTR *, const StartupInput *, void *);
static GpStatus (WINAPI *pGdipCreateBitmapFromScan0)(INT, INT, INT, INT, BYTE *, void **);
static GpStatus (WINAPI *pGdipGetImageGraphicsContext)(void *, void **);
static GpStatus (WINAPI *pGdipCreateFromHDC)(HDC, void **);
static GpStatus (WINAPI *pGdipGraphicsClear)(void *, DWORD);
static GpStatus (WINAPI *pGdipSetSmoothingMode)(void *, int);
static GpStatus (WINAPI *pGdipBitmapGetPixel)(void *, INT, INT, DWORD *);
static GpStatus (WINAPI *pGdipDeleteGraphics)(void *);
static GpStatus (WINAPI *pGdipDisposeImage)(void *);
static GpStatus (WINAPI *pGdipCreatePen1)(DWORD, float, int, void **);
static GpStatus (WINAPI *pGdipDeletePen)(void *);
static GpStatus (WINAPI *pGdipSetPenCompoundArray)(void *, const float *, INT);
static GpStatus (WINAPI *pGdipSetPenMode)(void *, int);
static GpStatus (WINAPI *pGdipSetPenLineJoin)(void *, int);
static GpStatus (WINAPI *pGdipSetPenMiterLimit)(void *, float);
static GpStatus (WINAPI *pGdipSetPenDashArray)(void *, const float *, INT);
static GpStatus (WINAPI *pGdipSetPenDashCap197819)(void *, int);
static GpStatus (WINAPI *pGdipSetPenDashOffset)(void *, float);
static GpStatus (WINAPI *pGdipDrawLines)(void *, void *, const PointF *, INT);
static GpStatus (WINAPI *pGdipDrawPolygon)(void *, void *, const PointF *, INT);
static GpStatus (WINAPI *pGdipFlush)(void *, int);
static GpStatus (WINAPI *pGdipCreateMatrix2)(float, float, float, float, float, float, void **);
static GpStatus (WINAPI *pGdipSetPenTransform)(void *, void *);
static GpStatus (WINAPI *pGdipDeleteMatrix)(void *);

#define UnitPixel 2
#define PixelFormat32bppARGB 0x26200a
#define LineJoinMiter 0
#define LineJoinRound 2
#define PenAlignmentInset 1
#define DashCapRound 2

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void *bitmap, *graphics;

static void *new_canvas(void)
{
    if (graphics) pGdipDeleteGraphics(graphics);
    if (bitmap) pGdipDisposeImage(bitmap);
    pGdipCreateBitmapFromScan0(320, 520, 0, PixelFormat32bppARGB, NULL, &bitmap);
    pGdipGetImageGraphicsContext(bitmap, &graphics);
    pGdipSetSmoothingMode(graphics, 3 /* SmoothingModeNone */);
    pGdipGraphicsClear(graphics, 0xffffffff);
    return graphics;
}

static int inked(int x, int y)
{
    DWORD c = 0;
    pGdipBitmapGetPixel(bitmap, x, y, &c);
    return (c & 0xffffff) != 0xffffff;
}

static void *pen(float width)
{
    void *p = NULL;
    pGdipCreatePen1(0xff000000, width, UnitPixel, &p);
    return p;
}

/* each EXPECT entry: x, y, inked */
static void expect(const char *name, const int (*pts)[3], int count)
{
    char buf[200];
    int i;
    for (i = 0; i < count; i++)
    {
        int got = inked(pts[i][0], pts[i][1]);
        sprintf(buf, "%s: (%d,%d) %s", name, pts[i][0], pts[i][1], got ? "inked" : "blank");
        check(got == pts[i][2], buf);
    }
}

#define LOAD(name) (p##name = (void *)GetProcAddress(g, #name))

int main(void)
{
    HMODULE g = LoadLibraryA("gdiplus.dll");
    StartupInput si = { 1 };
    ULONG_PTR token;
    void *p;

    LOAD(GdiplusStartup); LOAD(GdipCreateBitmapFromScan0); LOAD(GdipGetImageGraphicsContext); LOAD(GdipCreateFromHDC);
    LOAD(GdipGraphicsClear); LOAD(GdipSetSmoothingMode); LOAD(GdipBitmapGetPixel); LOAD(GdipDeleteGraphics);
    LOAD(GdipDisposeImage); LOAD(GdipCreatePen1); LOAD(GdipDeletePen); LOAD(GdipSetPenCompoundArray);
    LOAD(GdipSetPenMode); LOAD(GdipSetPenLineJoin); LOAD(GdipSetPenMiterLimit); LOAD(GdipSetPenDashArray);
    LOAD(GdipSetPenDashCap197819); LOAD(GdipSetPenDashOffset); LOAD(GdipDrawLines); LOAD(GdipDrawPolygon);
    LOAD(GdipFlush); LOAD(GdipCreateMatrix2); LOAD(GdipSetPenTransform); LOAD(GdipDeleteMatrix);
    pGdiplusStartup(&token, &si, NULL);

    /* compound line: a 30 px pen in bands 0-0.3 and 0.7-1 (y 35-44 and 56-65), a gap between */
    {
        static const PointF line[] = { {20, 50}, {180, 50} };
        static const float compound[] = { 0, 0.3f, 0.7f, 1 };
        static const int pts[][3] = { {100, 38, 1}, {100, 50, 0}, {100, 47, 0}, {100, 62, 1}, {100, 70, 0} };
        new_canvas();
        p = pen(30);
        pGdipSetPenCompoundArray(p, compound, 4);
        pGdipDrawLines(graphics, p, line, 2);
        expect("compound line", pts, 5);
        pGdipDeletePen(p);
    }

    /* inset: a 20 px pen around the square 50-100 draws x 50-70 inside it, nothing outside, in both
     * orientations */
    {
        static const PointF cw[] = { {50, 50}, {100, 50}, {100, 100}, {50, 100} };
        static const PointF ccw[] = { {50, 50}, {50, 100}, {100, 100}, {100, 50} };
        static const int pts[][3] = { {55, 75, 1}, {67, 75, 1}, {45, 75, 0}, {75, 75, 0}, {105, 75, 0},
                                      {95, 75, 1}, {75, 45, 0}, {75, 55, 1} };
        new_canvas();
        p = pen(20);
        pGdipSetPenMode(p, PenAlignmentInset);
        pGdipDrawPolygon(graphics, p, cw, 4);
        expect("inset, clockwise", pts, 8);
        new_canvas();
        pGdipDrawPolygon(graphics, p, ccw, 4);
        expect("inset, anticlockwise", pts, 8);
        pGdipDeletePen(p);
    }

    /* round join: a 40 px pen turning at (100,150); a point 18 px out on the diagonal is on the arc (a bevel
     * leaves it blank), the miter corner is not */
    {
        static const PointF turn[] = { {20, 150}, {100, 150}, {100, 250} };
        static const int pts[][3] = { {112, 137, 1}, {118, 132, 0}, {60, 150, 1} };
        new_canvas();
        p = pen(40);
        pGdipSetPenLineJoin(p, LineJoinRound);
        pGdipDrawLines(graphics, p, turn, 3);
        expect("round join", pts, 3);
        pGdipDeletePen(p);
    }

    /* a miter (20 px pen, square corner at (100,400), limit 1.2) is cut 12 px from the joint along the
     * diagonal: 11 px out is inked (a bevel stops at 7), 13 px out is not (the whole miter reaches 14) */
    {
        static const PointF turn[] = { {20, 400}, {100, 400}, {100, 480} };
        static const int pts[][3] = { {107, 392, 1}, {109, 390, 0}, {60, 400, 1} };
        new_canvas();
        p = pen(20);
        pGdipSetPenLineJoin(p, LineJoinMiter);
        pGdipSetPenMiterLimit(p, 1.2f);
        pGdipDrawLines(graphics, p, turn, 3);
        expect("miter cut at the limit", pts, 3);
        pGdipDeletePen(p);
    }

    /* round dash caps: 8 px pen, dashes and gaps of 3 widths (x 20-44, 68-92): the caps reach 4 px past each
     * dash end, the line's own start stays flat */
    {
        static const PointF line[] = { {20, 300}, {300, 300} };
        static const float dashes[] = { 3, 3 };
        static const int pts[][3] = { {30, 300, 1}, {46, 300, 1}, {56, 300, 0}, {66, 300, 1}, {17, 300, 0} };
        new_canvas();
        p = pen(8);
        pGdipSetPenDashArray(p, dashes, 2);
        pGdipSetPenDashCap197819(p, DashCapRound);
        pGdipDrawLines(graphics, p, line, 2);
        expect("round dash caps", pts, 5);
        pGdipDeletePen(p);
    }

    /* dash offset: 10 px pen, dashes and gaps of 2 widths, offset 1 width: the line starts halfway into a
     * dash (x 20-30), then a gap (30-50) */
    {
        static const PointF line[] = { {20, 300}, {300, 300} };
        static const float dashes[] = { 2, 2 };
        static const int pts[][3] = { {25, 300, 1}, {35, 300, 0}, {45, 300, 0}, {55, 300, 1} };
        new_canvas();
        p = pen(10);
        pGdipSetPenDashArray(p, dashes, 2);
        pGdipSetPenDashOffset(p, 1);
        pGdipDrawLines(graphics, p, line, 2);
        expect("dash offset", pts, 4);
        pGdipDeletePen(p);
    }

    /* a pen transform scaling y by 4: a 10 px pen draws a horizontal line 40 px thick, a vertical one 10 */
    {
        static const PointF across[] = { {20, 450}, {180, 450} }, down[] = { {250, 400}, {250, 500} };
        static const int pts[][3] = { {100, 465, 1}, {100, 435, 1}, {100, 475, 0}, {258, 450, 0}, {252, 450, 1} };
        void *m = NULL;
        new_canvas();
        p = pen(10);
        pGdipCreateMatrix2(1, 0, 0, 4, 0, 0, &m);
        pGdipSetPenTransform(p, m);
        pGdipDrawLines(graphics, p, across, 2);
        pGdipDrawLines(graphics, p, down, 2);
        expect("pen transform", pts, 5);
        pGdipDeleteMatrix(m);
        pGdipDeletePen(p);
    }

    /* the compound line through an HDC (a DIB section): GDI pens cannot draw it, so GDI+ widens it */
    {
        static const PointF line[] = { {20, 50}, {180, 50} };
        static const float compound[] = { 0, 0.3f, 0.7f, 1 };
        BITMAPINFO bi = {{sizeof(bi.bmiHeader), 200, -100, 1, 32, BI_RGB}};
        HDC hdc = CreateCompatibleDC(NULL);
        DWORD *bits;
        HBITMAP dib = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
        void *hg = NULL;
        char buf[100];

        SelectObject(hdc, dib);
        PatBlt(hdc, 0, 0, 200, 100, WHITENESS);
        pGdipCreateFromHDC(hdc, &hg);
        pGdipSetSmoothingMode(hg, 3);
        p = pen(30);
        pGdipSetPenCompoundArray(p, compound, 4);
        pGdipDrawLines(hg, p, line, 2);
        pGdipFlush(hg, 1);
        GdiFlush();
        sprintf(buf, "compound line on an HDC: band %06lx, gap %06lx, band %06lx", bits[38 * 200 + 100] & 0xffffff,
                bits[50 * 200 + 100] & 0xffffff, bits[62 * 200 + 100] & 0xffffff);
        check((bits[38 * 200 + 100] & 0xffffff) != 0xffffff && (bits[50 * 200 + 100] & 0xffffff) == 0xffffff &&
              (bits[62 * 200 + 100] & 0xffffff) != 0xffffff, buf);
        pGdipDeletePen(p);
        pGdipDeleteGraphics(hg);
        DeleteDC(hdc);
        DeleteObject(dib);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
