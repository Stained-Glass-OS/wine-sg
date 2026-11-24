// d2dfxres-probe: two things Paint.NET 5.2's adjustments need from Direct2D effects (Stained Glass OS wine-sg
// 9354, 9355).
//  - The Histogram effect's HistogramOutput (9354): drawing it fills NumBins floats, each the fraction of the
//    input's straight pixels whose selected channel falls in that bin (as Win2D's ComputeHistogram tests read
//    it); it was drawn as its input and the output stayed empty.
//  - A custom effect's resource textures (9355): ID2D1DrawInfo::SetResourceTexture puts a lookup table at a
//    shader register; it was never bound, so a lookup read zeros (Paint.NET's Posterize preview went
//    transparent, Curves black).
// Prints name=1/0 lines; see test/d2dhisto-gate.sh and test/d2dlut-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A00000C
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <d2d1effectauthor.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

DEFINE_GUID(CLSID_ProbeLut, 0x6a1f20d3,0x55b1,0x4f0e,0x8a,0x22,0x1c,0x93,0x27,0x41,0xd0,0x01);
DEFINE_GUID(probe_lut_shader, 0x6a1f20d3,0x55b1,0x4f0e,0x8a,0x22,0x1c,0x93,0x27,0x41,0xd0,0x02);
DEFINE_GUID(probe_lut_texture, 0x6a1f20d3,0x55b1,0x4f0e,0x8a,0x22,0x1c,0x93,0x27,0x41,0xd0,0x03);

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

/* the lookup: a one-input draw transform whose shader reads a 1D resource texture at t1 */
static const char lut_ps[] =
    "Texture2D t0 : register(t0); SamplerState s0 : register(s0);\n"
    "Texture1D<float4> lut : register(t1); SamplerState ls : register(s1);\n"
    "float4 main(float4 pos : SV_POSITION, float4 scene : SCENE_POSITION, float4 uv0 : TEXCOORD0) : SV_TARGET\n"
    "{ float4 c = t0.Sample(s0, uv0.xy); return lut.Sample(ls, c.r * 0.75 + 0.125); }\n";

class LutTransform : public ID2D1DrawTransform
{
public:
    LONG ref = 1;
    ID2D1ResourceTexture *texture = NULL;
    STDMETHOD(QueryInterface)(REFIID iid, void **out)
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1TransformNode) || iid == __uuidof(ID2D1Transform)
                || iid == __uuidof(ID2D1DrawTransform)) { *out = this; AddRef(); return S_OK; }
        *out = NULL; return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&ref); }
    STDMETHOD_(ULONG, Release)() { return InterlockedDecrement(&ref); }
    STDMETHOD_(UINT32, GetInputCount)() CONST { return 1; }
    STDMETHOD(MapOutputRectToInputRects)(CONST D2D1_RECT_L *out, D2D1_RECT_L *in, UINT32 count) CONST
    { if (count) in[0] = *out; return S_OK; }
    STDMETHOD(MapInputRectsToOutputRect)(CONST D2D1_RECT_L *in, CONST D2D1_RECT_L *opaque_in, UINT32 count,
            D2D1_RECT_L *out, D2D1_RECT_L *opaque_out)
    { *out = count ? in[0] : D2D1_RECT_L{}; memset(opaque_out, 0, sizeof(*opaque_out)); return S_OK; }
    STDMETHOD(MapInvalidRect)(UINT32 index, D2D1_RECT_L in, D2D1_RECT_L *out) CONST { *out = in; return S_OK; }
    STDMETHOD(SetDrawInfo)(ID2D1DrawInfo *info)
    {
        HRESULT hr = info->SetPixelShader(probe_lut_shader, D2D1_PIXEL_OPTIONS_NONE);
        if (SUCCEEDED(hr)) hr = info->SetResourceTexture(1, texture);
        return hr;
    }
};

static LutTransform lut_transform;

class LutEffect : public ID2D1EffectImpl
{
public:
    LONG ref = 1;
    STDMETHOD(QueryInterface)(REFIID iid, void **out)
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1EffectImpl)) { *out = this; AddRef(); return S_OK; }
        *out = NULL; return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&ref); }
    STDMETHOD_(ULONG, Release)() { ULONG r = InterlockedDecrement(&ref); if (!r) delete this; return r; }
    STDMETHOD(Initialize)(ID2D1EffectContext *context, ID2D1TransformGraph *graph)
    {
        /* red, green, blue, white: what a red value of 0, 1/3, 2/3 and 1 looks up */
        static const BYTE texels[16] = { 255, 0, 0, 255,  0, 255, 0, 255,  0, 0, 255, 255,  255, 255, 255, 255 };
        static const UINT32 extent = 4;
        static const D2D1_EXTEND_MODE clamp = D2D1_EXTEND_MODE_CLAMP;
        D2D1_RESOURCE_TEXTURE_PROPERTIES props = { &extent, 1, D2D1_BUFFER_PRECISION_8BPC_UNORM, D2D1_CHANNEL_DEPTH_4,
                D2D1_FILTER_MIN_MAG_MIP_POINT, &clamp };
        ID3DBlob *code = NULL;
        HRESULT hr;

        if (FAILED(hr = D3DCompile(lut_ps, sizeof(lut_ps) - 1, NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &code, NULL)))
        { printf("no_compiler=1\n"); return hr; }
        hr = context->LoadPixelShader(probe_lut_shader, (const BYTE *)code->GetBufferPointer(), code->GetBufferSize());
        code->Release();
        if (FAILED(hr)) return hr;
        if (!lut_transform.texture
                && FAILED(hr = context->CreateResourceTexture(&probe_lut_texture, &props, texels, NULL, sizeof(texels),
                        &lut_transform.texture)))
            return hr;
        return graph->SetSingleTransformNode(&lut_transform);
    }
    STDMETHOD(PrepareForRender)(D2D1_CHANGE_TYPE) { return S_OK; }
    STDMETHOD(SetGraph)(ID2D1TransformGraph *) { return E_NOTIMPL; }
};

static HRESULT WINAPI lut_factory(IUnknown **effect)
{
    *effect = static_cast<ID2D1EffectImpl *>(new LutEffect);
    return S_OK;
}

static ID2D1Bitmap1 *bitmap(UINT w, UINT h, const UINT32 *px)
{
    D2D1_BITMAP_PROPERTIES1 p = {};
    ID2D1Bitmap1 *b = NULL;
    p.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    p.dpiX = p.dpiY = 96.0f;
    dc->CreateBitmap(D2D1::SizeU(w, h), px, w * 4, &p, &b);
    return b;
}

/* the Histogram properties: NumBins 0, ChannelSelect 1, HistogramOutput 2 (not in mingw-w64's header) */
static void histogram(const char *name, ID2D1Bitmap1 *src, UINT32 bins, D2D1_CHANNEL_SELECTOR channel,
        const float *want)
{
    ID2D1Effect *fx = NULL;
    float got[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    int ok = 1;
    HRESULT hr;

    if (FAILED(dc->CreateEffect(CLSID_D2D1Histogram, &fx))) { printf("%s=0 no effect\n", name); return; }
    fx->SetInput(0, src);
    fx->SetValue(0u, bins);
    fx->SetValue(1u, channel);
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0, 0.0f));
    dc->DrawImage(fx);
    dc->EndDraw();
    hr = fx->GetValue(2u, (BYTE *)got, bins * sizeof(float));
    for (UINT32 i = 0; i < bins; i++) if (fabsf(got[i] - want[i]) > 0.01f) ok = 0;
    printf("%s=%d hr %#lx got", name, SUCCEEDED(hr) && ok, hr);
    for (UINT32 i = 0; i < bins; i++) printf(" %.3f", got[i]);
    printf("\n");
    fx->Release();
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

    /* red, lime, blue, white (Win2D's ComputeHistogram test): half of each channel 0, half 1 */
    const UINT32 four[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
    ID2D1Bitmap1 *b4 = bitmap(2, 2, four);
    const float halves[2] = { 0.5f, 0.5f }, blue4[4] = { 0.5f, 0.0f, 0.0f, 0.5f };
    histogram("histogram_red", b4, 2, D2D1_CHANNEL_SELECTOR_R, halves);
    histogram("histogram_green", b4, 2, D2D1_CHANNEL_SELECTOR_G, halves);
    histogram("histogram_blue_4bins", b4, 4, D2D1_CHANNEL_SELECTOR_B, blue4);
    /* straight colour: red 64 at alpha 128 is a red of 0.5 -- bin 2 of 4; alpha 128 is bin 2 too */
    const UINT32 half[1] = { 0x80400000 };
    ID2D1Bitmap1 *bh = bitmap(1, 1, half);
    const float bin2[4] = { 0, 0, 1, 0 };
    histogram("histogram_straight", bh, 4, D2D1_CHANNEL_SELECTOR_R, bin2);
    histogram("histogram_alpha", bh, 4, D2D1_CHANNEL_SELECTOR_A, bin2);
    {
        /* the effect draws its input */
        ID2D1Effect *fx = NULL;
        dc->CreateEffect(CLSID_D2D1Histogram, &fx);
        fx->SetInput(0, b4);
        dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(fx); dc->EndDraw();
        UINT32 got = pixel(1, 0);
        printf("histogram_passthrough=%d got %08x\n", got == 0xff00ff00, got);
        fx->Release();
    }

    /* the lookup effect on red = 85/255: texel 1, green */
    static const WCHAR xml[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Probe Lut'/>"
        L"<Property name='Author' type='string' value='probe'/><Property name='Category' type='string' value='probe'/>"
        L"<Property name='Description' type='string' value='probe'/><Inputs><Input name='Source'/></Inputs></Effect>";
    if (FAILED(factory->RegisterEffectFromString(CLSID_ProbeLut, xml, NULL, 0, lut_factory)))
    { printf("lut_registered=0\n"); return 0; }
    UINT32 px[16 * 16];
    for (int i = 0; i < 256; i++) px[i] = 0xff550000;
    ID2D1Bitmap1 *src = bitmap(16, 16, px);
    ID2D1Effect *fx = NULL;
    HRESULT hr = dc->CreateEffect(CLSID_ProbeLut, &fx);
    if (FAILED(hr)) { printf("lut_created=0 hr %#lx\n", hr); return 0; }
    fx->SetInput(0, src);
    dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(fx); hr = dc->EndDraw();
    UINT32 got = pixel(4, 4);
    printf("lut_lookup=%d hr %#lx got %08x want ff00ff00\n", got == 0xff00ff00, hr, got);
    /* red 255: the last texel, white */
    for (int i = 0; i < 256; i++) px[i] = 0xffff0000;
    src->CopyFromMemory(NULL, px, 16 * 4);
    dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(fx); dc->EndDraw();
    got = pixel(4, 4);
    printf("lut_last=%d got %08x want ffffffff\n", got == 0xffffffff, got);
    fx->Release();
    return 0;
}
