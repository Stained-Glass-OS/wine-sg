/* d3d11minlod-probe: ID3D11DeviceContext::SetResourceMinLOD / GetResourceMinLOD keep a texture's minimum LOD
 * (Stained Glass OS wine-sg 8248; test/d3d11minlod-gate.sh); Get always gave 0.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, double got )
{
    printf( "%s  %s (%g)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D11_TEXTURE2D_DESC desc2 = { 64, 64, 7, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE };
    D3D11_TEXTURE3D_DESC desc3 = { 16, 16, 16, 5, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE };
    D3D11_BUFFER_DESC bdesc = { 256, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    ID3D11Texture2D *tex2, *other;
    ID3D11Texture3D *tex3;
    ID3D11Buffer *buffer;
    HRESULT hr;

    hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, &context );
    if (FAILED(hr))
        hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, &context );
    if (FAILED(hr)) { printf( "FAIL  D3D11CreateDevice %#lx\nRESULT: FAIL\n", hr ); return 1; }
    ID3D11Device_CreateTexture2D( device, &desc2, NULL, &tex2 );
    ID3D11Device_CreateTexture2D( device, &desc2, NULL, &other );
    ID3D11Device_CreateTexture3D( device, &desc3, NULL, &tex3 );
    ID3D11Device_CreateBuffer( device, &bdesc, NULL, &buffer );

    check( ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)tex2 ) == 0.0f, "a new texture: 0", 0 );
    ID3D11DeviceContext_SetResourceMinLOD( context, (ID3D11Resource *)tex2, 2.5f );
    check( ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)tex2 ) == 2.5f, "Texture2D: the set 2.5 read back",
           ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)tex2 ) );
    check( ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)other ) == 0.0f, "another texture keeps 0", 0 );
    ID3D11DeviceContext_SetResourceMinLOD( context, (ID3D11Resource *)tex3, 3.0f );
    check( ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)tex3 ) == 3.0f, "Texture3D: 3 read back",
           ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)tex3 ) );
    ID3D11DeviceContext_SetResourceMinLOD( context, (ID3D11Resource *)tex2, 0.0f );
    check( ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)tex2 ) == 0.0f, "set back to 0", 0 );
    ID3D11DeviceContext_SetResourceMinLOD( context, (ID3D11Resource *)buffer, 1.0f );
    check( ID3D11DeviceContext_GetResourceMinLOD( context, (ID3D11Resource *)buffer ) == 0.0f, "a buffer has none (no crash)", 0 );
    ID3D11Buffer_Release( buffer );
    ID3D11Texture3D_Release( tex3 );
    ID3D11Texture2D_Release( other );
    ID3D11Texture2D_Release( tex2 );
    ID3D11DeviceContext_Release( context );
    ID3D11Device_Release( device );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
