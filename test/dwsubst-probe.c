/* dwsubst-probe NAME...: for each family name, what the system font
 * collection of DirectWrite answers, as a program that measures and draws
 * with it (Skia) asks:
 *   NAME=exists|index|ascent,descent,linegap,unitsPerEm|advance of "Stained Glass"
 *   NAME=none
 * and "listed=1" per name the collection's enumeration names. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dwrite.h>
#include <stdio.h>
#include <wchar.h>

int wmain(int argc, WCHAR **argv)
{
    IDWriteFactory *factory;
    IDWriteFontCollection *collection;
    UINT32 i, j, count;
    int a;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory))) return 1;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(factory, &collection, FALSE))) return 1;
    count = IDWriteFontCollection_GetFontFamilyCount(collection);

    for (a = 1; a < argc; a++)
    {
        UINT32 index;
        BOOL exists = FALSE, listed = FALSE;
        IDWriteFontFamily *family;
        IDWriteFont *font;
        IDWriteFontFace *face;
        DWRITE_FONT_METRICS m;
        UINT16 glyphs[32];
        UINT32 cps[32];
        DWRITE_GLYPH_METRICS gm[32];
        const WCHAR *text = L"Stained Glass";
        int n = wcslen(text), adv = 0;

        IDWriteFontCollection_FindFamilyName(collection, argv[a], &index, &exists);
        for (i = 0; i < count && !listed; i++)
        {
            IDWriteLocalizedStrings *names;
            WCHAR name[256];
            if (FAILED(IDWriteFontCollection_GetFontFamily(collection, i, &family))) continue;
            if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(family, &names)))
            {
                for (j = 0; j < IDWriteLocalizedStrings_GetCount(names); j++)
                    if (SUCCEEDED(IDWriteLocalizedStrings_GetString(names, j, name, 256)) && !_wcsicmp(name, argv[a]))
                        listed = TRUE;
                IDWriteLocalizedStrings_Release(names);
            }
            IDWriteFontFamily_Release(family);
        }
        if (!exists)
        {
            printf("%ls=none\n", argv[a]);
            continue;
        }
        IDWriteFontCollection_GetFontFamily(collection, index, &family);
        IDWriteFontFamily_GetFirstMatchingFont(family, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                               DWRITE_FONT_STYLE_NORMAL, &font);
        IDWriteFont_CreateFontFace(font, &face);
        IDWriteFontFace_GetMetrics(face, &m);
        for (i = 0; i < (UINT32)n; i++) cps[i] = text[i];
        IDWriteFontFace_GetGlyphIndices(face, cps, n, glyphs);
        IDWriteFontFace_GetDesignGlyphMetrics(face, glyphs, n, gm, FALSE);
        for (i = 0; i < (UINT32)n; i++) adv += gm[i].advanceWidth;
        printf("%ls=exists|%u|%u,%u,%d,%u|%d%s\n", argv[a], index, m.ascent, m.descent, m.lineGap, m.designUnitsPerEm,
               adv, listed ? "|listed" : "");
    }
    return 0;
}
