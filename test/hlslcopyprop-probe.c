/* hlslcopyprop-probe: the HLSL compiler's copy propagation and stores through indices (patches/sg/6530).
 *
 * A store with a constant index past an array's end in dead code (legal before shader model 6, and
 * what an unrolled loop's last iteration leaves) invalidated components past the variable: the
 * compiler wrote past its own bookkeeping and crashed now and then (Firefox's WebRender gradient
 * shader brought down its GPU process). A store into an inner array forgot the outer index and
 * invalidated the wrong components, so a later read used the value from before the store.
 * Compiles both with D3DCompile, the first a few hundred times with the heap laid out differently,
 * and runs the second as a compute shader.
 * Prints name=value lines; see test/hlslcopyprop-gate.sh.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>

static const char oob[] =
    "cbuffer c : register(b0) { float4 p; };\n"
    "float4 main() : sv_target\n"
    "{\n"
    "    static float4 arr[4];\n"
    "    bool x = false;\n"
    "    arr[0] = p;\n"
    "    if (x)\n"
    "    {\n"
    "        arr[100000] = 0;\n"
    "        return arr[100000];\n"
    "    }\n"
    "    return arr[0];\n"
    "}\n";

static const char nested[] =
    "cbuffer c : register(b0) { float4 a; };\n"
    "RWBuffer<float4> o : register(u0);\n"
    "[numthreads(1, 1, 1)] void main()\n"
    "{\n"
    "    float arr[2][2] = {1, 2, 3, 4};\n"
    "    if (a.x > 0)\n"
    "        arr[1][1] = 5;\n"
    "    o[0] = float4(arr[0][0], arr[0][1], arr[1][0], arr[1][1]);\n"
    "}\n";

int main( void )
{
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx;
    ID3DBlob *code, *err;
    ID3D11ComputeShader *cs = NULL;
    ID3D11Buffer *cb, *ob, *rb;
    ID3D11UnorderedAccessView *uav;
    D3D11_BUFFER_DESC cbd = { 16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER };
    D3D11_BUFFER_DESC obd = { 16, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS };
    D3D11_BUFFER_DESC rbd = { 16, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ };
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud = { DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_UAV_DIMENSION_BUFFER };
    D3D11_MAPPED_SUBRESOURCE map;
    float cdata[4] = { 11, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA init = { cdata };
    HRESULT hr;
    int i, ok = 0;
    float *r;

    setvbuf( stdout, NULL, _IONBF, 0 );
    for (i = 0; i < 300; i++)
    {
        void *pad = malloc( 1 + i * 1021 );   /* another heap layout each time */
        code = err = NULL;
        hr = D3DCompile( oob, sizeof(oob) - 1, "oob", NULL, NULL, "main", "ps_4_0", 0, 0, &code, &err );
        if (SUCCEEDED(hr)) ok++;
        else if (i == 0 && err) printf( "%s\n", (char *)ID3D10Blob_GetBufferPointer( err ) );
        if (code) ID3D10Blob_Release( code );
        if (err) ID3D10Blob_Release( err );
        free( pad );
    }
    printf( "oob_dead_code=%d compiled=%d of 300\n", ok == 300, ok );

    code = err = NULL;
    hr = D3DCompile( nested, sizeof(nested) - 1, "nested", NULL, NULL, "main", "cs_5_0", 0, 0, &code, &err );
    printf( "nested_compiled=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;
    hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, NULL, &ctx );
    if (SUCCEEDED(hr))
        hr = ID3D11Device_CreateComputeShader( dev, ID3D10Blob_GetBufferPointer( code ), ID3D10Blob_GetBufferSize( code ), NULL, &cs );
    printf( "device=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;
    ID3D11Device_CreateBuffer( dev, &cbd, &init, &cb );
    ID3D11Device_CreateBuffer( dev, &obd, NULL, &ob );
    ID3D11Device_CreateBuffer( dev, &rbd, NULL, &rb );
    ud.Buffer.NumElements = 1;
    ID3D11Device_CreateUnorderedAccessView( dev, (ID3D11Resource *)ob, &ud, &uav );
    ID3D11DeviceContext_CSSetShader( ctx, cs, NULL, 0 );
    ID3D11DeviceContext_CSSetConstantBuffers( ctx, 0, 1, &cb );
    ID3D11DeviceContext_CSSetUnorderedAccessViews( ctx, 0, 1, &uav, NULL );
    ID3D11DeviceContext_Dispatch( ctx, 1, 1, 1 );
    ID3D11DeviceContext_CopyResource( ctx, (ID3D11Resource *)rb, (ID3D11Resource *)ob );
    ID3D11DeviceContext_Map( ctx, (ID3D11Resource *)rb, 0, D3D11_MAP_READ, 0, &map );
    r = map.pData;
    printf( "nested_store=%d result=%g,%g,%g,%g want=1,2,3,5\n", r[0] == 1 && r[1] == 2 && r[2] == 3 && r[3] == 5,
            r[0], r[1], r[2], r[3] );
    printf( "done=1\n" );
    return 0;
}
