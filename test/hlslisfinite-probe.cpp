// hlslisfinite-probe: the HLSL intrinsics isfinite() and isnan() (Stained Glass OS wine-sg 9205). The
// compiler (vkd3d-shader's HLSL front end) had isinf only: "Function "isfinite" is not defined", so
// Paint.NET 5.2's Auto-Level, which compiles its compute shader at run time, failed. A compute shader
// classifies 1, -2.5, +inf, -inf, NaN, 0 and FLT_MAX; a pixel-shader-model 2 compile takes them too.
// Prints name=1/0 lines; see test/hlslisfinite-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>

static const char cs_src[] =
    "StructuredBuffer<float> In : register(t0);\n"
    "RWStructuredBuffer<uint> Out : register(u0);\n"
    "[numthreads(1, 1, 1)]\n"
    "void main(uint3 id : SV_DispatchThreadID)\n"
    "{\n"
    "    float x = In[id.x];\n"
    "    float2 v = float2(x, 1.0);\n"
    "    bool2 f = isfinite(v);\n"
    "    Out[id.x] = (isfinite(x) ? 1u : 0u) | (isnan(x) ? 2u : 0u) | (isinf(x) ? 4u : 0u) | (f.y ? 8u : 0u)\n"
    "            | (isfinite(int(id.x)) ? 16u : 0u) | (isnan(int(id.x)) ? 32u : 0u);\n"
    "}\n";

static const char ps2_src[] =
    "float4 c; float4 main() : COLOR { return float4(isfinite(c.x), isnan(c.y), 0, 1); }\n";

int main()
{
    ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL level;
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;

    hr = D3DCompile(ps2_src, sizeof(ps2_src) - 1, NULL, NULL, NULL, "main", "ps_2_0", 0, 0, &code, &err);
    printf("compiled_ps2=%d hr %#lx %s\n", SUCCEEDED(hr), hr, err ? (const char *)err->GetBufferPointer() : "");
    if (code) code->Release(), code = NULL;
    if (err) err->Release(), err = NULL;
    hr = D3DCompile(cs_src, sizeof(cs_src) - 1, NULL, NULL, NULL, "main", "cs_5_0", 0, 0, &code, &err);
    printf("compiled_cs5=%d hr %#lx %s\n", SUCCEEDED(hr), hr, err ? (const char *)err->GetBufferPointer() : "");
    if (FAILED(hr)) { printf("done=1\n"); return 0; }

    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &level, &ctx))
            || level < D3D_FEATURE_LEVEL_11_0)
    { printf("no_d3d11=1\n"); return 1; }

    UINT32 bits[7] = { 0x3f800000, 0xc0200000, 0x7f800000, 0xff800000, 0x7fc00000, 0x00000000, 0x7f7fffff };
    const UINT32 want[7] = { 1 | 8, 1 | 8, 4 | 8, 4 | 8, 2 | 8, 1 | 8, 1 | 8 };
    D3D11_BUFFER_DESC bd = {};
    D3D11_SUBRESOURCE_DATA sd = { bits, 0, 0 };
    ID3D11Buffer *in = NULL, *out = NULL, *rb = NULL;
    bd.ByteWidth = sizeof(bits); bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; bd.StructureByteStride = 4;
    dev->CreateBuffer(&bd, &sd, &in);
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    dev->CreateBuffer(&bd, NULL, &out);
    bd.Usage = D3D11_USAGE_STAGING; bd.BindFlags = 0; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; bd.MiscFlags = 0;
    dev->CreateBuffer(&bd, NULL, &rb);
    ID3D11ShaderResourceView *srv = NULL; ID3D11UnorderedAccessView *uav = NULL; ID3D11ComputeShader *cs = NULL;
    dev->CreateShaderResourceView(in, NULL, &srv);
    dev->CreateUnorderedAccessView(out, NULL, &uav);
    dev->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), NULL, &cs);
    if (!srv || !uav || !cs || !rb) { printf("no_resources=1\n"); return 1; }
    ctx->CSSetShader(cs, NULL, 0);
    ctx->CSSetShaderResources(0, 1, &srv);
    ctx->CSSetUnorderedAccessViews(0, 1, &uav, NULL);
    ctx->Dispatch(7, 1, 1);
    ctx->CopyResource(rb, out);
    D3D11_MAPPED_SUBRESOURCE m;
    int ok = 1, ints = 1;
    if (FAILED(ctx->Map(rb, 0, D3D11_MAP_READ, 0, &m))) { printf("no_readback=1\n"); return 1; }
    const UINT32 *got = (const UINT32 *)m.pData;
    printf("classes");
    for (int i = 0; i < 7; i++)
    {
        printf(" %08x:%u(want %u)", bits[i], got[i] & 15, want[i]);
        if ((got[i] & 15) != want[i]) ok = 0;
        if ((got[i] & 48) != 16) ints = 0;
    }
    printf("\n");
    ctx->Unmap(rb, 0);
    printf("classified=%d\n", ok);
    printf("integers=%d\n", ints);
    printf("done=1\n");
    return 0;
}
