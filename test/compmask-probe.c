/* compmask-probe (wine-sg 6270): Windows.UI.Composition's CompositionMaskBrush -- ICompositor2::CreateMaskBrush,
 * its Mask and Source, the source drawn through the mask's alpha (a colour mask's alpha; a surface mask's
 * picture: clear where it is clear) -- and CompositionCapabilities (GetForCurrentView, AreEffectsSupported,
 * AreEffectsFast, Changed), which Teams' title bar overlay needs. Built from test/wuicomp-probe.c's helpers;
 * interface identity per windows-rs 0.52.0 (MIT OR Apache-2.0). Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(probe_IID_ICompositor, 0xb403ca50, 0x7f8c, 0x4e83, 0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8);
DEFINE_GUID(probe_IID_ICompositor2, 0x735081dc, 0x5e24, 0x45da, 0xa3, 0x8f, 0xe3, 0x2c, 0xc3, 0x49, 0xa9, 0xa0);
DEFINE_GUID(probe_IID_IVisual, 0x117e202d, 0xa859, 0x4c89, 0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3);
DEFINE_GUID(probe_IID_IContainerVisual, 0x02f6bc74, 0xed20, 0x4773, 0xaf, 0xe6, 0xd4, 0x9b, 0x4a, 0x93, 0xdb, 0x32);
DEFINE_GUID(probe_IID_ICompositionTarget, 0xa1bea8ba, 0xd726, 0x4663, 0x81, 0x29, 0x6b, 0x5e, 0x79, 0x27, 0xff, 0xa6);
DEFINE_GUID(probe_IID_ICompositorDesktopInterop, 0x29e691fa, 0x4567, 0x4dca, 0xb3, 0x19, 0xd0, 0xf2, 0x07, 0xeb, 0x68, 0x07);
DEFINE_GUID(probe_IID_ICompositorInterop, 0x25297d5c, 0x3ad4, 0x4c9c, 0xb5, 0xcf, 0xe3, 0x6a, 0x38, 0x51, 0x23, 0x30);
DEFINE_GUID(probe_IID_ICompositionDrawingSurfaceInterop, 0xfd04e6e3, 0xfe0c, 0x4c3c, 0xab, 0x19, 0xa0, 0x76, 0x01, 0xa5, 0x76, 0xee);
DEFINE_GUID(probe_IID_ICompositionBrush, 0xab0d7608, 0x30c0, 0x40e9, 0xb5, 0x68, 0xb6, 0x0a, 0x6b, 0xd1, 0xfb, 0x46);
DEFINE_GUID(probe_IID_ICompositionSurface, 0x1527540d, 0x42c7, 0x47a6, 0xa4, 0x08, 0x66, 0x8f, 0x79, 0xa9, 0x0d, 0xfb);
DEFINE_GUID(probe_IID_ICompositionMaskBrush, 0x522cf09e, 0xbe6b, 0x4f41, 0xbe, 0x49, 0xf9, 0x22, 0x6d, 0x47, 0x1b, 0x4a);
DEFINE_GUID(probe_IID_ICompositionCapabilities, 0x8253353e, 0xb517, 0x48bc, 0xb1, 0xe8, 0x4b, 0x35, 0x61, 0xa2, 0xe1, 0x81);
DEFINE_GUID(probe_IID_ICompositionEffectSourceParameter, 0x858ab13a, 0x3292, 0x4e4e, 0xb3, 0xbb, 0x2b, 0x6c, 0x65, 0x44, 0xa6, 0xee);
DEFINE_GUID(probe_IID_ICompositionEffectSourceParameterFactory, 0xb3d9f276, 0xaba3, 0x4724, 0xac, 0xf3, 0xd0, 0x39, 0x74, 0x64, 0xdb, 0x1c);
DEFINE_GUID(probe_IID_IGraphicsEffectSource, 0x2d8f9ddc, 0x4339, 0x4eb9, 0x92, 0x16, 0xf9, 0xde, 0xb7, 0x56, 0x58, 0xa2);
DEFINE_GUID(probe_IID_ICompositionSupportsSystemBackdrop, 0x397dafe4, 0xb6c2, 0x5bb9, 0x95, 0x1d, 0xf5, 0x70, 0x7d, 0xe8, 0xb7, 0xbc);
DEFINE_GUID(probe_IID_ICompositionCapabilitiesStatics, 0xf7b7a86e, 0x6416, 0x49e5, 0x8d, 0xdf, 0xaf, 0xe9, 0x49, 0xe2, 0x05, 0x62);

#define COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR 8
#define COMPOSITOR_CREATE_CONTAINER_VISUAL       9
#define COMPOSITOR_CREATE_SPRITE_VISUAL          22
#define COMPOSITOR_CREATE_SURFACE_BRUSH_WITH_SURFACE 24
#define COMPOSITOR2_CREATE_MASK_BRUSH            13
#define MASK_GET_MASK 6
#define MASK_PUT_MASK 7
#define MASK_GET_SOURCE 8
#define MASK_PUT_SOURCE 9
#define VISUAL_PUT_OFFSET                        21
#define VISUAL_PUT_SIZE                          36
#define CONTAINER_GET_CHILDREN                   6
#define COLLECTION_INSERT_AT_TOP                 9
#define SPRITE_PUT_BRUSH                         7
#define TARGET_PUT_ROOT                          7
#define DESKTOP_INTEROP_CREATE_TARGET            3
#define INTEROP_CREATE_GRAPHICS_DEVICE           5
#define GRAPHICS_DEVICE_CREATE_DRAWING_SURFACE   6
#define SURFACE_INTEROP_BEGIN_DRAW               3
#define SURFACE_INTEROP_END_DRAW                 4

struct ui_color { BYTE a, r, g, b; };
struct vector2 { float x, y; };
struct vector3 { float x, y, z; };
typedef HRESULT (WINAPI *get_fn)( void *, void ** );
typedef HRESULT (WINAPI *put_fn)( void *, void * );

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }
        Sleep( 10 );
    }
}

static void *qi( void *obj, const GUID *iid )
{
    void *out = NULL;
    if (obj) IUnknown_QueryInterface( (IUnknown *)obj, iid, &out );
    return out;
}

static void *color_brush( void *compositor, struct ui_color c )
{
    void *b = NULL;
    SLOT( compositor, COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR, HRESULT (WINAPI *)( void *, struct ui_color, void ** ) )( compositor, c, &b );
    return qi( b, &probe_IID_ICompositionBrush );
}

static void *make_sprite( void *compositor, void *brush, float x, float y, float w, float h )
{
    struct vector3 offset = { x, y, 0 };
    struct vector2 size = { w, h };
    void *sprite = NULL, *visual;

    if (FAILED( SLOT( compositor, COMPOSITOR_CREATE_SPRITE_VISUAL, get_fn )( compositor, &sprite ) )) return NULL;
    SLOT( sprite, SPRITE_PUT_BRUSH, put_fn )( sprite, brush );
    visual = qi( sprite, &probe_IID_IVisual );
    SLOT( visual, VISUAL_PUT_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 ) )( visual, offset );
    SLOT( visual, VISUAL_PUT_SIZE, HRESULT (WINAPI *)( void *, struct vector2 ) )( visual, size );
    return visual;
}

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static int close_to( COLORREF c, int r, int g, int b )
{
    return abs( GetRValue(c) - r ) <= 24 && abs( GetGValue(c) - g ) <= 24 && abs( GetBValue(c) - b ) <= 24;
}

static void capabilities( void )
{
    static const WCHAR name[] = L"Windows.UI.Composition.CompositionCapabilities";
    void *statics = NULL, *caps = NULL, *iface;
    boolean supported = 2, fast = 2;
    INT64 token = 0;
    HSTRING hs;
    HRESULT hr;

    WindowsCreateString( name, (UINT32)wcslen( name ), &hs );
    hr = RoGetActivationFactory( hs, &probe_IID_ICompositionCapabilitiesStatics, &statics );
    WindowsDeleteString( hs );
    printf( "caps_factory=%d (%#lx)\n", SUCCEEDED(hr) && statics, hr );
    if (!statics) return;
    hr = SLOT( statics, 6, get_fn )( statics, &caps );
    iface = qi( caps, &probe_IID_ICompositionCapabilities );
    printf( "caps_for_view=%d (%#lx)\n", SUCCEEDED(hr) && iface, hr );
    if (!iface) return;
    SLOT( iface, 6, HRESULT (WINAPI *)( void *, boolean * ) )( iface, &supported );
    SLOT( iface, 7, HRESULT (WINAPI *)( void *, boolean * ) )( iface, &fast );
    printf( "caps_effects=%d,%d\n", supported, fast );
    hr = SLOT( iface, 8, HRESULT (WINAPI *)( void *, void *, INT64 * ) )( iface, iface /* any object */, &token );
    printf( "caps_changed=%d\n", SUCCEEDED(hr) && token &&
            SUCCEEDED( SLOT( iface, 9, HRESULT (WINAPI *)( void *, INT64 ) )( iface, token ) ) );
}

static void source_parameter( void )
{
    static const WCHAR name[] = L"Windows.UI.Composition.CompositionEffectSourceParameter", source[] = L"Backdrop";
    void *factory = NULL, *param = NULL, *iface, *effect;
    HSTRING hs, src, got = NULL;
    HRESULT hr;

    WindowsCreateString( name, (UINT32)wcslen( name ), &hs );
    hr = RoGetActivationFactory( hs, &probe_IID_ICompositionEffectSourceParameterFactory, &factory );
    WindowsDeleteString( hs );
    printf( "param_factory=%d (%#lx)\n", SUCCEEDED(hr) && factory, hr );
    if (!factory) return;
    WindowsCreateString( source, (UINT32)wcslen( source ), &src );
    hr = SLOT( factory, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( factory, src, &param );
    iface = qi( param, &probe_IID_ICompositionEffectSourceParameter );
    printf( "param_made=%d (%#lx)\n", SUCCEEDED(hr) && iface, hr );
    if (!iface) return;
    SLOT( iface, 6, HRESULT (WINAPI *)( void *, HSTRING * ) )( iface, &got );
    printf( "param_name=%d\n", got && !wcscmp( WindowsGetStringRawBuffer( got, NULL ), source ) );
    effect = qi( param, &probe_IID_IGraphicsEffectSource );
    printf( "param_effect_source=%d\n", effect != NULL );
}

int main( void )
{
    static const WCHAR name[] = L"Windows.UI.Composition.Compositor";
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"compmask" };
    struct ui_color red = { 255, 255, 0, 0 }, white = { 255, 255, 255, 255 }, half = { 128, 0, 0, 0 };
    void *compositor, *compositor2, *desktop, *interop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL;
    void *mask1 = NULL, *mask2 = NULL, *m1, *m2, *got = NULL, *device = NULL, *surface = NULL, *sinterop, *sbrush = NULL;
    IInspectable *inspectable = NULL;
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *d2d_device;
    HSTRING hs, cls = NULL;
    HWND hwnd;
    HDC dc;
    HRESULT hr;
    COLORREF a, b, c;

    RoInitialize( RO_INIT_SINGLETHREADED );
    capabilities();
    source_parameter();
    if (FAILED( hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d, NULL, NULL ) ))
    {
        printf( "no_d3d11=%#lx\n", hr );
        return 0;
    }
    ID3D11Device_QueryInterface( d3d, &IID_IDXGIDevice, (void **)&dxgi );
    D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory );
    ID2D1Factory1_CreateDevice( factory, dxgi, &d2d_device );

    WindowsCreateString( name, (UINT32)wcslen( name ), &hs );
    hr = RoActivateInstance( hs, &inspectable );
    WindowsDeleteString( hs );
    if (FAILED( hr )) { printf( "compositor=0\n" ); return 1; }
    compositor = qi( inspectable, &probe_IID_ICompositor );
    compositor2 = qi( inspectable, &probe_IID_ICompositor2 );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );
    interop = qi( inspectable, &probe_IID_ICompositorInterop );

    hr = SLOT( compositor2, COMPOSITOR2_CREATE_MASK_BRUSH, get_fn )( compositor2, &mask1 );
    m1 = qi( mask1, &probe_IID_ICompositionMaskBrush );
    printf( "mask_brush=%d (%#lx)\n", SUCCEEDED(hr) && m1, hr );
    if (!m1) return 0;
    IInspectable_GetRuntimeClassName( (IInspectable *)m1, &cls );
    printf( "mask_class=%d\n", cls && !wcscmp( WindowsGetStringRawBuffer( cls, NULL ), L"Windows.UI.Composition.CompositionMaskBrush" ) );
    SLOT( compositor2, COMPOSITOR2_CREATE_MASK_BRUSH, get_fn )( compositor2, &mask2 );
    m2 = qi( mask2, &probe_IID_ICompositionMaskBrush );

    /* the left square: red through a black mask at half alpha, on white */
    SLOT( m1, MASK_PUT_SOURCE, put_fn )( m1, color_brush( compositor, red ) );
    SLOT( m1, MASK_PUT_MASK, put_fn )( m1, color_brush( compositor, half ) );
    SLOT( m1, MASK_GET_MASK, get_fn )( m1, &got );
    printf( "mask_roundtrip=%d\n", got != NULL );

    /* the right square: red through a picture opaque on its left half and clear on its right */
    SLOT( interop, INTEROP_CREATE_GRAPHICS_DEVICE, HRESULT (WINAPI *)( void *, IUnknown *, void ** ) )( interop, (IUnknown *)d2d_device, &device );
    if (device)
        SLOT( device, GRAPHICS_DEVICE_CREATE_DRAWING_SURFACE, HRESULT (WINAPI *)( void *, struct vector2, int, int, void ** ) )(
                device, (struct vector2){ 100, 100 }, DXGI_FORMAT_B8G8R8A8_UNORM, 1, &surface );
    sinterop = qi( surface, &probe_IID_ICompositionDrawingSurfaceInterop );
    if (sinterop)
    {
        ID2D1DeviceContext *ctx = NULL;
        ID2D1SolidColorBrush *opaque;
        POINT offset = { 0, 0 };
        D2D1_COLOR_F clear = { 0, 0, 0, 0 }, any = { 0.2f, 0.4f, 0.6f, 1 };

        SLOT( sinterop, SURFACE_INTEROP_BEGIN_DRAW, HRESULT (WINAPI *)( void *, const RECT *, REFIID, void **, POINT * ) )(
                sinterop, NULL, &IID_ID2D1DeviceContext, (void **)&ctx, &offset );
        if (ctx)
        {
            ID2D1RenderTarget_Clear( (ID2D1RenderTarget *)ctx, &clear );
            ID2D1RenderTarget_CreateSolidColorBrush( (ID2D1RenderTarget *)ctx, &any, NULL, &opaque );
            D2D1_RECT_F left = { offset.x, offset.y, offset.x + 50, offset.y + 100 };
            ID2D1RenderTarget_FillRectangle( (ID2D1RenderTarget *)ctx, &left, (ID2D1Brush *)opaque );
            IUnknown_Release( (IUnknown *)ctx );
        }
        SLOT( sinterop, SURFACE_INTEROP_END_DRAW, HRESULT (WINAPI *)( void * ) )( sinterop );
    }
    SLOT( compositor, COMPOSITOR_CREATE_SURFACE_BRUSH_WITH_SURFACE, HRESULT (WINAPI *)( void *, void *, void ** ) )(
            compositor, qi( surface, &probe_IID_ICompositionSurface ), &sbrush );
    printf( "surface_mask=%d\n", sbrush != NULL );
    SLOT( m2, MASK_PUT_SOURCE, put_fn )( m2, color_brush( compositor, red ) );
    SLOT( m2, MASK_PUT_MASK, put_fn )( m2, qi( sbrush, &probe_IID_ICompositionBrush ) );

    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"compmask", L"compmask", WS_POPUP | WS_VISIBLE, 50, 50, 200, 100, NULL, NULL, NULL, NULL );
    pump( 300 );
    SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )( desktop, hwnd, FALSE, &target );
    ctarget = qi( target, &probe_IID_ICompositionTarget );
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, color_brush( compositor, white ), 0, 0, 200, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, qi( m1, &probe_IID_ICompositionBrush ), 0, 0, 100, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, qi( m2, &probe_IID_ICompositionBrush ), 100, 0, 100, 100 ) );
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, qi( root, &probe_IID_IVisual ) );
    pump( 1500 );
    dc = GetDC( hwnd );
    a = GetPixel( dc, 50, 50 );
    b = GetPixel( dc, 125, 50 );
    c = GetPixel( dc, 175, 50 );
    ReleaseDC( hwnd, dc );
    printf( "color_mask=%d (%06lx)\n", close_to( a, 255, 128, 128 ), a );
    printf( "surface_mask_opaque=%d (%06lx)\n", close_to( b, 255, 0, 0 ), b );
    printf( "surface_mask_clear=%d (%06lx)\n", close_to( c, 255, 255, 255 ), c );
    DestroyWindow( hwnd );

    /* a window's system backdrop: under the root, over its client area */
    {
        struct ui_color blue = { 255, 0, 0, 255 };
        void *target2 = NULL, *ctarget2, *backdrop, *root2 = NULL, *got2 = NULL;
        HWND hwnd2 = CreateWindowExW( 0, L"compmask", L"backdrop", WS_POPUP | WS_VISIBLE, 300, 50, 100, 100, NULL, NULL, NULL, NULL );

        pump( 300 );
        SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )( desktop, hwnd2, FALSE, &target2 );
        backdrop = qi( target2, &probe_IID_ICompositionSupportsSystemBackdrop );
        printf( "backdrop_iface=%d\n", backdrop != NULL );
        if (backdrop)
        {
            SLOT( backdrop, 7, put_fn )( backdrop, color_brush( compositor, blue ) );
            SLOT( backdrop, 6, get_fn )( backdrop, &got2 );
            printf( "backdrop_roundtrip=%d\n", got2 != NULL );
            ctarget2 = qi( target2, &probe_IID_ICompositionTarget );
            SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root2 );
            SLOT( ctarget2, TARGET_PUT_ROOT, put_fn )( ctarget2, qi( root2, &probe_IID_IVisual ) );
            pump( 1500 );
            dc = GetDC( hwnd2 );
            a = GetPixel( dc, 50, 50 );
            ReleaseDC( hwnd2, dc );
            printf( "backdrop_drawn=%d (%06lx)\n", close_to( a, 0, 0, 255 ), a );
        }
        DestroyWindow( hwnd2 );
    }
    printf( "done=1\n" );
    return 0;
}
