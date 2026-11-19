/* gdippen-probe: EMF+ pens with optional data (Stained Glass OS wine-sg 8008). Played back, a pen's
 * dash pattern (PenDataDashedLine), transform, dash cap, alignment and compound lines were skipped with
 * "not supported" (a dashed line drew solid); recorded, a pen's alignment was written as a float and its
 * compound lines not at all. A pen record is written by hand as [MS-EMFPLUS] lays it out and played
 * onto a bitmap; a pen with an inset alignment and compound lines is recorded and its object record
 * read back. Prints name=value lines; see test/gdippen-gate.sh.
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
FN(GdipRecordMetafile); FN(GdipCreatePen1); FN(GdipSetPenMode); FN(GdipSetPenCompoundArray); FN(GdipDrawLineI); FN(GdipDeletePen);

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

static void *metafile;
static int object_status = -1;
static BYTE recorded_pen[256];
static UINT recorded_pen_size;

static BOOL CALLBACK play(int type, UINT flags, UINT size, const BYTE *data, void *ctx)
{
    GpStatus st = ((GpStatus (WINAPI *)(void *, int, UINT, UINT, const BYTE *))p_GdipPlayMetafileRecord)(
            metafile, type, flags, size, data);
    if (type == 0x4008) object_status = st;
    return TRUE;
}

static BOOL CALLBACK grab(int type, UINT flags, UINT size, const BYTE *data, void *ctx)
{
    if (type == 0x4008 && ((flags >> 8) & 0x7f) == 2 && size <= sizeof(recorded_pen))
    {
        memcpy(recorded_pen, data, size);
        recorded_pen_size = size;
    }
    return TRUE;
}

#define VERSION 0xdbc01002

/* the pen's optional data as given, then a solid brush */
static void pen_object(BYTE id, DWORD argb, float width, DWORD flags, const void *opt, UINT optlen)
{
    blen = 0;
    put32(VERSION); put32(0);
    put32(flags); put32(2); putf(width);        /* UnitPixel */
    put(opt, optlen);
    put32(VERSION); put32(0); put32(argb);
    record(0x4008, (2 << 8) | id, body, blen);
}

/* the columns of a line drawn near y (the metafile's frame may move it a pixel or two) */
static int count_line(void *bitmap, int y, DWORD rgb)
{
    DWORD c;
    int x, dy, n = 0;
    for (x = 0; x < 100; x++)
        for (dy = -5; dy <= 5; dy++)
        {
            ((GpStatus (WINAPI *)(void *, INT, INT, DWORD *))p_GdipBitmapGetPixel)(bitmap, x, y + dy, &c);
            if ((c & 0xffffff) == rgb) { n++; break; }
        }
    return n;
}

static void *play_records(void)
{
    void *bitmap = NULL, *graphics = NULL;
    GpRect dest = { 0, 0, 100, 100 };
    RECT frame;
    HDC ref, edc;
    HENHMETAFILE hemf;
    GpStatus st;

    ref = GetDC(0);
    SetRect(&frame, 0, 0, MulDiv(100, 2540, GetDeviceCaps(ref, LOGPIXELSX)), MulDiv(100, 2540, GetDeviceCaps(ref, LOGPIXELSY)));
    edc = CreateEnhMetaFileW(ref, NULL, &frame, NULL);
    {
        BYTE *comment = malloc(len + 4);
        DWORD sig = 0x2b464d45;
        memcpy(comment, &sig, 4);
        memcpy(comment + 4, buf, len);
        GdiComment(edc, len + 4, comment);
        free(comment);
    }
    hemf = CloseEnhMetaFile(edc);
    ReleaseDC(0, ref);
    st = ((GpStatus (WINAPI *)(HENHMETAFILE, BOOL, void **))p_GdipCreateMetafileFromEmf)(hemf, TRUE, &metafile);
    if (st) return NULL;
    ((GpStatus (WINAPI *)(INT, INT, INT, int, BYTE *, void **))p_GdipCreateBitmapFromScan0)(100, 100, 0, 0x26200a, NULL, &bitmap);
    ((GpStatus (WINAPI *)(void *, void **))p_GdipGetImageGraphicsContext)(bitmap, &graphics);
    ((GpStatus (WINAPI *)(void *, DWORD))p_GdipGraphicsClear)(graphics, 0xffffffff);
    ((GpStatus (WINAPI *)(void *, void *, const GpRect *, EnumProc, void *, void *))p_GdipEnumerateMetafileDestRectI)(
            graphics, metafile, &dest, play, NULL, NULL);
    ((GpStatus (WINAPI *)(void *))p_GdipDeleteGraphics)(graphics);
    return bitmap;
}

int main(void)
{
    ULONG_PTR token; StartupInput in = {1};
    void *bitmap;
    int solid, dashed;

    g = LoadLibraryA("gdiplus.dll");
#define L(n) p_##n = GetProcAddress(g, #n)
    L(GdiplusStartup); L(GdipCreateMetafileFromEmf); L(GdipCreateBitmapFromScan0); L(GdipGetImageGraphicsContext);
    L(GdipGraphicsClear); L(GdipEnumerateMetafileDestRectI); L(GdipPlayMetafileRecord); L(GdipBitmapGetPixel);
    L(GdipDeleteGraphics); L(GdipDisposeImage); L(GdipRecordMetafile); L(GdipCreatePen1); L(GdipSetPenMode);
    L(GdipSetPenCompoundArray); L(GdipDrawLineI); L(GdipDeletePen);
    if (((GpStatus (WINAPI *)(ULONG_PTR *, StartupInput *, void *))p_GdiplusStartup)(&token, &in, NULL))
        { printf("no_gdiplus=1\n"); return 1; }

    /* pen 0: solid red; pen 1: blue dashed 3-on 3-off (in pen widths), with a transform (identity), a
     * round dash cap, a center alignment and compound lines 0..1 -- all the optional data a pen has */
    blen = 0; put32(0x4001 /*header*/); /* placeholder to keep put() usable */
    len = 0;
    blen = 0; put32(VERSION); put32(1); put32(96); put32(96);
    record(0x4001, 0, body, blen);
    pen_object(0, 0xffff0000, 2.0f, 0, NULL, 0);
    {
        BYTE opt[128];
        UINT o = 0;
        float m[6] = { 1, 0, 0, 1, 0, 0 }, dashes[2] = { 3, 3 }, compound[2] = { 0, 1 };
        DWORD v;
        memcpy(opt + o, m, 24); o += 24;                       /* PenDataTransform */
        v = 2; memcpy(opt + o, &v, 4); o += 4;                 /* PenDataDashedLineCap: round */
        v = 2; memcpy(opt + o, &v, 4); o += 4;                 /* PenDataDashedLine: count */
        memcpy(opt + o, dashes, 8); o += 8;
        v = 0; memcpy(opt + o, &v, 4); o += 4;                 /* PenDataNonCenter: center */
        v = 2; memcpy(opt + o, &v, 4); o += 4;                 /* PenDataCompoundLine */
        memcpy(opt + o, compound, 8); o += 8;
        pen_object(1, 0xff0000ff, 2.0f, 0x0001 | 0x0040 | 0x0100 | 0x0200 | 0x0400, opt, o);
    }
    {
        short p[] = { 0, 20, 99, 20 };
        blen = 0; put32(2); put(p, sizeof(p));
        record(0x400d, 0x4000 | 0, body, blen);               /* DrawLines, red, y=20 */
        short q[] = { 0, 60, 99, 60 };
        blen = 0; put32(2); put(q, sizeof(q));
        record(0x400d, 0x4000 | 1, body, blen);               /* DrawLines, blue dashed, y=60 */
    }
    record(0x4002, 0, NULL, 0);
    bitmap = play_records();
    if (!bitmap) { printf("no_metafile=1\n"); return 1; }
    solid = count_line(bitmap, 20, 0xff0000);
    dashed = count_line(bitmap, 60, 0x0000ff);
    printf("pen_object_played=%d status %d\n", object_status == 0, object_status);
    printf("solid_line=%d %d pixels\n", solid >= 95, solid);
    printf("dashed_line=%d %d pixels\n", dashed >= 30 && dashed <= 70, dashed);

    /* recording: a pen inset with compound lines */
    {
        void *rec = NULL, *rg = NULL, *pen = NULL, *bmp2 = NULL, *g2 = NULL;
        float frame[4] = { 0, 0, 100, 100 }, compound[4] = { 0, 0.3f, 0.7f, 1 };
        GpRect dest = { 0, 0, 100, 100 };
        HDC ref = GetDC(0);
        DWORD flags, align, count;

        ((GpStatus (WINAPI *)(HDC, int, const float *, int, const WCHAR *, void **))p_GdipRecordMetafile)(
                ref, 4 /* EmfTypeEmfPlusOnly */, frame, 2 /* MetafileFrameUnitPixel */, NULL, &rec);
        ReleaseDC(0, ref);
        ((GpStatus (WINAPI *)(void *, void **))p_GdipGetImageGraphicsContext)(rec, &rg);
        ((GpStatus (WINAPI *)(DWORD, float, int, void **))p_GdipCreatePen1)(0xff000000, 6.0f, 2, &pen);
        ((GpStatus (WINAPI *)(void *, int))p_GdipSetPenMode)(pen, 1 /* PenAlignmentInset */);
        ((GpStatus (WINAPI *)(void *, const float *, int))p_GdipSetPenCompoundArray)(pen, compound, 4);
        ((GpStatus (WINAPI *)(void *, void *, int, int, int, int))p_GdipDrawLineI)(rg, pen, 10, 10, 90, 90);
        ((GpStatus (WINAPI *)(void *))p_GdipDeletePen)(pen);
        ((GpStatus (WINAPI *)(void *))p_GdipDeleteGraphics)(rg);
        ((GpStatus (WINAPI *)(INT, INT, INT, int, BYTE *, void **))p_GdipCreateBitmapFromScan0)(100, 100, 0, 0x26200a, NULL, &bmp2);
        ((GpStatus (WINAPI *)(void *, void **))p_GdipGetImageGraphicsContext)(bmp2, &g2);
        ((GpStatus (WINAPI *)(void *, void *, const GpRect *, EnumProc, void *, void *))p_GdipEnumerateMetafileDestRectI)(
                g2, rec, &dest, grab, NULL, NULL);
        /* Version, Type, PenDataFlags, PenUnit, PenWidth, then DashedLineOffset (always), NonCenter, CompoundLine */
        memcpy(&flags, recorded_pen + 8, 4);
        memcpy(&align, recorded_pen + 24, 4);
        memcpy(&count, recorded_pen + 28, 4);
        printf("recorded_pen=%d %u bytes flags %#lx\n", recorded_pen_size > 28, recorded_pen_size, flags);
        printf("recorded_align_int=%d %#lx\n", (flags & 0x200) && align == 1, align);
        printf("recorded_compound=%d count %lu\n", (flags & 0x400) && count == 4, count);
    }
    printf("done=1\n");
    return 0;
}
