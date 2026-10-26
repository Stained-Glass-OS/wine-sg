/* GDI+ text on a 32-bit compatible bitmap keeps its alpha (patches/sg/6122): as on a 32-bit DIB
 * section, the text's opaque pixels get alpha 255.  Wine drew text on a device bitmap with GDI,
 * which left alpha 0: SumatraPDF draws its tab bar (shown in the DWM frame) into a
 * CreateCompatibleBitmap buffer, and the selected tab's label was transparent there.
 * Prints, for each text rendering hint, "DDB|DIB hint H: ... text pixels N, of them alpha 255: M". */
#include <windows.h>
#include <stdio.h>
typedef int GpStatus; typedef void GpGraphics, GpBrush, GpFont, GpFontFamily;
typedef struct { UINT32 v; void *cb; BOOL a, b; } SI;
typedef struct { float X, Y, Width, Height; } RectF;
GpStatus WINAPI GdiplusStartup(ULONG_PTR*, const SI*, void*);
GpStatus WINAPI GdipCreateFromHDC(HDC, GpGraphics**);
GpStatus WINAPI GdipDeleteGraphics(GpGraphics*);
GpStatus WINAPI GdipCreateSolidFill(DWORD, GpBrush**);
GpStatus WINAPI GdipCreateFontFamilyFromName(const WCHAR*, void*, GpFontFamily**);
GpStatus WINAPI GdipCreateFont(GpFontFamily*, float, int, int, GpFont**);
GpStatus WINAPI GdipDrawString(GpGraphics*, const WCHAR*, int, GpFont*, const RectF*, void*, GpBrush*);
GpStatus WINAPI GdipSetTextRenderingHint(GpGraphics*, int);
static void test(const char *what, HBITMAP bmp, int hint)
{
    HDC screen = GetDC(0), mem = CreateCompatibleDC(screen);
    GpGraphics *g; GpBrush *b; GpFontFamily *fam; GpFont *font; RectF r = {2, 2, 200, 40};
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), 120, -30, 1, 32, BI_RGB}};
    DWORD px[120*30]; RECT rc = {0, 0, 120, 30}; HBRUSH white = CreateSolidBrush(RGB(255,255,255));
    int i, dark = 0, dark_opaque = 0, st;
    SelectObject(mem, bmp);
    FillRect(mem, &rc, white);              /* GDI fill: alpha stays 0 */
    GdipCreateFromHDC(mem, &g);
    if (hint >= 0) GdipSetTextRenderingHint(g, hint);
    GdipCreateSolidFill(0xff000000, &b);
    GdipCreateFontFamilyFromName(L"Tahoma", NULL, &fam);
    GdipCreateFont(fam, 14, 1, 2, &font);
    st = GdipDrawString(g, L"Tab label", -1, font, &r, NULL, b);
    GdipDeleteGraphics(g);
    GetDIBits(mem, bmp, 0, 30, px, &bi, DIB_RGB_COLORS);
    for (i = 0; i < 120*30; i++) if ((px[i] & 0xffffff) < 0x808080) { dark++; if ((px[i] >> 24) == 0xff) dark_opaque++; }
    printf("%s hint %d: st %d text pixels %d, of them alpha 255: %d\n", what, hint, st, dark, dark_opaque);
    ReleaseDC(0, screen); DeleteDC(mem);
}
int main(void)
{
    SI in = {1}; ULONG_PTR tok; HDC screen; void *bits;
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), 120, -30, 1, 32, BI_RGB}};
    int h;
    GdiplusStartup(&tok, &in, NULL);
    screen = GetDC(0);
    printf("screen bpp %d\n", GetDeviceCaps(screen, BITSPIXEL));
    for (h = -1; h <= 5; h += 1) {
        if (h == 0 || h == 2 || h == 3) continue;
        test("DDB", CreateCompatibleBitmap(screen, 120, 30), h);
        test("DIB", CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0), h);
    }
    return 0;
}
