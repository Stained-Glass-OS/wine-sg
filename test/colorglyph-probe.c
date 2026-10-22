/* colorglyph-probe: glyphs drawn from glyph images (color emoji fonts) in
 * DirectWrite and Direct2D (Stained Glass OS wine-sg 6007).
 *   colorglyph-probe.exe FONT.ttf   (from colorglyph-font.py: "A" an outline,
 *   U+2605 a 64x64 red PNG image at 64 ppem, origin at its bottom-left) */
#define COBJMACROS
#include <windows.h>
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <stdio.h>

static const GUID IID_IDWriteFactory4_ = {0x4b0b5bd3,0x0797,0x4549,{0x8a,0xc5,0xfe,0x91,0x5c,0xc5,0x38,0x56}};
static const GUID IID_IDWriteFactory5_ = {0x958db99a,0xbe2a,0x4f09,{0xaf,0x7d,0x65,0x18,0x98,0x03,0xd1,0xd3}};

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static IWICBitmap *wic_bitmap;

static DWORD pixel( int x, int y )
{
    WICRect rect = { x, y, 1, 1 };
    DWORD p = 0;
    IWICBitmap_CopyPixels( wic_bitmap, &rect, 4, 4, (BYTE *)&p );
    return p & 0xffffff;
}

static ID2D1RenderTarget *rt;
static ID2D1DeviceContext4 *ctx;

static void clear( void )
{
    D2D1_COLOR_F white = { 1.0f, 1.0f, 1.0f, 1.0f };
    ID2D1RenderTarget_BeginDraw( rt );
    ID2D1RenderTarget_Clear( rt, &white );
    ID2D1RenderTarget_EndDraw( rt, NULL, NULL );
}

int wmain( int argc, WCHAR **argv )
{
    D2D1_RENDER_TARGET_PROPERTIES props = { D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f };
    IDWriteColorGlyphRunEnumerator1 *runs;
    const DWRITE_COLOR_GLYPH_RUN1 *run1;
    IDWriteFactory4 *dw;
    IDWriteFontFile *file;
    IDWriteFontFace *face;
    IWICImagingFactory *wic;
    ID2D1Factory *d2d;
    DWRITE_GLYPH_RUN run = {0};
    UINT32 codepoints[2] = { 0x2605, 'A' };
    UINT16 glyphs[2];
    D2D1_POINT_2F origin = { 20.0f, 80.0f };
    BOOL has;
    HRESULT hr;
    int n, saw_png = 0, saw_outline = 0;

    if (argc < 2) return 2;
    CoInitialize( NULL );
    DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory4_, (IUnknown **)&dw );
    hr = IDWriteFactory4_CreateFontFileReference( dw, argv[1], NULL, &file );
    check( hr == S_OK, "the test font", hr );
    hr = IDWriteFactory4_CreateFontFace( dw, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face );
    check( hr == S_OK, "its font face", hr );
    if (FAILED(hr)) { printf( "RESULT: FAIL\n" ); return 1; }
    IDWriteFontFace_GetGlyphIndices( face, codepoints, 2, glyphs );
    run.fontFace = face;
    run.fontEmSize = 64.0f;
    run.glyphCount = 1;
    run.glyphIndices = glyphs;

    /* DirectWrite: the image glyph is a run of its own image format */
    hr = IDWriteFactory4_TranslateColorGlyphRun( dw, origin, &run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
            DWRITE_GLYPH_IMAGE_FORMATS_PNG, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &runs );
    check( hr == S_OK, "TranslateColorGlyphRun of an image glyph", hr );
    if (hr == S_OK)
    {
        has = FALSE;
        IDWriteColorGlyphRunEnumerator1_MoveNext( runs, &has );
        IDWriteColorGlyphRunEnumerator1_GetCurrentRun( runs, &run1 );
        check( has && run1->glyphImageFormat == DWRITE_GLYPH_IMAGE_FORMATS_PNG && run1->glyphRun.glyphCount == 1 &&
               run1->glyphRun.glyphIndices[0] == glyphs[0], "a PNG run with the glyph", has ? run1->glyphImageFormat : 0 );
        IDWriteColorGlyphRunEnumerator1_Release( runs );
    }
    hr = IDWriteFactory4_TranslateColorGlyphRun( dw, origin, &run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE,
            DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &runs );
    check( hr == DWRITE_E_NOCOLOR, "no image format asked for: DWRITE_E_NOCOLOR", hr );

    run.glyphCount = 2;
    hr = IDWriteFactory4_TranslateColorGlyphRun( dw, origin, &run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
            DWRITE_GLYPH_IMAGE_FORMATS_PNG, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &runs );
    for (n = 0; hr == S_OK && n < 5; n++)
    {
        has = FALSE;
        IDWriteColorGlyphRunEnumerator1_MoveNext( runs, &has );
        if (!has) break;
        IDWriteColorGlyphRunEnumerator1_GetCurrentRun( runs, &run1 );
        if (run1->glyphImageFormat == DWRITE_GLYPH_IMAGE_FORMATS_PNG && run1->glyphRun.glyphCount == 1) saw_png = 1;
        if (run1->glyphImageFormat == DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE) saw_outline = 1;
    }
    if (hr == S_OK) IDWriteColorGlyphRunEnumerator1_Release( runs );
    check( hr == S_OK && saw_png && saw_outline, "a mixed run: an outline run and a PNG run", hr );
    run.glyphCount = 1;

    /* Direct2D draws the image */
    CoCreateInstance( &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic );
    IWICImagingFactory_CreateBitmap( wic, 200, 100, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wic_bitmap );
    D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&d2d );
    hr = ID2D1Factory_CreateWicBitmapRenderTarget( d2d, wic_bitmap, &props, &rt );
    check( hr == S_OK, "a render target", hr );
    hr = ID2D1RenderTarget_QueryInterface( rt, &IID_ID2D1DeviceContext4, (void **)&ctx );
    check( hr == S_OK, "ID2D1DeviceContext4", hr );
    if (!ctx) { printf( "RESULT: FAIL\n" ); return 1; }

    clear();
    ID2D1RenderTarget_BeginDraw( rt );
    ID2D1DeviceContext4_DrawColorBitmapGlyphRun( ctx, DWRITE_GLYPH_IMAGE_FORMATS_PNG, origin, &run,
            DWRITE_MEASURING_MODE_NATURAL, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT );
    hr = ID2D1RenderTarget_EndDraw( rt, NULL, NULL );
    check( hr == S_OK, "DrawColorBitmapGlyphRun", hr );
    check( pixel( 50, 50 ) == 0xff0000, "the glyph's image is drawn (red)", pixel( 50, 50 ) );
    check( pixel( 10, 50 ) == 0xffffff && pixel( 95, 50 ) == 0xffffff && pixel( 50, 90 ) == 0xffffff,
           "at its place and size (64x64 from the origin up)", pixel( 95, 50 ) );
    check( pixel( 22, 18 ) == 0xff0000 && pixel( 82, 78 ) == 0xff0000, "all of it", pixel( 82, 78 ) );

    clear();
    ID2D1RenderTarget_BeginDraw( rt );
    ID2D1DeviceContext4_DrawColorBitmapGlyphRun( ctx, DWRITE_GLYPH_IMAGE_FORMATS_COLR, origin, &run,
            DWRITE_MEASURING_MODE_NATURAL, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT );
    hr = ID2D1RenderTarget_EndDraw( rt, NULL, NULL );
    check( hr == E_INVALIDARG, "DrawColorBitmapGlyphRun of a non-bitmap format: E_INVALIDARG", hr );

    {
        ID2D1Image *image = NULL;
        D2D1_MATRIX_3X2_F m;
        D2D1_POINT_2F at = { 10.0f, 70.0f };
        hr = ID2D1DeviceContext4_GetColorBitmapGlyphImage( ctx, DWRITE_GLYPH_IMAGE_FORMATS_PNG, at, face, 32.0f,
                glyphs[0], FALSE, NULL, 96.0f, 96.0f, &m, &image );
        check( hr == S_OK && image, "GetColorBitmapGlyphImage", hr );
        check( hr == S_OK && m._11 > 0.49f && m._11 < 0.51f && m.dx > 9.9f && m.dx < 10.1f && m.dy > 37.9f && m.dy < 38.1f,
               "its transform: half size (32 of 64 ppem), at the glyph's place", (long)(m._11 * 100) );
        if (image) ID2D1Image_Release( image );
    }

    /* text drawn with color fonts on: the image */
    {
        IDWriteFontSetBuilder1 *builder;
        IDWriteFontSet *set;
        IDWriteFontCollection1 *coll = NULL;
        IDWriteTextFormat *format = NULL;
        ID2D1SolidColorBrush *brush;
        D2D1_COLOR_F black = { 0, 0, 0, 1 };
        D2D1_RECT_F rect = { 20.0f, 0.0f, 200.0f, 100.0f };
        IDWriteFactory5 *dw5;

        IDWriteFactory4_QueryInterface( dw, &IID_IDWriteFactory5_, (void **)&dw5 );
        IDWriteFactory5_CreateFontSetBuilder( dw5, &builder );
        IDWriteFontSetBuilder1_AddFontFile( builder, file );
        IDWriteFontSetBuilder1_CreateFontSet( builder, &set );
        IDWriteFactory5_CreateFontCollectionFromFontSet( dw5, set, &coll );
        hr = IDWriteFactory5_CreateTextFormat( dw5, L"SG Color Glyph Test", (IDWriteFontCollection *)coll,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 64.0f, L"en-us", &format );
        check( hr == S_OK, "a text format of the test font", hr );
        ID2D1RenderTarget_CreateSolidColorBrush( rt, &black, NULL, &brush );
        clear();
        ID2D1RenderTarget_BeginDraw( rt );
        ID2D1RenderTarget_DrawText( rt, L"\x2605", 1, format, &rect, (ID2D1Brush *)brush,
                D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, DWRITE_MEASURING_MODE_NATURAL );
        hr = ID2D1RenderTarget_EndDraw( rt, NULL, NULL );
        check( hr == S_OK && pixel( 50, 40 ) == 0xff0000, "DrawText with ENABLE_COLOR_FONT draws the image", pixel( 50, 40 ) );
        /* (an outline glyph of an sbix font is not drawn: FreeType takes sbix
         * fonts for bitmap fonts and leaves their outlines; the outline run of
         * a mixed run is checked above) */
    }

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
