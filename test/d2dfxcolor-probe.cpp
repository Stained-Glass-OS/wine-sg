// d2dfxcolor-probe: Direct2D built-in effects that were drawn as their input (Stained Glass OS wine-sg 9509):
// Linear, Gamma, Table and Discrete Transfer (the Filter Effects spec's feComponentTransfer), Hue Rotation
// (feColorMatrix hueRotate), Luminance To Alpha and Exposure, each applied to a solid bitmap and compared
// with the colour the formula gives. Prints name=1/0 lines; see test/d2dfxcolor-gate.sh.
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

/* the discrete table {0.2, 0.5, 0.9}: entry floor(x * 3), the last for x = 1 */
static double red_k(double x)
{
    static const double t[3] = { 0.2, 0.5, 0.9 };
    int k = (int)floor(x * 3);
    return t[k > 2 ? 2 : k];
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

    fx = make(CLSID_D2D1LinearTransfer);
    if (fx)
    {
        fx->SetValue(D2D1_LINEARTRANSFER_PROP_RED_SLOPE, 0.5f);
        fx->SetValue(D2D1_LINEARTRANSFER_PROP_RED_Y_INTERCEPT, 0.1f);
        fx->SetValue(D2D1_LINEARTRANSFER_PROP_GREEN_SLOPE, 0.0f);
        fx->SetValue(D2D1_LINEARTRANSFER_PROP_GREEN_DISABLE, TRUE);
        fx->SetValue(D2D1_LINEARTRANSFER_PROP_BLUE_SLOPE, 2.0f);
    }
    run("linear_transfer", fx, pack(0.5 * R + 0.1, G, 2 * B, 1));

    fx = make(CLSID_D2D1GammaTransfer);
    if (fx)
    {
        fx->SetValue(D2D1_GAMMATRANSFER_PROP_RED_EXPONENT, 2.0f);
        fx->SetValue(D2D1_GAMMATRANSFER_PROP_GREEN_AMPLITUDE, 0.5f);
        fx->SetValue(D2D1_GAMMATRANSFER_PROP_GREEN_OFFSET, 0.25f);
        fx->SetValue(D2D1_GAMMATRANSFER_PROP_ALPHA_AMPLITUDE, 0.5f);
    }
    run("gamma_transfer", fx, pack(R * R, 0.5 * G + 0.25, B, 0.5));

    fx = make(CLSID_D2D1TableTransfer);
    if (fx)
    {
        float red[3] = { 0.0f, 1.0f, 0.0f }, blue[2] = { 1.0f, 0.0f };
        fx->SetValue(D2D1_TABLETRANSFER_PROP_RED_TABLE, (const BYTE *)red, sizeof(red));
        fx->SetValue(D2D1_TABLETRANSFER_PROP_BLUE_TABLE, (const BYTE *)blue, sizeof(blue));
    }
    {
        double x = R * 2, rr = x < 1 ? x : 1 - (x - 1);
        run("table_transfer", fx, pack(rr, G, 1 - B, 1));
    }

    fx = make(CLSID_D2D1DiscreteTransfer);
    if (fx)
    {
        float red[3] = { 0.2f, 0.5f, 0.9f }, green[2] = { 0.3f, 0.7f };
        fx->SetValue(D2D1_DISCRETETRANSFER_PROP_RED_TABLE, (const BYTE *)red, sizeof(red));
        fx->SetValue(D2D1_DISCRETETRANSFER_PROP_GREEN_TABLE, (const BYTE *)green, sizeof(green));
    }
    run("discrete_transfer", fx, pack(red_k(R), 0.3, B, 1));

    fx = make(CLSID_D2D1HueRotation);
    if (fx) fx->SetValue(0 /* Angle */, 90.0f);
    {
        double co = cos(M_PI / 2), si = sin(M_PI / 2);
        double r = (0.213 + 0.787 * co - 0.213 * si) * R + (0.715 - 0.715 * co - 0.715 * si) * G + (0.072 - 0.072 * co + 0.928 * si) * B;
        double g = (0.213 - 0.213 * co + 0.143 * si) * R + (0.715 + 0.285 * co + 0.140 * si) * G + (0.072 - 0.072 * co - 0.283 * si) * B;
        double b = (0.213 - 0.213 * co - 0.787 * si) * R + (0.715 - 0.715 * co + 0.715 * si) * G + (0.072 + 0.928 * co + 0.072 * si) * B;
        run("hue_rotation", fx, pack(r, g, b, 1));
    }

    fx = make(CLSID_D2D1LuminanceToAlpha);
    run("luminance_to_alpha", fx, pack(0, 0, 0, 0.2125 * R + 0.7154 * G + 0.0721 * B));

    fx = make(CLSID_D2D1Exposure);
    if (fx) fx->SetValue(D2D1_EXPOSURE_PROP_EXPOSURE_VALUE, -1.0f);
    run("exposure", fx, pack(R / 2, G / 2, B / 2, 1));
    return 0;
}
