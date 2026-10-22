/* d3d9share-probe: Direct3D 9Ex shared surfaces (Stained Glass OS wine-sg 6023): a render target made
 * shared by one D3D9Ex device is opened by another and by a Direct3D 11 device, and a Direct3D 11 shared
 * texture is opened by a D3D9Ex device; the pixels go across.   d3d9share-probe.exe */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <stdio.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static IDirect3DDevice9Ex *create_device( IDirect3D9Ex *d3d, HWND hwnd )
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9Ex *device = NULL;

    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = 64;
    pp.BackBufferHeight = 64;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.hDeviceWindow = hwnd;
    IDirect3D9Ex_CreateDeviceEx( d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &pp, NULL, &device );
    return device;
}

/* the color of a pixel of a render target of the device, 0xAARRGGBB */
static DWORD rt_pixel( IDirect3DDevice9Ex *device, IDirect3DSurface9 *rt )
{
    IDirect3DSurface9 *sys;
    D3DLOCKED_RECT locked;
    DWORD color = 0xdeadbeef;

    if (FAILED(IDirect3DDevice9Ex_CreateOffscreenPlainSurface( device, 32, 32, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, NULL )))
        return color;
    if (SUCCEEDED(IDirect3DDevice9Ex_GetRenderTargetData( device, rt, sys ))
            && SUCCEEDED(IDirect3DSurface9_LockRect( sys, &locked, NULL, D3DLOCK_READONLY )))
    {
        color = ((DWORD *)locked.pBits)[16 * locked.Pitch / 4 + 16];
        IDirect3DSurface9_UnlockRect( sys );
    }
    IDirect3DSurface9_Release( sys );
    return color;
}

static void scene( IDirect3DDevice9Ex *device )
{
    IDirect3DDevice9Ex_BeginScene( device );
    IDirect3DDevice9Ex_EndScene( device );
}

int main( void )
{
    HWND hwnd = CreateWindowA( "static", "d3d9share", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL );
    IDirect3D9Ex *d3d = NULL;
    IDirect3DDevice9Ex *dev1, *dev2;
    IDirect3DSurface9 *rt1 = NULL, *level0 = NULL, *rt2 = NULL;
    IDirect3DTexture9 *tex2 = NULL, *tex3 = NULL;
    ID3D11Device *d11 = NULL;
    ID3D11DeviceContext *ctx = NULL;
    ID3D11Texture2D *t11 = NULL, *staging = NULL, *t11c = NULL;
    D3D11_TEXTURE2D_DESC desc = {0};
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11RenderTargetView *rtv = NULL;
    IDXGIResource *dxgi_res = NULL;
    HANDLE handle = NULL, h11 = NULL;
    HRESULT hr;
    DWORD color;

    if (FAILED(Direct3DCreate9Ex( D3D_SDK_VERSION, &d3d )) || !(dev1 = create_device( d3d, hwnd )) || !(dev2 = create_device( d3d, hwnd )))
    {
        printf( "FAIL  no D3D9Ex devices\nRESULT: FAIL\n" );
        return 1;
    }

    /* a shared render target */
    hr = IDirect3DDevice9Ex_CreateRenderTarget( dev1, 32, 32, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt1, &handle );
    check( hr == S_OK && rt1 && handle, "CreateRenderTarget gives a shared handle", hr );
    if (!handle) { printf( "RESULT: FAIL\n" ); return 1; }

    /* another D3D9Ex device opens it as a texture */
    hr = IDirect3DDevice9Ex_CreateTexture( dev2, 32, 32, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex2, &handle );
    check( hr == S_OK && tex2, "another device opens it (CreateTexture with the handle)", hr );
    hr = IDirect3DDevice9Ex_CreateTexture( dev2, 16, 32, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex3, &handle );
    check( hr == D3DERR_INVALIDCALL, "with other dimensions: D3DERR_INVALIDCALL", hr );

    /* a Direct3D 11 device opens it */
    hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d11, NULL, &ctx );
    if (SUCCEEDED(hr))
    {
        hr = ID3D11Device_OpenSharedResource( d11, handle, &IID_ID3D11Texture2D, (void **)&t11 );
        check( hr == S_OK && t11, "Direct3D 11 opens it", hr );
    }
    else check( 0, "a Direct3D 11 device", hr );

    /* the creator renders red */
    IDirect3DDevice9Ex_ColorFill( dev1, rt1, NULL, D3DCOLOR_ARGB( 0xff, 0xff, 0, 0 ) );
    scene( dev1 );

    if (tex2)
    {
        IDirect3DTexture9_GetSurfaceLevel( tex2, 0, &level0 );
        scene( dev2 );
        color = rt_pixel( dev2, level0 );
        check( (color & 0xffffff) == 0xff0000, "the other D3D9Ex device sees red", color );
    }
    if (t11)
    {
        ID3D11Texture2D_GetDesc( t11, &desc );
        check( desc.Width == 32 && desc.Height == 32 && desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM,
               "as a 32x32 B8G8R8A8 texture", desc.Format );
        ID3D11DeviceContext_Flush( ctx );
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        ID3D11Device_CreateTexture2D( d11, &desc, NULL, &staging );
        ID3D11DeviceContext_CopyResource( ctx, (ID3D11Resource *)staging, (ID3D11Resource *)t11 );
        color = 0;
        if (SUCCEEDED(ID3D11DeviceContext_Map( ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped )))
        {
            color = ((DWORD *)mapped.pData)[16 * mapped.RowPitch / 4 + 16];
            ID3D11DeviceContext_Unmap( ctx, (ID3D11Resource *)staging, 0 );
        }
        check( (color & 0xffffff) == 0xff0000, "Direct3D 11 sees red", color );
    }

    /* a Direct3D 11 shared texture, opened by D3D9Ex */
    if (d11)
    {
        float green[4] = {0, 1, 0, 1};

        memset( &desc, 0, sizeof(desc) );
        desc.Width = desc.Height = 32;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        hr = ID3D11Device_CreateTexture2D( d11, &desc, NULL, &t11c );
        if (SUCCEEDED(hr)) hr = ID3D11Texture2D_QueryInterface( t11c, &IID_IDXGIResource, (void **)&dxgi_res );
        if (SUCCEEDED(hr)) hr = IDXGIResource_GetSharedHandle( dxgi_res, &h11 );
        check( hr == S_OK && h11, "a Direct3D 11 shared texture's handle", hr );
        hr = IDirect3DDevice9Ex_CreateRenderTarget( dev2, 32, 32, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt2, &h11 );
        check( hr == S_OK && rt2, "D3D9Ex opens it (CreateRenderTarget with the handle)", hr );
        ID3D11Device_CreateRenderTargetView( d11, (ID3D11Resource *)t11c, NULL, &rtv );
        ID3D11DeviceContext_ClearRenderTargetView( ctx, rtv, green );
        ID3D11DeviceContext_Flush( ctx );
        if (rt2)
        {
            scene( dev2 );
            color = rt_pixel( dev2, rt2 );
            check( (color & 0xffffff) == 0x00ff00, "D3D9Ex sees Direct3D 11's green", color );
        }
    }

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
