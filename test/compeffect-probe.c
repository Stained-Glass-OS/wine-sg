/* compeffect-probe (wine-sg 9804): ICompositor::CreateEffectFactory / CreateEffectFactoryWithProperties and
 * CompositionEffectBrush. The effect descriptions are the probe's own IGraphicsEffect objects with
 * IGraphicsEffectD2D1Interop (as Win2D's effects are): a Flood (ColorSourceEffect), an Invert over a source
 * parameter bound to a green colour brush, and a Flood whose Color is animatable ("Flood.Color", mapped
 * COLOR_TO_VECTOR4) set through the brush's Properties. The brushes are drawn on sprite visuals in a window
 * and read back. They were E_NOTIMPL. IGraphicsEffectD2D1Interop's IID and method order per windows-rs 0.58
 * (MIT/Apache-2.0, generated from the Windows metadata). Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

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
#define STATICS_CREATE_SINGLE_ARRAY              33

struct ui_color { BYTE a, r, g, b; };
struct vector2 { float x, y; };
struct vector3 { float x, y, z; };
typedef HRESULT (WINAPI *get_fn)( void *, void ** );
typedef HRESULT (WINAPI *put_fn)( void *, void * );

static void *statics;

/* ---- an effect description: IGraphicsEffect (+ IGraphicsEffectSource) and IGraphicsEffectD2D1Interop ---- */
struct effect
{
    const void * const *effect_vtbl;   /* IGraphicsEffect, also its IGraphicsEffectSource and IInspectable */
    const void * const *interop_vtbl;
    LONG ref;
    GUID clsid;
    HSTRING name;
    UINT prop_count;
    IInspectable *props[4];
    UINT source_count;
    IInspectable *sources[4];
    const WCHAR *mapped;               /* a property GetNamedPropertyMapping knows */
    UINT mapped_index, mapping;
    int with_interop;
};

static struct effect *from_effect( void *iface ) { return CONTAINING_RECORD( iface, struct effect, effect_vtbl ); }
static struct effect *from_interop( void *iface ) { return CONTAINING_RECORD( iface, struct effect, interop_vtbl ); }

static HRESULT WINAPI eff_QueryInterface( void *iface, REFIID iid, void **out )
{
    struct effect *e = from_effect( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &probe_IID_IAgileObject ) ||
        IsEqualGUID( iid, &probe_IID_IGraphicsEffect ) || IsEqualGUID( iid, &probe_IID_IGraphicsEffectSource ))
        *out = &e->effect_vtbl;
    else if (e->with_interop && IsEqualGUID( iid, &probe_IID_IGraphicsEffectD2D1Interop ))
        *out = &e->interop_vtbl;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement( &e->ref );
    return S_OK;
}
static ULONG WINAPI eff_AddRef( void *iface ) { return InterlockedIncrement( &from_effect( iface )->ref ); }
static ULONG WINAPI eff_Release( void *iface ) { return InterlockedDecrement( &from_effect( iface )->ref ); }
static HRESULT WINAPI eff_GetIids( void *iface, ULONG *count, IID **iids ) { *count = 0; *iids = NULL; return S_OK; }
static HRESULT WINAPI eff_GetRuntimeClassName( void *iface, HSTRING *name ) { *name = NULL; return S_OK; }
static HRESULT WINAPI eff_GetTrustLevel( void *iface, TrustLevel *level ) { *level = BaseTrust; return S_OK; }
static HRESULT WINAPI eff_get_Name( void *iface, HSTRING *name ) { return WindowsDuplicateString( from_effect( iface )->name, name ); }
static HRESULT WINAPI eff_put_Name( void *iface, HSTRING name )
{
    struct effect *e = from_effect( iface );
    WindowsDeleteString( e->name );
    return WindowsDuplicateString( name, &e->name );
}
static const void * const effect_vtbl[] =
{
    eff_QueryInterface, eff_AddRef, eff_Release, eff_GetIids, eff_GetRuntimeClassName, eff_GetTrustLevel,
    eff_get_Name, eff_put_Name,
};

static HRESULT WINAPI ei_QueryInterface( void *iface, REFIID iid, void **out ) { return eff_QueryInterface( &from_interop( iface )->effect_vtbl, iid, out ); }
static ULONG WINAPI ei_AddRef( void *iface ) { return eff_AddRef( &from_interop( iface )->effect_vtbl ); }
static ULONG WINAPI ei_Release( void *iface ) { return eff_Release( &from_interop( iface )->effect_vtbl ); }
static HRESULT WINAPI ei_GetEffectId( void *iface, GUID *id ) { *id = from_interop( iface )->clsid; return S_OK; }
static HRESULT WINAPI ei_GetNamedPropertyMapping( void *iface, const WCHAR *name, UINT *index, UINT *mapping )
{
    struct effect *e = from_interop( iface );
    if (!e->mapped || wcscmp( name, e->mapped )) return E_INVALIDARG;
    *index = e->mapped_index;
    *mapping = e->mapping;
    return S_OK;
}
static HRESULT WINAPI ei_GetPropertyCount( void *iface, UINT *count ) { *count = from_interop( iface )->prop_count; return S_OK; }
static HRESULT WINAPI ei_GetProperty( void *iface, UINT index, IInspectable **value )
{
    struct effect *e = from_interop( iface );
    if (index >= e->prop_count) return E_INVALIDARG;
    return IInspectable_QueryInterface( e->props[index], &probe_IID_IPropertyValue, (void **)value );
}
static HRESULT WINAPI ei_GetSource( void *iface, UINT index, IInspectable **source )
{
    struct effect *e = from_interop( iface );
    if (index >= e->source_count) return E_INVALIDARG;
    return IInspectable_QueryInterface( e->sources[index], &probe_IID_IGraphicsEffectSource, (void **)source );
}
static HRESULT WINAPI ei_GetSourceCount( void *iface, UINT *count ) { *count = from_interop( iface )->source_count; return S_OK; }
static const void * const interop_vtbl[] =
{
    ei_QueryInterface, ei_AddRef, ei_Release, ei_GetEffectId, ei_GetNamedPropertyMapping, ei_GetPropertyCount,
    ei_GetProperty, ei_GetSource, ei_GetSourceCount,
};

static void effect_init( struct effect *e, const GUID *clsid, const WCHAR *name )
{
    memset( e, 0, sizeof(*e) );
    e->effect_vtbl = effect_vtbl;
    e->interop_vtbl = interop_vtbl;
    e->ref = 1;
    e->clsid = *clsid;
    e->with_interop = 1;
    if (name) WindowsCreateString( name, (UINT32)wcslen( name ), &e->name );
}

static IInspectable *floats( const float *f, UINT n )
{
    IInspectable *v = NULL;
    SLOT( statics, STATICS_CREATE_SINGLE_ARRAY, HRESULT (WINAPI *)( void *, UINT32, const float *, IInspectable ** ) )( statics, n, f, &v );
    return v;
}

/* ---- IIterable<HSTRING> of one name ---- */
struct names { const void * const *iterable_vtbl; const void * const *iterator_vtbl; const WCHAR *name; int pos; };
static struct names *from_iterable( void *iface ) { return CONTAINING_RECORD( iface, struct names, iterable_vtbl ); }
static struct names *from_iterator( void *iface ) { return CONTAINING_RECORD( iface, struct names, iterator_vtbl ); }
static HRESULT WINAPI it_QueryInterface( void *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &probe_IID_IIterable_HSTRING ))
    { *out = iface; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI it_AddRef( void *iface ) { return 2; }
static ULONG WINAPI it_Release( void *iface ) { return 1; }
static HRESULT WINAPI it_First( void *iface, void **out )
{
    struct names *n = from_iterable( iface );
    n->pos = 0;
    *out = &n->iterator_vtbl;
    return S_OK;
}
static const void * const iterable_vtbl[] = { it_QueryInterface, it_AddRef, it_Release, eff_GetIids, eff_GetRuntimeClassName, eff_GetTrustLevel, it_First };
static HRESULT WINAPI itr_QueryInterface( void *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &probe_IID_IIterator_HSTRING ))
    { *out = iface; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI itr_get_Current( void *iface, HSTRING *out )
{
    struct names *n = from_iterator( iface );
    if (n->pos) return (HRESULT)0x8000000b; /* E_BOUNDS */
    return WindowsCreateString( n->name, (UINT32)wcslen( n->name ), out );
}
static HRESULT WINAPI itr_get_HasCurrent( void *iface, boolean *out ) { *out = !from_iterator( iface )->pos; return S_OK; }
static HRESULT WINAPI itr_MoveNext( void *iface, boolean *out ) { from_iterator( iface )->pos++; *out = FALSE; return S_OK; }
static const void * const iterator_vtbl[] = { itr_QueryInterface, it_AddRef, it_Release, eff_GetIids, eff_GetRuntimeClassName, eff_GetTrustLevel,
                                              itr_get_Current, itr_get_HasCurrent, itr_MoveNext };

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

static void *factory_brush( void *factory )
{
    void *brush = NULL;
    if (!factory) return NULL;
    SLOT( factory, 6, get_fn )( factory, &brush );
    return brush;
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
    static const float red[4] = { 1, 0, 0, 1 }, yellow[4] = { 1, 1, 0, 1 };
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"compeffect" };
    struct ui_color green = { 255, 0, 255, 0 }, white = { 255, 255, 255, 255 }, blue = { 255, 0, 0, 255 };
    struct effect flood, invert, tinted, nointerop;
    struct names animatable = { iterable_vtbl, iterator_vtbl, L"Flood.Color", 0 };
    struct names bad_name = { iterable_vtbl, iterator_vtbl, L"Flood.NoSuchProperty", 0 };
    void *compositor, *desktop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL;
    void *f_flood = NULL, *f_invert = NULL, *f_tinted = NULL, *f_bad = NULL, *b_flood, *b_invert, *b_tinted;
    void *param_factory = NULL, *param = NULL, *got = NULL, *props = NULL, *eb_invert, *green_brush;
    IInspectable *inspectable = NULL;
    INT32 status = -5;
    HRESULT hr, ext = -1;
    ID3D11Device *d3d;
    HSTRING h;
    HWND hwnd;
    HDC dc;
    COLORREF a, b, c, d;

    RoInitialize( RO_INIT_SINGLETHREADED );
    h = hs( L"Windows.UI.Composition.Compositor" );
    hr = RoActivateInstance( h, &inspectable );
    WindowsDeleteString( h );
    if (FAILED( hr )) { printf( "compositor=0\n" ); return 1; }
    compositor = qi( inspectable, &probe_IID_ICompositor );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );
    h = hs( L"Windows.Foundation.PropertyValue" );
    RoGetActivationFactory( h, &probe_IID_IPropertyValueStatics, &statics );
    WindowsDeleteString( h );
    h = hs( L"Windows.UI.Composition.CompositionEffectSourceParameter" );
    RoGetActivationFactory( h, &probe_IID_ICompositionEffectSourceParameterFactory, &param_factory );
    WindowsDeleteString( h );
    printf( "helpers=%d\n", statics && param_factory );
    if (!statics || !param_factory) return 1;
    h = hs( L"src" );
    SLOT( param_factory, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( param_factory, h, &param );
    WindowsDeleteString( h );

    /* a Flood: the colour everywhere */
    effect_init( &flood, &probe_CLSID_D2D1Flood, L"Flood" );
    flood.prop_count = 1;
    flood.props[0] = floats( red, 4 );
    hr = SLOT( compositor, COMPOSITOR_CREATE_EFFECT_FACTORY, HRESULT (WINAPI *)( void *, void *, void ** ) )( compositor, &flood.effect_vtbl, &f_flood );
    printf( "factory=%d (%#lx)\n", SUCCEEDED(hr) && f_flood, hr );
    printf( "factory_iface=%d\n", qi( f_flood, &probe_IID_ICompositionEffectFactory ) != NULL && qi( f_flood, &probe_IID_ICompositionObject ) != NULL );
    printf( "factory_class=%d\n", is_class( f_flood, L"Windows.UI.Composition.CompositionEffectFactory" ) );
    if (f_flood)
    {
        SLOT( f_flood, 7, HRESULT (WINAPI *)( void *, HRESULT * ) )( f_flood, &ext );
        SLOT( f_flood, 8, HRESULT (WINAPI *)( void *, INT32 * ) )( f_flood, &status );
    }
    printf( "factory_status=%d (%d %#lx)\n", status == 0 && ext == S_OK, status, ext );
    b_flood = factory_brush( f_flood );
    printf( "brush=%d\n", b_flood != NULL && qi( b_flood, &probe_IID_ICompositionBrush ) != NULL );
    printf( "brush_class=%d\n", is_class( b_flood, L"Windows.UI.Composition.CompositionEffectBrush" ) );
    /* the factory read its description once: a change to it afterwards reaches no brush */
    IInspectable_Release( flood.props[0] );
    flood.props[0] = floats( yellow, 4 );

    /* an Invert of the source parameter "src" */
    effect_init( &invert, &probe_CLSID_D2D1Invert, L"Invert" );
    invert.source_count = 1;
    invert.sources[0] = param;
    hr = SLOT( compositor, COMPOSITOR_CREATE_EFFECT_FACTORY, HRESULT (WINAPI *)( void *, void *, void ** ) )( compositor, &invert.effect_vtbl, &f_invert );
    printf( "factory_param=%d (%#lx)\n", SUCCEEDED(hr) && f_invert, hr );
    b_invert = factory_brush( f_invert );
    eb_invert = qi( b_invert, &probe_IID_ICompositionEffectBrush );
    green_brush = color_brush( compositor, green );
    if (eb_invert)
    {
        h = hs( L"src" );
        hr = SLOT( eb_invert, 7, HRESULT (WINAPI *)( void *, HSTRING, void * ) )( eb_invert, h, green_brush );
        SLOT( eb_invert, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( eb_invert, h, &got );
        WindowsDeleteString( h );
        printf( "source_param=%d (%#lx)\n", SUCCEEDED(hr) && got && qi( got, &probe_IID_ICompositionBrush ) == green_brush, hr );
        h = hs( L"other" );
        hr = SLOT( eb_invert, 7, HRESULT (WINAPI *)( void *, HSTRING, void * ) )( eb_invert, h, green_brush );
        WindowsDeleteString( h );
        printf( "source_param_unknown=%d (%#lx)\n", hr == E_INVALIDARG, hr );
    }

    /* a Flood whose Color is animatable, set through the brush's Properties */
    effect_init( &tinted, &probe_CLSID_D2D1Flood, L"Flood" );
    tinted.prop_count = 1;
    tinted.props[0] = floats( red, 4 );
    tinted.mapped = L"Color";
    tinted.mapped_index = 0;
    tinted.mapping = 10;   /* GRAPHICS_EFFECT_PROPERTY_MAPPING_COLOR_TO_VECTOR4 */
    hr = SLOT( compositor, COMPOSITOR_CREATE_EFFECT_FACTORY_WITH_PROPERTIES, HRESULT (WINAPI *)( void *, void *, void *, void ** ) )(
            compositor, &tinted.effect_vtbl, &animatable.iterable_vtbl, &f_tinted );
    printf( "factory_props=%d (%#lx)\n", SUCCEEDED(hr) && f_tinted, hr );
    b_tinted = factory_brush( f_tinted );
    if (b_tinted)
    {
        void *object = qi( b_tinted, &probe_IID_ICompositionObject );
        SLOT( object, OBJECT_GET_PROPERTIES, get_fn )( object, &props );
        h = hs( L"Flood.Color" );
        hr = props ? SLOT( props, PROPSET_INSERT_COLOR, HRESULT (WINAPI *)( void *, HSTRING, struct ui_color ) )( props, h, blue ) : E_FAIL;
        WindowsDeleteString( h );
        printf( "props_insert=%d (%#lx)\n", SUCCEEDED(hr), hr );
    }

    /* what the factory refuses */
    hr = SLOT( compositor, COMPOSITOR_CREATE_EFFECT_FACTORY_WITH_PROPERTIES, HRESULT (WINAPI *)( void *, void *, void *, void ** ) )(
            compositor, &tinted.effect_vtbl, &bad_name.iterable_vtbl, &f_bad );
    printf( "bad_animatable=%d (%#lx)\n", FAILED(hr) && !f_bad, hr );
    effect_init( &nointerop, &probe_CLSID_D2D1Flood, L"x" );
    nointerop.with_interop = 0;
    f_bad = NULL;
    hr = SLOT( compositor, COMPOSITOR_CREATE_EFFECT_FACTORY, HRESULT (WINAPI *)( void *, void *, void ** ) )( compositor, &nointerop.effect_vtbl, &f_bad );
    printf( "no_interop=%d (%#lx)\n", FAILED(hr) && !f_bad, hr );
    hr = SLOT( compositor, COMPOSITOR_CREATE_EFFECT_FACTORY, HRESULT (WINAPI *)( void *, void *, void ** ) )( compositor, NULL, &f_bad );
    printf( "null_effect=%d (%#lx)\n", FAILED(hr), hr );

    if (FAILED( hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d, NULL, NULL ) ))
    {
        printf( "no_d3d11=%#lx\n", hr );
        printf( "done=1\n" );
        return 0;
    }
    if (!b_flood || !b_invert || !b_tinted) { printf( "done=1\n" ); return 0; }

    /* white, then three squares: the flood, the inverted green, the tinted flood */
    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"compeffect", L"compeffect", WS_POPUP | WS_VISIBLE, 50, 50, 400, 100, NULL, NULL, NULL, NULL );
    pump( 300 );
    SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )( desktop, hwnd, FALSE, &target );
    ctarget = qi( target, &probe_IID_ICompositionTarget );
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, color_brush( compositor, white ), 0, 0, 400, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, qi( b_flood, &probe_IID_ICompositionBrush ), 0, 0, 100, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, qi( b_invert, &probe_IID_ICompositionBrush ), 100, 0, 100, 100 ) );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, make_sprite( compositor, qi( b_tinted, &probe_IID_ICompositionBrush ), 200, 0, 100, 100 ) );
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, qi( root, &probe_IID_IVisual ) );
    pump( 1500 );
    dc = GetDC( hwnd );
    a = GetPixel( dc, 50, 50 );
    b = GetPixel( dc, 150, 50 );
    c = GetPixel( dc, 250, 50 );
    d = GetPixel( dc, 350, 50 );
    ReleaseDC( hwnd, dc );
    printf( "flood_drawn=%d (%06lx)\n", close_to( a, 255, 0, 0 ), a );
    printf( "invert_drawn=%d (%06lx)\n", close_to( b, 255, 0, 255 ), b );
    printf( "props_color_drawn=%d (%06lx)\n", close_to( c, 0, 0, 255 ), c );
    printf( "outside_white=%d (%06lx)\n", close_to( d, 255, 255, 255 ), d );
    DestroyWindow( hwnd );
    printf( "done=1\n" );
    return 0;
}
