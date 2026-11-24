// d2dstroke-probe: Direct2D stroke styles are drawn (Stained Glass OS wine-sg 9351). DrawLine, DrawRectangle
// and DrawGeometry with an ID2D1StrokeStyle drew a plain solid line before ("Ignoring stroke style"): no dashes,
// no dash offset, flat ends whatever the caps, mitred corners whatever the join. Black strokes on white;
// each check prints name=1/0 with the pixel. See test/d2dstroke-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A00000C
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <cstdio>
#include <cstdlib>

static ID2D1Factory1 *factory;
static ID2D1DeviceContext *dc;
static ID2D1Bitmap1 *target, *readback;
static ID2D1SolidColorBrush *black;

static UINT32 pixel(int x, int y)
{
    D2D1_MAPPED_RECT m;
    D2D1_POINT_2U at = { 0, 0 };
    UINT32 v = 0xdeadbeef;
    dc->Flush(NULL, NULL);
    if (FAILED(readback->CopyFromBitmap(&at, target, NULL))) return v;
    if (SUCCEEDED(readback->Map(D2D1_MAP_OPTIONS_READ, &m)))
    {
        v = *(UINT32 *)(m.bits + y * m.pitch + x * 4) & 0xffffff;
        readback->Unmap();
    }
    return v;
}

/* ink: dark (the stroke covers the pixel), paper: white */
static void expect(const char *name, int x, int y, bool ink)
{
    UINT32 got = pixel(x, y);
    int g = (got >> 8) & 0xff;
    bool ok = ink ? g < 64 : g > 192;
    printf("%s=%d at %d,%d got %06x want %s\n", name, ok, x, y, got, ink ? "ink" : "paper");
}

static ID2D1StrokeStyle *style(D2D1_CAP_STYLE start, D2D1_CAP_STYLE end, D2D1_CAP_STYLE dash_cap, D2D1_LINE_JOIN join,
        D2D1_DASH_STYLE dash, float offset, const float *dashes = NULL, UINT32 count = 0)
{
    D2D1_STROKE_STYLE_PROPERTIES p = { start, end, dash_cap, join, 10.0f, dash, offset };
    ID2D1StrokeStyle *s = NULL;
    factory->CreateStrokeStyle(&p, dashes, count, &s);
    return s;
}

static void begin()
{
    dc->BeginDraw();
    dc->SetTransform(D2D1::Matrix3x2F::Identity());
    dc->Clear(D2D1::ColorF(1, 1, 1, 1));
}

static void line(float x0, float y0, float x1, float y1, float w, ID2D1StrokeStyle *s)
{
    begin();
    dc->DrawLine(D2D1::Point2F(x0, y0), D2D1::Point2F(x1, y1), black, w, s);
    dc->EndDraw();
}

static void corner(float w, ID2D1StrokeStyle *s)
{
    /* a right angle pointing up at (60,60) */
    ID2D1PathGeometry *g; ID2D1GeometrySink *k;
    factory->CreatePathGeometry(&g);
    g->Open(&k);
    k->BeginFigure(D2D1::Point2F(20, 100), D2D1_FIGURE_BEGIN_HOLLOW);
    k->AddLine(D2D1::Point2F(60, 60));
    k->AddLine(D2D1::Point2F(100, 100));
    k->EndFigure(D2D1_FIGURE_END_OPEN);
    k->Close();
    begin();
    dc->DrawGeometry(g, black, w, s);
    dc->EndDraw();
    k->Release(); g->Release();
}

int main()
{
    ID3D11Device *d3d;
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                 D3D11_SDK_VERSION, &d3d, NULL, NULL))) { printf("no_d3d=1\n"); return 1; }
    IDXGIDevice *dxgi; d3d->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi);
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), NULL, (void **)&factory)))
        { printf("no_factory=1\n"); return 1; }
    ID2D1Device *dev; factory->CreateDevice(dxgi, &dev);
    dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);
    D2D1_BITMAP_PROPERTIES1 p = {};
    p.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    p.dpiX = p.dpiY = 96.0f;
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    dc->CreateBitmap(D2D1::SizeU(128, 128), NULL, 0, &p, &target);
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    dc->CreateBitmap(D2D1::SizeU(128, 128), NULL, 0, &p, &readback);
    if (!target || !readback) { printf("no_bitmaps=1\n"); return 1; }
    dc->SetTarget(target);
    dc->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &black);

    const D2D1_CAP_STYLE F = D2D1_CAP_STYLE_FLAT, R = D2D1_CAP_STYLE_ROUND, S = D2D1_CAP_STYLE_SQUARE;

    /* no style: the solid line, as before */
    line(8, 16, 120, 16, 4, NULL);
    expect("solid_plain", 20, 16, true);

    /* DASH: 2 on, 2 off in stroke widths -- width 4 from x = 8: ink 8-16, paper 16-24, ink 24-32 */
    ID2D1StrokeStyle *dash = style(F, F, F, D2D1_LINE_JOIN_MITER, D2D1_DASH_STYLE_DASH, 0);
    line(8, 16, 120, 16, 4, dash);
    expect("dash_first", 12, 16, true);
    expect("dash_gap", 20, 16, false);
    expect("dash_second", 28, 16, true);
    expect("dash_gap2", 36, 16, false);

    /* dash offset 1 stroke width: the line starts 4 px into the pattern -- ink 8-12, paper 12-20, ink 20-28 */
    ID2D1StrokeStyle *offs = style(F, F, F, D2D1_LINE_JOIN_MITER, D2D1_DASH_STYLE_DASH, 1.0f);
    line(8, 16, 120, 16, 4, offs);
    expect("offset_gap", 16, 16, false);
    expect("offset_dash", 24, 16, true);

    /* custom dashes {3, 1} at width 2: ink 8-14, paper 14-16, ink 16-22 */
    const float custom[2] = { 3.0f, 1.0f };
    ID2D1StrokeStyle *cust = style(F, F, F, D2D1_LINE_JOIN_MITER, D2D1_DASH_STYLE_CUSTOM, 0, custom, 2);
    line(8, 16, 120, 16, 2, cust);
    expect("custom_dash", 11, 16, true);
    expect("custom_gap", 14, 16, false);
    expect("custom_next", 18, 16, true);

    /* DOT with round dash caps, width 6: dots (no length) every 12 px, a disc of radius 3 each */
    ID2D1StrokeStyle *dot = style(F, F, R, D2D1_LINE_JOIN_MITER, D2D1_DASH_STYLE_DOT, 0);
    line(8, 16, 120, 16, 6, dot);
    expect("dot_round", 32, 16, true);
    expect("dot_between", 38, 16, false);

    /* round and square caps, width 10 from x = 40 to 80: 3 px beyond the start is inside the cap */
    ID2D1StrokeStyle *round = style(R, R, F, D2D1_LINE_JOIN_MITER, D2D1_DASH_STYLE_SOLID, 0);
    line(40, 40, 80, 40, 10, round);
    expect("round_cap", 37, 40, true);
    expect("round_cap_corner", 36, 36, false);
    expect("round_cap_end", 83, 40, true);
    ID2D1StrokeStyle *square = style(S, S, F, D2D1_LINE_JOIN_MITER, D2D1_DASH_STYLE_SOLID, 0);
    line(40, 40, 80, 40, 10, square);
    expect("square_cap_corner", 36, 36, true);
    expect("square_cap_beyond", 33, 40, false);
    line(40, 40, 80, 40, 10, NULL);
    expect("flat_cap", 37, 40, false);

    /* joins at a right angle, width 16: the miter reaches 11.3 px above the vertex, the bevel 5.7 */
    corner(16, NULL);
    expect("join_miter_default", 60, 50, true);
    ID2D1StrokeStyle *bevel = style(F, F, F, D2D1_LINE_JOIN_BEVEL, D2D1_DASH_STYLE_SOLID, 0);
    corner(16, bevel);
    expect("join_bevel", 60, 51, false);
    expect("join_bevel_inside", 60, 56, true);
    ID2D1StrokeStyle *rjoin = style(F, F, F, D2D1_LINE_JOIN_ROUND, D2D1_DASH_STYLE_SOLID, 0);
    corner(16, rjoin);
    expect("join_round_in", 60, 53, true);
    expect("join_round_out", 60, 50, false);

    /* a dashed rectangle: DrawRectangle takes the style too (width 2, DASH: 4 on 4 off from the corner) */
    begin();
    dc->DrawRectangle(D2D1::RectF(16, 64, 112, 112), black, 2, dash);
    dc->EndDraw();
    expect("rect_dash", 18, 64, true);
    expect("rect_gap", 22, 64, false);

    /* a fixed stroke keeps its width under a scale: 2 px, not 8 */
    D2D1_STROKE_STYLE_PROPERTIES1 fp = { F, F, F, D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_SOLID, 0,
                                         D2D1_STROKE_TRANSFORM_TYPE_FIXED };
    ID2D1StrokeStyle1 *fixed = NULL;
    factory->CreateStrokeStyle(&fp, NULL, 0, &fixed);
    begin();
    dc->SetTransform(D2D1::Matrix3x2F::Scale(4, 4));
    dc->DrawLine(D2D1::Point2F(2, 25), D2D1::Point2F(30, 25), black, 2, fixed);
    dc->EndDraw();
    expect("fixed_on", 60, 100, true);
    expect("fixed_thin", 60, 103, false);
    return 0;
}
