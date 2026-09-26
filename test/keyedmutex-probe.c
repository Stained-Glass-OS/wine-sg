/* IDXGIKeyedMutex on a D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX texture, with
 * Wine's own d3d11 (wine-sg 0424).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>

static IDXGIKeyedMutex *mutex;
static volatile LONG woke;

static DWORD WINAPI waiter( void *arg )
{
    if (IDXGIKeyedMutex_AcquireSync( mutex, 7, 10000 ) == S_OK)
    {
        InterlockedExchange( &woke, 1 );
        IDXGIKeyedMutex_ReleaseSync( mutex, 0 );
    }
    return 0;
}

int main( void )
{
    D3D11_TEXTURE2D_DESC desc = { 64, 64, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 },
        D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0,
        D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX };
    ID3D11Device *device;
    ID3D11Texture2D *texture, *plain;
    IUnknown *unk;
    HANDLE thread;
    HRESULT hr;

    if (FAILED(hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
            D3D11_SDK_VERSION, &device, NULL, NULL )))
    { printf( "NODEVICE %08lx\n", hr ); return 1; }
    if (FAILED(hr = ID3D11Device_CreateTexture2D( device, &desc, NULL, &texture )))
    { printf( "CREATE %08lx\n", hr ); return 1; }
    printf( "CREATE ok\n" );
    hr = ID3D11Texture2D_QueryInterface( texture, &IID_IDXGIKeyedMutex, (void **)&mutex );
    printf( "QI %08lx\n", hr );
    if (FAILED(hr)) return 1;
    printf( "ACQUIRE0 %08lx\n", IDXGIKeyedMutex_AcquireSync( mutex, 0, 0 ) );
    printf( "ACQUIRE_OWNED %08lx\n", IDXGIKeyedMutex_AcquireSync( mutex, 0, 10 ) );
    thread = CreateThread( NULL, 0, waiter, NULL, 0, NULL );
    Sleep( 200 );
    printf( "WAITING %ld\n", woke );
    printf( "RELEASE7 %08lx\n", IDXGIKeyedMutex_ReleaseSync( mutex, 7 ) );
    WaitForSingleObject( thread, 10000 );
    printf( "WOKE %ld\n", woke );
    printf( "ACQUIRE_WRONGKEY %08lx\n", IDXGIKeyedMutex_AcquireSync( mutex, 5, 10 ) );
    printf( "ACQUIRE0_AGAIN %08lx\n", IDXGIKeyedMutex_AcquireSync( mutex, 0, 0 ) );
    IDXGIKeyedMutex_ReleaseSync( mutex, 0 );
    hr = IDXGIKeyedMutex_QueryInterface( mutex, &IID_ID3D11Texture2D, (void **)&unk );
    printf( "BACK %d\n", SUCCEEDED(hr) && unk == (IUnknown *)texture );
    if (SUCCEEDED(hr)) IUnknown_Release( unk );

    desc.MiscFlags = 0;
    ID3D11Device_CreateTexture2D( device, &desc, NULL, &plain );
    hr = ID3D11Texture2D_QueryInterface( plain, &IID_IDXGIKeyedMutex, (void **)&unk );
    printf( "PLAIN_QI %08lx\n", hr );
    return 0;
}
