// hlslcastvec-probe: the HLSL compiler kept a uint's cast to int and its cast to float apart (Stained Glass
// OS wine-sg 9207). vkd3d-shader's expression vectorizer merged two HLSL_OP1_CASTs of the same source into
// one, whatever their result types: "int2(gtid.x, 0)" and "gtid.x + 0.5" became one utof, and the texture
// was indexed with the float's bits -- every thread but the first wrote nowhere (Paint.NET 5.2's compute
// shaders: Auto-Level came out transparent). Compute shaders index a 4x1 texture with int(id) and write
// float(id) + 0.5; all four texels must be written.
// Prints name=1/0 lines; see test/hlslcastvec-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>

static const char *srcs[] = {
    "RWTexture2D<float4> O : register(u0);\n"
    "[numthreads(4,1,1)] void main(uint3 gtid : SV_GroupThreadID) { O[int2(gtid.x, 0)] = float4(gtid.x + 0.5, 0, 0, 1); }\n",
    "RWTexture2D<float4> O : register(u0);\n"
    "[numthreads(4,1,1)] void main(uint3 id : SV_DispatchThreadID) { float f = id.x; int i = id.x; O[int2(i, 0)] = float4(f + 0.5, i * 2, 0, 1); }\n",
};
static const char *names[] = { "group_thread", "dispatch_thread" };

int main()
{
    ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL lv;
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &lv, &ctx))
            || lv < D3D_FEATURE_LEVEL_11_0) { printf("no_d3d11=1\n"); return 1; }
    for (int v = 0; v < 2; v++)
    {
        ID3DBlob *code = NULL, *err = NULL;
        if (FAILED(D3DCompile(srcs[v], strlen(srcs[v]), NULL, NULL, NULL, "main", "cs_5_0", 0, 0, &code, &err)))
        { printf("%s=0 compile failed %s\n", names[v], err ? (char *)err->GetBufferPointer() : ""); continue; }
        D3D11_TEXTURE2D_DESC d = {};
        d.Width = 4; d.Height = 1; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        ID3D11Texture2D *t = NULL, *s = NULL;
        dev->CreateTexture2D(&d, NULL, &t);
        d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        dev->CreateTexture2D(&d, NULL, &s);
        ID3D11UnorderedAccessView *u = NULL; ID3D11ComputeShader *cs = NULL;
        dev->CreateUnorderedAccessView(t, NULL, &u);
        dev->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), NULL, &cs);
        if (!t || !s || !u || !cs) { printf("no_resources=1\n"); return 1; }
        ctx->CSSetShader(cs, NULL, 0);
        ctx->CSSetUnorderedAccessViews(0, 1, &u, NULL);
        ctx->Dispatch(1, 1, 1);
        ctx->CopyResource(s, t);
        D3D11_MAPPED_SUBRESOURCE m;
        if (FAILED(ctx->Map(s, 0, D3D11_MAP_READ, 0, &m))) { printf("no_readback=1\n"); return 1; }
        const float *f = (const float *)m.pData;
        int ok = f[0] == 0.5f && f[4] == 1.5f && f[8] == 2.5f && f[12] == 3.5f;
        if (v == 1) ok = ok && f[5] == 2.0f && f[13] == 6.0f;
        printf("%s=%d %.2f %.2f %.2f %.2f\n", names[v], ok, f[0], f[4], f[8], f[12]);
        ctx->Unmap(s, 0);
    }
    printf("done=1\n");
    return 0;
}
