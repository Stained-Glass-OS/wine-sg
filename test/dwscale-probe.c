/* dwscale-probe: a glyph run analysis whose size is in its transform
 * (patches/sg/0222). cairo -- GTK's text: Pinta, Inkscape, GIMP 3 -- draws
 * each glyph at em size 1 with the size in the matrix and a glyph offset
 * that puts its ink box at 0,0; Wine hinted at 1 ppem, the box snapped to
 * whole ems, and the tops of lower-case letters were cut off ("o" drawn
 * as "u"). Compares that call with drawing at the real size: the same box
 * and the same pixels. Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <dwrite_3.h>
#include <stdio.h>
#include <string.h>

static IDWriteFactory3 *factory;

static IDWriteGlyphRunAnalysis *analyse( DWRITE_GLYPH_RUN *run, DWRITE_MATRIX *m )
{
    IDWriteGlyphRunAnalysis *a = NULL;
    IDWriteFactory3_CreateGlyphRunAnalysis( factory, run, m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC,
                                            DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_ENABLED,
                                            DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0, &a );
    return a;
}

int main( int argc, char **argv )
{
    IDWriteFontCollection *coll; IDWriteFontFamily *fam; IDWriteFont *font; IDWriteFontFace *face;
    const char *chars = "onBgd";
    UINT32 idx; BOOL exists; int i, same_box = 0, same_pixels = 0, total = 0;
    WCHAR name[64];

    MultiByteToWideChar( CP_ACP, 0, argc > 1 ? argv[1] : "Liberation Sans", -1, name, 64 );
    DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory3, (IUnknown **)&factory );
    IDWriteFactory_GetSystemFontCollection( (IDWriteFactory *)factory, &coll, FALSE );
    if (FAILED( IDWriteFontCollection_FindFamilyName( coll, name, &idx, &exists ) ) || !exists)
    {
        printf( "font=missing\n" );
        return 0;
    }
    IDWriteFontCollection_GetFontFamily( coll, idx, &fam );
    IDWriteFontFamily_GetFirstMatchingFont( fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                            DWRITE_FONT_STYLE_NORMAL, &font );
    IDWriteFont_CreateFontFace( font, &face );

    for (i = 0; chars[i]; i++)
    {
        UINT32 cp = chars[i];
        UINT16 g;
        FLOAT adv = 0;
        DWRITE_GLYPH_RUN run = { 0 };
        DWRITE_MATRIX plain = { 1, 0, 0, 1, 0, 0 }, scaled = { 16, 0, 0, 16, 0, 0 };
        DWRITE_GLYPH_OFFSET off;
        IDWriteGlyphRunAnalysis *a;
        RECT direct, cairo, want;
        static BYTE t1[8192], t2[8192];
        int w, h;

        IDWriteFontFace_GetGlyphIndices( face, &cp, 1, &g );
        run.fontFace = face; run.glyphCount = 1; run.glyphIndices = &g; run.glyphAdvances = &adv;

        /* at the real size */
        run.fontEmSize = 16;
        a = analyse( &run, &plain );
        IDWriteGlyphRunAnalysis_GetAlphaTextureBounds( a, DWRITE_TEXTURE_ALIASED_1x1, &direct );
        w = direct.right - direct.left; h = direct.bottom - direct.top;
        memset( t1, 0, sizeof(t1) );
        IDWriteGlyphRunAnalysis_CreateAlphaTexture( a, DWRITE_TEXTURE_ALIASED_1x1, &direct, t1, w * h );
        IDWriteGlyphRunAnalysis_Release( a );

        /* as cairo: em size 1, the size in the matrix, the ink box moved to 0,0 */
        run.fontEmSize = 1;
        off.advanceOffset = -direct.left / 16.0f;
        off.ascenderOffset = direct.top / 16.0f;     /* Y is up for the offset */
        run.glyphOffsets = &off;
        a = analyse( &run, &scaled );
        IDWriteGlyphRunAnalysis_GetAlphaTextureBounds( a, DWRITE_TEXTURE_ALIASED_1x1, &cairo );
        SetRect( &want, 0, 0, w, h );
        memset( t2, 0, sizeof(t2) );
        IDWriteGlyphRunAnalysis_CreateAlphaTexture( a, DWRITE_TEXTURE_ALIASED_1x1, &want, t2, w * h );
        IDWriteGlyphRunAnalysis_Release( a );

        total++;
        if (EqualRect( &cairo, &want )) same_box++;
        if (!memcmp( t1, t2, w * h )) same_pixels++;
        else
        {
            /* close enough: rounding may move an edge pixel's grey a little */
            int k, diff = 0;
            for (k = 0; k < w * h; k++) if (abs( t1[k] - t2[k] ) > 64) diff++;
            if (!diff) same_pixels++;
        }
        printf( "%c: direct %ld,%ld-%ld,%ld  cairo %ld,%ld-%ld,%ld\n", chars[i], direct.left, direct.top, direct.right,
                direct.bottom, cairo.left, cairo.top, cairo.right, cairo.bottom );
    }
    printf( "same_box=%d/%d\nsame_pixels=%d/%d\n", same_box, total, same_pixels, total );
    return 0;
}
