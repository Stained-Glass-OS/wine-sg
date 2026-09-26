/* Direct2D geometry answers and command list bounds (wine-sg 0427).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <stdio.h>

static ID2D1Factory1 *factory;

static ID2D1PathGeometry *path( const D2D1_POINT_2F *pts, unsigned int n, bool closed )
{
    ID2D1PathGeometry *g;
    ID2D1GeometrySink *sink;
    factory->CreatePathGeometry( &g );
    g->Open( &sink );
    sink->BeginFigure( pts[0], D2D1_FIGURE_BEGIN_FILLED );
    sink->AddLines( pts + 1, n - 1 );
    sink->EndFigure( closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN );
    sink->Close();
    sink->Release();
    return g;
}

static ID2D1PathGeometry *rect_path( float l, float t, float r, float b )
{
    D2D1_POINT_2F p[4] = { { l, t }, { r, t }, { r, b }, { l, b } };
    return path( p, 4, true );
}

static const char *relation( ID2D1Geometry *a, ID2D1Geometry *b )
{
    static char buf[32];
    D2D1_GEOMETRY_RELATION rel = D2D1_GEOMETRY_RELATION_UNKNOWN;
    HRESULT hr = a->CompareWithGeometry( b, NULL, 0.25f, &rel );
    snprintf( buf, sizeof(buf), "%08lx %d", hr, (int)rel );
    return buf;
}

int main( void )
{
    D2D1_POINT_2F tri[3] = { { 0, 0 }, { 100, 0 }, { 0, 100 } }, line[2] = { { 0, 0 }, { 100, 0 } };
    D2D1_POINT_2F pt = { 0, 0 }, tan = { 0, 0 };
    D2D1_RECT_F r = { 0, 0, 0, 0 };
    ID2D1PathGeometry *g, *a, *b, *arc;
    ID2D1GeometrySink *sink;
    BOOL in;
    float v;
    HRESULT hr;

    D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), NULL, (void **)&factory );

    g = path( tri, 3, true );
    v = -1; hr = g->ComputeArea( NULL, 0.25f, &v );
    printf( "AREA %08lx %.1f\n", hr, v );
    v = -1; hr = g->ComputeLength( NULL, 0.25f, &v );
    printf( "LENGTH %08lx %.1f\n", hr, v );
    hr = g->ComputePointAtLength( 50.0f, NULL, 0.25f, &pt, &tan );
    printf( "POINTAT %08lx %.1f,%.1f tangent %.1f,%.1f\n", hr, pt.x, pt.y, tan.x, tan.y );
    in = FALSE; hr = g->FillContainsPoint( D2D1::Point2F( 10, 10 ), NULL, 0.25f, &in );
    printf( "INSIDE %08lx %d\n", hr, in );
    in = TRUE; hr = g->FillContainsPoint( D2D1::Point2F( 90, 90 ), NULL, 0.25f, &in );
    printf( "OUTSIDE %08lx %d\n", hr, in );
    in = FALSE; hr = g->StrokeContainsPoint( D2D1::Point2F( 50, 2 ), 6.0f, NULL, NULL, 0.25f, &in );
    printf( "ONSTROKE %08lx %d\n", hr, in );

    hr = path( line, 2, false )->GetWidenedBounds( 10.0f, NULL, NULL, 0.25f, &r );
    printf( "WIDENED %08lx %.1f,%.1f,%.1f,%.1f\n", hr, r.left, r.top, r.right, r.bottom );

    a = rect_path( 0, 0, 100, 100 );
    b = rect_path( 25, 25, 75, 75 );
    printf( "CONTAINS %s\n", relation( a, b ) );
    printf( "CONTAINED %s\n", relation( b, a ) );
    printf( "DISJOINT %s\n", relation( a, rect_path( 200, 200, 300, 300 ) ) );
    printf( "OVERLAP %s\n", relation( a, rect_path( 50, 50, 150, 150 ) ) );

    /* a half circle of radius 50 over (0,50)-(100,50) */
    factory->CreatePathGeometry( &arc );
    arc->Open( &sink );
    sink->BeginFigure( D2D1::Point2F( 0, 50 ), D2D1_FIGURE_BEGIN_FILLED );
    sink->AddArc( D2D1::ArcSegment( D2D1::Point2F( 100, 50 ), D2D1::SizeF( 50, 50 ), 0.0f,
            D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL ) );
    sink->EndFigure( D2D1_FIGURE_END_CLOSED );
    sink->Close();
    hr = arc->GetBounds( NULL, &r );
    printf( "ARCBOUNDS %08lx %.0f,%.0f,%.0f,%.0f\n", hr, r.left, r.top, r.right, r.bottom );
    v = -1; hr = arc->ComputeArea( NULL, 0.1f, &v );
    printf( "ARCAREA %08lx %.0f\n", hr, v );

    /* a command list's bounds, as an effect that takes it as input needs */
    {
        ID3D11Device *d3d;
        IDXGIDevice *dxgi;
        ID2D1Device *device;
        ID2D1DeviceContext *dc;
        ID2D1CommandList *list;
        ID2D1SolidColorBrush *brush;
        D2D1_RECT_F r1 = { 10, 20, 50, 60 }, r2 = { 70, 30, 90, 40 };

        if (FAILED(hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                NULL, 0, D3D11_SDK_VERSION, &d3d, NULL, NULL )))
        { printf( "NOD3D %08lx\n", hr ); return 0; }
        d3d->QueryInterface( __uuidof(IDXGIDevice), (void **)&dxgi );
        factory->CreateDevice( dxgi, &device );
        device->CreateDeviceContext( D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc );
        dc->CreateCommandList( &list );
        dc->SetTarget( list );
        dc->CreateSolidColorBrush( D2D1::ColorF( 1, 0, 0, 1 ), &brush );
        dc->BeginDraw();
        dc->FillRectangle( &r1, brush );
        dc->FillRectangle( &r2, brush );
        dc->EndDraw();
        list->Close();
        dc->SetTarget( NULL );
        r = D2D1::RectF( 0, 0, 0, 0 );
        hr = dc->GetImageLocalBounds( list, &r );
        printf( "LISTBOUNDS %08lx %.0f,%.0f,%.0f,%.0f\n", hr, r.left, r.top, r.right, r.bottom );
    }
    return 0;
}
