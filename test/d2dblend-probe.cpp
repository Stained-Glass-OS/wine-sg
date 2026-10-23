// d2dblend-probe: Direct2D blend and composite modes, BlendImage, and effects' invalid and required
// rectangles (Stained Glass OS wine-sg 6503). Prints name=value lines; see test/d2dblend-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A00000C
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effects.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

static ID2D1DeviceContext6 *dc;
static ID2D1Bitmap1 *target, *readback;

static ID2D1Bitmap1 *solid(UINT w, UINT h, BYTE b, BYTE g, BYTE r, BYTE a)
{
    UINT32 *px = (UINT32 *)malloc(w * h * 4);
    for (UINT i = 0; i < w * h; i++) px[i] = (a << 24) | (r << 16) | (g << 8) | b;
    D2D1_BITMAP_PROPERTIES1 p = {};
    p.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    p.dpiX = p.dpiY = 96.0f;
    ID2D1Bitmap1 *bmp = NULL;
    dc->CreateBitmap(D2D1::SizeU(w, h), px, w * 4, &p, &bmp);
    free(px);
    return bmp;
}

/* the target's pixel at x, y: 0xAARRGGBB */
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

static int close_to(UINT32 got, UINT32 want)
{
    for (int s = 0; s < 32; s += 8)
        if (abs((int)((got >> s) & 0xff) - (int)((want >> s) & 0xff)) > 3) return 0;
    return 1;
}

static int near_rect(const D2D1_RECT_F &r, float l, float t, float rr, float b)
{
    return fabsf(r.left - l) < 0.01f && fabsf(r.top - t) < 0.01f && fabsf(r.right - rr) < 0.01f && fabsf(r.bottom - b) < 0.01f;
}

static UINT32 two_inputs(const GUID &clsid, UINT32 index, D2D1_PROPERTY_TYPE type, const void *value, UINT32 size,
                         ID2D1Bitmap1 *dest, ID2D1Bitmap1 *src)
{
    ID2D1Effect *fx = NULL;
    if (FAILED(dc->CreateEffect(clsid, &fx))) return 0xdeadbeef;
    fx->SetInput(0, dest);
    fx->SetInput(1, src);
    if (value) fx->SetValue(index, type, (const BYTE *)value, size);
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0, 0.0f));
    dc->DrawImage(fx);
    dc->EndDraw();
    fx->Release();
    return pixel(8, 8);
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
    ID2D1DeviceContext *dc0; dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc0);
    if (FAILED(dc0->QueryInterface(__uuidof(ID2D1DeviceContext6), (void **)&dc))) { printf("no_dc6=1\n"); return 1; }
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

    ID2D1Bitmap1 *gray = solid(16, 16, 128, 128, 128, 255), *red = solid(16, 16, 0, 0, 255, 255);
    ID2D1Bitmap1 *opaque_red = red, *half_blue = solid(16, 16, 128, 0, 0, 128);

    /* the Blend effect: destination gray, source red */
    struct { const char *name; UINT32 mode; UINT32 want; } blends[] = {
        { "multiply", D2D1_BLEND_MODE_MULTIPLY, 0xff800000 },
        { "screen", D2D1_BLEND_MODE_SCREEN, 0xffff8080 },
        { "darken", D2D1_BLEND_MODE_DARKEN, 0xff800000 },
        { "lighten", D2D1_BLEND_MODE_LIGHTEN, 0xffff8080 },
        { "difference", D2D1_BLEND_MODE_DIFFERENCE, 0xff7f8080 },
        { "exclusion", D2D1_BLEND_MODE_EXCLUSION, 0xff7f8080 },
        { "subtract", D2D1_BLEND_MODE_SUBTRACT, 0xff008080 },
        { "linear_dodge", D2D1_BLEND_MODE_LINEAR_DODGE, 0xffff8080 },
        { "color", D2D1_BLEND_MODE_COLOR, 0xffff4a4a },
    };
    for (auto &b : blends)
    {
        UINT32 got = two_inputs(CLSID_D2D1Blend, D2D1_BLEND_PROP_MODE, D2D1_PROPERTY_TYPE_ENUM, &b.mode, 4, gray, red);
        printf("blend_%s=%d got %08x want %08x\n", b.name, close_to(got, b.want), got, b.want);
    }

    /* the Composite effect: destination opaque red, source half-transparent blue */
    struct { const char *name; UINT32 mode; UINT32 want; } comps[] = {
        { "source_over", D2D1_COMPOSITE_MODE_SOURCE_OVER, 0xff7f0080 },
        { "source_in", D2D1_COMPOSITE_MODE_SOURCE_IN, 0x80000080 },
        { "destination_out", D2D1_COMPOSITE_MODE_DESTINATION_OUT, 0x7f7f0000 },
        { "xor", D2D1_COMPOSITE_MODE_XOR, 0x7f7f0000 },
        { "source_copy", D2D1_COMPOSITE_MODE_SOURCE_COPY, 0x80000080 },
    };
    for (auto &c : comps)
    {
        UINT32 got = two_inputs(CLSID_D2D1Composite, D2D1_COMPOSITE_PROP_MODE, D2D1_PROPERTY_TYPE_ENUM, &c.mode, 4,
                                opaque_red, half_blue);
        printf("composite_%s=%d got %08x want %08x\n", c.name, close_to(got, c.want), got, c.want);
    }

    /* Arithmetic Composite: destination + source */
    {
        D2D1_VECTOR_4F k = { 0.0f, 1.0f, 1.0f, 0.0f };
        UINT32 got = two_inputs(CLSID_D2D1ArithmeticComposite, D2D1_ARITHMETICCOMPOSITE_PROP_COEFFICIENTS,
                                D2D1_PROPERTY_TYPE_VECTOR4, &k, sizeof(k), gray, red);
        printf("arithmetic_sum=%d got %08x\n", close_to(got, 0xffff8080), got);
    }

    /* (the 2D affine transform alone, as BlendImage moves its image) */
    {
        ID2D1Effect *mv; dc->CreateEffect(CLSID_D2D12DAffineTransform, &mv);
        D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Translation(8.0f, 8.0f);
        mv->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX, m);
        mv->SetInput(0, red);
        dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(mv); dc->EndDraw();
        UINT32 a = pixel(4, 4), b = pixel(12, 12);
        printf("affine_moves=%d got %08x %08x\n", a == 0 && close_to(b, 0xffff0000), a, b);
        mv->Release();
    }

    /* BlendImage: red multiplied into a gray target at 8,8 */
    {
        D2D1_POINT_2F at = { 8.0f, 8.0f };
        dc->BeginDraw();
        dc->Clear(D2D1::ColorF(128 / 255.0f, 128 / 255.0f, 128 / 255.0f, 1.0f));
        dc->BlendImage(red, D2D1_BLEND_MODE_MULTIPLY, &at, NULL, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        HRESULT hr = dc->EndDraw();
        UINT32 in = pixel(12, 12), out = pixel(40, 40), edge = pixel(4, 4);
        printf("blendimage_inside=%d got %08x\n", close_to(in, 0xff800000), in);
        printf("blendimage_outside=%d got %08x %08x hr %#lx\n", close_to(out, 0xff808080) && close_to(edge, 0xff808080), out, edge,
               (long)hr);
    }

    /* GetEffectRequiredInputRectangles */
    {
        ID2D1Effect *blur; dc->CreateEffect(CLSID_D2D1GaussianBlur, &blur);
        float sigma = 2.0f;
        blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma);
        blur->SetInput(0, gray);
        D2D1_EFFECT_INPUT_DESCRIPTION d = { blur, 0, D2D1::RectF(0, 0, 64, 64) };
        D2D1_RECT_F out = D2D1::RectF(10, 10, 20, 20), in = {};
        HRESULT hr = dc->GetEffectRequiredInputRectangles(blur, &out, &d, &in, 1);
        printf("required_blur=%d hr %#lx in %.0f,%.0f,%.0f,%.0f\n", SUCCEEDED(hr) && near_rect(in, 4, 4, 26, 26), (long)hr,
               in.left, in.top, in.right, in.bottom);
        hr = dc->GetEffectRequiredInputRectangles(blur, &out, &d, &in, 2);
        printf("required_count_checked=%d hr %#lx\n", hr == E_INVALIDARG, (long)hr);

        ID2D1Effect *crop; dc->CreateEffect(CLSID_D2D1Crop, &crop);
        D2D1_VECTOR_4F r = { 0, 0, 12, 12 };
        crop->SetValue(D2D1_CROP_PROP_RECT, r);
        crop->SetInput(0, gray);
        D2D1_EFFECT_INPUT_DESCRIPTION dc2 = { crop, 0, D2D1::RectF(0, 0, 16, 16) };
        out = D2D1::RectF(5, 5, 30, 30);
        hr = dc->GetEffectRequiredInputRectangles(crop, &out, &dc2, &in, 1);
        printf("required_crop=%d hr %#lx in %.0f,%.0f,%.0f,%.0f\n", SUCCEEDED(hr) && near_rect(in, 5, 5, 12, 12), (long)hr,
               in.left, in.top, in.right, in.bottom);

        /* GetEffectInvalidRectangleCount / Rectangles: a new effect is all invalid */
        UINT32 n = 99;
        D2D1_RECT_F rects[4];
        ID2D1Effect *b1; dc->CreateEffect(CLSID_D2D1GaussianBlur, &b1);
        b1->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 1.0f);
        b1->SetInput(0, gray);
        dc->GetEffectInvalidRectangleCount(b1, &n);
        hr = dc->GetEffectInvalidRectangles(b1, rects, n);
        printf("invalid_new=%d n %u %.0f,%.0f,%.0f,%.0f\n", n == 1 && near_rect(rects[0], -3, -3, 19, 19), n,
               rects[0].left, rects[0].top, rects[0].right, rects[0].bottom);
        dc->BeginDraw(); dc->DrawImage(b1); dc->EndDraw();
        dc->GetEffectInvalidRectangleCount(b1, &n);
        printf("invalid_drawn=%d n %u\n", n == 0, n);
        D2D1_RECT_F part = D2D1::RectF(2, 2, 3, 3);
        hr = dc->InvalidateEffectInputRectangle(b1, 0, &part);
        dc->GetEffectInvalidRectangleCount(b1, &n);
        dc->GetEffectInvalidRectangles(b1, rects, n < 4 ? n : 4);
        printf("invalid_input=%d hr %#lx n %u %.0f,%.0f,%.0f,%.0f\n", SUCCEEDED(hr) && n == 1 && near_rect(rects[0], -1, -1, 6, 6),
               (long)hr, n, rects[0].left, rects[0].top, rects[0].right, rects[0].bottom);
        hr = dc->InvalidateEffectInputRectangle(b1, 1, &part);
        printf("invalid_bad_input=%d hr %#lx\n", hr == E_INVALIDARG, (long)hr);

        /* carried through an effect drawn on top of it */
        ID2D1Effect *b2; dc->CreateEffect(CLSID_D2D1GaussianBlur, &b2);
        b2->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 1.0f);
        b2->SetInputEffect(0, b1);
        dc->BeginDraw(); dc->DrawImage(b2); dc->EndDraw();
        dc->GetEffectInvalidRectangleCount(b2, &n);
        UINT32 n1 = 9; dc->GetEffectInvalidRectangleCount(b1, &n1);
        printf("invalid_chain_drawn=%d n %u inner %u\n", n == 0 && n1 == 0, n, n1);
        dc->InvalidateEffectInputRectangle(b1, 0, &part);
        dc->GetEffectInvalidRectangleCount(b2, &n);
        dc->GetEffectInvalidRectangles(b2, rects, n < 4 ? n : 4);
        printf("invalid_chain=%d n %u %.0f,%.0f,%.0f,%.0f\n", n == 1 && near_rect(rects[0], -4, -4, 9, 9), n,
               rects[0].left, rects[0].top, rects[0].right, rects[0].bottom);
        b1->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 2.0f);
        dc->GetEffectInvalidRectangleCount(b1, &n);
        printf("invalid_property=%d n %u\n", n == 1, n);
    }
    printf("done=1\n");
    return 0;
}
