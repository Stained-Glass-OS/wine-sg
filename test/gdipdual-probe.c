/* gdiplus (patch 2677): EMF-only and EMF+ dual recordings carry GDI records.
 * Everything drawn on a recorded metafile also goes into the GDI half of the
 * file, so a program that can only play GDI (PlayEnhMetaFile) sees the
 * drawing; GDI+ plays the EMF+ half and does not draw the GDI records again.
 * The recorder drew nothing for EMF-only metafiles, and refused lines. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int Status;
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
typedef struct { float X, Y, Width, Height; } RectF;

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    fflush(stdout);
    if (!ok) failures++;
}

#define FN(ret, name, args) static ret (WINAPI *p##name) args
FN(Status, GdiplusStartup, (ULONG_PTR *, const StartupInput *, void *));
FN(Status, GdipRecordMetafile, (HDC, int, const RectF *, int, const WCHAR *, void **));
FN(Status, GdipGetImageGraphicsContext, (void *, void **));
FN(Status, GdipDeleteGraphics, (void *));
FN(Status, GdipDisposeImage, (void *));
FN(Status, GdipCreateSolidFill, (UINT32, void **));
FN(Status, GdipDeleteBrush, (void *));
FN(Status, GdipCreatePen1, (UINT32, float, int, void **));
FN(Status, GdipDeletePen, (void *));
FN(Status, GdipFillRectangleI, (void *, void *, int, int, int, int));
FN(Status, GdipFillEllipseI, (void *, void *, int, int, int, int));
FN(Status, GdipDrawLineI, (void *, void *, int, int, int, int));
FN(Status, GdipDrawRectangleI, (void *, void *, int, int, int, int));
FN(Status, GdipTranslateWorldTransform, (void *, float, float, int));
FN(Status, GdipSetClipRectI, (void *, int, int, int, int, int));
FN(Status, GdipGetHemfFromMetafile, (void *, HENHMETAFILE *));
FN(Status, GdipCreateBitmapFromScan0, (int, int, int, int, BYTE *, void **));
FN(Status, GdipGetImageGraphicsContext2, (void *, void **));
FN(Status, GdipDrawImageI, (void *, void *, int, int));
FN(Status, GdipBitmapGetPixel, (void *, int, int, UINT32 *));
FN(Status, GdipGraphicsClear, (void *, UINT32));
FN(Status, GdipGetMetafileHeaderFromMetafile, (void *, void *));

struct counts { int comments, gdi_draw, gdi_total; };

static int CALLBACK count_proc(HDC dc, HANDLETABLE *ht, const ENHMETARECORD *r, int n, LPARAM arg)
{
    struct counts *c = (struct counts *)arg;

    c->gdi_total++;
    switch (r->iType)
    {
    case EMR_GDICOMMENT: c->comments++; break;
    case EMR_RECTANGLE: case EMR_ELLIPSE: case EMR_POLYGON: case EMR_POLYGON16: case EMR_POLYBEZIER:
    case EMR_POLYBEZIER16: case EMR_POLYLINE: case EMR_POLYLINE16: case EMR_POLYBEZIERTO16:
    case EMR_POLYPOLYGON16: case EMR_LINETO: case EMR_STROKEPATH: case EMR_FILLPATH:
    case EMR_STROKEANDFILLPATH: case EMR_EXTTEXTOUTW: case EMR_STRETCHDIBITS: case EMR_ALPHABLEND:
    case EMR_BITBLT: case EMR_STRETCHBLT: case EMR_FILLRGN: case EMR_POLYBEZIERTO:
        c->gdi_draw++;
        break;
    }
    return 1;
}

/* the pixel at (x,y) after GDI plays the metafile onto white */
static COLORREF play_gdi_pixel(HENHMETAFILE emf, int x, int y, int size)
{
    HDC dc = CreateCompatibleDC(0);
    HBITMAP bmp = CreateCompatibleBitmap(GetDC(0), size, size), old;
    RECT rc = { 0, 0, size, size };
    COLORREF c;

    old = SelectObject(dc, bmp);
    FillRect(dc, &rc, GetStockObject(WHITE_BRUSH));
    PlayEnhMetaFile(dc, emf, &rc);
    c = GetPixel(dc, x, y);
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    return c;
}

static HDC refdc;
static void *record(int type)
{
    RectF frame = { 0, 0, 200, 200 };
    void *mf = NULL;

    if (pGdipRecordMetafile(refdc, type, &frame, 2 /* MetafileFrameUnitPixel */, L"sg", &mf) != 0) return NULL;
    return mf;
}

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;
    void *mf, *g, *red, *blue, *pen, *bmp, *bg, *half;
    HENHMETAFILE emf;
    struct counts c;
    UINT32 px;
    COLORREF cr;
    Status st;
    struct { DWORD type; BYTE rest[600]; } hdr;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipRecordMetafile); LOAD(GdipGetImageGraphicsContext); LOAD(GdipDeleteGraphics);
    LOAD(GdipDisposeImage); LOAD(GdipCreateSolidFill); LOAD(GdipDeleteBrush); LOAD(GdipCreatePen1);
    LOAD(GdipDeletePen); LOAD(GdipFillRectangleI); LOAD(GdipFillEllipseI); LOAD(GdipDrawLineI);
    LOAD(GdipDrawRectangleI); LOAD(GdipTranslateWorldTransform); LOAD(GdipSetClipRectI);
    LOAD(GdipGetHemfFromMetafile); LOAD(GdipCreateBitmapFromScan0); LOAD(GdipGetImageGraphicsContext);
    LOAD(GdipDrawImageI); LOAD(GdipBitmapGetPixel); LOAD(GdipGraphicsClear);
    LOAD(GdipGetMetafileHeaderFromMetafile);
    if (!gp || pGdiplusStartup(&token, &in, NULL) != 0) { printf("FAIL  start\nRESULT: FAIL\n"); return 1; }
    refdc = CreateCompatibleDC(0);

    pGdipCreateSolidFill(0xffff0000, &red);   /* ARGB red */
    pGdipCreateSolidFill(0xff0000ff, &blue);
    pGdipCreatePen1(0xff0000ff, 3.0f, 2, &pen);

    /* ---- EMF+ dual ---- */
    mf = record(5 /* EmfTypeEmfPlusDual */);
    check(mf != NULL, "record an EMF+ dual metafile");
    pGdipGetImageGraphicsContext(mf, &g);
    st = pGdipFillRectangleI(g, red, 20, 20, 60, 60);
    check(st == 0, "dual: FillRectangle");
    st = pGdipFillEllipseI(g, blue, 100, 20, 60, 60);
    check(st == 0, "dual: FillEllipse");
    st = pGdipDrawLineI(g, pen, 10, 150, 190, 150);
    check(st == 0, "dual: DrawLine");
    st = pGdipDrawRectangleI(g, pen, 20, 100, 40, 30);
    check(st == 0, "dual: DrawRectangle");
    pGdipTranslateWorldTransform(g, 0.0f, 40.0f, 0);
    st = pGdipFillRectangleI(g, blue, 150, 100, 20, 20);   /* lands at 150,140 */
    check(st == 0, "dual: FillRectangle under a translation");
    pGdipDeleteGraphics(g);
    /* GDI+ plays the EMF+ half only: a translucent fill is blended once */
    pGdipCreateBitmapFromScan0(200, 200, 0, 0x26200A /* 32bppARGB */, NULL, &bmp);
    pGdipGetImageGraphicsContext2 = (void *)GetProcAddress(gp, "GdipGetImageGraphicsContext");
    pGdipGetImageGraphicsContext2(bmp, &bg);
    pGdipGraphicsClear(bg, 0xffffffff);
    pGdipDrawImageI(bg, mf, 0, 0);
    pGdipBitmapGetPixel(bmp, 50, 50, &px);
    check((px & 0xffffff) == 0xff0000, "GDI+ plays the dual file's EMF+ half (%08lx)", (unsigned long)px);
    pGdipBitmapGetPixel(bmp, 130, 50, &px);
    check((px & 0xffffff) == 0x0000ff, "and the ellipse (%08lx)", (unsigned long)px);
    pGdipBitmapGetPixel(bmp, 5, 5, &px);
    check((px & 0xffffff) == 0xffffff, "and paints nothing else (%08lx)", (unsigned long)px);
    pGdipDeleteGraphics(bg);
    pGdipDisposeImage(bmp);
    emf = NULL;
    pGdipGetHemfFromMetafile(mf, &emf);
    check(emf != NULL, "the recording has an enhanced metafile");
    memset(&c, 0, sizeof(c));
    EnumEnhMetaFile(NULL, emf, count_proc, &c, NULL);
    check(c.comments > 0, "dual: EMF+ records are there (%d comments)", c.comments);
    check(c.gdi_draw >= 3, "dual: GDI drawing records are there too (%d)", c.gdi_draw);
    cr = play_gdi_pixel(emf, 50, 50, 200);
    check(cr == RGB(255, 0, 0), "GDI plays the filled rectangle red (%06lx)", cr);
    cr = play_gdi_pixel(emf, 130, 50, 200);
    check(cr == RGB(0, 0, 255), "GDI plays the filled ellipse blue (%06lx)", cr);
    cr = play_gdi_pixel(emf, 100, 150, 200);
    check(cr == RGB(0, 0, 255), "GDI plays the line (%06lx)", cr);
    cr = play_gdi_pixel(emf, 160, 150, 200);
    check(cr == RGB(0, 0, 255), "GDI plays the rectangle moved by the world transform (%06lx)", cr);
    cr = play_gdi_pixel(emf, 160, 110, 200);
    check(cr == RGB(255, 255, 255), "and not where it would be without the transform (%06lx)", cr);
    cr = play_gdi_pixel(emf, 5, 5, 200);
    check(cr == RGB(255, 255, 255), "nothing else is painted (%06lx)", cr);
    memset(&hdr, 0, sizeof(hdr));
    pGdipGetMetafileHeaderFromMetafile(mf, &hdr);
    check(hdr.type == 5 /* MetafileTypeEmfPlusDual */, "the header says dual (%lu)", hdr.type);

    pGdipDisposeImage(mf);

    /* translucent fill in a dual file: GDI+ blends once, not twice */
    pGdipCreateSolidFill(0x80ff0000, &half);
    mf = record(5);
    pGdipGetImageGraphicsContext(mf, &g);
    pGdipFillRectangleI(g, half, 20, 20, 60, 60);
    pGdipDeleteGraphics(g);
    pGdipCreateBitmapFromScan0(200, 200, 0, 0x26200A, NULL, &bmp);
    pGdipGetImageGraphicsContext2(bmp, &bg);
    pGdipGraphicsClear(bg, 0xffffffff);
    pGdipDrawImageI(bg, mf, 0, 0);
    pGdipBitmapGetPixel(bmp, 50, 50, &px);
    check(((px >> 8) & 0xff) > 0x60 && ((px >> 8) & 0xff) < 0x90 && ((px >> 16) & 0xff) == 0xff,
          "a translucent fill is blended once by GDI+ (%08lx)", (unsigned long)px);
    pGdipDeleteGraphics(bg);
    pGdipDisposeImage(bmp);
    pGdipDisposeImage(mf);
    pGdipDeleteBrush(half);

    /* clip */
    mf = record(5);
    pGdipGetImageGraphicsContext(mf, &g);
    pGdipSetClipRectI(g, 0, 0, 100, 200, 0);
    pGdipFillRectangleI(g, red, 20, 20, 160, 60);
    pGdipDeleteGraphics(g);
    emf = NULL;
    pGdipGetHemfFromMetafile(mf, &emf);
    cr = play_gdi_pixel(emf, 50, 50, 200);
    check(cr == RGB(255, 0, 0), "clip: GDI paints inside the clip (%06lx)", cr);
    cr = play_gdi_pixel(emf, 150, 50, 200);
    check(cr == RGB(255, 255, 255), "clip: and not outside it (%06lx)", cr);
    pGdipDisposeImage(mf);

    /* ---- EMF only ---- */
    mf = record(3 /* EmfTypeEmfOnly */);
    check(mf != NULL, "record an EMF-only metafile");
    pGdipGetImageGraphicsContext(mf, &g);
    st = pGdipFillRectangleI(g, red, 20, 20, 60, 60);
    check(st == 0, "emf only: FillRectangle");
    st = pGdipDrawLineI(g, pen, 10, 150, 190, 150);
    check(st == 0, "emf only: DrawLine is no longer refused (%d)", st);
    st = pGdipFillEllipseI(g, blue, 100, 20, 60, 60);
    check(st == 0, "emf only: FillEllipse");
    pGdipDeleteGraphics(g);
    emf = NULL;
    pGdipGetHemfFromMetafile(mf, &emf);
    memset(&c, 0, sizeof(c));
    EnumEnhMetaFile(NULL, emf, count_proc, &c, NULL);
    check(c.comments == 0, "emf only: no EMF+ comments (%d)", c.comments);
    check(c.gdi_draw >= 2, "emf only: the GDI drawing records (%d)", c.gdi_draw);
    cr = play_gdi_pixel(emf, 50, 50, 200);
    check(cr == RGB(255, 0, 0), "emf only: the rectangle (%06lx)", cr);
    cr = play_gdi_pixel(emf, 100, 150, 200);
    check(cr == RGB(0, 0, 255), "emf only: the line (%06lx)", cr);
    cr = play_gdi_pixel(emf, 130, 50, 200);
    check(cr == RGB(0, 0, 255), "emf only: the ellipse (%06lx)", cr);
    memset(&hdr, 0, sizeof(hdr));
    pGdipGetMetafileHeaderFromMetafile(mf, &hdr);
    check(hdr.type == 3 /* MetafileTypeEmf */, "the header says EMF (%lu)", hdr.type);
    pGdipDisposeImage(mf);

    pGdipDeleteBrush(red);
    pGdipDeleteBrush(blue);
    pGdipDeletePen(pen);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
