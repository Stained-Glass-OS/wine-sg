/* d3d11minlodclamp-probe: ID3D11DeviceContext::SetResourceMinLOD clamps sampling (Stained Glass OS wine-sg 8254;
 * test/d3d11minlodclamp-gate.sh). A 16x16 texture with five levels, each one colour, is drawn on a 16x16 target
 * at level 0 (SampleLevel 0, point filtering); with the minimum LOD at 2 the pixel is level 2's colour, at 0
 * level 0's again, and a view starting at level 1 with the minimum LOD at 3 shows level 3.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>

static const DWORD colours[5] = { 0xff0000ff, 0xff00ff00, 0xffff0000, 0xff00ffff, 0xffff00ff }; /* ABGR in memory: R, G, B, Y, M */
static const char vs_src[] =
    "float4 main(uint id : SV_VertexID) : SV_Position {"
    " float2 p = float2((id << 1) & 2, id & 2); return float4(p * float2(2, -2) + float2(-1, 1), 0, 1); }";
static const char ps_src[] =
    "Texture2D t : register(t0); SamplerState s : register(s0);"
    "float4 main(float4 pos : SV_Position) : SV_Target { return t.SampleLevel(s, pos.xy / 16.0, 0); }";

static int failures;
static ID3D11Device *device;
static ID3D11DeviceContext *ctx;
static ID3D11Texture2D *target, *staging;
static ID3D11RenderTargetView *rtv;

static DWORD draw( ID3D11ShaderResourceView *srv )
{
    D3D11_MAPPED_SUBRESOURCE map;
    float black[4] = { 0 };
    DWORD pixel;

    ID3D11DeviceContext_ClearRenderTargetView( ctx, rtv, black );
    ID3D11DeviceContext_PSSetShaderResources( ctx, 0, 1, &srv );
    ID3D11DeviceContext_Draw( ctx, 3, 0 );
    ID3D11DeviceContext_CopyResource( ctx, (ID3D11Resource *)staging, (ID3D11Resource *)target );
    ID3D11DeviceContext_Map( ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map );
    pixel = ((DWORD *)((BYTE *)map.pData + 8 * map.RowPitch))[8];
    ID3D11DeviceContext_Unmap( ctx, (ID3D11Resource *)staging, 0 );
    return pixel;
}

static void check( int ok, const char *what, DWORD got )
{
    printf( "%s  %s (%08lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    HRESULT (WINAPI *pD3DCompile)(const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *, ID3DInclude *,
                                  const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **);
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    D3D11_TEXTURE2D_DESC desc = { 16, 16, 5, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE };
    D3D11_SAMPLER_DESC sdesc = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP,
                                 D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1, D3D11_COMPARISON_NEVER, { 0 }, 0, D3D11_FLOAT32_MAX };
    D3D11_SHADER_RESOURCE_VIEW_DESC vdesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_SRV_DIMENSION_TEXTURE2D };
    D3D11_SUBRESOURCE_DATA data[5];
    D3D11_VIEWPORT vp = { 0, 0, 16, 16, 0, 1 };
    static DWORD pixels[5][256];
    ID3DBlob *vsb, *psb, *err;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11SamplerState *sampler;
    ID3D11ShaderResourceView *srv, *srv1;
    ID3D11Texture2D *tex;
    DWORD got;
    int i, j;

    pD3DCompile = (void *)GetProcAddress( LoadLibraryA( "d3dcompiler_47.dll" ), "D3DCompile" );
    if (!pD3DCompile) { printf( "FAIL  D3DCompile\nRESULT: FAIL\n" ); return 1; }
    if (FAILED(D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &level, 1, D3D11_SDK_VERSION, &device, NULL, &ctx )))
    { printf( "FAIL  D3D11CreateDevice\nRESULT: FAIL\n" ); return 1; }
    if (FAILED(pD3DCompile( vs_src, sizeof(vs_src) - 1, "vs", NULL, NULL, "main", "vs_4_0", 0, 0, &vsb, &err )) ||
        FAILED(pD3DCompile( ps_src, sizeof(ps_src) - 1, "ps", NULL, NULL, "main", "ps_4_0", 0, 0, &psb, &err )))
    { printf( "FAIL  shaders\nRESULT: FAIL\n" ); return 1; }
    ID3D11Device_CreateVertexShader( device, ID3D10Blob_GetBufferPointer( vsb ), ID3D10Blob_GetBufferSize( vsb ), NULL, &vs );
    ID3D11Device_CreatePixelShader( device, ID3D10Blob_GetBufferPointer( psb ), ID3D10Blob_GetBufferSize( psb ), NULL, &ps );

    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 256; j++) pixels[i][j] = colours[i];
        data[i].pSysMem = pixels[i];
        data[i].SysMemPitch = (16 >> i) * 4;
        data[i].SysMemSlicePitch = 0;
    }
    ID3D11Device_CreateTexture2D( device, &desc, data, &tex );
    ID3D11Device_CreateShaderResourceView( device, (ID3D11Resource *)tex, NULL, &srv );
    vdesc.Texture2D.MostDetailedMip = 1;
    vdesc.Texture2D.MipLevels = 4;
    ID3D11Device_CreateShaderResourceView( device, (ID3D11Resource *)tex, &vdesc, &srv1 );
    desc.MipLevels = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Device_CreateTexture2D( device, &desc, NULL, &target );
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Device_CreateTexture2D( device, &desc, NULL, &staging );
    ID3D11Device_CreateRenderTargetView( device, (ID3D11Resource *)target, NULL, &rtv );
    ID3D11Device_CreateSamplerState( device, &sdesc, &sampler );

    ID3D11DeviceContext_OMSetRenderTargets( ctx, 1, &rtv, NULL );
    ID3D11DeviceContext_RSSetViewports( ctx, 1, &vp );
    ID3D11DeviceContext_IASetPrimitiveTopology( ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    ID3D11DeviceContext_VSSetShader( ctx, vs, NULL, 0 );
    ID3D11DeviceContext_PSSetShader( ctx, ps, NULL, 0 );
    ID3D11DeviceContext_PSSetSamplers( ctx, 0, 1, &sampler );

    got = draw( srv );
    check( got == colours[0], "no minimum LOD: level 0", got );
    ID3D11DeviceContext_SetResourceMinLOD( ctx, (ID3D11Resource *)tex, 2.0f );
    got = draw( srv );
    check( got == colours[2], "minimum LOD 2: level 2", got );
    got = draw( srv1 );
    check( got == colours[2], "  through a view from level 1 too (its level 1 = level 2)", got );
    ID3D11DeviceContext_SetResourceMinLOD( ctx, (ID3D11Resource *)tex, 3.0f );
    got = draw( srv1 );
    check( got == colours[3], "minimum LOD 3 through the level-1 view: level 3", got );
    ID3D11DeviceContext_SetResourceMinLOD( ctx, (ID3D11Resource *)tex, 0.0f );
    got = draw( srv );
    check( got == colours[0], "back to 0: level 0", got );
    got = draw( srv1 );
    check( got == colours[1], "  and the level-1 view shows level 1", got );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
