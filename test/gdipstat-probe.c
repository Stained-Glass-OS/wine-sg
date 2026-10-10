/* gdiplus: status results the Wine tests record from Windows (patches/sg/2613):
 * files that cannot be opened, the size of a metafile that is still being
 * recorded, converting a metafile that already is EMF+, bitmaps that no graphics
 * can be made for, and container (state) values that do not repeat across
 * graphics objects. The flat API is reached through GetProcAddress. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int Status;
enum { Ok = 0, InvalidParameter = 2, OutOfMemory = 3, NotImplemented = 6 };
typedef struct { ULONG_PTR a; ULONG_PTR b; } Dummy;
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
typedef struct { float X, Y, Width, Height; } RectF;

#define PixelFormat32bppARGB 0x26200A
#define PixelFormat16bppGrayScale 0x101004
#define PixelFormat8bppIndexed 0x30803

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
FN(Status, GdipCreateBitmapFromFile, (const WCHAR *, void **));
FN(Status, GdipLoadImageFromFile, (const WCHAR *, void **));
FN(Status, GdipLoadImageFromFileICM, (const WCHAR *, void **));
FN(Status, GdipDisposeImage, (void *));
FN(Status, GdipCreateBitmapFromScan0, (int, int, int, int, BYTE *, void **));
FN(Status, GdipSaveImageToFile, (void *, const WCHAR *, const GUID *, const void *));
FN(Status, GdipRecordMetafile, (HDC, int, const RectF *, int, const WCHAR *, void **));
FN(Status, GdipGetImageHorizontalResolution, (void *, float *));
FN(Status, GdipGetImageVerticalResolution, (void *, float *));
FN(Status, GdipGetImageWidth, (void *, UINT *));
FN(Status, GdipGetImageHeight, (void *, UINT *));
FN(Status, GdipGetImageGraphicsContext, (void *, void **));
FN(Status, GdipDeleteGraphics, (void *));
FN(Status, GdipCreateFromHDC, (HDC, void **));
FN(Status, GdipBeginContainer2, (void *, UINT *));
FN(Status, GdipSaveGraphics, (void *, UINT *));
FN(Status, GdipEndContainer, (void *, UINT));
FN(Status, GdipRestoreGraphics, (void *, UINT));
FN(Status, GdipSetInterpolationMode, (void *, int));
FN(Status, GdipGetInterpolationMode, (void *, int *));
FN(Status, GdipConvertToEmfPlus, (void *, void *, BOOL *, int, const WCHAR *, void **));

static const GUID png_encoder = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};

static void test_files(void)
{
    WCHAR path[MAX_PATH];
    void *obj;
    Status st;

    obj = (void *)0xdeadbeef;
    st = pGdipCreateBitmapFromFile(L"nonexistent-file.png", &obj);
    check(st == InvalidParameter && !obj, "CreateBitmapFromFile of a missing file = InvalidParameter (%d, %p)", st, obj);
    obj = (void *)0xdeadbeef;
    st = pGdipLoadImageFromFile(L"nonexistent-file.png", &obj);
    check(st == OutOfMemory && !obj, "LoadImageFromFile of a missing file = OutOfMemory (%d, %p)", st, obj);
    obj = (void *)0xdeadbeef;
    st = pGdipLoadImageFromFileICM(L"nonexistent-file.png", &obj);
    check(st == OutOfMemory && !obj, "LoadImageFromFileICM of a missing file = OutOfMemory (%d, %p)", st, obj);
    obj = (void *)0xdeadbeef;
    st = pGdipCreateBitmapFromFile(NULL, &obj);
    check(st == InvalidParameter && obj == (void *)0xdeadbeef, "CreateBitmapFromFile(NULL) leaves the result alone (%d)", st);

    /* an existing file still loads */
    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"gdipstat.png");
    {
        void *bmp = NULL;

        st = pGdipCreateBitmapFromScan0(4, 4, 0, PixelFormat32bppARGB, NULL, &bmp);
        check(st == Ok && bmp, "a bitmap to save (%d)", st);
        st = pGdipSaveImageToFile(bmp, path, &png_encoder, NULL);
        check(st == Ok, "saved as PNG (%d)", st);
        pGdipDisposeImage(bmp);
    }
    obj = NULL;
    st = pGdipCreateBitmapFromFile(path, &obj);
    check(st == Ok && obj, "CreateBitmapFromFile of the saved file (%d)", st);
    if (obj) pGdipDisposeImage(obj);
    obj = NULL;
    st = pGdipLoadImageFromFile(path, &obj);
    check(st == Ok && obj, "LoadImageFromFile of the saved file (%d)", st);
    if (obj) pGdipDisposeImage(obj);
    obj = NULL;
    st = pGdipLoadImageFromFileICM(path, &obj);
    check(st == Ok && obj, "LoadImageFromFileICM of the saved file (%d)", st);
    if (obj) pGdipDisposeImage(obj);
    DeleteFileW(path);
}

static void test_recording(void)
{
    static const RectF frame = {0.0f, 0.0f, 5.0f, 5.0f};
    HDC hdc = CreateCompatibleDC(0);
    void *meta = NULL, *graphics = NULL;
    float dpix = -1, dpiy = -1;
    UINT width = 99, height = 99;
    Status st;

    st = pGdipRecordMetafile(hdc, 4 /* EmfTypeEmfPlusOnly */, &frame, 4 /* inch */, NULL, &meta);
    check(st == Ok && meta, "GdipRecordMetafile (%d)", st);
    DeleteDC(hdc);
    if (!meta) return;

    st = pGdipGetImageHorizontalResolution(meta, &dpix);
    check(st == InvalidParameter, "horizontal resolution while recording (%d)", st);
    st = pGdipGetImageVerticalResolution(meta, &dpiy);
    check(st == InvalidParameter, "vertical resolution while recording (%d)", st);
    st = pGdipGetImageWidth(meta, &width);
    check(st == InvalidParameter && width == 99, "width while recording (%d)", st);
    st = pGdipGetImageHeight(meta, &height);
    check(st == InvalidParameter && height == 99, "height while recording (%d)", st);

    st = pGdipGetImageGraphicsContext(meta, &graphics);
    check(st == Ok && graphics, "graphics context of the recording (%d)", st);
    st = pGdipGetImageHorizontalResolution(meta, &dpix);
    check(st == InvalidParameter, "horizontal resolution while drawing (%d)", st);
    st = pGdipGetImageWidth(meta, &width);
    check(st == InvalidParameter, "width while drawing (%d)", st);

    if (pGdipConvertToEmfPlus)
    {
        void *out = (void *)0x1234;

        st = pGdipConvertToEmfPlus(graphics, meta, NULL, 4 /* EmfTypeEmfPlusOnly */, NULL, &out);
        check(st == InvalidParameter, "an EMF+ only metafile is not converted again (%d)", st);
        st = pGdipConvertToEmfPlus(graphics, meta, NULL, 5 /* dual */, NULL, &out);
        check(st == InvalidParameter, "also not when asking for a dual one (%d)", st);
        st = pGdipConvertToEmfPlus(NULL, meta, NULL, 4, NULL, &out);
        check(st == InvalidParameter, "ConvertToEmfPlus without graphics (%d)", st);
    }

    pGdipDeleteGraphics(graphics);

    st = pGdipGetImageHorizontalResolution(meta, &dpix);
    check(st == Ok && dpix > 0.0f, "horizontal resolution once recorded (%d, %g)", st, dpix);
    st = pGdipGetImageVerticalResolution(meta, &dpiy);
    check(st == Ok && dpiy > 0.0f, "vertical resolution once recorded (%d, %g)", st, dpiy);
    st = pGdipGetImageWidth(meta, &width);
    check(st == Ok && width > 0 && width != 99, "width once recorded (%d, %u)", st, width);
    st = pGdipGetImageHeight(meta, &height);
    check(st == Ok && height > 0 && height != 99, "height once recorded (%d, %u)", st, height);

    pGdipDisposeImage(meta);
}

static void test_dual(void)
{
    static const RectF frame = {0.0f, 0.0f, 5.0f, 5.0f};
    HDC hdc = CreateCompatibleDC(0);
    void *meta = NULL, *graphics = NULL, *out = NULL;
    Status st;

    st = pGdipRecordMetafile(hdc, 5 /* EmfTypeEmfPlusDual */, &frame, 4, NULL, &meta);
    DeleteDC(hdc);
    if (!meta || !pGdipConvertToEmfPlus) return;
    pGdipGetImageGraphicsContext(meta, &graphics);
    st = pGdipConvertToEmfPlus(graphics, meta, NULL, 4, NULL, &out);
    check(st == InvalidParameter, "a dual metafile is not converted either (%d)", st);
    pGdipDeleteGraphics(graphics);
    pGdipDisposeImage(meta);
}

static void test_bitmap_graphics(void)
{
    void *bmp = NULL, *graphics = NULL;
    Status st;

    st = pGdipCreateBitmapFromScan0(10, 10, 0, PixelFormat16bppGrayScale, NULL, &bmp);
    check(st == Ok && bmp, "a 16 bit gray bitmap (%d)", st);
    graphics = (void *)0x1234;
    st = pGdipGetImageGraphicsContext(bmp, &graphics);
    check(st == OutOfMemory, "no graphics for 16 bit gray (%d)", st);
    if (st == Ok) pGdipDeleteGraphics(graphics);
    pGdipDisposeImage(bmp);

    bmp = graphics = NULL;
    pGdipCreateBitmapFromScan0(10, 10, 0, PixelFormat32bppARGB, NULL, &bmp);
    st = pGdipGetImageGraphicsContext(bmp, &graphics);
    check(st == Ok && graphics, "graphics for a 32 bit bitmap (%d)", st);
    if (graphics) pGdipDeleteGraphics(graphics);
    pGdipDisposeImage(bmp);

    bmp = graphics = NULL;
    pGdipCreateBitmapFromScan0(10, 10, 0, PixelFormat8bppIndexed, NULL, &bmp);
    st = pGdipGetImageGraphicsContext(bmp, &graphics);
    check(st == Ok && graphics, "graphics for an indexed bitmap (%d)", st);
    if (graphics) pGdipDeleteGraphics(graphics);
    pGdipDisposeImage(bmp);
}

static int in_list(const UINT *list, int n, UINT v, int skip)
{
    int i;

    for (i = 0; i < n; i++)
        if (i != skip && list[i] == v) return 1;
    return 0;
}

static void test_states(void)
{
    HDC hdc = CreateCompatibleDC(0);
    UINT states[8];
    int n = 0, i, mode, dup = 0;
    void *g1, *g2, *g3;
    Status st;

    pGdipCreateFromHDC(hdc, &g1);
    pGdipSetInterpolationMode(g1, 3);
    st = pGdipBeginContainer2(g1, &states[n++]);
    check(st == Ok, "BeginContainer2 (%d)", st);
    pGdipSetInterpolationMode(g1, 6);
    st = pGdipSaveGraphics(g1, &states[n++]);
    check(st == Ok, "SaveGraphics (%d)", st);
    pGdipSetInterpolationMode(g1, 5);
    st = pGdipEndContainer(g1, states[0]);
    pGdipGetInterpolationMode(g1, &mode);
    check(st == Ok && mode == 3, "EndContainer restores the first state (%d, %d)", st, mode);
    st = pGdipRestoreGraphics(g1, states[1]);
    pGdipGetInterpolationMode(g1, &mode);
    check(mode == 3, "the second state is gone with the first (%d)", mode);

    pGdipCreateFromHDC(hdc, &g2);
    pGdipSetInterpolationMode(g2, 4);
    pGdipBeginContainer2(g2, &states[n++]);
    pGdipSetInterpolationMode(g2, 6);
    pGdipSaveGraphics(g2, &states[n++]);
    pGdipSetInterpolationMode(g2, 5);
    st = pGdipRestoreGraphics(g2, states[3]);
    pGdipGetInterpolationMode(g2, &mode);
    check(st == Ok && mode == 6, "RestoreGraphics on the second graphics (%d, %d)", st, mode);
    st = pGdipEndContainer(g2, states[2]);
    pGdipGetInterpolationMode(g2, &mode);
    check(st == Ok && mode == 4, "EndContainer on the second graphics (%d, %d)", st, mode);

    /* a graphics made for a bitmap numbers its states apart from the others too */
    {
        void *bmp = NULL;

        pGdipCreateBitmapFromScan0(8, 8, 0, PixelFormat32bppARGB, NULL, &bmp);
        pGdipGetImageGraphicsContext(bmp, &g3);
        pGdipBeginContainer2(g3, &states[n++]);
        pGdipSaveGraphics(g3, &states[n++]);
        pGdipDeleteGraphics(g3);
        pGdipDisposeImage(bmp);
    }

    for (i = 0; i < n; i++)
        if (in_list(states, n, states[i], i)) dup++;
    check(dup == 0, "%d state values from three graphics objects, %d repeated", n, dup);

    pGdipDeleteGraphics(g1);
    pGdipDeleteGraphics(g2);
    DeleteDC(hdc);
}

#define LOAD(name) do { p##name = (void *)GetProcAddress(mod, #name); if (!p##name) { printf("FAIL  no %s\nRESULT: FAIL\n", #name); return 1; } } while (0)

int main(void)
{
    HMODULE mod = LoadLibraryA("gdiplus.dll");
    StartupInput input = {1};
    ULONG_PTR token;

    if (!mod) { printf("FAIL  no gdiplus\nRESULT: FAIL\n"); return 1; }
    LOAD(GdiplusStartup);
    LOAD(GdipCreateBitmapFromFile); LOAD(GdipLoadImageFromFile); LOAD(GdipLoadImageFromFileICM); LOAD(GdipDisposeImage);
    LOAD(GdipCreateBitmapFromScan0); LOAD(GdipSaveImageToFile); LOAD(GdipRecordMetafile);
    LOAD(GdipGetImageHorizontalResolution); LOAD(GdipGetImageVerticalResolution); LOAD(GdipGetImageWidth);
    LOAD(GdipGetImageHeight); LOAD(GdipGetImageGraphicsContext); LOAD(GdipDeleteGraphics); LOAD(GdipCreateFromHDC);
    LOAD(GdipBeginContainer2); LOAD(GdipSaveGraphics); LOAD(GdipEndContainer); LOAD(GdipRestoreGraphics);
    LOAD(GdipSetInterpolationMode); LOAD(GdipGetInterpolationMode);
    pGdipConvertToEmfPlus = (void *)GetProcAddress(mod, "GdipConvertToEmfPlus");

    if (pGdiplusStartup(&token, &input, NULL)) { printf("FAIL  GdiplusStartup\nRESULT: FAIL\n"); return 1; }

    test_files();
    test_recording();
    test_dual();
    test_bitmap_graphics();
    test_states();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
