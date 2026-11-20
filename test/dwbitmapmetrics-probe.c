/* dwbitmapmetrics gate probe (wine-sg 8407): IDWriteFontFace::GetDesignGlyphMetrics of a glyph in a
 * bitmap-only font (no outlines, only fixed-size strikes). Prints KEY=1 per check, then done=1.
 *   dwbitmapmetrics-probe.exe FONTFILE */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>

static BOOL sane(INT32 v, UINT32 upem) { return v > -2 * (INT32)upem && v < 2 * (INT32)upem; }

int wmain(int argc, WCHAR **argv)
{
    IDWriteFactory *f;
    IDWriteFontFile *file;
    IDWriteFontFace *face;
    DWRITE_FONT_METRICS fm;
    DWRITE_GLYPH_METRICS gm[2];
    BOOL supported;
    DWRITE_FONT_FILE_TYPE ft;
    DWRITE_FONT_FACE_TYPE fft;
    UINT32 n, cp = 0x2605;
    UINT16 g[2] = { 0, 0 };
    HRESULT hr;

    if (argc < 2 || FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&f)) ||
        FAILED(IDWriteFactory_CreateFontFileReference(f, argv[1], NULL, &file)) ||
        FAILED(IDWriteFontFile_Analyze(file, &supported, &ft, &fft, &n)) || !supported ||
        FAILED(IDWriteFactory_CreateFontFace(f, fft, 1, &file, 0, 0, &face)))
    {
        printf("face=0\ndone=1\n");
        return 0;
    }
    printf("face=1\n");
    IDWriteFontFace_GetMetrics(face, &fm);
    IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g[0]);
    g[1] = g[0];
    memset(gm, 0xcc, sizeof(gm));
    hr = IDWriteFontFace_GetDesignGlyphMetrics(face, g, 2, gm, FALSE);
    printf("metrics: hr %08lx glyph %u adv %u lsb %d rsb %d tsb %d advh %u bsb %d vorig %d\n", hr, g[0],
           gm[0].advanceWidth, gm[0].leftSideBearing, gm[0].rightSideBearing, gm[0].topSideBearing,
           gm[0].advanceHeight, gm[0].bottomSideBearing, gm[0].verticalOriginY);
    printf("advance=%d\n", hr == S_OK && g[0] && gm[0].advanceWidth == 2550);
    printf("bearings=%d\n", hr == S_OK && sane(gm[0].leftSideBearing, fm.designUnitsPerEm) &&
           sane(gm[0].rightSideBearing, fm.designUnitsPerEm) && sane(gm[0].topSideBearing, fm.designUnitsPerEm) &&
           sane(gm[0].bottomSideBearing, fm.designUnitsPerEm) && gm[0].advanceHeight < 3 * fm.designUnitsPerEm &&
           gm[0].verticalOriginY == fm.ascent);
    /* the second (cached) answer is the same */
    printf("cached=%d\n", !memcmp(&gm[0], &gm[1], sizeof(gm[0])));
    printf("done=1\n");
    return 0;
}
