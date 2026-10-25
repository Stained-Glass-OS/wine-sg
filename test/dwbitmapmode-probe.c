/* dwrite (patches/sg/6105): a glyph's bitmap is cached per rendering mode. Tahoma has bitmap
 * strikes (8 to 16 pixels); drawn aliased its bitmap is 1 bit a pixel with a 1-bit pitch, drawn
 * GDI-classic it is a byte a pixel. Both were one cache entry: the mode that came second read the
 * first one's bitmap with its own pitch, every other row of the glyph lost (Chromium draws a font
 * both ways: Cursor's and VS Code's text at 13 px). Each mode's texture of a glyph must match the
 * texture a fresh factory draws in that mode alone. */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <dwrite_3.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

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

static IDWriteFontFace *tahoma(IDWriteFactory **ret)
{
    IDWriteFontCollection *coll;
    IDWriteFontFamily *family;
    IDWriteFontFace *face;
    IDWriteFactory *factory;
    IDWriteFont *font;
    UINT32 index;
    BOOL exists = FALSE;

    /* an isolated factory has a font face, and so a glyph cache, of its own */
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&factory)))
        return NULL;
    IDWriteFactory_GetSystemFontCollection(factory, &coll, FALSE);
    IDWriteFontCollection_FindFamilyName(coll, L"Tahoma", &index, &exists);
    if (!exists) return NULL;
    IDWriteFontCollection_GetFontFamily(coll, index, &family);
    IDWriteFontFamily_GetFirstMatchingFont(family, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, &font);
    IDWriteFont_CreateFontFace(font, &face);
    IDWriteFont_Release(font);
    IDWriteFontFamily_Release(family);
    IDWriteFontCollection_Release(coll);
    *ret = factory;
    return face;
}

/* the glyph's texture in a mode, reduced to one ink byte a pixel */
static int draw(IDWriteFactory *factory, IDWriteFontFace *face, UINT16 glyph, DWRITE_RENDERING_MODE mode,
        RECT *bounds, BYTE *ink, int size)
{
    DWRITE_TEXTURE_TYPE type = mode == DWRITE_RENDERING_MODE_ALIASED ? DWRITE_TEXTURE_ALIASED_1x1 : DWRITE_TEXTURE_CLEARTYPE_3x1;
    int bpp = type == DWRITE_TEXTURE_ALIASED_1x1 ? 1 : 3, i, n;
    DWRITE_GLYPH_RUN run = { face, 13.0f, 1, &glyph, NULL, NULL, FALSE, 0 };
    IDWriteGlyphRunAnalysis *analysis;
    static BYTE buf[16384];
    HRESULT hr;

    hr = IDWriteFactory_CreateGlyphRunAnalysis(factory, &run, 1.0f, NULL, mode, DWRITE_MEASURING_MODE_GDI_CLASSIC,
            0.0f, 0.0f, &analysis);
    if (FAILED(hr)) return -1;
    IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(analysis, type, bounds);
    n = (bounds->right - bounds->left) * (bounds->bottom - bounds->top);
    if (n <= 0 || n > size) { IDWriteGlyphRunAnalysis_Release(analysis); return -1; }
    hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, type, bounds, buf, n * bpp);
    IDWriteGlyphRunAnalysis_Release(analysis);
    if (FAILED(hr)) return -1;
    for (i = 0; i < n; i++) ink[i] = buf[i * bpp] ? 1 : 0;
    return n;
}

static void compare(const char *what, UINT32 ch, DWRITE_RENDERING_MODE first, DWRITE_RENDERING_MODE second)
{
    IDWriteFactory *f1 = NULL, *f2 = NULL;
    IDWriteFontFace *alone = tahoma(&f1), *both = tahoma(&f2);
    BYTE ref[4096], got[4096], scratch[4096];
    RECT rref, rgot, rfirst;
    UINT16 glyph = 0;
    int n1, n2, rows = 0, y;

    if (!alone || !both) { check(0, "%s: Tahoma is in the system collection", what); return; }
    IDWriteFontFace_GetGlyphIndices(alone, &ch, 1, &glyph);

    n1 = draw(f1, alone, glyph, second, &rref, ref, sizeof(ref));
    draw(f2, both, glyph, first, &rfirst, scratch, sizeof(scratch));
    n2 = draw(f2, both, glyph, second, &rgot, got, sizeof(got));
    if (n1 > 0 && n1 == n2 && EqualRect(&rref, &rgot))
        for (y = 0; y < rref.bottom - rref.top; y++)
        {
            int w = rref.right - rref.left;
            if (memchr(ref + y * w, 1, w)) rows++;
        }
    check(n1 > 0 && n1 == n2 && EqualRect(&rref, &rgot) && !memcmp(ref, got, n1),
            "%s: '%c' drawn after the other mode is the glyph drawn alone (%d ink rows, %d pixels vs %d)",
            what, (char)ch, rows, n1, n2);

    IDWriteFontFace_Release(alone);
    IDWriteFontFace_Release(both);
    IDWriteFactory_Release(f1);
    IDWriteFactory_Release(f2);
}

int main(void)
{
    compare("GDI classic after aliased", 'L', DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC);
    compare("aliased after GDI classic", 'L', DWRITE_RENDERING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_ALIASED);
    compare("GDI classic after aliased", 'g', DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
