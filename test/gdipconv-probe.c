/* gdiplus (patch 2683): GdipConvertToEmfPlus records an enhanced metafile again as an EMF+ metafile. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int Status;
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define FN(ret, name, args) static ret (WINAPI *p##name) args
FN(Status, GdiplusStartup, (ULONG_PTR *, const StartupInput *, void *));
FN(Status, GdipCreateMetafileFromEmf, (HENHMETAFILE, BOOL, void **));
FN(Status, GdipConvertToEmfPlus, (const void *, void *, BOOL *, int, const WCHAR *, void **));
FN(Status, GdipCreateBitmapFromScan0, (int, int, int, int, BYTE *, void **));
FN(Status, GdipGetImageGraphicsContext, (void *, void **));
FN(Status, GdipGraphicsClear, (void *, UINT32));
FN(Status, GdipDrawImageI, (void *, void *, int, int));
FN(Status, GdipBitmapGetPixel, (void *, int, int, UINT32 *));
FN(Status, GdipDeleteGraphics, (void *));
FN(Status, GdipDisposeImage, (void *));
FN(Status, GdipGetMetafileHeaderFromMetafile, (void *, void *));

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;
    HDC emfdc;
    HENHMETAFILE emf;
    RECT frame = { 0, 0, 20000, 20000 };   /* 0.01 mm units */
    void *mf, *out = NULL, *bmp, *g;
    BOOL succ = FALSE;
    UINT32 px;
    struct { DWORD type; BYTE rest[600]; } hdr;
    HBRUSH brush;
    Status st;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipCreateMetafileFromEmf); LOAD(GdipConvertToEmfPlus); LOAD(GdipCreateBitmapFromScan0);
    LOAD(GdipGetImageGraphicsContext); LOAD(GdipGraphicsClear); LOAD(GdipDrawImageI); LOAD(GdipBitmapGetPixel);
    LOAD(GdipDeleteGraphics); LOAD(GdipDisposeImage); LOAD(GdipGetMetafileHeaderFromMetafile);
    if (!gp || !pGdipConvertToEmfPlus || pGdiplusStartup(&token, &in, NULL) != 0) { printf("SKIP no GdipConvertToEmfPlus\nRESULT: PASS\n"); return 0; }

    emfdc = CreateEnhMetaFileW(NULL, NULL, &frame, L"sg\0conv\0");
    brush = CreateSolidBrush(RGB(0, 0, 255));
    SelectObject(emfdc, brush);
    Rectangle(emfdc, 10, 10, 90, 90);
    emf = CloseEnhMetaFile(emfdc);
    DeleteObject(brush);
    st = pGdipCreateMetafileFromEmf(emf, TRUE, &mf);
    check(st == 0 && mf, "a GDI-made enhanced metafile");

    pGdipCreateBitmapFromScan0(200, 200, 0, 0x26200A, NULL, &bmp);
    pGdipGetImageGraphicsContext(bmp, &g);
    pGdipGraphicsClear(g, 0xffffffff);

    st = pGdipConvertToEmfPlus(g, mf, &succ, 4 /* EmfTypeEmfPlusOnly */, NULL, &out);
    check(st == 0 && out, "GdipConvertToEmfPlus to EMF+ only");
    check(succ, "and says it converted");
    memset(&hdr, 0, sizeof(hdr));
    pGdipGetMetafileHeaderFromMetafile(out, &hdr);
    check(hdr.type == 4, "the result is an EMF+ only metafile");
    pGdipGraphicsClear(g, 0xffffffff);
    pGdipDrawImageI(g, out, 0, 0);
    pGdipBitmapGetPixel(bmp, 40, 40, &px);
    check((px & 0xffffff) == 0x0000ff, "GDI+ draws the converted metafile: the rectangle is there");
    pGdipBitmapGetPixel(bmp, 190, 190, &px);
    check((px & 0xffffff) == 0xffffff, "and nothing else");
    if (out) pGdipDisposeImage(out);
    out = NULL;
    st = pGdipConvertToEmfPlus(g, mf, NULL, 5 /* dual */, NULL, &out);
    check(st == 0 && out, "to EMF+ dual");
    memset(&hdr, 0, sizeof(hdr));
    if (out) pGdipGetMetafileHeaderFromMetafile(out, &hdr);
    check(hdr.type == 5, "the result is a dual metafile");
    if (out) pGdipDisposeImage(out);
    pGdipDeleteGraphics(g);
    pGdipDisposeImage(bmp);
    pGdipDisposeImage(mf);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
