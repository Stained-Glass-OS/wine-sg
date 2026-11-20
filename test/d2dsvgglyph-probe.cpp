/* d2dsvgglyph-probe: ID2D1DeviceContext4::DrawSvgGlyphRun draws a glyph's OpenType SVG document, a glyph
 * without one as an outline in the default fill brush, and GetSvgGlyphImage gives the SVG glyph as a command
 * list (Stained Glass OS wine-sg 8227). They were stubs, and dwrite read the SVG table header unpacked (no
 * SVG glyph data at all). argv[1]: test/d2dsvgglyph-font.py's font.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define NTDDI_VERSION 0x0A000006
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <stdio.h>

static int failures;
static void check( bool ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

static bool close_to( UINT32 px, int r, int g, int b )
{
    return abs( (int)((px >> 16) & 0xff) - r ) < 40 && abs( (int)((px >> 8) & 0xff) - g ) < 40 && abs( (int)(px & 0xff) - b ) < 40;
}

int main( int argc, char **argv )
{
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *factory = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc0 = NULL;
    ID2D1DeviceContext4 *dc = NULL;
    ID2D1Bitmap1 *target = NULL, *readback = NULL;
    ID2D1SolidColorBrush *blue = NULL;
    IDWriteFactory *dwrite = NULL;
    IDWriteFontFile *file = NULL;
    IDWriteFontFace *face = NULL;
    ID2D1CommandList *list = NULL;
    D2D1_MATRIX_3X2_F glyph_transform;
    WCHAR path[MAX_PATH];
    UINT32 codes[2] = { 'A', 'B' };
    UINT16 glyphs[2] = { 0, 0 };
    D2D1_BITMAP_PROPERTIES1 props = {};
    D2D1_MAPPED_RECT mapped;
    D2D1_POINT_2U zero = { 0, 0 };
    D2D1_RECT_U all = { 0, 0, 64, 64 };
    HRESULT hr;

    if (argc < 2) return 1;
    MultiByteToWideChar( CP_ACP, 0, argv[1], -1, path, MAX_PATH );
    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                            D3D11_SDK_VERSION, &d3d, NULL, NULL );
    if (FAILED(hr)) { printf( "SKIP  no d3d11 device (%#lx)\nRESULT: PASS\n", hr ); return 0; }
    d3d->QueryInterface( __uuidof(IDXGIDevice), (void **)&dxgi );
    hr = D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), NULL, (void **)&factory );
    check( SUCCEEDED(hr), "ID2D1Factory1", hr );
    if (FAILED(hr)) goto done;
    factory->CreateDevice( dxgi, &device );
    device->CreateDeviceContext( D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc0 );
    if (dc0) dc0->QueryInterface( __uuidof(ID2D1DeviceContext4), (void **)&dc );
    check( dc != NULL, "ID2D1DeviceContext4", 0 );
    if (!dc) goto done;

    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX = props.dpiY = 96.0f;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    dc->CreateBitmap( D2D1::SizeU( 64, 64 ), NULL, 0, &props, &target );
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    dc->CreateBitmap( D2D1::SizeU( 64, 64 ), NULL, 0, &props, &readback );
    dc->CreateSolidColorBrush( D2D1::ColorF( 0.0f, 0.0f, 1.0f, 1.0f ), &blue );

    DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&dwrite );
    hr = dwrite->CreateFontFileReference( path, NULL, &file );
    if (SUCCEEDED(hr)) hr = dwrite->CreateFontFace( DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face );
    check( SUCCEEDED(hr), "the test font loads", hr );
    if (FAILED(hr)) goto done;
    face->GetGlyphIndices( codes, 2, glyphs );
    {
        /* dwrite gives the glyph's SVG document (its SVG table header is packed: the document list at offset 2) */
        IDWriteFontFace4 *face4 = NULL;
        DWRITE_GLYPH_IMAGE_DATA data = {};
        void *ctx = NULL;
        face->QueryInterface( __uuidof(IDWriteFontFace4), (void **)&face4 );
        hr = face4 ? face4->GetGlyphImageData( glyphs[0], 1000, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &data, &ctx ) : E_NOINTERFACE;
        check( SUCCEEDED(hr) && data.imageDataSize > 4 && !memcmp( data.imageData, "<svg", 4 ),
               "IDWriteFontFace4::GetGlyphImageData gives the SVG document", data.imageDataSize );
        if (face4) { face4->ReleaseGlyphImageData( ctx ); face4->Release(); }
    }

    {
        DWRITE_GLYPH_RUN run = {};
        run.fontFace = face;
        run.fontEmSize = 40.0f;
        run.glyphCount = 2;
        run.glyphIndices = glyphs;
        dc->SetTarget( target );
        dc->BeginDraw();
        dc->Clear( D2D1::ColorF( 1.0f, 1.0f, 1.0f, 1.0f ) );
        dc->DrawSvgGlyphRun( D2D1::Point2F( 10.0f, 50.0f ), &run, blue, NULL, 0, DWRITE_MEASURING_MODE_NATURAL );
        hr = dc->EndDraw();
        check( SUCCEEDED(hr), "EndDraw after DrawSvgGlyphRun", hr );
    }
    readback->CopyFromBitmap( &zero, target, &all );
    if (SUCCEEDED(readback->Map( D2D1_MAP_OPTIONS_READ, &mapped )))
    {
        UINT32 a = *(UINT32 *)(mapped.bits + 40 * mapped.pitch + 20 * 4);   /* inside A's SVG rectangle */
        UINT32 b = *(UINT32 *)(mapped.bits + 40 * mapped.pitch + 45 * 4);   /* inside B's outline square */
        UINT32 w = *(UINT32 *)(mapped.bits + 5 * mapped.pitch + 60 * 4);    /* outside both */
        printf( "      A %08x B %08x outside %08x\n", a, b, w );
        check( close_to( a, 255, 0, 0 ), "the SVG glyph is drawn (red rectangle)", a );
        check( close_to( b, 0, 0, 255 ), "a glyph without SVG is drawn with the default fill brush", b );
        check( close_to( w, 255, 255, 255 ), "nothing outside the glyphs", w );
        readback->Unmap();
    }
    else check( false, "map the read-back bitmap", 0 );

    hr = dc->GetSvgGlyphImage( D2D1::Point2F( 0.0f, 0.0f ), face, 40.0f, glyphs[0], FALSE, NULL, blue, NULL, 0,
                               &glyph_transform, &list );
    check( hr == S_OK && list, "GetSvgGlyphImage of the SVG glyph gives a command list", hr );
    if (list) list->Release();
    list = NULL;
    hr = dc->GetSvgGlyphImage( D2D1::Point2F( 0.0f, 0.0f ), face, 40.0f, glyphs[1], FALSE, NULL, blue, NULL, 0,
                               &glyph_transform, &list );
    check( hr == DWRITE_E_NOCOLOR && !list, "GetSvgGlyphImage of a glyph without SVG: DWRITE_E_NOCOLOR", hr );

done:
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
