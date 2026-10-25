/* dwoutline-probe: IDWriteFontFace::GetGlyphRunOutline gives the glyph's design outline scaled to the
 * em size, unhinted (patches/sg/6538). cairo asks for outlines at an em size of 1 and scales them up
 * (Pango path text: RawTherapee's image info); hinted at 1 px and rounded to 1/64 they were broken.
 * The outline of 'N' (Tahoma Bold) at em size 1, scaled by 100, must match the one at em size 100.
 * Prints name=value lines.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d2d1.h>
#include <dwrite.h>
#include <stdio.h>
#include <math.h>

static D2D1_POINT_2F pts[256];
static int npts;

static HRESULT WINAPI qi( ID2D1SimplifiedGeometrySink *s, REFIID r, void **o ) { *o = s; return S_OK; }
static ULONG WINAPI ref( ID2D1SimplifiedGeometrySink *s ) { return 2; }
static void WINAPI set_fill( ID2D1SimplifiedGeometrySink *s, D2D1_FILL_MODE m ) {}
static void WINAPI set_seg( ID2D1SimplifiedGeometrySink *s, D2D1_PATH_SEGMENT f ) {}
static void add( D2D1_POINT_2F p ) { if (npts < 256) pts[npts++] = p; }
static void WINAPI begin( ID2D1SimplifiedGeometrySink *s, D2D1_POINT_2F p, D2D1_FIGURE_BEGIN b ) { add( p ); }
static void WINAPI lines( ID2D1SimplifiedGeometrySink *s, const D2D1_POINT_2F *p, UINT n ) { UINT i; for (i = 0; i < n; i++) add( p[i] ); }
static void WINAPI beziers( ID2D1SimplifiedGeometrySink *s, const D2D1_BEZIER_SEGMENT *b, UINT n )
{ UINT i; for (i = 0; i < n; i++) { add( b[i].point1 ); add( b[i].point2 ); add( b[i].point3 ); } }
static void WINAPI end( ID2D1SimplifiedGeometrySink *s, D2D1_FIGURE_END e ) {}
static HRESULT WINAPI close_sink( ID2D1SimplifiedGeometrySink *s ) { return S_OK; }
static ID2D1SimplifiedGeometrySinkVtbl vtbl = { (void *)qi, (void *)ref, (void *)ref, set_fill, set_seg, begin, lines, beziers, end, close_sink };
static ID2D1SimplifiedGeometrySink sink = { &vtbl };

static int outline( IDWriteFontFace *face, UINT16 glyph, float em, D2D1_POINT_2F *out )
{
    npts = 0;
    if (FAILED(IDWriteFontFace_GetGlyphRunOutline( face, em, &glyph, NULL, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)&sink )))
        return -1;
    memcpy( out, pts, npts * sizeof(*pts) );
    return npts;
}

int main( int argc, char **argv )
{
    static D2D1_POINT_2F small[256], big[256];
    const char *name = argc > 1 ? argv[1] : "Tahoma";
    IDWriteFactory *factory;
    IDWriteFontCollection *collection;
    IDWriteFontFamily *family;
    IDWriteFont *font;
    IDWriteFontFace *face;
    WCHAR wname[64];
    UINT32 index, cp = 'N';
    UINT16 glyph;
    BOOL exists;
    int ns, nb, i, offgrid = 0;
    float maxdiff = 0;

    MultiByteToWideChar( CP_ACP, 0, name, -1, wname, 64 );
    DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory );
    IDWriteFactory_GetSystemFontCollection( factory, &collection, FALSE );
    IDWriteFontCollection_FindFamilyName( collection, wname, &index, &exists );
    printf( "family=%d\n", exists );
    if (!exists) return 1;
    IDWriteFontCollection_GetFontFamily( collection, index, &family );
    IDWriteFontFamily_GetFirstMatchingFont( family, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font );
    IDWriteFont_CreateFontFace( font, &face );
    IDWriteFontFace_GetGlyphIndices( face, &cp, 1, &glyph );

    ns = outline( face, glyph, 1.0f, small );
    nb = outline( face, glyph, 100.0f, big );
    printf( "points small=%d big=%d\n", ns, nb );
    for (i = 0; i < nb && i < ns; i++)
    {
        float dx = fabsf( small[i].x * 100 - big[i].x ), dy = fabsf( small[i].y * 100 - big[i].y );
        if (dx > maxdiff) maxdiff = dx;
        if (dy > maxdiff) maxdiff = dy;
        if (fabsf( big[i].x * 64 - floorf( big[i].x * 64 + 0.5f ) ) > 0.01f) offgrid++;
    }
    printf( "maxdiff=%.3f offgrid=%d\n", maxdiff, offgrid );
    printf( "same_points=%d\n", ns > 4 && ns == nb );
    printf( "scales=%d\n", ns > 4 && ns == nb && maxdiff < 0.5f );
    printf( "unhinted=%d\n", offgrid > 0 );
    printf( "done=1\n" );
    return 0;
}
