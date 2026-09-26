/* wuicomp-probe: Windows.UI.Composition's first slice, and the Direct3D 11,
 * DXGI and user32 pieces Paint.NET needs around it (patches/sg/0229-0234).
 *
 *  - Windows.UI.Composition.Compositor is activatable; a desktop window
 *    target made through ICompositorDesktopInterop shows a container visual
 *    whose children are a sprite visual with a color brush and a sprite
 *    visual with a surface brush, the surface a composition drawing surface
 *    drawn with Direct2D through ICompositionDrawingSurfaceInterop. The
 *    probe reads the window's pixels;
 *  - Direct3D 11 devices are ID3D11Device5 and their immediate contexts
 *    ID3D11DeviceContext4; DXGI devices are IDXGIDevice4;
 *  - SetWindowFeedbackSetting and GetWindowFeedbackSetting work;
 *  - WIC converts to 64bppPRGBAHalf (linear) and Direct2D makes a bitmap of
 *    that format.
 *
 * Interfaces the MinGW headers lack are called through their vtable slots;
 * their IIDs are the public ones.
 *
 * Prints name=value lines; see test/wuicomp-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(probe_IID_ICompositor, 0xb403ca50, 0x7f8c, 0x4e83, 0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8);
DEFINE_GUID(probe_IID_IVisual, 0x117e202d, 0xa859, 0x4c89, 0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3);
DEFINE_GUID(probe_IID_IContainerVisual, 0x02f6bc74, 0xed20, 0x4773, 0xaf, 0xe6, 0xd4, 0x9b, 0x4a, 0x93, 0xdb, 0x32);
DEFINE_GUID(probe_IID_ICompositionTarget, 0xa1bea8ba, 0xd726, 0x4663, 0x81, 0x29, 0x6b, 0x5e, 0x79, 0x27, 0xff, 0xa6);
DEFINE_GUID(probe_IID_ICompositorDesktopInterop, 0x29e691fa, 0x4567, 0x4dca, 0xb3, 0x19, 0xd0, 0xf2, 0x07, 0xeb, 0x68, 0x07);
DEFINE_GUID(probe_IID_ICompositorInterop, 0x25297d5c, 0x3ad4, 0x4c9c, 0xb5, 0xcf, 0xe3, 0x6a, 0x38, 0x51, 0x23, 0x30);
DEFINE_GUID(probe_IID_ICompositionDrawingSurfaceInterop, 0xfd04e6e3, 0xfe0c, 0x4c3c, 0xab, 0x19, 0xa0, 0x76, 0x01, 0xa5, 0x76, 0xee);
DEFINE_GUID(probe_IID_ICompositionBrush, 0xab0d7608, 0x30c0, 0x40e9, 0xb5, 0x68, 0xb6, 0x0a, 0x6b, 0xd1, 0xfb, 0x46);
DEFINE_GUID(probe_IID_ICompositionSurface, 0x1527540d, 0x42c7, 0x47a6, 0xa4, 0x08, 0x66, 0x8f, 0x79, 0xa9, 0x0d, 0xfb);
DEFINE_GUID(probe_IID_ID3D11Device5, 0x8ffde202, 0xa0e7, 0x45df, 0x9e, 0x01, 0xe8, 0x37, 0x80, 0x1b, 0x5e, 0xa0);
DEFINE_GUID(probe_IID_ID3D11DeviceContext4, 0x917600da, 0xf58c, 0x4c33, 0x98, 0xd8, 0x3e, 0x15, 0xb3, 0x90, 0xfa, 0x24);
DEFINE_GUID(probe_IID_IDXGIDevice4, 0x95b4f95f, 0xd8da, 0x4ca4, 0x9e, 0xe6, 0x3b, 0x76, 0xd5, 0x96, 0x8a, 0x10);
DEFINE_GUID(probe_WICPixelFormat64bppPRGBAHalf, 0x58ad26c2, 0xc623, 0x4d9d, 0xb3, 0x20, 0x38, 0x7e, 0x49, 0xf8, 0xc4, 0x42);

/* vtable slots, counted from the IDL */
#define COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR 8
#define COMPOSITOR_CREATE_CONTAINER_VISUAL       9
#define COMPOSITOR_CREATE_SPRITE_VISUAL          22
#define COMPOSITOR_CREATE_SURFACE_BRUSH_WITH_SURFACE 24
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
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        Sleep( 10 );
    }
}

static void *qi( void *obj, const GUID *iid )
{
    void *out = NULL;
    if (obj) IUnknown_QueryInterface( (IUnknown *)obj, iid, &out );
    return out;
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
    IUnknown_Release( (IUnknown *)sprite );
    return visual;
}

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void check_composition( ID2D1Device *d2d_device )
{
    static const WCHAR name[] = L"Windows.UI.Composition.Compositor";
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"wuicomp" };
    struct ui_color red = { 255, 255, 0, 0 };
    void *compositor = NULL, *desktop, *interop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL;
    void *red_brush = NULL, *sprite1, *sprite2, *device = NULL, *surface = NULL, *surface_interop, *surface_brush = NULL;
    void *csurface, *cbrush;
    IInspectable *inspectable = NULL;
    HSTRING hs;
    HWND hwnd;
    HDC dc;
    COLORREF left = 0xdeadbeef, right = 0xdeadbeef, outside = 0xdeadbeef;
    HRESULT hr;

    WindowsCreateString( name, (UINT32)wcslen( name ), &hs );
    hr = RoActivateInstance( hs, &inspectable );
    WindowsDeleteString( hs );
    printf( "compositor_activated=%d (%#lx)\n", SUCCEEDED(hr), hr );
    if (FAILED( hr )) return;
    compositor = qi( inspectable, &probe_IID_ICompositor );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );
    interop = qi( inspectable, &probe_IID_ICompositorInterop );
    printf( "compositor_interfaces=%d\n", compositor && desktop && interop );
    if (!compositor || !desktop || !interop) return;

    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"wuicomp", L"wuicomp", WS_POPUP | WS_VISIBLE, 50, 50, 200, 100, NULL, NULL, NULL, NULL );
    pump( 300 );

    hr = SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )(
            desktop, hwnd, FALSE, &target );
    printf( "desktop_target=%d (%#lx)\n", SUCCEEDED(hr) && target, hr );
    if (FAILED( hr )) return;
    ctarget = qi( target, &probe_IID_ICompositionTarget );

    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );

    /* a red sprite on the left */
    SLOT( compositor, COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR, HRESULT (WINAPI *)( void *, struct ui_color, void ** ) )(
            compositor, red, &red_brush );
    cbrush = qi( red_brush, &probe_IID_ICompositionBrush );
    sprite1 = make_sprite( compositor, cbrush, 0, 0, 100, 100 );

    /* a drawing surface, drawn green with Direct2D, on the right */
    hr = SLOT( interop, INTEROP_CREATE_GRAPHICS_DEVICE, HRESULT (WINAPI *)( void *, IUnknown *, void ** ) )(
            interop, (IUnknown *)d2d_device, &device );
    if (SUCCEEDED( hr ))
        hr = SLOT( device, GRAPHICS_DEVICE_CREATE_DRAWING_SURFACE,
                   HRESULT (WINAPI *)( void *, struct vector2, int, int, void ** ) )(
                device, (struct vector2){ 100, 100 }, DXGI_FORMAT_B8G8R8A8_UNORM, 1 /* premultiplied */, &surface );
    printf( "drawing_surface=%d (%#lx)\n", SUCCEEDED(hr) && surface, hr );
    surface_interop = qi( surface, &probe_IID_ICompositionDrawingSurfaceInterop );
    if (surface_interop)
    {
        ID2D1DeviceContext *ctx = NULL;
        POINT offset = { 0, 0 };
        D2D1_COLOR_F green = { 0, 1, 0, 1 };

        hr = SLOT( surface_interop, SURFACE_INTEROP_BEGIN_DRAW,
                   HRESULT (WINAPI *)( void *, const RECT *, REFIID, void **, POINT * ) )(
                surface_interop, NULL, &IID_ID2D1DeviceContext, (void **)&ctx, &offset );
        printf( "surface_begin_draw=%d (%#lx)\n", SUCCEEDED(hr) && ctx, hr );
        if (ctx)
        {
            ID2D1RenderTarget_Clear( (ID2D1RenderTarget *)ctx, &green );
            IUnknown_Release( (IUnknown *)ctx );
        }
        hr = SLOT( surface_interop, SURFACE_INTEROP_END_DRAW, HRESULT (WINAPI *)( void * ) )( surface_interop );
        printf( "surface_end_draw=%d (%#lx)\n", SUCCEEDED(hr), hr );
    }
    csurface = qi( surface, &probe_IID_ICompositionSurface );
    SLOT( compositor, COMPOSITOR_CREATE_SURFACE_BRUSH_WITH_SURFACE, HRESULT (WINAPI *)( void *, void *, void ** ) )(
            compositor, csurface, &surface_brush );
    sprite2 = make_sprite( compositor, qi( surface_brush, &probe_IID_ICompositionBrush ), 100, 0, 100, 50 );

    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, sprite1 );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, sprite2 );
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, qi( root, &probe_IID_IVisual ) );

    /* changes are committed from the thread's message loop */
    pump( 1500 );
    dc = GetDC( hwnd );
    left = GetPixel( dc, 50, 50 );
    right = GetPixel( dc, 150, 25 );
    outside = GetPixel( dc, 150, 75 );
    ReleaseDC( hwnd, dc );
    printf( "window_left=%d (%06lx)\n", left == RGB(255, 0, 0), left );
    printf( "window_right=%d (%06lx)\n", right == RGB(0, 255, 0), right );
    printf( "window_uncovered=%d (%06lx)\n", outside != RGB(255, 0, 0) && outside != RGB(0, 255, 0), outside );
    DestroyWindow( hwnd );
}

static void check_devices( ID3D11Device *d3d )
{
    ID3D11DeviceContext *immediate;
    IUnknown *device5, *context4, *dxgi4;

    device5 = qi( d3d, &probe_IID_ID3D11Device5 );
    ID3D11Device_GetImmediateContext( d3d, &immediate );
    context4 = qi( immediate, &probe_IID_ID3D11DeviceContext4 );
    dxgi4 = qi( d3d, &probe_IID_IDXGIDevice4 );
    printf( "d3d11_device5=%d\n", device5 != NULL );
    printf( "d3d11_context4=%d\n", context4 != NULL );
    printf( "dxgi_device4=%d\n", dxgi4 != NULL );
    if (device5) IUnknown_Release( device5 );
    if (context4) IUnknown_Release( context4 );
    if (dxgi4) IUnknown_Release( dxgi4 );
    ID3D11DeviceContext_Release( immediate );

}


/* last: a build without it has only a stub that ends the process */
static void check_feedback( void )
{
    BOOL enabled = TRUE, got = TRUE;
    HWND hwnd;

    hwnd = CreateWindowExW( 0, L"static", L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL );
    enabled = FALSE;
    /* FEEDBACK_TOUCH_CONTACTVISUALIZATION */
    printf( "feedback_set=%d\n", SetWindowFeedbackSetting( hwnd, 1, 0, sizeof(enabled), &enabled ) );
    {
        UINT32 size = sizeof(got);
        printf( "feedback_get=%d\n", GetWindowFeedbackSetting( hwnd, 1, 0, &size, &got ) && !got );
    }
    DestroyWindow( hwnd );
}

/* 32bppBGRA ff102030 as linear premultiplied halves: r 0.0052, g 0.0144, b 0.0296, a 1 */
static void check_wic_half( ID2D1DeviceContext *ctx )
{
    DWORD pixel = 0xff102030;
    IWICImagingFactory *factory;
    IWICBitmap *bitmap;
    IWICBitmapSource *half = NULL;
    WORD px[4] = { 0 };
    ID2D1Bitmap *d2d_bitmap = NULL;
    HRESULT hr;

    if (FAILED( CoCreateInstance( &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                                  (void **)&factory ) ))
        return;
    IWICImagingFactory_CreateBitmapFromMemory( factory, 1, 1, &GUID_WICPixelFormat32bppBGRA, 4, 4, (BYTE *)&pixel,
                                               &bitmap );
    hr = WICConvertBitmapSource( &probe_WICPixelFormat64bppPRGBAHalf, (IWICBitmapSource *)bitmap, &half );
    if (SUCCEEDED( hr )) hr = IWICBitmapSource_CopyPixels( half, NULL, 8, 8, (BYTE *)px );
    printf( "wic_half=%d (%#lx %04x %04x %04x %04x)\n", SUCCEEDED(hr) && px[0] == 0x1d4e && px[1] == 0x2365
            && px[2] == 0x2791 && px[3] == 0x3c00, hr, px[0], px[1], px[2], px[3] );
    if (half)
    {
        hr = ID2D1RenderTarget_CreateBitmapFromWicBitmap( (ID2D1RenderTarget *)ctx, half, NULL, &d2d_bitmap );
        printf( "d2d_from_wic_half=%d (%#lx)\n", SUCCEEDED(hr), hr );
        if (d2d_bitmap) ID2D1Bitmap_Release( d2d_bitmap );
        IWICBitmapSource_Release( half );
    }
    IWICBitmap_Release( bitmap );
    IWICImagingFactory_Release( factory );
}

int main( void )
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *ctx;
    HRESULT hr;

    RoInitialize( RO_INIT_SINGLETHREADED );
    if (FAILED( hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d, NULL, NULL ) ))
    {
        printf( "no_d3d11=%#lx\n", hr );
        return 1;
    }
    ID3D11Device_QueryInterface( d3d, &IID_IDXGIDevice, (void **)&dxgi );
    D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory );
    ID2D1Factory1_CreateDevice( factory, dxgi, &device );
    ID2D1Device_CreateDeviceContext( device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &ctx );

    check_devices( d3d );
    check_wic_half( ctx );
    check_composition( device );
    fflush( stdout );
    check_feedback();
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
