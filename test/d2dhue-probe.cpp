// d2dhue-probe: the Direct2D RGB To Hue and Hue To RGB effects (Stained Glass OS wine-sg 9352), drawn as their
// input before ("Built-in effect {23f3e5ec-...} is drawn as its input", seen from Paint.NET 5.2): hue,
// saturation and value (HSV) or lightness (HSL) in the colour channels, each 0 to 1, and back. A solid
// bitmap through each, compared with the colour the formula gives. Prints name=1/0 lines; see
// test/d2dhue-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A00000C
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

static ID2D1DeviceContext *dc;
static ID2D1Bitmap1 *target, *readback, *source;

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

/* straight r, g, b, a in 0..1 to the premultiplied 0xAARRGGBB a target holds */
static UINT32 pack(double r, double g, double b, double a)
{
    auto q = [](double v) { v = v < 0 ? 0 : v > 1 ? 1 : v; return (UINT32)lround(v * 255); };
    a = a < 0 ? 0 : a > 1 ? 1 : a;
    return (q(a) << 24) | (q(r * a) << 16) | (q(g * a) << 8) | q(b * a);
}

static void run(const char *name, ID2D1Effect *fx, UINT32 want)
{
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0, 0.0f));
    if (fx) dc->DrawImage(fx);
    dc->EndDraw();
    UINT32 got = pixel(4, 4);
    int ok = 1;
    for (int s = 0; s < 32; s += 8)
        if (abs((int)((got >> s) & 0xff) - (int)((want >> s) & 0xff)) > 3) ok = 0;
    printf("%s=%d got %08x want %08x\n", name, ok, got, want);
    if (fx) fx->Release();
}

static ID2D1Effect *make(const GUID &clsid)
{
    ID2D1Effect *fx = NULL;
    if (SUCCEEDED(dc->CreateEffect(clsid, &fx))) fx->SetInput(0, source);
    return fx;
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
    dc->CreateBitmap(D2D1::SizeU(16, 16), NULL, 0, &p, &target);
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    dc->CreateBitmap(D2D1::SizeU(16, 16), NULL, 0, &p, &readback);
    if (!target || !readback) { printf("no_bitmaps=1\n"); return 1; }
    dc->SetTarget(target);

    /* the source: straight (0.8, 0.4, 0.2), opaque */
    const double R = 204 / 255.0, G = 102 / 255.0, B = 51 / 255.0;
    UINT32 px[16 * 16];
    for (int i = 0; i < 256; i++) px[i] = 0xff000000 | (204 << 16) | (102 << 8) | 51;
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
    dc->CreateBitmap(D2D1::SizeU(16, 16), px, 16 * 4, &p, &source);

    ID2D1Effect *fx;
    run("passthrough_check", NULL, 0);

    /* (0.8, 0.4, 0.2): HSV hue (0.4 - 0.2) / 0.6 / 6, saturation 0.6 / 0.8, value 0.8 */
    fx = make(CLSID_D2D1RgbToHue);
    run("rgb_to_hsv", fx, pack((G - B) / (R - B) / 6, (R - B) / R, R, 1));
    /* HSL: lightness (0.8 + 0.2) / 2, saturation 0.6 / (1 - |2 l - 1|) */
    fx = make(CLSID_D2D1RgbToHue);
    if (fx) fx->SetValue(D2D1_RGBTOHUE_PROP_OUTPUT_COLOR_SPACE, D2D1_RGBTOHUE_OUTPUT_COLOR_SPACE_HUE_SATURATION_LIGHTNESS);
    run("rgb_to_hsl", fx, pack((G - B) / (R - B) / 6, (R - B) / (1 - fabs(R + B - 1)), (R + B) / 2, 1));

    /* the source read as HSV (hue 0.8 = 288 degrees, saturation 0.4, value 0.2): chroma 0.08 over 0.12 */
    fx = make(CLSID_D2D1HueToRgb);
    {
        double c = B * G, m0 = B - c, h = R * 6;
        double kr = fmin(fmax(fabs(h - 3) - 1, 0), 1), kg = fmin(fmax(2 - fabs(h - 2), 0), 1), kb = fmin(fmax(2 - fabs(h - 4), 0), 1);
        run("hsv_to_rgb", fx, pack(kr * c + m0, kg * c + m0, kb * c + m0, 1));
    }

    /* there and back, HSL both ways: the source again */
    {
        ID2D1Effect *a = make(CLSID_D2D1RgbToHue), *b = NULL;
        dc->CreateEffect(CLSID_D2D1HueToRgb, &b);
        if (a && b)
        {
            a->SetValue(D2D1_RGBTOHUE_PROP_OUTPUT_COLOR_SPACE, D2D1_RGBTOHUE_OUTPUT_COLOR_SPACE_HUE_SATURATION_LIGHTNESS);
            b->SetValue(D2D1_HUETORGB_PROP_INPUT_COLOR_SPACE, D2D1_HUETORGB_INPUT_COLOR_SPACE_HUE_SATURATION_LIGHTNESS);
            b->SetInputEffect(0, a);
            a->Release();
        }
        run("hsl_round_trip", b, pack(R, G, B, 1));
    }
    return 0;
}
