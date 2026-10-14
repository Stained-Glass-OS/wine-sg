/* gdiplus smoke: clip/origin calls on an EMF-only recording, EmfToWmfBits,
   RecordMetafileFileNameI, ConvertToEmfPlusToFile, metafile texture brush */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;
typedef struct { INT X, Y, Width, Height; } GpRect;
static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HMODULE g;
#define FN(name) static FARPROC p_##name
#define LOAD(name) p_##name = GetProcAddress(g, #name)
FN(GdiplusStartup); FN(GdipRecordMetafile); FN(GdipGetImageGraphicsContext); FN(GdipSetClipRect);
FN(GdipResetClip); FN(GdipTranslateClip); FN(GdipSetClipPath); FN(GdipCreatePath); FN(GdipAddPathRectangle);
FN(GdipDeletePath); FN(GdipSetRenderingOrigin); FN(GdipEmfToWmfBits); FN(GdipRecordMetafileFileNameI);
FN(GdipCreateMetafileFromEmf); FN(GdipConvertToEmfPlusToFile); FN(GdipCreateFromHDC); FN(GdipCreateTextureIA);
FN(GdipDisposeImage); FN(GdipDeleteGraphics); FN(GdipDeleteBrush); FN(GdipCreateSolidFill); FN(GdipGraphicsClear);
FN(GdipFillRectangle);

int main(void)
{
    ULONG_PTR token; StartupInput in = {1};
    void *meta = NULL, *gfx = NULL, *path = NULL, *out = NULL, *emfmeta = NULL, *tex = NULL, *brush = NULL;
    HDC refdc; GpStatus st; BOOL ok = FALSE;
    HENHMETAFILE hemf; HDC edc; UINT size, size2; BYTE *buf;
    WCHAR tmp[MAX_PATH], tmp2[MAX_PATH]; GpRect frame = {0, 0, 100, 100};

    g = LoadLibraryA("gdiplus.dll");
    if (!g) { printf("FAIL no gdiplus\n"); return 1; }
#define L(n) LOAD(n)
    L(GdiplusStartup); L(GdipRecordMetafile); L(GdipGetImageGraphicsContext); L(GdipSetClipRect); L(GdipResetClip);
    L(GdipTranslateClip); L(GdipSetClipPath); L(GdipCreatePath); L(GdipAddPathRectangle); L(GdipDeletePath);
    L(GdipSetRenderingOrigin); L(GdipEmfToWmfBits); L(GdipRecordMetafileFileNameI); L(GdipCreateMetafileFromEmf);
    L(GdipConvertToEmfPlusToFile); L(GdipCreateFromHDC); L(GdipCreateTextureIA); L(GdipDisposeImage);
    L(GdipDeleteGraphics); L(GdipDeleteBrush); L(GdipCreateSolidFill); L(GdipGraphicsClear); L(GdipFillRectangle);
    st = ((GpStatus (WINAPI *)(ULONG_PTR *, StartupInput *, void *))p_GdiplusStartup)(&token, &in, NULL);
    CHECK(st == 0, "startup %d", st);

    refdc = GetDC(0);
    {
        float rc[4] = {0, 0, 100, 100};
        st = ((GpStatus (WINAPI *)(HDC, int, void *, int, const WCHAR *, void **))p_GdipRecordMetafile)(refdc, 3 /* EmfOnly */, rc, 2 /* pixel */, NULL, &meta);
        CHECK(st == 0 && meta, "RecordMetafile %d", st);
    }
    if (meta)
    {
        st = ((GpStatus (WINAPI *)(void *, void **))p_GdipGetImageGraphicsContext)(meta, &gfx);
        CHECK(st == 0 && gfx, "graphics %d", st);
        st = ((GpStatus (WINAPI *)(void *, float, float, float, float, int))p_GdipSetClipRect)(gfx, 10, 10, 50, 50, 0);
        CHECK(st == 0, "SetClipRect %d", st);
        st = ((GpStatus (WINAPI *)(void *, float, float))p_GdipTranslateClip)(gfx, 5, 5);
        CHECK(st == 0, "TranslateClip %d", st);
        ((GpStatus (WINAPI *)(int, void **))p_GdipCreatePath)(0, &path);
        ((GpStatus (WINAPI *)(void *, float, float, float, float))p_GdipAddPathRectangle)(path, 0, 0, 40, 40);
        st = ((GpStatus (WINAPI *)(void *, void *, int))p_GdipSetClipPath)(gfx, path, 0);
        CHECK(st == 0, "SetClipPath %d", st);
        st = ((GpStatus (WINAPI *)(void *))p_GdipResetClip)(gfx);
        CHECK(st == 0, "ResetClip %d", st);
        st = ((GpStatus (WINAPI *)(void *, int, int))p_GdipSetRenderingOrigin)(gfx, 2, 3);
        CHECK(st == 0, "SetRenderingOrigin %d", st);
        ((GpStatus (WINAPI *)(void *))p_GdipDeletePath)(path);
        ((GpStatus (WINAPI *)(void *))p_GdipDeleteGraphics)(gfx);
        ((GpStatus (WINAPI *)(void *))p_GdipDisposeImage)(meta);
    }

    /* EMF -> WMF */
    edc = CreateEnhMetaFileA(NULL, NULL, NULL, NULL);
    Rectangle(edc, 10, 10, 90, 90);
    hemf = CloseEnhMetaFile(edc);
    size = ((UINT (WINAPI *)(HENHMETAFILE, UINT, BYTE *, INT, INT))p_GdipEmfToWmfBits)(hemf, 0, NULL, MM_ANISOTROPIC, 0);
    CHECK(size > 18, "EmfToWmfBits size %u", size);
    if (size > 18)
    {
        buf = calloc(1, size + 64);
        size2 = ((UINT (WINAPI *)(HENHMETAFILE, UINT, BYTE *, INT, INT))p_GdipEmfToWmfBits)(hemf, size, buf, MM_ANISOTROPIC, 0);
        CHECK(size2 == size, "EmfToWmfBits copy %u != %u", size2, size);
        CHECK(buf[0] == 1 && buf[2] == 9 && buf[3] == 0, "WMF header %02x %02x %02x", buf[0], buf[1], buf[2]);
        free(buf);
        size = ((UINT (WINAPI *)(HENHMETAFILE, UINT, BYTE *, INT, INT))p_GdipEmfToWmfBits)(hemf, 0, NULL, MM_ANISOTROPIC, 2);
        CHECK(size > 22, "placeable size %u", size);
        buf = calloc(1, size + 64);
        size2 = ((UINT (WINAPI *)(HENHMETAFILE, UINT, BYTE *, INT, INT))p_GdipEmfToWmfBits)(hemf, size, buf, MM_ANISOTROPIC, 2);
        CHECK(size2 == size && *(DWORD *)buf == 0x9ac6cdd7, "placeable header %#lx size %u/%u", *(DWORD *)buf, size2, size);
        free(buf);
    }

    /* record to a file with an integer rectangle */
    GetTempPathW(MAX_PATH, tmp); lstrcatW(tmp, L"sg_gapgdip_rec.emf");
    meta = NULL;
    st = ((GpStatus (WINAPI *)(const WCHAR *, HDC, int, GpRect *, int, const WCHAR *, void **))p_GdipRecordMetafileFileNameI)(tmp, refdc, 3, &frame, 2, NULL, &meta);
    CHECK(st == 0 && meta, "RecordMetafileFileNameI %d", st);
    if (meta) ((GpStatus (WINAPI *)(void *))p_GdipDisposeImage)(meta);
    CHECK(GetFileAttributesW(tmp) != INVALID_FILE_ATTRIBUTES, "recorded file missing");
    DeleteFileW(tmp);

    /* convert an EMF to EMF+ in a file */
    ((GpStatus (WINAPI *)(HENHMETAFILE, BOOL, void **))p_GdipCreateMetafileFromEmf)(hemf, FALSE, &emfmeta);
    ((GpStatus (WINAPI *)(HDC, void **))p_GdipCreateFromHDC)(refdc, &gfx);
    GetTempPathW(MAX_PATH, tmp2); lstrcatW(tmp2, L"sg_gapgdip_conv.emf");
    st = ((GpStatus (WINAPI *)(void *, void *, BOOL *, const WCHAR *, int, const WCHAR *, void **))p_GdipConvertToEmfPlusToFile)(gfx, emfmeta, &ok, tmp2, 4, NULL, &out);
    CHECK(st == 0 && ok && out, "ConvertToEmfPlusToFile %d ok %d", st, ok);
    if (out) ((GpStatus (WINAPI *)(void *))p_GdipDisposeImage)(out);
    CHECK(GetFileAttributesW(tmp2) != INVALID_FILE_ATTRIBUTES, "converted file missing");
    DeleteFileW(tmp2);

    /* a metafile as a texture */
    st = ((GpStatus (WINAPI *)(void *, void *, float, float, float, float, void **))p_GdipCreateTextureIA)(emfmeta, NULL, 0, 0, 20, 20, &tex);
    CHECK(st == 0 && tex, "CreateTextureIA(metafile) %d", st);
    if (tex) ((GpStatus (WINAPI *)(void *))p_GdipDeleteBrush)(tex);

    ((GpStatus (WINAPI *)(void *))p_GdipDeleteGraphics)(gfx);
    ((GpStatus (WINAPI *)(void *))p_GdipDisposeImage)(emfmeta);
    DeleteEnhMetaFile(hemf);
    ReleaseDC(0, refdc);
    printf("RESULT: %s\n", failed ? "FAIL" : "PASS");
    return failed != 0;
}
