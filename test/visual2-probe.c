/* visual2-probe: Windows.UI.Composition IVisual2 (patches/sg/7707) -- a visual's offset and size
 * partly relative to its parent's size (the root's parent: its window), as Teams' title bar
 * overlay uses them. Built from test/wuicomp-probe.c's helpers. Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
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
DEFINE_GUID(probe_IID_IVisual2, 0x3052b611, 0x56c3, 0x4c3e, 0x8b, 0xf3, 0xf6, 0xe1, 0xad, 0x47, 0x3f, 0x06);
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


#define V2_GET_PARENT 6
#define V2_PUT_PARENT 7
#define V2_GET_REL_OFFSET 8
#define V2_PUT_REL_OFFSET 9
#define V2_GET_REL_SIZE 10
#define V2_PUT_REL_SIZE 11

static void rel( void *visual, struct vector3 offset, struct vector2 size )
{
    void *v2 = qi( visual, &probe_IID_IVisual2 );
    if (!v2) return;
    SLOT( v2, V2_PUT_REL_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 ) )( v2, offset );
    SLOT( v2, V2_PUT_REL_SIZE, HRESULT (WINAPI *)( void *, struct vector2 ) )( v2, size );
    IUnknown_Release( (IUnknown *)v2 );
}

int main( void )
{
    static const WCHAR name[] = L"Windows.UI.Composition.Compositor";
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"visual2" };
    struct ui_color red = { 255, 255, 0, 0 }, green = { 255, 0, 255, 0 };
    void *compositor, *desktop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL;
    void *red_brush = NULL, *green_brush = NULL, *half, *corner, *rootv, *v2, *parent = (void *)1;
    struct vector3 o3 = { 9, 9, 9 };
    struct vector2 o2 = { 9, 9 };
    IInspectable *inspectable = NULL;
    HSTRING hs;
    HWND hwnd;
    HDC dc;
    HRESULT hr;

    RoInitialize( RO_INIT_SINGLETHREADED );
    WindowsCreateString( name, (UINT32)wcslen( name ), &hs );
    hr = RoActivateInstance( hs, &inspectable );
    WindowsDeleteString( hs );
    if (FAILED( hr )) { printf( "compositor=0\n" ); return 1; }
    compositor = qi( inspectable, &probe_IID_ICompositor );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );

    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"visual2", L"visual2", WS_POPUP | WS_VISIBLE, 50, 50, 200, 100, NULL, NULL, NULL, NULL );
    pump( 300 );
    SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )( desktop, hwnd, FALSE, &target );
    ctarget = qi( target, &probe_IID_ICompositionTarget );
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    rootv = qi( root, &probe_IID_IVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );

    v2 = qi( rootv, &probe_IID_IVisual2 );
    printf( "visual2=%d\n", !!v2 );
    if (!v2) return 1;
    SLOT( v2, V2_GET_REL_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 * ) )( v2, &o3 );
    SLOT( v2, V2_GET_REL_SIZE, HRESULT (WINAPI *)( void *, struct vector2 * ) )( v2, &o2 );
    SLOT( v2, V2_GET_PARENT, get_fn )( v2, &parent );
    printf( "defaults=%d\n", !o3.x && !o3.y && !o3.z && !o2.x && !o2.y && !parent );
    /* the root fills its window */
    rel( rootv, (struct vector3){ 0, 0, 0 }, (struct vector2){ 1, 1 } );
    SLOT( v2, V2_GET_REL_SIZE, HRESULT (WINAPI *)( void *, struct vector2 * ) )( v2, &o2 );
    printf( "roundtrip=%d\n", o2.x == 1 && o2.y == 1 );

    SLOT( compositor, COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR, HRESULT (WINAPI *)( void *, struct ui_color, void ** ) )( compositor, red, &red_brush );
    SLOT( compositor, COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR, HRESULT (WINAPI *)( void *, struct ui_color, void ** ) )( compositor, green, &green_brush );
    /* the right half: no size of its own, half the parent's width, all its height, moved by half its width */
    half = make_sprite( compositor, qi( red_brush, &probe_IID_ICompositionBrush ), 0, 0, 0, 0 );
    rel( half, (struct vector3){ 0.5f, 0, 0 }, (struct vector2){ 0.5f, 1 } );
    /* 20x20 at 10 + half the parent's height down */
    corner = make_sprite( compositor, qi( green_brush, &probe_IID_ICompositionBrush ), 10, 0, 20, 20 );
    rel( corner, (struct vector3){ 0, 0.5f, 0 }, (struct vector2){ 0, 0 } );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, half );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, corner );
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, rootv );
    pump( 1500 );
    dc = GetDC( hwnd );
    printf( "right_half=%d (%06lx)\n", GetPixel( dc, 150, 50 ) == RGB(255, 0, 0), GetPixel( dc, 150, 50 ) );
    printf( "left_half_not=%d (%06lx)\n", GetPixel( dc, 50, 20 ) != RGB(255, 0, 0), GetPixel( dc, 50, 20 ) );
    printf( "corner=%d (%06lx)\n", GetPixel( dc, 20, 60 ) == RGB(0, 255, 0), GetPixel( dc, 20, 60 ) );
    printf( "corner_not_top=%d (%06lx)\n", GetPixel( dc, 20, 10 ) != RGB(0, 255, 0), GetPixel( dc, 20, 10 ) );
    ReleaseDC( hwnd, dc );
    /* the window grows: so does what is relative to it */
    SetWindowPos( hwnd, NULL, 0, 0, 300, 100, SWP_NOMOVE | SWP_NOZORDER );
    /* (a target is drawn at a commit: a resize alone does not commit here, so something is set) */
    SLOT( rootv, VISUAL_PUT_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 ) )( rootv, (struct vector3){ 0, 0, 0 } );
    pump( 1500 );
    dc = GetDC( hwnd );
    printf( "follows_window=%d (%06lx %06lx)\n", GetPixel( dc, 250, 50 ) == RGB(255, 0, 0) && GetPixel( dc, 120, 50 ) != RGB(255, 0, 0),
            GetPixel( dc, 250, 50 ), GetPixel( dc, 120, 50 ) );
    ReleaseDC( hwnd, dc );
    DestroyWindow( hwnd );
    return 0;
}
