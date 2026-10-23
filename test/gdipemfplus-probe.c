/* gdipemfplus-probe: EMF+ records that GDI+ on Windows writes, played back (Stained Glass OS
 * wine-sg 6504): DrawLines (compressed points), FillPolygon (relative points, a colour), DrawBeziers,
 * DrawCurve, DrawClosedCurve, DrawString (a font and a string format object), SetClipRect +
 * OffsetClip + ResetClip, SetTextContrast, Comment. The records are put into an EMF by hand
 * (GdiComment "EMF+"), as [MS-EMFPLUS] lays them out, played through GdipEnumerateMetafileDestRectI
 * and GdipPlayMetafileRecord onto a 100x100 bitmap. Prints name=value lines; see
 * test/gdipemfplus-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;
typedef struct { INT X, Y, Width, Height; } GpRect;
typedef BOOL (CALLBACK *EnumProc)(int type, UINT flags, UINT size, const BYTE *data, void *ctx);

static HMODULE g;
#define FN(name) static FARPROC p_##name
FN(GdiplusStartup); FN(GdipCreateMetafileFromEmf); FN(GdipCreateBitmapFromScan0); FN(GdipGetImageGraphicsContext);
FN(GdipGraphicsClear); FN(GdipEnumerateMetafileDestRectI); FN(GdipPlayMetafileRecord); FN(GdipBitmapGetPixel);
FN(GdipDeleteGraphics); FN(GdipDisposeImage);

/* the EMF+ records, one GdiComment */
static BYTE buf[8192];
static UINT len;

static void record(WORD type, WORD flags, const void *data, UINT size)
{
    UINT padded = (size + 3) & ~3u, total = 12 + padded;
    memcpy(buf + len, &type, 2);
    memcpy(buf + len + 2, &flags, 2);
    memcpy(buf + len + 4, &total, 4);
    memcpy(buf + len + 8, &padded, 4);
    memset(buf + len + 12, 0, padded);
    memcpy(buf + len + 12, data, size);
    len += total;
}

/* a growing record body */
static BYTE body[2048];
static UINT blen;
static void put(const void *p, UINT n) { memcpy(body + blen, p, n); blen += n; }
static void put32(DWORD v) { put(&v, 4); }
static void putf(float v) { put(&v, 4); }

#define VERSION 0xdbc01002
enum { T_HEADER = 0x4001, T_EOF = 0x4002, T_COMMENT = 0x4003, T_OBJECT = 0x4008, T_FILLPOLYGON = 0x400c,
       T_DRAWLINES = 0x400d, T_DRAWCLOSEDCURVE = 0x4017, T_DRAWCURVE = 0x4018, T_DRAWBEZIERS = 0x4019,
       T_DRAWSTRING = 0x401c, T_SETTEXTCONTRAST = 0x4020, T_RESETCLIP = 0x4031, T_SETCLIPRECT = 0x4032,
       T_OFFSETCLIP = 0x4035 };

static int status_of[0x50];
static int played[0x50];
static void *metafile;

static BOOL CALLBACK play(int type, UINT flags, UINT size, const BYTE *data, void *ctx)
{
    GpStatus st = ((GpStatus (WINAPI *)(void *, int, UINT, UINT, const BYTE *))p_GdipPlayMetafileRecord)(
            metafile, type, flags, size, data);
    if (type >= 0x4000 && type < 0x4050) { status_of[type - 0x4000] = st; played[type - 0x4000] = 1; }
    return TRUE;
}

static void pen_object(BYTE id, DWORD argb, float width)
{
    blen = 0;
    put32(VERSION); put32(0);                  /* pen, type 0 */
    put32(0); put32(2); putf(width);           /* no optional data, UnitPixel */
    put32(VERSION); put32(0); put32(argb);     /* a solid brush */
    record(T_OBJECT, (2 << 8) | id, body, blen);   /* ObjectTypePen */
}

int main(void)
{
    ULONG_PTR token; StartupInput in = {1};
    void *bitmap = NULL, *graphics = NULL;
    GpRect dest = { 0, 0, 100, 100 };
    RECT frame;
    HDC ref, edc;
    HENHMETAFILE hemf;
    GpStatus st;
    DWORD c;
    int i, dark;

    g = LoadLibraryA("gdiplus.dll");
#define L(n) p_##n = GetProcAddress(g, #n)
    L(GdiplusStartup); L(GdipCreateMetafileFromEmf); L(GdipCreateBitmapFromScan0); L(GdipGetImageGraphicsContext);
    L(GdipGraphicsClear); L(GdipEnumerateMetafileDestRectI); L(GdipPlayMetafileRecord); L(GdipBitmapGetPixel);
    L(GdipDeleteGraphics); L(GdipDisposeImage);
    if (((GpStatus (WINAPI *)(ULONG_PTR *, StartupInput *, void *))p_GdiplusStartup)(&token, &in, NULL))
        { printf("no_gdiplus=1\n"); return 1; }

    /* header: EMF+ only, 96 dpi */
    blen = 0; put32(VERSION); put32(1); put32(96); put32(96);
    record(T_HEADER, 0, body, blen);
    record(T_COMMENT, 0, "hello", 5);
    pen_object(0, 0xffff0000, 3.0f);            /* red */
    pen_object(1, 0xff0000ff, 3.0f);            /* blue */
    pen_object(2, 0xff00ff00, 3.0f);            /* green */

    /* DrawLines, compressed: red across y=10 */
    {
        short p[] = { 10, 10, 50, 10, 90, 10 };
        blen = 0; put32(3); put(p, sizeof(p));
        record(T_DRAWLINES, 0x4000 | 0, body, blen);
    }
    /* FillPolygon, relative points (EmfPlusInteger7/15), a colour: green 10,20 - 40,40 */
    {
        BYTE r[] = { 10, 20,  30, 0,  0, 20,  0x7f & (BYTE)-30, 0 };  /* -30 as a 7-bit integer */
        blen = 0; put32(0xff00ff00); put32(4); put(r, sizeof(r));
        record(T_FILLPOLYGON, 0x8000 | 0x800, body, blen);
    }
    /* DrawBeziers, floats: blue, a straight one along y=25 from 50 to 90 */
    {
        float p[] = { 50, 25, 60, 25, 80, 25, 90, 25 };
        blen = 0; put32(4); put(p, sizeof(p));
        record(T_DRAWBEZIERS, 1, body, blen);
    }
    /* DrawCurve: green through 50,45 70,45 90,45 */
    {
        float p[] = { 50, 45, 70, 45, 90, 45 };
        blen = 0; putf(0.5f); put32(0); put32(2); put32(3); put(p, sizeof(p));
        record(T_DRAWCURVE, 2, body, blen);
    }
    /* DrawClosedCurve, compressed: red round 10,50 30,50 30,56 */
    {
        short p[] = { 10, 50, 30, 50, 30, 56 };
        blen = 0; putf(0.5f); put32(3); put(p, sizeof(p));
        record(T_DRAWCLOSEDCURVE, 0x4000 | 0, body, blen);
    }
    /* SetClipRect (replace) 0,60 50x40, OffsetClip by 50: FillPolygon yellow over 0,60-100,100 shows on the right */
    {
        float rc[] = { 0, 60, 50, 40 }, off[] = { 50, 0 };
        short p[] = { 0, 60, 100, 60, 100, 100, 0, 100 };
        record(T_SETCLIPRECT, 0 << 8, rc, sizeof(rc));
        record(T_OFFSETCLIP, 0, off, sizeof(off));
        blen = 0; put32(0xffffff00); put32(4); put(p, sizeof(p));
        record(T_FILLPOLYGON, 0x8000 | 0x4000, body, blen);
        record(T_RESETCLIP, 0, NULL, 0);
    }
    record(T_SETTEXTCONTRAST, 1400, NULL, 0);
    /* DrawString: a font (object 3), a string format (object 4), black, in 0,62 - 50,98 */
    {
        static const WCHAR family[] = L"Tahoma";
        static const WCHAR text[] = L"WWWW";
        blen = 0; put32(VERSION); putf(18.0f); put32(2); put32(1 /* bold */); put32(0); put32(6); put(family, 12);
        record(T_OBJECT, (6 << 8) | 3, body, blen);      /* ObjectTypeFont */
        blen = 0; put32(VERSION); put32(0x1000 /* no wrap */); put32(0x409); put32(0); put32(0); put32(0); put32(0x409);
        putf(0); put32(0); putf(0); putf(0); putf(1); put32(0); put32(0); put32(0);
        record(T_OBJECT, (7 << 8) | 4, body, blen);      /* ObjectTypeStringFormat */
        blen = 0; put32(0xff000000); put32(4); put32(4);
        { float rc[] = { 2, 62, 46, 36 }; put(rc, sizeof(rc)); }
        put(text, 8);
        record(T_DRAWSTRING, 0x8000 | 3, body, blen);
    }
    record(T_EOF, 0, NULL, 0);

    ref = GetDC(0);
    SetRect(&frame, 0, 0, MulDiv(100, 2540, GetDeviceCaps(ref, LOGPIXELSX)), MulDiv(100, 2540, GetDeviceCaps(ref, LOGPIXELSY)));
    edc = CreateEnhMetaFileW(ref, NULL, &frame, NULL);
    {
        BYTE *comment = malloc(len + 4);
        DWORD sig = 0x2b464d45;   /* "EMF+" */
        memcpy(comment, &sig, 4);
        memcpy(comment + 4, buf, len);
        GdiComment(edc, len + 4, comment);
        free(comment);
    }
    hemf = CloseEnhMetaFile(edc);
    ReleaseDC(0, ref);

    st = ((GpStatus (WINAPI *)(HENHMETAFILE, BOOL, void **))p_GdipCreateMetafileFromEmf)(hemf, TRUE, &metafile);
    if (st) { printf("no_metafile=1 status %d\n", st); return 1; }
    ((GpStatus (WINAPI *)(INT, INT, INT, int, BYTE *, void **))p_GdipCreateBitmapFromScan0)(100, 100, 0, 0x26200a /* 32bppARGB */,
            NULL, &bitmap);
    ((GpStatus (WINAPI *)(void *, void **))p_GdipGetImageGraphicsContext)(bitmap, &graphics);
    ((GpStatus (WINAPI *)(void *, DWORD))p_GdipGraphicsClear)(graphics, 0xffffffff);
    st = ((GpStatus (WINAPI *)(void *, void *, const GpRect *, EnumProc, void *, void *))p_GdipEnumerateMetafileDestRectI)(
            graphics, metafile, &dest, play, NULL, NULL);
    printf("enumerated=%d status %d\n", st == 0, st);

#define PIX(x, y) (((GpStatus (WINAPI *)(void *, INT, INT, DWORD *))p_GdipBitmapGetPixel)(bitmap, x, y, &c), c & 0xffffff)
    {
        static const struct { const char *name; int type; } recs[] = {
            { "comment", T_COMMENT }, { "drawlines", T_DRAWLINES }, { "fillpolygon", T_FILLPOLYGON },
            { "drawbeziers", T_DRAWBEZIERS }, { "drawcurve", T_DRAWCURVE }, { "drawclosedcurve", T_DRAWCLOSEDCURVE },
            { "offsetclip", T_OFFSETCLIP }, { "resetclip", T_RESETCLIP }, { "settextcontrast", T_SETTEXTCONTRAST },
            { "drawstring", T_DRAWSTRING }, { "object", T_OBJECT },
        };
        for (i = 0; i < sizeof(recs) / sizeof(recs[0]); i++)
            printf("played_%s=%d status %d seen %d\n", recs[i].name, played[recs[i].type - 0x4000] &&
                   !status_of[recs[i].type - 0x4000], status_of[recs[i].type - 0x4000], played[recs[i].type - 0x4000]);
    }
    printf("pixel_lines=%d %06lx\n", PIX(50, 10) == 0xff0000, PIX(50, 10));
    printf("pixel_polygon=%d %06lx\n", PIX(25, 30) == 0x00ff00, PIX(25, 30));
    /* a 3-pixel line round y: its colour somewhere across it */
    for (dark = 0, i = 21; i <= 28; i++) if (PIX(70, i) == 0x0000ff) dark++;
    printf("pixel_beziers=%d %d rows\n", dark > 0, dark);
    for (dark = 0, i = 41; i <= 48; i++) if (PIX(70, i) == 0x00ff00) dark++;
    printf("pixel_curve=%d %d rows\n", dark > 0, dark);
    printf("pixel_clipped_out=%d %06lx\n", PIX(25, 70) != 0xffff00, PIX(25, 70));
    printf("pixel_clipped_in=%d %06lx\n", PIX(75, 80) == 0xffff00, PIX(75, 80));
    for (dark = 0, i = 0; i < 46 * 36; i++)
        if (PIX(2 + i % 46, 62 + i / 46) == 0) dark++;
    printf("pixel_string=%d %d dark\n", dark > 20, dark);
    printf("done=1\n");
    return 0;
}
