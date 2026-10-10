/* gdiplus (patches/sg/2652): the data of a region made from a GDI region (rectangle or ellipse) has the path flags
 * 0x6000, the data of a region made of a path of integers 0x4000 (what the Wine tests record from Windows). */
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
FN(Status, GdipCreateRegionHrgn, (HRGN, void **));
FN(Status, GdipGetRegionDataSize, (void *, UINT *));
FN(Status, GdipGetRegionData, (void *, BYTE *, UINT, UINT *));
FN(Status, GdipCreatePath, (int, void **));
FN(Status, GdipAddPathRectangleI, (void *, int, int, int, int));
FN(Status, GdipCreateRegionPath, (void *, void **));
FN(Status, GdipCreateRegionRgnData, (const BYTE *, INT, void **));
FN(Status, GdipCloneRegion, (void *, void **));

static DWORD path_flags(void *region, UINT *size_out)
{
    DWORD buf[512] = {0};
    UINT needed = 0;

    pGdipGetRegionDataSize(region, &needed);
    if (size_out) *size_out = needed;
    pGdipGetRegionData(region, (BYTE *)buf, sizeof(buf), &needed);
    return buf[8];
}

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR token;
    void *region = NULL, *clone = NULL, *path = NULL, *again = NULL;
    HRGN rgn;
    UINT size;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdipCreateRegionHrgn); LOAD(GdipGetRegionDataSize); LOAD(GdipGetRegionData);
    LOAD(GdipCreatePath); LOAD(GdipAddPathRectangleI); LOAD(GdipCreateRegionPath); LOAD(GdipCreateRegionRgnData);
    LOAD(GdipCloneRegion);
    if (!gp || pGdiplusStartup(&token, &in, NULL) != 0) { printf("FAIL  start\nRESULT: FAIL\n"); return 1; }

    rgn = CreateRectRgn(0, 0, 100, 10);
    pGdipCreateRegionHrgn(rgn, &region);
    check(path_flags(region, &size) == 0x6000, "rectangle GDI region: flags %#lx", path_flags(region, &size));
    pGdipCloneRegion(region, &clone);
    check(path_flags(clone, &size) == 0x6000, "its clone: flags %#lx", path_flags(clone, &size));
    {
        DWORD buf[512] = {0};
        UINT needed = 0;

        pGdipGetRegionData(region, (BYTE *)buf, sizeof(buf), &needed);
        pGdipCreateRegionRgnData((BYTE *)buf, needed, &again);
        check(again && path_flags(again, &size) == 0x6000, "made again from its data: flags %#lx", again ? path_flags(again, &size) : 0);
    }
    DeleteObject(rgn);

    rgn = CreateEllipticRgn(0, 0, 100, 10);
    pGdipCreateRegionHrgn(rgn, &region);
    check(path_flags(region, &size) == 0x6000, "ellipse GDI region: flags %#lx", path_flags(region, &size));
    DeleteObject(rgn);

    pGdipCreatePath(0, &path);
    pGdipAddPathRectangleI(path, 0, 0, 20, 30);
    pGdipCreateRegionPath(path, &region);
    check(path_flags(region, &size) == 0x4000, "integer path region: flags %#lx", path_flags(region, &size));

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
