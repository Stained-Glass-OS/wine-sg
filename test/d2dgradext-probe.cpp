// d2dgradext-probe: Direct2D gradient extend modes and gamma (Stained Glass OS wine-sg 9507). A gradient
// stop collection's WRAP and MIRROR extend modes repeat the gradient beyond its ends (they were drawn as
// CLAMP, "Ignoring extend mode"), and D2D1_GAMMA_1_0 interpolates the stop colours in linear light
// ("Ignoring gamma"). Linear and radial brushes, red at 0 to blue at 1. Prints name=1/0 lines with the
// pixels; see test/d2dgradext-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A00000C
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

static ID2D1DeviceContext *dc;
static ID2D1Bitmap1 *target, *readback;

static UINT32 pixel(int x, int y)
{
    D2D1_MAPPED_RECT m;
    D2D1_POINT_2U at = { 0, 0 };
    UINT32 v = 0xdeadbeef;
    dc->Flush(NULL, NULL);
    if (FAILED(readback->CopyFromBitmap(&at, target, NULL))) return v;
    if (SUCCEEDED(readback->Map(D2D1_MAP_OPTIONS_READ, &m)))
    {
        v = *(UINT32 *)(m.bits + y * m.pitch + x * 4);
        readback->Unmap();
    }
    return v;
}

static double to_linear(double c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
static double to_srgb(double c) { return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }

/* red -> blue at position p (already extended), as 0xffRRGGBB */
static UINT32 want_at(double p, bool linear_light)
{
    double r = 1.0 - p, b = p;
    if (p < 0) { r = 1; b = 0; }
    if (p > 1) { r = 0; b = 1; }
    if (linear_light)
    {
        r = to_srgb((1.0 - p) * to_linear(1.0) + p * to_linear(0.0));
        b = to_srgb(p * to_linear(1.0));
    }
    return 0xff000000 | ((UINT32)lround(r * 255) << 16) | (UINT32)lround(b * 255);
}

static int close_to(UINT32 got, UINT32 want)
{
    for (int s = 0; s < 32; s += 8)
        if (abs((int)((got >> s) & 0xff) - (int)((want >> s) & 0xff)) > 5) return 0;
    return 1;
}

static void expect(const char *name, int x, int y, UINT32 want)
{
    UINT32 got = pixel(x, y);
    printf("%s=%d at %d,%d got %08x want %08x\n", name, close_to(got, want), x, y, got, want);
}

static ID2D1GradientStopCollection *stops(D2D1_GAMMA gamma, D2D1_EXTEND_MODE mode)
{
    D2D1_GRADIENT_STOP s[2] = { { 0.0f, { 1, 0, 0, 1 } }, { 1.0f, { 0, 0, 1, 1 } } };
    ID2D1GradientStopCollection *c = NULL;
    dc->CreateGradientStopCollection(s, 2, gamma, mode, &c);
    return c;
}

static void linear(D2D1_GAMMA gamma, D2D1_EXTEND_MODE mode)
{
    ID2D1GradientStopCollection *c = stops(gamma, mode);
    ID2D1LinearGradientBrush *b = NULL;
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES p = { { 0, 0 }, { 32, 0 } };
    dc->CreateLinearGradientBrush(&p, NULL, c, &b);
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0, 1.0f));
    if (b) dc->FillRectangle(D2D1::RectF(0, 0, 64, 64), b);
    dc->EndDraw();
    if (b) b->Release();
    if (c) c->Release();
}

static void radial(D2D1_EXTEND_MODE mode)
{
    ID2D1GradientStopCollection *c = stops(D2D1_GAMMA_2_2, mode);
    ID2D1RadialGradientBrush *b = NULL;
    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES p = { { 32, 32 }, { 0, 0 }, 16, 16 };
    dc->CreateRadialGradientBrush(&p, NULL, c, &b);
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0, 1.0f));
    if (b) dc->FillRectangle(D2D1::RectF(0, 0, 64, 64), b);
    dc->EndDraw();
    if (b) b->Release();
    if (c) c->Release();
}

int main()
{
    ID3D11Device *d3d;
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                 D3D11_SDK_VERSION, &d3d, NULL, NULL))) { printf("no_d3d=1\n"); return 1; }
    IDXGIDevice *dxgi; d3d->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi);
    ID2D1Factory1 *factory;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), NULL, (void **)&factory)))
        { printf("no_factory=1\n"); return 1; }
    ID2D1Device *dev; factory->CreateDevice(dxgi, &dev);
    dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);
    D2D1_BITMAP_PROPERTIES1 p = {};
    p.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    p.dpiX = p.dpiY = 96.0f;
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    dc->CreateBitmap(D2D1::SizeU(64, 64), NULL, 0, &p, &target);
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    dc->CreateBitmap(D2D1::SizeU(64, 64), NULL, 0, &p, &readback);
    if (!target || !readback) { printf("no_bitmaps=1\n"); return 1; }
    dc->SetTarget(target);

    /* pixel x has its centre at x + 0.5: p = (x + 0.5) / 32 */
    linear(D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP);
    expect("linear_clamp_inside", 8, 4, want_at(8.5 / 32, false));
    expect("linear_clamp_beyond", 40, 4, want_at(2, false));
    linear(D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_WRAP);
    expect("linear_wrap_inside", 8, 4, want_at(8.5 / 32, false));
    expect("linear_wrap_beyond", 40, 4, want_at(40.5 / 32 - 1, false));
    linear(D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_MIRROR);
    expect("linear_mirror_beyond", 40, 4, want_at(2 - 40.5 / 32, false));
    expect("linear_mirror_far", 60, 4, want_at(2 - 60.5 / 32, false));
    linear(D2D1_GAMMA_1_0, D2D1_EXTEND_MODE_CLAMP);
    expect("linear_gamma10_middle", 16, 4, want_at(16.5 / 32, true));
    linear(D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP);
    expect("linear_gamma22_middle", 16, 4, want_at(16.5 / 32, false));

    /* radial, radius 16 at (32,32): 24 px out (x = 56) is 1.5 radii */
    radial(D2D1_EXTEND_MODE_CLAMP);
    expect("radial_clamp_beyond", 56, 32, want_at(2, false));
    radial(D2D1_EXTEND_MODE_WRAP);
    expect("radial_wrap_beyond", 56, 32, want_at(24.5 / 16 - 1, false));
    radial(D2D1_EXTEND_MODE_MIRROR);
    expect("radial_mirror_beyond", 56, 32, want_at(2 - 24.5 / 16, false));
    return 0;
}
