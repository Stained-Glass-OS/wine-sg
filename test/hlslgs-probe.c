/* test/hlslgs-gate.sh probe: HLSL geometry shaders compile (wine-sg 6273, vkd3d 2.1). A shader in the
 * shape of RetroArch's d3d11 menu sprite shader -- a point in, a triangle strip of four vertices out
 * (point VSOut in[1], inout TriangleStream<GSOut>, [maxvertexcount(4)], Append, RestartStrip) --
 * compiles with D3DCompile for gs_4_0 and gs_5_0, reflection gives its [maxvertexcount], and a Direct3D 11
 * device takes it (CreateGeometryShader). With vkd3d 1.14 it did not parse ("Unknown modifier
 * point") and RetroArch's first start exited. Prints name=value lines. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>

static const char src[] =
    "struct VSOut { float4 pos : POSITION; float4 coords : TEXCOORD0; float4 color : COLOR; };\n"
    "struct GSOut { float4 pos : SV_POSITION; float2 tex : TEXCOORD0; float4 color : COLOR; };\n"
    "[maxvertexcount(4)]\n"
    "void GSMain(point VSOut in_vertex[1], inout TriangleStream<GSOut> stream)\n"
    "{\n"
    "    GSOut o;\n"
    "    float4 p = in_vertex[0].pos;\n"
    "    o.color = in_vertex[0].color;\n"
    "    o.pos = float4(p.xy, 0.0, 1.0); o.tex = float2(0.0, 0.0); stream.Append(o);\n"
    "    o.pos = float4(p.x + p.z, p.y, 0.0, 1.0); o.tex = float2(1.0, 0.0); stream.Append(o);\n"
    "    o.pos = float4(p.x, p.y - p.w, 0.0, 1.0); o.tex = float2(0.0, 1.0); stream.Append(o);\n"
    "    o.pos = float4(p.x + p.z, p.y - p.w, 0.0, 1.0); o.tex = float2(1.0, 1.0); stream.Append(o);\n"
    "    stream.RestartStrip();\n"
    "}\n";

int main(void)
{
    static const char *profiles[] = { "gs_4_0", "gs_5_0" };
    ID3D11Device *device = NULL;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0, got;
    unsigned int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &level, 1, D3D11_SDK_VERSION, &device, &got, NULL);
    for (i = 0; i < 2; i++)
    {
        ID3DBlob *code = NULL, *errors = NULL;
        ID3D11ShaderReflection *refl = NULL;
        D3D11_SHADER_DESC desc;
        HRESULT hr;

        hr = D3DCompile(src, sizeof(src) - 1, "menu.hlsl", NULL, NULL, "GSMain", profiles[i], 0, 0, &code, &errors);
        printf("compile_%s=%d (%#lx%s%s)\n", profiles[i], hr == S_OK && code, hr, errors ? ": " : "",
               errors ? (char *)ID3D10Blob_GetBufferPointer(errors) : "");
        if (hr != S_OK || !code) continue;
        hr = D3DReflect(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), &IID_ID3D11ShaderReflection, (void **)&refl);
        memset(&desc, 0, sizeof(desc));
        if (refl) refl->lpVtbl->GetDesc(refl, &desc);
        printf("reflect_%s=%d (input %d, topology %d, max %u)\n", profiles[i],
               refl && desc.GSMaxOutputVertexCount == 4,
               desc.InputPrimitive, desc.GSOutputTopology, desc.GSMaxOutputVertexCount);
        if (device)
        {
            ID3D11GeometryShader *gs = NULL;
            hr = ID3D11Device_CreateGeometryShader(device, ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), NULL, &gs);
            printf("device_%s=%d (%#lx)\n", profiles[i], hr == S_OK && gs, hr);
            if (gs) ID3D11GeometryShader_Release(gs);
        }
        else printf("no_device=1\n");
    }
    printf("done=1\n");
    return 0;
}
