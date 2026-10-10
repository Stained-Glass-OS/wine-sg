/* gdiplus (patches/sg/2653): a 2x1 image scaled by a world transform onto an 8x1 bitmap, nearest neighbour; with the
 * half pixel offset a destination pixel belongs to the image when its centre is inside (values recorded from Windows). */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int Status;
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

#define FN(ret, name, args) static ret (WINAPI *p##name) args
FN(Status, GdiplusStartup, (ULONG_PTR *, const StartupInput *, void *));
FN(Status, GdipCreateBitmapFromScan0, (int, int, int, int, BYTE *, void **));
FN(Status, GdipGetImageGraphicsContext, (void *, void **));
FN(Status, GdipSetInterpolationMode, (void *, int));
FN(Status, GdipSetPixelOffsetMode, (void *, int));
FN(Status, GdipCreateMatrix2, (float, float, float, float, float, float, void **));
FN(Status, GdipSetWorldTransform, (void *, void *));
FN(Status, GdipDrawImageI, (void *, void *, int, int));
FN(Status, GdipBitmapSetResolution, (void *, float, float));

#define PF24 0x21808
static const struct { float scale; int mode; const char *exp; } td[] =
{
    { 1.2f, 4, "40 80 cc cc 40 40 40 40" },
    { 1.5f, 4, "40 80 80 cc 40 40 40 40" },
    { 2.0f, 4, "40 40 80 80 cc cc 40 40" },
    { 2.5f, 4, "40 40 80 80 80 cc cc 40" },
    { 1.5f, 2, "40 80 80 cc 40 40 40 40" },
    { 2.5f, 2, "40 40 80 80 80 cc cc 40" },
    { 1.5f, 3, "40 40 80 cc 40 40 40 40" },
    { 1.0f, 4, "40 80 cc 40 40 40 40 40" },
};

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;
    BYTE src_bits[6] = { 0x80,0x80,0x80, 0xcc,0xcc,0xcc }, dst[24];
    void *src = NULL, *dstbmp = NULL, *g = NULL, *m = NULL;
    unsigned i, k;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipCreateBitmapFromScan0); LOAD(GdipGetImageGraphicsContext);
    LOAD(GdipSetInterpolationMode); LOAD(GdipSetPixelOffsetMode); LOAD(GdipCreateMatrix2);
    LOAD(GdipSetWorldTransform); LOAD(GdipDrawImageI); LOAD(GdipBitmapSetResolution);
    if (!gp || pGdiplusStartup(&token, &in, NULL) != 0) { printf("FAIL  start\nRESULT: FAIL\n"); return 1; }

    pGdipCreateBitmapFromScan0(2, 1, 4, PF24, src_bits, &src);
    pGdipBitmapSetResolution(src, 100.0f, 100.0f);
    pGdipCreateBitmapFromScan0(8, 1, 24, PF24, dst, &dstbmp);
    pGdipBitmapSetResolution(dstbmp, 100.0f, 100.0f);
    pGdipGetImageGraphicsContext(dstbmp, &g);
    pGdipSetInterpolationMode(g, 5);

    for (i = 0; i < sizeof(td) / sizeof(td[0]); i++)
    {
        char got[64] = "";
        pGdipSetPixelOffsetMode(g, td[i].mode);
        pGdipCreateMatrix2(td[i].scale, 0, 0, 1, 0, 0, &m);
        pGdipSetWorldTransform(g, m);
        memset(dst, 0x40, sizeof(dst));
        pGdipDrawImageI(g, src, 1, 0);
        for (k = 0; k < 8; k++) sprintf(got + strlen(got), "%s%02x", k ? " " : "", dst[k * 3]);
        check(!strcmp(got, td[i].exp), "scale %.1f offset mode %d: %s (expected %s)", td[i].scale, td[i].mode, got, td[i].exp);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
