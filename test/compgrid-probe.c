/* compgrid-probe (wine-sg 8215): ICompositor2's DropShadow (with ISpriteVisual2.Shadow), CompositionNineGridBrush
 * (drawn: corners and edges of its source, a hollow centre left out) and CompositionBackdropBrush (what is
 * behind shows). They were E_NOTIMPL. Built from test/compmask-probe.c; interface identity per windows-rs 0.30
 * (MIT OR Apache-2.0). Prints name=value lines.
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
DEFINE_GUID(probe_IID_IDropShadow, 0xcb977c07, 0xa154, 0x4851, 0x85, 0xe7, 0xa8, 0x92, 0x4c, 0x84, 0xfa, 0xd8);
DEFINE_GUID(probe_IID_ICompositionShadow, 0x329e52e2, 0x4335, 0x49cc, 0xb1, 0x4a, 0x37, 0x78, 0x2d, 0x10, 0xf0, 0xc4);
DEFINE_GUID(probe_IID_ICompositionNineGridBrush, 0xf25154e4, 0xbc8c, 0x4be7, 0xb8, 0x0f, 0x86, 0x85, 0xb8, 0x3c, 0x01, 0x86);
DEFINE_GUID(probe_IID_ICompositionBackdropBrush, 0xc5acae58, 0x3898, 0x499e, 0x8d, 0x7f, 0x22, 0x4e, 0x91, 0x28, 0x6a, 0x5d);
DEFINE_GUID(probe_IID_ISpriteVisual2, 0x588c9664, 0x997a, 0x4850, 0x91, 0xfe, 0x53, 0xcb, 0x58, 0xf8, 0x1c, 0xe9);
DEFINE_GUID(probe_IID_ISpriteVisual, 0x08e05581, 0x1ad1, 0x4f97, 0x97, 0x57, 0x40, 0x2d, 0x76, 0xe4, 0x23, 0x3b);
DEFINE_GUID(probe_IID_ICompositionCapabilitiesStatics, 0xf7b7a86e, 0x6416, 0x49e5, 0x8d, 0xdf, 0xaf, 0xe9, 0x49, 0xe2, 0x05, 0x62);

#define COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR 8
#define COMPOSITOR_CREATE_CONTAINER_VISUAL       9
#define COMPOSITOR_CREATE_SPRITE_VISUAL          22
#define COMPOSITOR_CREATE_SURFACE_BRUSH_WITH_SURFACE 24
#define COMPOSITOR2_CREATE_BACKDROP_BRUSH        8
#define COMPOSITOR2_CREATE_DROP_SHADOW           10
#define COMPOSITOR2_CREATE_NINE_GRID_BRUSH       14
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


static int is_class( void *obj, const WCHAR *want )
{
    HSTRING cls = NULL;
    int ret;
    if (!obj || FAILED( IInspectable_GetRuntimeClassName( (IInspectable *)obj, &cls ) )) return 0;
    ret = !wcscmp( WindowsGetStringRawBuffer( cls, NULL ), want );
    WindowsDeleteString( cls );
    return ret;
}

int main( void )
{
    static const WCHAR name[] = L"Windows.UI.Composition.Compositor";
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"compgrid" };
    struct ui_color red = { 255, 255, 0, 0 }, white = { 255, 255, 255, 255 }, blue = { 255, 0, 0, 255 }, got_color = { 0 };
    void *compositor, *compositor2, *desktop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL;
    void *shadow = NULL, *ds, *grid = NULL, *ng, *backdrop = NULL, *bd, *sprite = NULL, *sprite2, *got = NULL, *gshadow;
    IInspectable *inspectable = NULL;
    ID3D11Device *d3d;
    float radius = 0, opacity = 0, inset = 0;
    boolean hollow = 0;
    HSTRING hs;
    HWND hwnd;
    HDC dc;
    HRESULT hr;
    COLORREF a, b, c, d;

    RoInitialize( RO_INIT_SINGLETHREADED );
    WindowsCreateString( name, (UINT32)wcslen( name ), &hs );
    hr = RoActivateInstance( hs, &inspectable );
    WindowsDeleteString( hs );
    if (FAILED( hr )) { printf( "compositor=0\n" ); return 1; }
    compositor = qi( inspectable, &probe_IID_ICompositor );
    compositor2 = qi( inspectable, &probe_IID_ICompositor2 );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );

    /* DropShadow */
    hr = SLOT( compositor2, COMPOSITOR2_CREATE_DROP_SHADOW, get_fn )( compositor2, &shadow );
    ds = qi( shadow, &probe_IID_IDropShadow );
    printf( "drop_shadow=%d (%#lx)\n", SUCCEEDED(hr) && ds, hr );
    printf( "drop_shadow_class=%d\n", is_class( ds, L"Windows.UI.Composition.DropShadow" ) );
    printf( "drop_shadow_is_shadow=%d\n", qi( ds, &probe_IID_ICompositionShadow ) != NULL );
    if (ds)
    {
        SLOT( ds, 6, HRESULT (WINAPI *)( void *, float * ) )( ds, &radius );
        SLOT( ds, 14, HRESULT (WINAPI *)( void *, float * ) )( ds, &opacity );
        SLOT( ds, 8, HRESULT (WINAPI *)( void *, struct ui_color * ) )( ds, &got_color );
        printf( "drop_shadow_defaults=%d (%.1f %.1f %02x%02x%02x%02x)\n", radius == 9.0f && opacity == 1.0f &&
                got_color.a == 255 && !got_color.r && !got_color.g && !got_color.b, radius, opacity,
                got_color.a, got_color.r, got_color.g, got_color.b );
        SLOT( ds, 7, HRESULT (WINAPI *)( void *, float ) )( ds, 20.0f );
        SLOT( ds, 6, HRESULT (WINAPI *)( void *, float * ) )( ds, &radius );
        printf( "drop_shadow_radius=%d\n", radius == 20.0f );
    }
    SLOT( compositor, COMPOSITOR_CREATE_SPRITE_VISUAL, get_fn )( compositor, &sprite );
    sprite2 = qi( sprite, &probe_IID_ISpriteVisual2 );
    printf( "sprite2=%d\n", sprite2 != NULL );
    if (sprite2 && ds)
    {
        gshadow = qi( ds, &probe_IID_ICompositionShadow );
        hr = SLOT( sprite2, 7, put_fn )( sprite2, gshadow );
        SLOT( sprite2, 6, get_fn )( sprite2, &got );
        printf( "sprite_shadow=%d (%#lx)\n", SUCCEEDED(hr) && got && qi( got, &probe_IID_IDropShadow ) == qi( ds, &probe_IID_IDropShadow ), hr );
    }

    /* CompositionNineGridBrush */
    hr = SLOT( compositor2, COMPOSITOR2_CREATE_NINE_GRID_BRUSH, get_fn )( compositor2, &grid );
    ng = qi( grid, &probe_IID_ICompositionNineGridBrush );
    printf( "nine_grid=%d (%#lx)\n", SUCCEEDED(hr) && ng, hr );
    printf( "nine_grid_class=%d\n", is_class( ng, L"Windows.UI.Composition.CompositionNineGridBrush" ) );
    if (ng)
    {
        SLOT( ng, 21, put_fn )( ng, color_brush( compositor, red ) );
        SLOT( ng, 26, HRESULT (WINAPI *)( void *, float ) )( ng, 10.0f );
        SLOT( ng, 11, HRESULT (WINAPI *)( void *, boolean ) )( ng, 1 );
        SLOT( ng, 12, HRESULT (WINAPI *)( void *, float * ) )( ng, &inset );
        SLOT( ng, 10, HRESULT (WINAPI *)( void *, boolean * ) )( ng, &hollow );
        got = NULL;
        SLOT( ng, 20, get_fn )( ng, &got );
        printf( "nine_grid_props=%d\n", inset == 10.0f && hollow && got );
    }

    /* CompositionBackdropBrush */
    hr = SLOT( compositor2, COMPOSITOR2_CREATE_BACKDROP_BRUSH, get_fn )( compositor2, &backdrop );
    bd = qi( backdrop, &probe_IID_ICompositionBackdropBrush );
    printf( "backdrop_brush=%d (%#lx)\n", SUCCEEDED(hr) && bd, hr );
    printf( "backdrop_class=%d\n", is_class( bd, L"Windows.UI.Composition.CompositionBackdropBrush" ) );

    if (FAILED( hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d, NULL, NULL ) ))
    {
        printf( "no_d3d11=%#lx\n", hr );
        printf( "done=1\n" );
        return 0;
    }
    if (!ng || !bd) { printf( "done=1\n" ); return 0; }

    /* white everywhere; the nine-grid over the left square, the backdrop brush over a blue right square */
    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"compgrid", L"compgrid", WS_POPUP | WS_VISIBLE, 50, 50, 200, 100, NULL, NULL, NULL, NULL );
    pump( 300 );
    SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )( desktop, hwnd, FALSE, &target );
    ctarget = qi( target, &probe_IID_ICompositionTarget );
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, color_brush( compositor, white ), 0, 0, 200, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, color_brush( compositor, blue ), 100, 0, 100, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, qi( ng, &probe_IID_ICompositionBrush ), 0, 0, 100, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, qi( bd, &probe_IID_ICompositionBrush ), 100, 0, 100, 100 ) );
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, qi( root, &probe_IID_IVisual ) );
    pump( 1500 );
    dc = GetDC( hwnd );
    a = GetPixel( dc, 4, 50 );
    b = GetPixel( dc, 50, 4 );
    c = GetPixel( dc, 50, 50 );
    d = GetPixel( dc, 150, 50 );
    ReleaseDC( hwnd, dc );
    printf( "nine_grid_edge=%d (%06lx %06lx)\n", close_to( a, 255, 0, 0 ) && close_to( b, 255, 0, 0 ), a, b );
    printf( "nine_grid_hollow=%d (%06lx)\n", close_to( c, 255, 255, 255 ), c );
    printf( "backdrop_shows_behind=%d (%06lx)\n", close_to( d, 0, 0, 255 ), d );
    DestroyWindow( hwnd );
    printf( "done=1\n" );
    return 0;
}
