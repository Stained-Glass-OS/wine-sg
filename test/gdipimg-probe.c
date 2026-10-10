/* gdiplus: results the Wine tests record from Windows (patches/sg/2624): pixel formats
 * that cannot be converted (16 bit gray, CMYK), the flags of fixed palettes, and size
 * and version of the header of a metafile made from a Windows metafile. The flat API is
 * reached through GetProcAddress. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int Status;
enum { Ok = 0, GenericError = 1, InvalidParameter = 2, OutOfMemory = 3, Win32Error = 7 };
typedef struct { UINT32 GdiplusVersion; void *cb; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } StartupInput;
typedef struct { UINT Width, Height; INT Stride; INT PixelFormat; void *Scan0; UINT_PTR Reserved; } BitmapData;
typedef struct { UINT Flags; UINT Count; UINT Entries[256]; } Palette;
#include <pshpack2.h>
typedef struct { DWORD Key; INT16 Hmf; INT16 Left, Top, Right, Bottom; WORD Inch; DWORD Reserved; WORD Checksum; } Placeable;
#include <poppack.h>

#define PixelFormat32bppARGB 0x26200A
#define PixelFormat32bppPARGB 0xE200B
#define PixelFormat16bppGrayScale 0x101004
#define PixelFormat32bppCMYK 0x200F
#define PixelFormat24bppRGB 0x21808

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
FN(Status, GdipDisposeImage, (void *));
FN(Status, GdipCreateHBITMAPFromBitmap, (void *, HBITMAP *, UINT));
FN(Status, GdipGetImageThumbnail, (void *, UINT, UINT, void **, void *, void *));
FN(Status, GdipBitmapLockBits, (void *, const void *, UINT, int, BitmapData *));
FN(Status, GdipBitmapUnlockBits, (void *, BitmapData *));
FN(Status, GdipSaveImageToStream, (void *, IStream *, const GUID *, const void *));
FN(Status, GdipInitializePalette, (Palette *, int, int, BOOL, void *));
FN(Status, GdipCreateMetafileFromWmf, (HMETAFILE, BOOL, const Placeable *, void **));
FN(Status, GdipGetMetafileHeaderFromMetafile, (void *, void *));
FN(Status, GdipCloneImage, (void *, void **));

static const GUID png_encoder = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
static const GUID tiff_encoder = {0x557cf405, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};

static void test_formats(void)
{
    static const struct { int format; const char *name; } tests[] =
    {
        {PixelFormat16bppGrayScale, "16bppGrayScale"},
        {PixelFormat32bppCMYK, "32bppCMYK"},
    };
    unsigned int i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    {
        void *bmp = NULL, *thumb = NULL;
        BitmapData data;
        HBITMAP hbm = NULL;
        Status st;

        st = pGdipCreateBitmapFromScan0(1, 1, 0, tests[i].format, NULL, &bmp);
        check(st == Ok && bmp, "%s: bitmap (%d)", tests[i].name, st);
        if (!bmp) continue;

        st = pGdipCreateHBITMAPFromBitmap(bmp, &hbm, 0);
        check(st == InvalidParameter, "%s: no HBITMAP (%d)", tests[i].name, st);
        if (hbm) DeleteObject(hbm);

        st = pGdipGetImageThumbnail(bmp, 0, 0, &thumb, NULL, NULL);
        check(st == OutOfMemory, "%s: no thumbnail (%d)", tests[i].name, st);
        if (st == Ok && thumb) pGdipDisposeImage(thumb);

        st = pGdipBitmapLockBits(bmp, NULL, 1 /* read */, PixelFormat32bppPARGB, &data);
        check(st == InvalidParameter, "%s: no conversion to 32bppPARGB (%d)", tests[i].name, st);
        if (st == Ok) pGdipBitmapUnlockBits(bmp, &data);

        st = pGdipBitmapLockBits(bmp, NULL, 1, tests[i].format, &data);
        check(st == Ok, "%s: locking in its own format (%d)", tests[i].name, st);
        if (st == Ok) pGdipBitmapUnlockBits(bmp, &data);
        pGdipDisposeImage(bmp);
    }

    /* the formats that do convert still do */
    {
        void *bmp = NULL, *thumb = NULL;
        BitmapData data;
        HBITMAP hbm = NULL;
        Status st;

        pGdipCreateBitmapFromScan0(1, 1, 0, PixelFormat24bppRGB, NULL, &bmp);
        st = pGdipCreateHBITMAPFromBitmap(bmp, &hbm, 0);
        check(st == Ok && hbm, "24bppRGB: HBITMAP (%d)", st);
        if (hbm) DeleteObject(hbm);
        st = pGdipGetImageThumbnail(bmp, 0, 0, &thumb, NULL, NULL);
        check(st == Ok, "24bppRGB: thumbnail (%d)", st);
        if (thumb) pGdipDisposeImage(thumb);
        st = pGdipBitmapLockBits(bmp, NULL, 1, PixelFormat32bppPARGB, &data);
        check(st == Ok, "24bppRGB: conversion to 32bppPARGB (%d)", st);
        if (st == Ok) pGdipBitmapUnlockBits(bmp, &data);
        pGdipDisposeImage(bmp);
    }
}

static void test_save(void)
{
    static const GUID *encoders[2] = { &png_encoder, &tiff_encoder };
    static const char *names[2] = { "PNG", "TIFF" };
    int i;

    for (i = 0; i < 2; ++i)
    {
        void *bmp = NULL;
        IStream *stream;
        Status st;

        pGdipCreateBitmapFromScan0(8, 8, 0, PixelFormat16bppGrayScale, NULL, &bmp);
        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        st = pGdipSaveImageToStream(bmp, stream, encoders[i], NULL);
        check(st == GenericError || st == Win32Error, "%s: 16bppGrayScale cannot be saved (%d)", names[i], st);
        IStream_Release(stream);
        pGdipDisposeImage(bmp);
    }
    {
        void *bmp = NULL;
        IStream *stream;
        Status st;

        pGdipCreateBitmapFromScan0(8, 8, 0, PixelFormat24bppRGB, NULL, &bmp);
        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        st = pGdipSaveImageToStream(bmp, stream, &png_encoder, NULL);
        check(st == Ok, "24bppRGB saves as PNG (%d)", st);
        IStream_Release(stream);
        pGdipDisposeImage(bmp);
    }
}

static void test_palettes(void)
{
    static const struct { int type; UINT flags; const char *name; } tests[] =
    {
        {2, 0x200, "FixedBW"},
        {3, 0x300, "FixedHalftone8"},
        {4, 0x400, "FixedHalftone27"},
        {5, 0x500, "FixedHalftone64"},
        {6, 0x600, "FixedHalftone125"},
        {7, 0x700, "FixedHalftone216"},
        {8, 0x800, "FixedHalftone252"},
        {9, 0x900, "FixedHalftone256"},
    };
    Palette *palette = calloc(1, sizeof(*palette));
    unsigned int i;
    Status st;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    {
        palette->Flags = 0;
        palette->Count = 256;
        st = pGdipInitializePalette(palette, tests[i].type, 1, FALSE, NULL);
        check(st == Ok, "%s: initialised (%d)", tests[i].name, st);
        check(palette->Flags == tests[i].flags, "%s: flags %#x", tests[i].name, palette->Flags);
    }
    palette->Flags = 0x1234;
    st = pGdipInitializePalette(palette, 0 /* custom */, 16, FALSE, NULL);
    check(st == Ok && palette->Flags == 0x1234, "a custom palette keeps its flags (%#x)", palette->Flags);
    free(palette);
}

static void test_wmf(void)
{
    static const Placeable placeable = { 0x9ac6cdd7, 0, 0, 0, 320, 320, 1440, 0, 0 };
    HDC hdc = CreateMetaFileW(NULL);
    HMETAFILE hmf;
    BYTE header[512];
    void *meta = NULL, *copy = NULL;
    UINT size;
    Status st;

    Rectangle(hdc, 1, 1, 100, 100);
    hmf = CloseMetaFile(hdc);
    size = GetMetaFileBitsEx(hmf, 0, NULL);
    check(size > 18, "a Windows metafile of %u bytes", size);

    st = pGdipCreateMetafileFromWmf(hmf, FALSE, &placeable, &meta);
    check(st == Ok && meta, "from a placeable metafile (%d)", st);
    if (meta)
    {
        memset(header, 0, sizeof(header));
        st = pGdipGetMetafileHeaderFromMetafile(meta, header);
        check(st == Ok && *(DWORD *)header == 2 /* WmfPlaceable */, "placeable type %lu", *(DWORD *)header);
        check(*(DWORD *)(header + 4) == size, "size %lu, the metafile has %u bytes", *(DWORD *)(header + 4), size);
        check(*(DWORD *)(header + 8) == 0x300, "version %#lx", *(DWORD *)(header + 8));

        st = pGdipCloneImage(meta, &copy);
        if (st == Ok && copy)
        {
            memset(header, 0, sizeof(header));
            pGdipGetMetafileHeaderFromMetafile(copy, header);
            check(*(DWORD *)(header + 4) == size && *(DWORD *)(header + 8) == 0x300, "a clone has the same size and version");
            pGdipDisposeImage(copy);
        }
        pGdipDisposeImage(meta);
    }

    meta = NULL;
    st = pGdipCreateMetafileFromWmf(hmf, FALSE, NULL, &meta);
    check(st == Ok && meta, "from a metafile without placeable header (%d)", st);
    if (meta)
    {
        memset(header, 0, sizeof(header));
        pGdipGetMetafileHeaderFromMetafile(meta, header);
        check(*(DWORD *)header == 1 /* Wmf */ && *(DWORD *)(header + 4) == size && *(DWORD *)(header + 8) == 0x300,
                "plain Wmf: type %lu size %lu version %#lx", *(DWORD *)header, *(DWORD *)(header + 4), *(DWORD *)(header + 8));
        pGdipDisposeImage(meta);
    }
    DeleteMetaFile(hmf);
}

int main(void)
{
    HMODULE gdip = LoadLibraryW(L"gdiplus.dll");
    ULONG_PTR token;
    StartupInput input = { 1, NULL, FALSE, FALSE };
    Status st;

    CoInitialize(NULL);
    check(gdip != NULL, "gdiplus loads");
    if (!gdip) goto done;
#define LOAD(n) p##n = (void *)GetProcAddress(gdip, #n); if (!p##n) { check(0, "%s is exported", #n); goto done; }
    LOAD(GdiplusStartup) LOAD(GdipCreateBitmapFromScan0) LOAD(GdipDisposeImage) LOAD(GdipCreateHBITMAPFromBitmap)
    LOAD(GdipGetImageThumbnail) LOAD(GdipBitmapLockBits) LOAD(GdipBitmapUnlockBits) LOAD(GdipSaveImageToStream)
    LOAD(GdipInitializePalette) LOAD(GdipCreateMetafileFromWmf) LOAD(GdipGetMetafileHeaderFromMetafile) LOAD(GdipCloneImage)
    st = pGdiplusStartup(&token, &input, NULL);
    check(st == Ok, "GdiplusStartup (%d)", st);
    if (st != Ok) goto done;

    test_formats();
    test_save();
    test_palettes();
    test_wmf();
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
