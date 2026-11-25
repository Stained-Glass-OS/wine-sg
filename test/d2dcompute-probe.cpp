// d2dcompute-probe: Direct2D custom effects with a compute transform (Stained Glass OS wine-sg 9204).
// ID2D1ComputeTransform was never given its ID2D1ComputeInfo (SetComputeInfo was not called) and nothing
// dispatched its shader: Paint.NET 5.2's Auto-Level failed with "Unexpected null" and drew nothing. As
// Direct2D does, the transform is told its compute info, says how many thread groups its output takes
// (CalculateThreadgroups), and its shader runs with Direct2D's constants at b0 (resultRect, outputOffset,
// sceneToInput0X/Y), its own at b1, its input at t0/s0 and the output at u0.
// Prints name=1/0 lines; see test/d2dcompute-gate.sh.
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
#include <cstring>

/* not in mingw-w64's d2d1effectauthor.h: as the Windows SDK declares them */
interface ID2D1ComputeInfo : public ID2D1RenderInfo
{
    STDMETHOD(SetComputeShaderConstantBuffer)(CONST BYTE *buffer, UINT32 size) PURE;
    STDMETHOD(SetComputeShader)(REFGUID id) PURE;
    STDMETHOD(SetResourceTexture)(UINT32 index, ID2D1ResourceTexture *texture) PURE;
};
__CRT_UUID_DECL(ID2D1ComputeInfo, 0x5598b14b,0x9fd7,0x48b7,0x9b,0xdb,0x8f,0x09,0x64,0xeb,0x38,0xbc);
interface ID2D1ComputeTransform : public ID2D1Transform
{
    STDMETHOD(SetComputeInfo)(ID2D1ComputeInfo *info) PURE;
    STDMETHOD(CalculateThreadgroups)(CONST D2D1_RECT_L *rect, UINT32 *x, UINT32 *y, UINT32 *z) PURE;
};
__CRT_UUID_DECL(ID2D1ComputeTransform, 0x0d85573c,0x01e3,0x4f7d,0xbf,0xd9,0x0d,0x60,0x60,0x8b,0xf3,0xc3);

interface ID2D1AnalysisTransform : public IUnknown
{
    STDMETHOD(ProcessAnalysisResults)(CONST BYTE *data, UINT32 size) PURE;
};
__CRT_UUID_DECL(ID2D1AnalysisTransform, 0x0359dc30,0x95e6,0x4568,0x90,0x55,0x27,0x72,0x0d,0x13,0x0e,0x93);

DEFINE_GUID(CLSID_ProbeAnalysis, 0x7b2e31e4,0x66c2,0x4a1f,0x9b,0x33,0x2d,0xa4,0x38,0x52,0xe1,0x03);
DEFINE_GUID(probe_analysis_cs, 0x7b2e31e4,0x66c2,0x4a1f,0x9b,0x33,0x2d,0xa4,0x38,0x52,0xe1,0x04);
DEFINE_GUID(CLSID_ProbeCompute, 0x7b2e31e4,0x66c2,0x4a1f,0x9b,0x33,0x2d,0xa4,0x38,0x52,0xe1,0x01);
DEFINE_GUID(probe_cs, 0x7b2e31e4,0x66c2,0x4a1f,0x9b,0x33,0x2d,0xa4,0x38,0x52,0xe1,0x02);

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

/* the shader as d2d1effecthelpers.hlsli lays out a compute effect: red and green swapped, times b1's scale */
static const char cs_src[] =
    "cbuffer systemConstants : register(b0) { int4 resultRect; int2 outputOffset; float2 sceneToInput0X; float2 sceneToInput0Y; };\n"
    "cbuffer constants : register(b1) { float4 scale; };\n"
    "Texture2D<float4> InputTexture : register(t0); SamplerState InputSampler : register(s0);\n"
    "RWTexture2D<float4> OutputTexture : register(u0);\n"
    "[numthreads(8, 8, 1)]\n"
    "void main(uint3 id : SV_DispatchThreadID)\n"
    "{\n"
    "    int2 p = int2(id.xy) + resultRect.xy;\n"
    "    if (p.x >= resultRect.z || p.y >= resultRect.w) return;\n"
    "    float2 uv = float2((p.x + 0.5) * sceneToInput0X.x + sceneToInput0X.y, (p.y + 0.5) * sceneToInput0Y.x + sceneToInput0Y.y);\n"
    "    float4 c = InputTexture.SampleLevel(InputSampler, uv, 0);\n"
    "    OutputTexture[int2(id.xy) + outputOffset + resultRect.xy] = float4(c.g, c.r, c.b, c.a) * scale;\n"
    "}\n";

static int info_set, groups_asked;
static D2D1_RECT_L asked_rect;

class SwapTransform : public ID2D1ComputeTransform
{
public:
    LONG ref = 1;
    float scale[4] = { 1, 1, 1, 1 };
    STDMETHOD(QueryInterface)(REFIID iid, void **out)
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1TransformNode) || iid == __uuidof(ID2D1Transform)
                || iid == __uuidof(ID2D1ComputeTransform)) { *out = this; AddRef(); return S_OK; }
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
    STDMETHOD(SetComputeInfo)(ID2D1ComputeInfo *info)
    {
        HRESULT hr = info->SetComputeShader(probe_cs);
        if (SUCCEEDED(hr)) hr = info->SetComputeShaderConstantBuffer((const BYTE *)scale, sizeof(scale));
        if (SUCCEEDED(hr)) info_set = 1;
        return hr;
    }
    STDMETHOD(CalculateThreadgroups)(CONST D2D1_RECT_L *rect, UINT32 *x, UINT32 *y, UINT32 *z)
    {
        groups_asked++;
        asked_rect = *rect;
        *x = (rect->right - rect->left + 7) / 8;
        *y = (rect->bottom - rect->top + 7) / 8;
        *z = 1;
        return S_OK;
    }
};

static SwapTransform swap_transform;

class SwapEffect : public ID2D1EffectImpl
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
        ID3DBlob *code = NULL, *errors = NULL;
        HRESULT hr;

        if (FAILED(hr = D3DCompile(cs_src, sizeof(cs_src) - 1, NULL, NULL, NULL, "main", "cs_5_0", 0, 0, &code, &errors)))
        {
            printf("no_compiler=1 %#lx %s\n", hr, errors ? (const char *)errors->GetBufferPointer() : "");
            return hr;
        }
        hr = context->LoadComputeShader(probe_cs, (const BYTE *)code->GetBufferPointer(), code->GetBufferSize());
        code->Release();
        printf("shader_loaded=%d hr %#lx\n", SUCCEEDED(hr), hr);
        if (FAILED(hr)) return hr;
        return graph->SetSingleTransformNode(&swap_transform);
    }
    STDMETHOD(PrepareForRender)(D2D1_CHANGE_TYPE) { return S_OK; }
    STDMETHOD(SetGraph)(ID2D1TransformGraph *) { return E_NOTIMPL; }
};

/* an analysis effect: a compute transform whose 4x1 output, 32-bit float, is handed back on the CPU
 * (ID2D1AnalysisTransform::ProcessAnalysisResults) -- counts beyond what a 16-bit float holds exactly. An
 * analysis compute shader writes a RWStructuredBuffer<float> at u0, and Direct2D's constants are only
 * sceneToInput0X/Y (as Paint.NET 5.2's Histogram shader declares them) */
static const char analysis_src[] =
    "cbuffer systemConstants : register(b0) { float2 sceneToInput0X; float2 sceneToInput0Y; };\n"
    "RWStructuredBuffer<float> Result : register(u0);\n"
    "[numthreads(8, 8, 1)]\n"
    "void main(uint3 id : SV_DispatchThreadID)\n"
    "{\n"
    "    if (id.x >= 4 || id.y >= 1) return;\n"
    "    uint o = 4 * id.x;\n"
    "    float x = id.x;\n"
    "    Result[o] = x + 0.5; Result[o + 1] = 70001.0 + x; Result[o + 2] = 1000.25; Result[o + 3] = 1.0;\n"
    "}\n";

static int analysis_calls;
static UINT32 analysis_size;
static float analysis_data[16];

class AnalysisTransform : public ID2D1ComputeTransform, public ID2D1AnalysisTransform
{
public:
    LONG ref = 1;
    STDMETHOD(QueryInterface)(REFIID iid, void **out)
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1TransformNode) || iid == __uuidof(ID2D1Transform)
                || iid == __uuidof(ID2D1ComputeTransform)) { *out = static_cast<ID2D1ComputeTransform *>(this); AddRef(); return S_OK; }
        if (iid == __uuidof(ID2D1AnalysisTransform)) { *out = static_cast<ID2D1AnalysisTransform *>(this); AddRef(); return S_OK; }
        *out = NULL; return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&ref); }
    STDMETHOD_(ULONG, Release)() { return InterlockedDecrement(&ref); }
    STDMETHOD_(UINT32, GetInputCount)() CONST { return 1; }
    STDMETHOD(MapOutputRectToInputRects)(CONST D2D1_RECT_L *out, D2D1_RECT_L *in, UINT32 count) CONST
    { if (count) in[0] = D2D1_RECT_L{ 0, 0, 1, 1 }; return S_OK; }
    STDMETHOD(MapInputRectsToOutputRect)(CONST D2D1_RECT_L *in, CONST D2D1_RECT_L *opaque_in, UINT32 count,
            D2D1_RECT_L *out, D2D1_RECT_L *opaque_out)
    { *out = D2D1_RECT_L{ 0, 0, 4, 1 }; memset(opaque_out, 0, sizeof(*opaque_out)); return S_OK; }
    STDMETHOD(MapInvalidRect)(UINT32 index, D2D1_RECT_L in, D2D1_RECT_L *out) CONST { *out = D2D1_RECT_L{ 0, 0, 4, 1 }; return S_OK; }
    STDMETHOD(SetComputeInfo)(ID2D1ComputeInfo *info)
    {
        HRESULT hr = info->SetComputeShader(probe_analysis_cs);
        if (SUCCEEDED(hr)) hr = info->SetOutputBuffer(D2D1_BUFFER_PRECISION_32BPC_FLOAT, D2D1_CHANNEL_DEPTH_4);
        return hr;
    }
    STDMETHOD(CalculateThreadgroups)(CONST D2D1_RECT_L *rect, UINT32 *x, UINT32 *y, UINT32 *z)
    { *x = 1; *y = 1; *z = 1; return S_OK; }
    STDMETHOD(ProcessAnalysisResults)(CONST BYTE *data, UINT32 size)
    {
        analysis_calls++;
        analysis_size = size;
        if (data) memcpy(analysis_data, data, size < sizeof(analysis_data) ? size : sizeof(analysis_data));
        return S_OK;
    }
};

static AnalysisTransform analysis_transform;

class AnalysisEffect : public ID2D1EffectImpl
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
        ID3DBlob *code = NULL;
        HRESULT hr;
        if (FAILED(hr = D3DCompile(analysis_src, sizeof(analysis_src) - 1, NULL, NULL, NULL, "main", "cs_5_0", 0, 0, &code, NULL)))
            return hr;
        hr = context->LoadComputeShader(probe_analysis_cs, (const BYTE *)code->GetBufferPointer(), code->GetBufferSize());
        code->Release();
        if (FAILED(hr)) return hr;
        return graph->SetSingleTransformNode(static_cast<ID2D1ComputeTransform *>(&analysis_transform));
    }
    STDMETHOD(PrepareForRender)(D2D1_CHANGE_TYPE) { return S_OK; }
    STDMETHOD(SetGraph)(ID2D1TransformGraph *) { return E_NOTIMPL; }
};

static HRESULT WINAPI analysis_factory(IUnknown **effect)
{
    *effect = static_cast<ID2D1EffectImpl *>(new AnalysisEffect);
    return S_OK;
}

static HRESULT WINAPI swap_factory(IUnknown **effect)
{
    *effect = static_cast<ID2D1EffectImpl *>(new SwapEffect);
    return S_OK;
}

int main()
{
    ID3D11Device *d3d;
    D3D_FEATURE_LEVEL level;
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                 D3D11_SDK_VERSION, &d3d, &level, NULL))) { printf("no_d3d=1\n"); return 1; }
    if (level < D3D_FEATURE_LEVEL_11_0) { printf("no_compute_level=1 %#x\n", level); return 1; }
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

    static const WCHAR xml[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Probe Compute'/>"
        L"<Property name='Author' type='string' value='probe'/><Property name='Category' type='string' value='probe'/>"
        L"<Property name='Description' type='string' value='probe'/><Inputs><Input name='Source'/></Inputs></Effect>";
    if (FAILED(factory->RegisterEffectFromString(CLSID_ProbeCompute, xml, NULL, 0, swap_factory)))
    { printf("registered=0\n"); return 0; }

    /* the source: red on the left half, blue on the right */
    UINT32 px[16 * 16];
    for (int i = 0; i < 256; i++) px[i] = (i % 16) < 8 ? 0xffff0000 : 0xff0000ff;
    D2D1_BITMAP_PROPERTIES1 sp = {};
    sp.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    sp.dpiX = sp.dpiY = 96.0f;
    ID2D1Bitmap1 *src = NULL;
    dc->CreateBitmap(D2D1::SizeU(16, 16), px, 16 * 4, &sp, &src);

    ID2D1Effect *fx = NULL;
    HRESULT hr = dc->CreateEffect(CLSID_ProbeCompute, &fx);
    if (FAILED(hr)) { printf("created=0 hr %#lx\n", hr); return 0; }
    fx->SetInput(0, src);
    dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(fx); hr = dc->EndDraw();
    UINT32 a = pixel(3, 3), b = pixel(12, 12);
    printf("info_set=%d\n", info_set);
    printf("groups_asked=%d %d times rect %ld,%ld-%ld,%ld\n", groups_asked > 0 && asked_rect.right - asked_rect.left == 16,
            groups_asked, asked_rect.left, asked_rect.top, asked_rect.right, asked_rect.bottom);
    printf("dispatched=%d hr %#lx red->%08x (want ff00ff00) blue->%08x (want ff0000ff)\n",
            a == 0xff00ff00 && b == 0xff0000ff, hr, a, b);

    /* through a crop to 4,4-12,12: the output rectangle does not start at 0 -- resultRect, outputOffset and
     * sceneToInput place it; outside it is transparent */
    ID2D1Effect *crop = NULL;
    dc->CreateEffect(CLSID_D2D1Crop, &crop);
    crop->SetInput(0, src);
    crop->SetValue(D2D1_CROP_PROP_RECT, D2D1::RectF(4, 4, 12, 12));
    ID2D1Image *crop_out = NULL;
    crop->GetOutput(&crop_out);
    fx->SetInput(0, crop_out);
    dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(fx); hr = dc->EndDraw();
    UINT32 in1 = pixel(5, 5), in2 = pixel(10, 10), outside = pixel(2, 2), outside2 = pixel(13, 6);
    printf("offset_rect=%d rect %ld,%ld-%ld,%ld (5,5) %08x (10,10) %08x\n",
            asked_rect.left == 4 && asked_rect.top == 4 && in1 == 0xff00ff00 && in2 == 0xff0000ff,
            asked_rect.left, asked_rect.top, asked_rect.right, asked_rect.bottom, in1, in2);
    printf("offset_outside=%d (2,2) %08x (13,6) %08x\n", outside == 0 && outside2 == 0, outside, outside2);

    /* the effect's own constants at b1: a scale of 0.5 halves the colour (premultiplied: alpha too) */
    swap_transform.scale[0] = swap_transform.scale[1] = swap_transform.scale[2] = swap_transform.scale[3] = 0.5f;
    {
        /* SetComputeInfo is called once: the scale goes in through the kept info, as an effect does in
         * PrepareForRender */
        ID2D1Effect *fx2 = NULL;
        info_set = 0;
        dc->CreateEffect(CLSID_ProbeCompute, &fx2);
        fx2->SetInput(0, src);
        dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(fx2); dc->EndDraw();
        UINT32 h = pixel(3, 3);
        UINT32 g = (h >> 8) & 0xff, al = h >> 24;
        printf("constants_b1=%d %08x\n", g >= 0x7c && g <= 0x83 && al >= 0x7c && al <= 0x83, h);
        fx2->Release();
    }
    crop_out->Release();
    crop->Release();
    fx->Release();

    /* the analysis effect: drawn, its transform is handed its whole 4x1 output as 32-bit floats */
    static const WCHAR axml[] =
        L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='Probe Analysis'/>"
        L"<Property name='Author' type='string' value='probe'/><Property name='Category' type='string' value='probe'/>"
        L"<Property name='Description' type='string' value='probe'/><Inputs><Input name='Source'/></Inputs></Effect>";
    if (SUCCEEDED(factory->RegisterEffectFromString(CLSID_ProbeAnalysis, axml, NULL, 0, analysis_factory)))
    {
        ID2D1Effect *ax = NULL;
        if (SUCCEEDED(dc->CreateEffect(CLSID_ProbeAnalysis, &ax)))
        {
            ax->SetInput(0, src);
            dc->BeginDraw(); dc->Clear(D2D1::ColorF(0, 0.0f)); dc->DrawImage(ax); hr = dc->EndDraw();
            printf("analysis_called=%d %d calls, %u bytes (want 64) hr %#lx\n", analysis_calls >= 1 && analysis_size == 64,
                    analysis_calls, analysis_size, hr);
            printf("analysis_data=%d %.2f %.2f %.2f %.2f | %.2f %.2f\n",
                    analysis_data[0] == 0.5f && analysis_data[4] == 1.5f && analysis_data[12] == 3.5f
                    && analysis_data[1] == 70001.0f && analysis_data[13] == 70004.0f && analysis_data[2] == 1000.25f,
                    analysis_data[0], analysis_data[4], analysis_data[8], analysis_data[12], analysis_data[1], analysis_data[13]);
            ax->Release();
        }
        else printf("analysis_called=0 not created\n");
    }
    printf("done=1\n");
    return 0;
}
