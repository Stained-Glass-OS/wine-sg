/* gdiplus (patches/sg/2664): GdipCreatePen1 returns GdiplusNotInitialized (18) without a GdiplusStartup in force. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

typedef int Status;
enum { Ok = 0 };
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
typedef struct { float X, Y, Width, Height; } RectF;
typedef struct { INT First, Length; } CharRange;

#define PixelFormat16bppRGB555 0x21005
#define PixelFormat16bppRGB565 0x21006
#define PixelFormat16bppARGB1555 0x61007
#define PixelFormat24bppRGB 0x21808
#define PixelFormat32bppARGB 0x26200A

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
FN(ULONG, GdiplusShutdown, (ULONG_PTR));
FN(Status, GdipCreatePen1, (DWORD, float, int, void **));
FN(Status, GdipDeletePen, (void *));

int main(void)
{
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    StartupInput in = { 1 };
    ULONG_PTR t1, t2;
    void *pen = NULL;
    Status st;

#define LOAD(n) p##n = (void *)GetProcAddress(gp, #n)
    LOAD(GdiplusStartup); LOAD(GdiplusShutdown); LOAD(GdipCreatePen1); LOAD(GdipDeletePen);
    if (!gp) { printf("FAIL  load\nRESULT: FAIL\n"); return 1; }

    st = pGdipCreatePen1(0xffff00ff, 10.0f, 2, &pen);
    check(st == 18, "no pen before GdiplusStartup (%d)", st);
    pGdiplusStartup(&t1, &in, NULL);
    st = pGdipCreatePen1(0xffff00ff, 10.0f, 2, &pen);
    check(st == Ok && pen, "a pen after it (%d)", st);
    if (pen) pGdipDeletePen(pen);
    pGdiplusStartup(&t2, &in, NULL);
    pGdiplusShutdown(t2);
    pen = NULL;
    st = pGdipCreatePen1(0xffff00ff, 10.0f, 2, &pen);
    check(st == Ok && pen, "still a pen with one of two startups undone (%d)", st);
    if (pen) pGdipDeletePen(pen);
    pGdiplusShutdown(t1);
    pen = NULL;
    st = pGdipCreatePen1(0xffff00ff, 10.0f, 2, &pen);
    check(st == 18 && !pen, "no pen after the last GdiplusShutdown (%d)", st);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
