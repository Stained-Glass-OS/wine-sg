/* ID2D1PathGeometry1::ComputePointAndSegmentAtLength gives the point, its tangent, and the segment
 * and figure it lies on (patches/sg/8213). It returned E_NOTIMPL. */
#include <windows.h>
#include <d2d1_1.h>
#include <stdio.h>
#include <math.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  "); } else printf("ok    "); \
                           printf(__VA_ARGS__); printf("\n"); } while (0)

static int close_to( float a, float b ) { return fabsf( a - b ) < 0.05f; }

static void check_desc( ID2D1PathGeometry1 *geo, float length, UINT32 start, float x, float y, float tx, float ty,
                        UINT32 seg, UINT32 fig, float before, const char *what )
{
    D2D1_POINT_DESCRIPTION d;
    HRESULT hr;

    memset( &d, 0xcc, sizeof(d) );
    hr = geo->ComputePointAndSegmentAtLength( length, start, NULL, 0.0f, &d );
    CHECK( hr == S_OK && close_to( d.point.x, x ) && close_to( d.point.y, y ) && close_to( d.unitTangentVector.x, tx ) &&
           close_to( d.unitTangentVector.y, ty ) && d.endSegment == seg && d.endFigure == fig &&
           close_to( d.lengthToEndSegment, before ),
           "%s: hr %#lx point (%.2f,%.2f) tangent (%.2f,%.2f) segment %u figure %u before %.2f "
           "(want (%.2f,%.2f) (%.2f,%.2f) %u %u %.2f)", what, hr, d.point.x, d.point.y, d.unitTangentVector.x,
           d.unitTangentVector.y, d.endSegment, d.endFigure, d.lengthToEndSegment, x, y, tx, ty, seg, fig, before );
}

int main(void)
{
    D2D1_FACTORY_OPTIONS opts = { D2D1_DEBUG_LEVEL_NONE };
    ID2D1Factory1 *factory;
    ID2D1PathGeometry1 *geo;
    ID2D1GeometrySink *sink;
    D2D1_POINT_DESCRIPTION d;
    D2D1_BEZIER_SEGMENT bz;
    HRESULT hr;

    hr = D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &opts, (void **)&factory );
    if (FAILED(hr)) { printf( "FAIL  no ID2D1Factory1: %#lx\nRESULT: FAIL\n", hr ); return 1; }
    factory->CreatePathGeometry( &geo );
    geo->Open( &sink );
    /* figure 0: (0,0) -> (100,0) -> (100,50), closed (the closing line is segment 2, length 111.80) */
    sink->BeginFigure( D2D1::Point2F(0, 0), D2D1_FIGURE_BEGIN_FILLED );
    sink->AddLine( D2D1::Point2F(100, 0) );
    sink->AddLine( D2D1::Point2F(100, 50) );
    sink->EndFigure( D2D1_FIGURE_END_CLOSED );
    /* figure 1: a straight Bézier (200,0) -> (200,90), open (segment 3) */
    sink->BeginFigure( D2D1::Point2F(200, 0), D2D1_FIGURE_BEGIN_HOLLOW );
    bz.point1 = D2D1::Point2F(200, 30);
    bz.point2 = D2D1::Point2F(200, 60);
    bz.point3 = D2D1::Point2F(200, 90);
    sink->AddBezier( &bz );
    sink->EndFigure( D2D1_FIGURE_END_OPEN );
    sink->Close();
    sink->Release();

    check_desc( geo, 40.0f, 0, 40, 0, 1, 0, 0, 0, 0, "on the first line" );
    check_desc( geo, 120.0f, 0, 100, 20, 0, 1, 1, 0, 100, "on the second line" );
    check_desc( geo, 150.0f + 55.9017f, 0, 50, 25, -0.8944f, -0.4472f, 2, 0, 150, "on the closing line" );
    check_desc( geo, 150.0f + 111.8034f + 30.0f, 0, 200, 30, 0, 1, 3, 1, 261.8034f, "on the Bézier of figure 1" );
    check_desc( geo, 10.0f, 1, 100, 10, 0, 1, 1, 0, 0, "from segment 1" );
    check_desc( geo, 1000.0f, 0, 200, 90, 0, 1, 3, 1, 261.8034f, "beyond the end" );

    hr = geo->ComputePointAndSegmentAtLength( 1.0f, 9, NULL, 0.0f, &d );
    CHECK( FAILED(hr), "start segment past the end fails: %#lx", hr );

    geo->Release();

    /* a curved Bézier: the point agrees with ComputePointAtLength */
    {
        D2D1_POINT_2F pt, tan;
        factory->CreatePathGeometry( &geo );
        geo->Open( &sink );
        sink->BeginFigure( D2D1::Point2F(0, 0), D2D1_FIGURE_BEGIN_HOLLOW );
        bz.point1 = D2D1::Point2F(0, 50);
        bz.point2 = D2D1::Point2F(50, 100);
        bz.point3 = D2D1::Point2F(100, 100);
        sink->AddBezier( &bz );
        sink->EndFigure( D2D1_FIGURE_END_OPEN );
        sink->Close();
        sink->Release();
        geo->ComputePointAtLength( 60.0f, NULL, 0.0f, &pt, &tan );
        hr = geo->ComputePointAndSegmentAtLength( 60.0f, 0, NULL, 0.0f, &d );
        CHECK( hr == S_OK && fabsf( d.point.x - pt.x ) < 0.5f && fabsf( d.point.y - pt.y ) < 0.5f && d.endSegment == 0 &&
               fabsf( d.unitTangentVector.x * d.unitTangentVector.x + d.unitTangentVector.y * d.unitTangentVector.y - 1 ) < 0.01f,
               "curved Bézier: (%.2f,%.2f) vs ComputePointAtLength (%.2f,%.2f)", d.point.x, d.point.y, pt.x, pt.y );
        geo->Release();
    }
    factory->Release();
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
