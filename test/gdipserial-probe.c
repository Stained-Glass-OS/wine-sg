/* gdipserial-probe.c: test/gdipserial-gate.sh (wine-sg 9505). An EMF+ EmfPlusSerializableObject record (an
 * image effect: its GUID and parameters, [MS-EMFPLUS] 2.3.5.2) gives its effect to the images that
 * DrawImagePoints records with the E flag draw ("Not implemented for EMF+ record type 56"; drawn unchanged).
 * A metafile with a red image is recorded, then played onto a bitmap with a colour matrix object (red and
 * blue swapped) played before its DrawImagePoints record, with E set: the image comes out blue; played as
 * recorded, red (an image recorded without image attributes is drawn: its record was refused); without
 * the E flag, or with E and no object in that playback, red.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;
typedef struct { INT X, Y, Width, Height; } GpRect;
typedef struct { float X, Y, Width, Height; } GpRectF;
typedef BOOL (CALLBACK *EnumProc)(int type, UINT flags, UINT size, const BYTE *data, void *ctx);

static GpStatus (WINAPI *pGdiplusStartup)(ULONG_PTR *, const StartupInput *, void *);
static GpStatus (WINAPI *pGdipRecordMetafile)(HDC, int, const GpRectF *, int, const WCHAR *, void **);
static GpStatus (WINAPI *pGdipGetImageGraphicsContext)(void *, void **);
static GpStatus (WINAPI *pGdipCreateBitmapFromScan0)(INT, INT, INT, INT, BYTE *, void **);
static GpStatus (WINAPI *pGdipGraphicsClear)(void *, DWORD);
static GpStatus (WINAPI *pGdipDrawImageRectI)(void *, void *, INT, INT, INT, INT);
static GpStatus (WINAPI *pGdipDeleteGraphics)(void *);
static GpStatus (WINAPI *pGdipDisposeImage)(void *);
static GpStatus (WINAPI *pGdipEnumerateMetafileDestRectI)(void *, void *, const GpRect *, EnumProc, void *, void *);
static GpStatus (WINAPI *pGdipPlayMetafileRecord)(void *, int, UINT, UINT, const BYTE *);
static GpStatus (WINAPI *pGdipBitmapGetPixel)(void *, INT, INT, DWORD *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void *metafile;
static int inject, set_e, draws, serial_status = -1, draw_status = -1, draw_type;

/* ColorMatrixEffectGuid {718F2615-7933-40e3-A511-5F68FE14DD74} and a matrix that swaps red and blue */
static const GUID color_matrix_guid = { 0x718f2615, 0x7933, 0x40e3, { 0xa5, 0x11, 0x5f, 0x68, 0xfe, 0x14, 0xdd, 0x74 } };

static BOOL CALLBACK play(int type, UINT flags, UINT size, const BYTE *data, void *ctx)
{
    if ((type == 0x401a || type == 0x401b) && inject)
    {
        BYTE rec[sizeof(GUID) + 4 + 100];
        float m[25] = { 0 };
        UINT len = sizeof(m);
        m[0 * 5 + 2] = 1; m[1 * 5 + 1] = 1; m[2 * 5 + 0] = 1; m[3 * 5 + 3] = 1; m[4 * 5 + 4] = 1;
        memcpy(rec, &color_matrix_guid, sizeof(GUID));
        memcpy(rec + sizeof(GUID), &len, 4);
        memcpy(rec + sizeof(GUID) + 4, m, sizeof(m));
        serial_status = pGdipPlayMetafileRecord(metafile, 0x4038, 0, sizeof(rec), rec);
    }
    if (type == 0x401a || type == 0x401b)
    {
        draws++;
        draw_type = type;
        /* E (0x2000): the image is drawn through the effect of the latest SerializableObject */
        draw_status = pGdipPlayMetafileRecord(metafile, type, set_e ? flags | 0x2000 : flags, size, data);
    }
    else pGdipPlayMetafileRecord(metafile, type, flags, size, data);
    return TRUE;
}

static DWORD play_at(int x, int y, int with_effect, int e_flag)
{
    void *canvas = NULL, *g = NULL;
    GpRect dest = { 0, 0, 100, 100 };
    DWORD c = 0;

    inject = with_effect;
    set_e = e_flag;
    pGdipCreateBitmapFromScan0(100, 100, 0, 0x26200a, NULL, &canvas);
    pGdipGetImageGraphicsContext(canvas, &g);
    pGdipGraphicsClear(g, 0xffffffff);
    pGdipEnumerateMetafileDestRectI(g, metafile, &dest, play, NULL, NULL);
    pGdipDeleteGraphics(g);
    pGdipBitmapGetPixel(canvas, x, y, &c);
    pGdipDisposeImage(canvas);
    return c;
}

#define LOAD(name) (p##name = (void *)GetProcAddress(gdip, #name))

int main(void)
{
    HMODULE gdip = LoadLibraryA("gdiplus.dll");
    StartupInput si = { 1 };
    ULONG_PTR token;
    GpRectF frame = { 0, 0, 100, 100 };
    void *g = NULL, *red = NULL, *rg = NULL;
    HDC hdc = CreateCompatibleDC(NULL);
    char buf[200];
    DWORD c;

    LOAD(GdiplusStartup); LOAD(GdipRecordMetafile); LOAD(GdipGetImageGraphicsContext); LOAD(GdipCreateBitmapFromScan0);
    LOAD(GdipGraphicsClear); LOAD(GdipDrawImageRectI); LOAD(GdipDeleteGraphics); LOAD(GdipDisposeImage);
    LOAD(GdipEnumerateMetafileDestRectI); LOAD(GdipPlayMetafileRecord); LOAD(GdipBitmapGetPixel);
    pGdiplusStartup(&token, &si, NULL);

    /* the metafile: a 20 x 20 red image drawn at 10,10 40 x 40 */
    pGdipCreateBitmapFromScan0(20, 20, 0, 0x26200a, NULL, &red);
    pGdipGetImageGraphicsContext(red, &rg);
    pGdipGraphicsClear(rg, 0xffff0000);
    pGdipDeleteGraphics(rg);
    check(!pGdipRecordMetafile(hdc, 4 /* EmfTypeEmfPlusOnly */, &frame, 2 /* MetafileFrameUnitPixel */, NULL, &metafile),
          "a metafile is recorded");
    pGdipGetImageGraphicsContext(metafile, &g);
    pGdipDrawImageRectI(g, red, 10, 10, 40, 40);
    pGdipDeleteGraphics(g);

    c = play_at(30, 30, 0, 0);
    sprintf(buf, "played as recorded, the image is red (%08lx, %d image records, %#x status %d)", c, draws,
            draw_type, draw_status);
    check((c & 0xffffff) == 0xff0000 && draws == 1, buf);
    c = play_at(30, 30, 1, 1);
    sprintf(buf, "a colour matrix SerializableObject before a DrawImagePoints with E makes it blue (%08lx, status %d)",
            c, serial_status);
    check((c & 0xffffff) == 0x0000ff && serial_status == 0, buf);
    c = play_at(70, 70, 1, 1);
    sprintf(buf, "outside the image the page stays white (%08lx)", c);
    check((c & 0xffffff) == 0xffffff, buf);
    c = play_at(30, 30, 1, 0);
    sprintf(buf, "without the E flag the object does not touch the image: red (%08lx)", c);
    check((c & 0xffffff) == 0xff0000, buf);
    c = play_at(30, 30, 0, 1);
    sprintf(buf, "E with no object in this playback: drawn as it is, red (%08lx, status %d)", c, draw_status);
    check((c & 0xffffff) == 0xff0000 && draw_status == 0, buf);

    pGdipDisposeImage(metafile);
    pGdipDisposeImage(red);
    DeleteDC(hdc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
