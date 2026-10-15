/* gdiplus GdipSetPenUnit / GdipCreateHalftonePalette and opengl32 wglGetLayerPaletteEntries (patch 3595). */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

struct startup { UINT version; void *cb; BOOL no_thread, no_codecs; };

int main(void)
{
    HMODULE gdip = LoadLibraryA("gdiplus.dll"), gl = LoadLibraryA("opengl32.dll");
    int (WINAPI *startup)(ULONG_PTR *, const struct startup *, void *) = (void *)GetProcAddress(gdip, "GdiplusStartup");
    int (WINAPI *create_pen)(DWORD, float, int, void **) = (void *)GetProcAddress(gdip, "GdipCreatePen1");
    int (WINAPI *delete_pen)(void *) = (void *)GetProcAddress(gdip, "GdipDeletePen");
    int (WINAPI *set_unit)(void *, int) = (void *)GetProcAddress(gdip, "GdipSetPenUnit");
    int (WINAPI *get_unit)(void *, int *) = (void *)GetProcAddress(gdip, "GdipGetPenUnit");
    HPALETTE (WINAPI *halftone)(void) = (void *)GetProcAddress(gdip, "GdipCreateHalftonePalette");
    int (WINAPI *layer_pal)(HDC, int, int, int, COLORREF *) = (void *)GetProcAddress(gl, "wglGetLayerPaletteEntries");
    struct startup si = {1};
    ULONG_PTR token;
    void *pen = NULL;
    int unit = -1, st;
    HPALETTE pal;
    WORD entries = 0;
    COLORREF colors[4];
    HDC hdc;

    check(gdip && gl && set_unit && layer_pal, "libraries and exports");
    if (!set_unit) { printf("RESULT: FAIL\n"); return 1; }
    startup(&token, &si, NULL);
    create_pen(0xff000000, 1.0f, 2 /* UnitPixel */, &pen);
    get_unit(pen, &unit);
    check(unit == 2, "pen starts with the unit it was created with");
    st = set_unit(pen, 3 /* UnitPoint */);
    get_unit(pen, &unit);
    check(st == 0 && unit == 3, "GdipSetPenUnit changes the unit");
    check(set_unit(NULL, 2) == 2, "GdipSetPenUnit rejects a NULL pen");
    check(set_unit(pen, 99) == 2, "GdipSetPenUnit rejects an unknown unit");
    get_unit(pen, &unit);
    check(unit == 3, "a rejected unit changes nothing");
    delete_pen(pen);

    pal = halftone();
    check(pal != NULL, "GdipCreateHalftonePalette returns a palette");
    if (pal)
    {
        GetObjectW(pal, sizeof(entries), &entries);
        check(entries >= 20, "the halftone palette has entries");
        DeleteObject(pal);
    }

    hdc = GetDC(0);
    check(layer_pal(hdc, 1, 0, 4, colors) == 0, "wglGetLayerPaletteEntries finds no layer palette");
    ReleaseDC(0, hdc);
    printf("%d failures\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
