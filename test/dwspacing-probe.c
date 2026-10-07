/*
 * dwspacing-probe.c -- test/dwspacing-gate.sh's DirectWrite probe: does a
 * glyph drawn by IDWriteGlyphRunAnalysis land where its run put it, to the
 * fraction of a pixel, as Chromium's text (Skia) needs?
 *
 * Skia draws each glyph on its own: it asks DirectWrite for the glyph's
 * image at a sub-pixel offset of the baseline origin (0, 1/4, 1/2, 3/4 of a
 * pixel) and places that image at the whole-pixel part of the glyph's
 * position. If the offset is dropped, every glyph lands up to 3/4 pixel left
 * of where its advance put it, each by another amount: "Fun dam entals".
 *
 *   dwspacing-probe.exe FAMILY|FONTFILE PIXELS
 *
 * prints, for the family at that size (in its own words; the gate checks the
 * lines):
 *   face OK|MISSING
 *   shift F D        ink centre moved by D px when the origin moved by F px
 *   layout MAXERR E  a "Fundamentals" laid out as Skia does: each glyph's ink
 *                    centre vs where its advance puts it, worst error in px
 *   advance G N D    a glyph's GDI-compatible advance G, natural N (px) and
 *                    design advance D (px), for "Fundamentals"' glyphs
 *   gaps ...         the layout's gaps between neighbouring glyphs' ink
 *
 * Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later
 */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <dwrite_1.h>
#include <stdio.h>
#include <math.h>

static IDWriteFactory *factory;

/* Grey alpha of one glyph rendered at (ox, 0): returns its ink centre in x
 * (relative to the origin's whole-pixel part, as Skia places it) and the
 * texture's left edge. */
static BOOL glyph_centre(IDWriteFontFace *face, float px, UINT16 glyph, float ox, BOOL skia, double *centre)
{
    DWRITE_MATRIX xf = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    DWRITE_GLYPH_RUN run = {0};
    IDWriteGlyphRunAnalysis *ana;
    FLOAT adv = 0.0f;
    DWRITE_GLYPH_OFFSET off = {0};
    RECT r;
    BYTE *buf;
    double sum = 0, mom = 0;
    int w, h, x, y;
    HRESULT hr;

    run.fontFace = face;
    run.fontEmSize = px;
    run.glyphCount = 1;
    run.glyphIndices = &glyph;
    run.glyphAdvances = &adv;
    run.glyphOffsets = &off;
    /* Skia gives the fraction in the run's transform (dx), as Chromium's
     * renderer does; a plain caller gives it as the baseline origin */
    xf.dx = ox;
    hr = IDWriteFactory_CreateGlyphRunAnalysis(factory, &run, 1.0f, skia ? &xf : NULL,
            skia ? DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL : DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
            DWRITE_MEASURING_MODE_NATURAL, skia ? 0.0f : ox, 0.0f, &ana);
    if (FAILED(hr)) return FALSE;
    hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(ana, DWRITE_TEXTURE_CLEARTYPE_3x1, &r);
    if (FAILED(hr) || IsRectEmpty(&r)) { IDWriteGlyphRunAnalysis_Release(ana); return FALSE; }
    w = r.right - r.left; h = r.bottom - r.top;
    buf = calloc(1, w * h * 3);
    hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(ana, DWRITE_TEXTURE_CLEARTYPE_3x1, &r, buf, w * h * 3);
    IDWriteGlyphRunAnalysis_Release(ana);
    if (FAILED(hr)) { free(buf); return FALSE; }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
        {
            double a = (buf[(y * w + x) * 3] + buf[(y * w + x) * 3 + 1] + buf[(y * w + x) * 3 + 2]) / 3.0;
            sum += a;
            mom += a * (r.left + x + 0.5);
        }
    free(buf);
    if (sum <= 0) return FALSE;
    /* Skia places the image at the whole-pixel part of the position: the
     * texture's coordinates already include ox, so subtract nothing here;
     * the caller compares against ox. */
    *centre = mom / sum;
    return TRUE;
}

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR text[] = L"Fundamentals";
    IDWriteFontCollection *coll;
    IDWriteFontFamily *fam;
    IDWriteFont *font;
    IDWriteFontFace *face;
    DWRITE_FONT_METRICS fm;
    UINT32 cps[32], idx, n = lstrlenW(text), i;
    UINT16 glyphs[32];
    DWRITE_GLYPH_METRICS dm[32], gm[32], nm[32];
    BOOL exists = FALSE;
    float px;
    double maxerr = 0, pos, c0[32], worst_shift = 0;
    static const float fracs[] = { 0.25f, 0.5f, 0.75f };
    HRESULT hr;

    if (argc < 3) { printf("usage: FAMILY PIXELS\n"); return 2; }
    px = (float)_wtof(argv[2]);
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory);
    if (FAILED(hr)) { printf("factory FAILED %#lx\n", hr); return 1; }
    if (wcschr(argv[1], '\\'))
    {
        /* a font file (the gate names Liberation Sans and Inter's files) */
        IDWriteFontFile *file;
        if (FAILED(IDWriteFactory_CreateFontFileReference(factory, argv[1], NULL, &file))
                || FAILED(IDWriteFactory_CreateFontFace(factory, wcsstr(argv[1], L".otf") ? DWRITE_FONT_FACE_TYPE_CFF
                        : DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face)))
        {
            printf("face MISSING\n");
            return 1;
        }
    }
    else
    {
        IDWriteFactory_GetSystemFontCollection(factory, &coll, FALSE);
        IDWriteFontCollection_FindFamilyName(coll, argv[1], &idx, &exists);
        if (!exists) { printf("face MISSING\n"); return 1; }
        IDWriteFontCollection_GetFontFamily(coll, idx, &fam);
        IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, &font);
        IDWriteFont_CreateFontFace(font, &face);
    }
    IDWriteFontFace_GetMetrics(face, &fm);
    printf("face OK\n");

    for (i = 0; i < n; i++) cps[i] = text[i];
    IDWriteFontFace_GetGlyphIndices(face, cps, n, glyphs);
    IDWriteFontFace_GetDesignGlyphMetrics(face, glyphs, n, dm, FALSE);
    IDWriteFontFace_GetGdiCompatibleGlyphMetrics(face, px, 1.0f, NULL, FALSE, glyphs, n, gm, FALSE);
    IDWriteFontFace_GetGdiCompatibleGlyphMetrics(face, px, 1.0f, NULL, TRUE, glyphs, n, nm, FALSE);
    for (i = 0; i < n; i++)
        printf("advance %c %.3f %.3f %.3f\n", (char)text[i], gm[i].advanceWidth * px / fm.designUnitsPerEm,
               nm[i].advanceWidth * px / fm.designUnitsPerEm, dm[i].advanceWidth * px / fm.designUnitsPerEm);

    /* the ink centre of each glyph at a whole-pixel origin, and how it moves
     * with the origin's fraction */
    for (i = 0; i < n; i++)
    {
        int k;
        if (!glyph_centre(face, px, glyphs[i], 0.0f, FALSE, &c0[i])) { printf("render FAILED %u\n", i); return 1; }
        for (k = 0; k < 3; k++)
        {
            double c;
            if (!glyph_centre(face, px, glyphs[i], fracs[k], FALSE, &c)) { printf("render FAILED %u\n", i); return 1; }
            if (i == 0) printf("shift %.2f %.3f\n", fracs[k], c - c0[i]);
            if (fabs((c - c0[i]) - fracs[k]) > worst_shift) worst_shift = fabs((c - c0[i]) - fracs[k]);
        }
    }
    printf("shift WORST %.3f\n", worst_shift);

    /* "Fundamentals" laid out with design advances, as Skia with sub-pixel
     * positioning: each glyph's image at the quarter pixel of its position. */
    pos = 10.0;
    printf("gaps");
    {
        double prev = 0;
        for (i = 0; i < n; i++)
        {
            double whole = floor(pos), frac = pos - whole, q = floor(frac * 4 + 0.5) / 4, c, ideal;
            if (q >= 1.0) { whole += 1.0; q = 0; }
            glyph_centre(face, px, glyphs[i], (float)q, TRUE, &c);
            c += whole;
            ideal = pos + c0[i];
            if (fabs(c - ideal) > maxerr) maxerr = fabs(c - ideal);
            if (i) printf(" %.2f", c - prev);
            prev = c;
            pos += dm[i].advanceWidth * px / fm.designUnitsPerEm;
        }
    }
    printf("\nlayout MAXERR %.3f\n", maxerr);
    return 0;
}
