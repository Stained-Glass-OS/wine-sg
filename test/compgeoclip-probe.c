/* compgeoclip-probe (wine-sg 9806): ICompositor6::CreateGeometricClip(WithGeometry) and CompositionGeometricClip:
 * a red sprite clipped to a rectangle geometry (ICompositor5's) shows only inside it; Geometry and ViewBox are
 * kept. And CreateRedirectVisual(WithSourceVisual): a RedirectVisual draws its Source's tree again where it is.
 * They were E_NOTIMPL. Interface identity per windows-rs 0.58 (MIT/Apache-2.0). Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(probe_IID_ICompositor5, 0x48ea31ad, 0x7fcd, 0x4076, 0xa7, 0x9c, 0x90, 0xcc, 0x4b, 0x85, 0x2c, 0x9b);
DEFINE_GUID(probe_IID_ICompositor6, 0x7a38b2bd, 0xcec8, 0x4eeb, 0x83, 0x0f, 0xd8, 0xd0, 0x7a, 0xed, 0xeb, 0xc3);
DEFINE_GUID(probe_IID_ICompositionClip, 0x1ccd2a52, 0xcfc7, 0x4ace, 0x99, 0x83, 0x14, 0x6b, 0xb8, 0xeb, 0x6a, 0x3c);
DEFINE_GUID(probe_IID_ICompositionGeometricClip, 0xc840b581, 0x81c9, 0x4444, 0xa2, 0xc1, 0xcc, 0xae, 0xce, 0x3a, 0x50, 0xe5);
DEFINE_GUID(probe_IID_ICompositionRectangleGeometry, 0x0cd51428, 0x5356, 0x4246, 0xae, 0xcf, 0x7a, 0x0b, 0x76, 0x97, 0x54, 0x00);
DEFINE_GUID(probe_IID_IRedirectVisual, 0x8cc6e340, 0x8b75, 0x5422, 0xb0, 0x6f, 0x09, 0xff, 0xe9, 0xf8, 0x61, 0x7e);
DEFINE_GUID(probe_IID_ICompositor, 0xb403ca50, 0x7f8c, 0x4e83, 0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8);
DEFINE_GUID(probe_IID_IVisual, 0x117e202d, 0xa859, 0x4c89, 0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3);
DEFINE_GUID(probe_IID_IContainerVisual, 0x02f6bc74, 0xed20, 0x4773, 0xaf, 0xe6, 0xd4, 0x9b, 0x4a, 0x93, 0xdb, 0x32);
DEFINE_GUID(probe_IID_ICompositionTarget, 0xa1bea8ba, 0xd726, 0x4663, 0x81, 0x29, 0x6b, 0x5e, 0x79, 0x27, 0xff, 0xa6);
DEFINE_GUID(probe_IID_ICompositorDesktopInterop, 0x29e691fa, 0x4567, 0x4dca, 0xb3, 0x19, 0xd0, 0xf2, 0x07, 0xeb, 0x68, 0x07);
DEFINE_GUID(probe_IID_ICompositionBrush, 0xab0d7608, 0x30c0, 0x40e9, 0xb5, 0x68, 0xb6, 0x0a, 0x6b, 0xd1, 0xfb, 0x46);
DEFINE_GUID(probe_IID_ICompositionObject, 0xbcb4ad45, 0x7609, 0x4550, 0x93, 0x4f, 0x16, 0x00, 0x2a, 0x68, 0xfd, 0xed);
DEFINE_GUID(probe_IID_ICompositionEffectBrush, 0xbf7f795e, 0x83cc, 0x44bf, 0xa4, 0x47, 0x3e, 0x3c, 0x07, 0x17, 0x89, 0xec);
DEFINE_GUID(probe_IID_ICompositionEffectFactory, 0xbe5624af, 0xba7e, 0x4510, 0x98, 0x50, 0x41, 0xc0, 0xb4, 0xff, 0x74, 0xdf);
DEFINE_GUID(probe_IID_ICompositionEffectSourceParameterFactory, 0xb3d9f276, 0xaba3, 0x4724, 0xac, 0xf3, 0xd0, 0x39, 0x74, 0x64, 0xdb, 0x1c);
DEFINE_GUID(probe_IID_IGraphicsEffect, 0xcb51c0ce, 0x8fe6, 0x4636, 0xb2, 0x02, 0x86, 0x1f, 0xaa, 0x07, 0xd8, 0xf3);
DEFINE_GUID(probe_IID_IGraphicsEffectSource, 0x2d8f9ddc, 0x4339, 0x4eb9, 0x92, 0x16, 0xf9, 0xde, 0xb7, 0x56, 0x58, 0xa2);
DEFINE_GUID(probe_IID_IGraphicsEffectD2D1Interop, 0x2fc57384, 0xa068, 0x44d7, 0xa3, 0x31, 0x30, 0x98, 0x2f, 0xcf, 0x71, 0x77);
DEFINE_GUID(probe_IID_IPropertyValueStatics, 0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(probe_IID_IPropertyValue, 0x4bd682dd, 0x7554, 0x40e9, 0x9a, 0x9b, 0x82, 0x65, 0x4e, 0xde, 0x7e, 0x62);
DEFINE_GUID(probe_IID_IIterable_HSTRING, 0xe2fcc7c1, 0x3bfc, 0x5a0b, 0xb2, 0xb0, 0x72, 0xe7, 0x69, 0xd1, 0xcb, 0x7e);
DEFINE_GUID(probe_IID_IIterator_HSTRING, 0x8c304ebb, 0x6615, 0x50a4, 0x88, 0x29, 0x87, 0x9e, 0xcd, 0x44, 0x32, 0x36);
DEFINE_GUID(probe_IID_IAgileObject, 0x94ea2b94, 0xe9cc, 0x49e0, 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90);
DEFINE_GUID(probe_CLSID_D2D1Flood, 0x61c23c20, 0xae69, 0x4d8e, 0x94, 0xcf, 0x50, 0x07, 0x8d, 0xf6, 0x38, 0xf2);
DEFINE_GUID(probe_CLSID_D2D1Invert, 0xe0c3784d, 0xcb39, 0x4e84, 0xb6, 0xfd, 0x6b, 0x72, 0xf0, 0x81, 0x02, 0x63);

#define COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR 8
#define COMPOSITOR_CREATE_CONTAINER_VISUAL       9
#define COMPOSITOR_CREATE_EFFECT_FACTORY         11
#define COMPOSITOR_CREATE_EFFECT_FACTORY_WITH_PROPERTIES 12
#define COMPOSITOR_CREATE_SPRITE_VISUAL          22
#define VISUAL_PUT_OFFSET                        21
#define VISUAL_PUT_SIZE                          36
#define CONTAINER_GET_CHILDREN                   6
#define COLLECTION_INSERT_AT_TOP                 9
#define SPRITE_PUT_BRUSH                         7
#define TARGET_PUT_ROOT                          7
#define DESKTOP_INTEROP_CREATE_TARGET            3
#define OBJECT_GET_PROPERTIES                    8
#define PROPSET_INSERT_COLOR                     6
#define COMPOSITOR5_CREATE_RECTANGLE_GEOMETRY    19
#define COMPOSITOR5_CREATE_VIEW_BOX              24
#define COMPOSITOR6_CREATE_GEOMETRIC_CLIP        6
#define COMPOSITOR6_CREATE_GEOMETRIC_CLIP_WITH   7
#define VISUAL_PUT_CLIP                          15
#define COMPOSITOR6_CREATE_REDIRECT              8
#define COMPOSITOR6_CREATE_REDIRECT_WITH         9
#define RECT_PUT_OFFSET                          7
#define RECT_PUT_SIZE                            9

struct ui_color { BYTE a, r, g, b; };
struct vector2 { float x, y; };
struct vector3 { float x, y, z; };
typedef HRESULT (WINAPI *get_fn)( void *, void ** );
typedef HRESULT (WINAPI *put_fn)( void *, void * );

/* ---- composition helpers ---- */
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

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, (UINT32)wcslen( s ), &h );
    return h;
}

int main( void )
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"compgeoclip" };
    struct ui_color red = { 255, 255, 0, 0 }, white = { 255, 255, 255, 255 };
    void *compositor, *c5, *c6, *desktop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL;
    void *redirect = NULL, *rv, *redirect2 = NULL, *rv2, *src = NULL;
    void *geometry = NULL, *rect, *clip = NULL, *clip2 = NULL, *gc, *gc2, *got = NULL, *vb = NULL, *sprite;
    IInspectable *inspectable = NULL;
    ID3D11Device *d3d;
    HRESULT hr;
    HSTRING h;
    HWND hwnd;
    HDC dc;
    COLORREF in, out, edge, rin, rout;

    RoInitialize( RO_INIT_SINGLETHREADED );
    h = hs( L"Windows.UI.Composition.Compositor" );
    hr = RoActivateInstance( h, &inspectable );
    WindowsDeleteString( h );
    if (FAILED( hr )) { printf( "compositor=0\n" ); return 1; }
    compositor = qi( inspectable, &probe_IID_ICompositor );
    c5 = qi( inspectable, &probe_IID_ICompositor5 );
    c6 = qi( inspectable, &probe_IID_ICompositor6 );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );
    printf( "compositor6=%d\n", c5 && c6 );
    if (!c5 || !c6) return 1;

    SLOT( c5, COMPOSITOR5_CREATE_RECTANGLE_GEOMETRY, get_fn )( c5, &geometry );
    rect = qi( geometry, &probe_IID_ICompositionRectangleGeometry );
    SLOT( rect, RECT_PUT_OFFSET, HRESULT (WINAPI *)( void *, struct vector2 ) )( rect, (struct vector2){ 10, 10 } );
    SLOT( rect, RECT_PUT_SIZE, HRESULT (WINAPI *)( void *, struct vector2 ) )( rect, (struct vector2){ 30, 30 } );

    hr = SLOT( c6, COMPOSITOR6_CREATE_GEOMETRIC_CLIP, get_fn )( c6, &clip );
    gc = qi( clip, &probe_IID_ICompositionGeometricClip );
    printf( "clip=%d (%#lx)\n", SUCCEEDED(hr) && gc && qi( clip, &probe_IID_ICompositionClip ), hr );
    printf( "clip_class=%d\n", is_class( gc, L"Windows.UI.Composition.CompositionGeometricClip" ) );
    if (gc)
    {
        SLOT( gc, 7, put_fn )( gc, geometry );
        SLOT( gc, 6, get_fn )( gc, &got );
        SLOT( c5, COMPOSITOR5_CREATE_VIEW_BOX, get_fn )( c5, &vb );
        SLOT( gc, 9, put_fn )( gc, vb );
        SLOT( gc, 9, put_fn )( gc, NULL );
        printf( "clip_props=%d\n", got != NULL );
    }
    hr = SLOT( c6, COMPOSITOR6_CREATE_GEOMETRIC_CLIP_WITH, HRESULT (WINAPI *)( void *, void *, void ** ) )( c6, geometry, &clip2 );
    gc2 = qi( clip2, &probe_IID_ICompositionGeometricClip );
    got = NULL;
    if (gc2) SLOT( gc2, 6, get_fn )( gc2, &got );
    printf( "clip_with=%d (%#lx)\n", SUCCEEDED(hr) && got != NULL, hr );

    hr = SLOT( c6, COMPOSITOR6_CREATE_REDIRECT, get_fn )( c6, &redirect );
    rv = qi( redirect, &probe_IID_IRedirectVisual );
    printf( "redirect=%d (%#lx)\n", SUCCEEDED(hr) && rv && qi( rv, &probe_IID_IContainerVisual ), hr );
    printf( "redirect_class=%d\n", is_class( rv, L"Windows.UI.Composition.RedirectVisual" ) );

    if (FAILED( hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d, NULL, NULL ) ))
    {
        printf( "no_d3d11=%#lx\n", hr );
        printf( "done=1\n" );
        return 0;
    }
    if (!gc) { printf( "done=1\n" ); return 0; }
    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"compgeoclip", L"compgeoclip", WS_POPUP | WS_VISIBLE, 50, 50, 200, 100, NULL, NULL, NULL, NULL );
    pump( 300 );
    SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )( desktop, hwnd, FALSE, &target );
    ctarget = qi( target, &probe_IID_ICompositionTarget );
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, color_brush( compositor, white ), 0, 0, 200, 100 ) );
    sprite = make_sprite( compositor, color_brush( compositor, red ), 0, 0, 100, 100 );
    SLOT( sprite, VISUAL_PUT_CLIP, put_fn )( sprite, qi( clip, &probe_IID_ICompositionClip ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, sprite );
    if (rv)
    {
        void *rvis = qi( rv, &probe_IID_IVisual );
        SLOT( rv, 7, put_fn )( rv, sprite );
        SLOT( rv, 6, get_fn )( rv, &src );
        printf( "redirect_source=%d\n", src != NULL );
        SLOT( rvis, VISUAL_PUT_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 ) )( rvis, (struct vector3){ 100, 0, 0 } );
        SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, rvis );
        hr = SLOT( c6, COMPOSITOR6_CREATE_REDIRECT_WITH, HRESULT (WINAPI *)( void *, void *, void ** ) )( c6, sprite, &redirect2 );
        rv2 = qi( redirect2, &probe_IID_IRedirectVisual );
        src = NULL;
        if (rv2) SLOT( rv2, 6, get_fn )( rv2, &src );
        printf( "redirect_with=%d (%#lx)\n", SUCCEEDED(hr) && src != NULL, hr );
    }
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, qi( root, &probe_IID_IVisual ) );
    pump( 1500 );
    dc = GetDC( hwnd );
    in = GetPixel( dc, 25, 25 );
    out = GetPixel( dc, 70, 70 );
    edge = GetPixel( dc, 5, 25 );
    rin = GetPixel( dc, 125, 25 );
    rout = GetPixel( dc, 170, 70 );
    ReleaseDC( hwnd, dc );
    printf( "inside_drawn=%d (%06lx)\n", close_to( in, 255, 0, 0 ), in );
    printf( "outside_clipped=%d (%06lx %06lx)\n", close_to( out, 255, 255, 255 ) && close_to( edge, 255, 255, 255 ), out, edge );
    printf( "redirect_drawn=%d (%06lx %06lx)\n", close_to( rin, 255, 0, 0 ) && close_to( rout, 255, 255, 255 ), rin, rout );
    DestroyWindow( hwnd );
    printf( "done=1\n" );
    return 0;
}
