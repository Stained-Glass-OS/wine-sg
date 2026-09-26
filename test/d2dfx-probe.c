/* d2dfx-probe: Direct2D as Paint.NET uses it (patches/sg/0223-0227).
 *
 *  - the built-in effects are registered, with their properties' types and
 *    defaults, and their properties can be set;
 *  - a custom effect whose factory hands back an IUnknown that is not its
 *    ID2D1EffectImpl (a .NET ComWrappers object's is not) is initialized
 *    through its ID2D1EffectImpl; its context is an ID2D1EffectContext2 that
 *    reports the feature level, and can make a transform node of an effect;
 *  - a WIC bitmap render target's device has feature level 11 (effects'
 *    shader model 5 shaders load there);
 *  - Flush says whether the context is drawing; an empty command list
 *    closes, and one set as the target mid-frame records;
 *  - sRGB color contexts carry an ICC profile; gradient stop collections are
 *    ID2D1GradientStopCollection1; geometry realizations exist and draw;
 *  - DXGI has a WARP adapter; shader reflection reports the minimum feature
 *    level; Windows Animation animates a variable along a storyboard.
 *
 * Prints name=value lines; see test/d2dfx-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effectauthor.h>
#include <d2d1effects_2.h>
#include <dxgi1_6.h>
#include <wincodec.h>
#include <uianimation.h>
#include <math.h>
#include <stdio.h>

/* d3dcompiler_47 (its header drags in one mingw cannot compile as C) */
HRESULT WINAPI D3DCompile( const void *data, SIZE_T size, const char *name, const void *defines, void *include,
                           const char *entry, const char *target, UINT flags1, UINT flags2, ID3DBlob **code,
                           ID3DBlob **errors );
HRESULT WINAPI D3DReflect( const void *data, SIZE_T size, REFIID iid, void **reflector );

/* public CLSIDs/IIDs, spelled out so the probe does not depend on the headers having them */
DEFINE_GUID(probe_CLSID_Histogram,        0x881db7d0,0xf7ee,0x4d4d,0xa6,0xd2,0x46,0x97,0xac,0xc6,0x6e,0xe8);
DEFINE_GUID(probe_CLSID_GaussianBlur,     0x1feb6d69,0x2fe6,0x4ac9,0x8c,0x58,0x1d,0x7f,0x93,0xe7,0xa6,0xa5);
DEFINE_GUID(probe_CLSID_ColorMatrix,      0x921f03d6,0x641c,0x47df,0x85,0x2d,0xb4,0xbb,0x61,0x53,0xae,0x11);
DEFINE_GUID(probe_CLSID_Flood,            0x61c23c20,0xae69,0x4d8e,0x94,0xcf,0x50,0x07,0x8d,0xf6,0x38,0xf2);
DEFINE_GUID(probe_CLSID_Blend,            0x81c5b77b,0x13f8,0x4cdd,0xad,0x20,0xc8,0x90,0x54,0x7a,0xc6,0x5d);
DEFINE_GUID(probe_CLSID_Border,           0x2a2d49c0,0x4acf,0x43c7,0x8c,0x6a,0x7c,0x4a,0x27,0x87,0x4d,0x27);
DEFINE_GUID(probe_CLSID_Morphology,       0xeae6c40d,0x626a,0x4c2d,0xbf,0xcb,0x39,0x10,0x01,0xab,0xe2,0x02);
DEFINE_GUID(probe_CLSID_Scale,            0x9daf9369,0x3846,0x4d0e,0xa4,0x4e,0x0c,0x60,0x79,0x34,0xa5,0xd7);
DEFINE_GUID(probe_CLSID_Tile,             0xb0784138,0x3b76,0x4bc5,0xb1,0x3b,0x0f,0xa2,0xad,0x02,0x65,0x9f);
DEFINE_GUID(probe_CLSID_Opacity,          0x811d79a4,0xde28,0x4454,0x80,0x94,0xc6,0x46,0x85,0xf8,0xbd,0x4c);
DEFINE_GUID(probe_CLSID_Premultiply,      0x06eab419,0xdeed,0x4018,0x80,0xd2,0x3e,0x1d,0x47,0x1a,0xde,0xb2);
DEFINE_GUID(probe_CLSID_UnPremultiply,    0xfb9ac489,0xad8d,0x41ed,0x99,0x99,0xbb,0x63,0x47,0xd1,0x10,0xf7);
DEFINE_GUID(probe_IID_ID2D1EffectContext1, 0x84ab595a,0xfc81,0x4546,0xba,0xcd,0xe8,0xef,0x4d,0x8a,0xbe,0x7a);
DEFINE_GUID(probe_IID_ID2D1EffectContext2, 0x577ad2a0,0x9fc7,0x4dda,0x8b,0x18,0xda,0xb8,0x10,0x14,0x00,0x52);
DEFINE_GUID(probe_CLSID_Custom,           0x5e8f3a1c,0x7d2b,0x4c9e,0x9a,0x10,0x53,0x47,0x46,0x58,0x44,0x01);
DEFINE_GUID(probe_CLSID_ColorManagement,  0x1a28524c,0xfdd6,0x4aa4,0xae,0x8f,0x83,0x7e,0xb8,0x26,0x7b,0x37);
DEFINE_GUID(probe_IID_ID2D1GradientStopCollection1, 0xae1572f4,0x5dd0,0x4777,0x99,0x8b,0x92,0x79,0x47,0x2a,0xe6,0x3b);
DEFINE_GUID(probe_IID_IDXGIFactory4,      0x1bc6ea02,0xef36,0x464f,0xbf,0x0c,0x21,0xca,0x39,0xe5,0x16,0x8a);
DEFINE_GUID(probe_IID_ID3D11ShaderReflection, 0x8d536ca1,0x0cca,0x4956,0xa8,0x37,0x78,0x69,0x63,0x75,0x55,0x84);
DEFINE_GUID(probe_IID_IDXGIOutput6,       0x068346e8,0xaaec,0x4b84,0xad,0xd7,0x13,0x7f,0x51,0x3f,0x77,0xa1);
DEFINE_GUID(probe_IID_IDXGIAdapter,       0x2411e7e1,0x12ac,0x4ccf,0xbd,0x14,0x97,0x98,0xe8,0x53,0x4d,0xc0);

/* vtable slots of what the C headers lack (from the interfaces' definitions) */
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
#define EFFECTCTX_CREATE_EFFECT       4
#define EFFECTCTX_MAX_FEATURE_LEVEL   5
#define EFFECTCTX_NODE_FROM_EFFECT    6
#define GRAPH_SET_SINGLE_NODE         4
#define EFFECT_SET_VALUE              9
#define EFFECT_GET_TYPE               6
#define EFFECT_GET_VALUE              11
#define COLORCTX_GET_PROFILE_SIZE     5
#define COLORCTX_GET_PROFILE          6
#define CMDLIST_CLOSE                 5
#define DC1_CREATE_FILLED_REALIZATION 92
#define DC1_DRAW_REALIZATION          94
#define REFLECTION_MIN_FEATURE_LEVEL  19
#define DC_GET_IMAGE_LOCAL_BOUNDS     70
#define EFFECT_SET_INPUT              14
#define EFFECT_GET_OUTPUT             18
#define GEOMETRY_COMBINE              11
#define GEOMETRY_WIDEN                16
#define DC_CREATE_BITMAP1             57
#define DC_CREATE_COLOR_CONTEXT       59
#define DC_CREATE_COMMAND_LIST        67
#define DC_SET_TARGET                 74
#define RT(ctx) ((ID2D1RenderTarget *)(ctx))
#define SET_TARGET(ctx, t) SLOT( ctx, DC_SET_TARGET, void (WINAPI *)( void *, IUnknown * ) )( ctx, (IUnknown *)(t) )
#define DXGIFACTORY4_ENUM_WARP        27
#define DXGIADAPTER_ENUM_OUTPUTS      7
#define DXGIOUTPUT6_HW_COMPOSITION    28

static const struct { const GUID *clsid; const char *name; } builtins[] =
{
    { &probe_CLSID_Histogram, "Histogram" },
    { &probe_CLSID_GaussianBlur, "GaussianBlur" },
    { &probe_CLSID_ColorMatrix, "ColorMatrix" },
    { &probe_CLSID_Flood, "Flood" },
    { &probe_CLSID_Blend, "Blend" },
    { &probe_CLSID_Border, "Border" },
    { &probe_CLSID_Morphology, "Morphology" },
    { &probe_CLSID_Scale, "Scale" },
    { &probe_CLSID_Tile, "Tile" },
    { &probe_CLSID_Opacity, "Opacity" },
    { &probe_CLSID_Premultiply, "Premultiply" },
    { &probe_CLSID_UnPremultiply, "UnPremultiply" },
};

/* The custom effect: an object with an IUnknown of its own (six slots, the
 * last three traps) and a separate ID2D1EffectImpl. */
struct custom
{
    IUnknown IUnknown_iface;
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;
    D3D_FEATURE_LEVEL level;
};

static int initialized, trapped, has_context2, effect_node;
static HRESULT max_level_hr = E_FAIL, level11_hr = E_FAIL;

static struct custom *from_unknown( IUnknown *iface ) { return CONTAINING_RECORD( iface, struct custom, IUnknown_iface ); }
static struct custom *from_impl( ID2D1EffectImpl *iface ) { return CONTAINING_RECORD( iface, struct custom, ID2D1EffectImpl_iface ); }

static HRESULT WINAPI unk_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    struct custom *c = from_unknown( iface );
    if (IsEqualGUID( iid, &IID_IUnknown )) *out = &c->IUnknown_iface;
    else if (IsEqualGUID( iid, &IID_ID2D1EffectImpl )) *out = &c->ID2D1EffectImpl_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement( &c->refcount );
    return S_OK;
}
static ULONG WINAPI unk_AddRef( IUnknown *iface ) { return InterlockedIncrement( &from_unknown( iface )->refcount ); }
static ULONG WINAPI unk_Release( IUnknown *iface )
{
    struct custom *c = from_unknown( iface );
    ULONG ref = InterlockedDecrement( &c->refcount );
    if (!ref) HeapFree( GetProcessHeap(), 0, c );
    return ref;
}
static HRESULT WINAPI trap( IUnknown *iface, void *a, void *b ) { trapped = 1; return E_NOTIMPL; }

static const struct
{
    HRESULT (WINAPI *QueryInterface)( IUnknown *, REFIID, void ** );
    ULONG (WINAPI *AddRef)( IUnknown * );
    ULONG (WINAPI *Release)( IUnknown * );
    HRESULT (WINAPI *trap1)( IUnknown *, void *, void * );
    HRESULT (WINAPI *trap2)( IUnknown *, void *, void * );
    HRESULT (WINAPI *trap3)( IUnknown *, void *, void * );
} unk_vtbl = { unk_QueryInterface, unk_AddRef, unk_Release, trap, trap, trap };

static HRESULT WINAPI impl_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    return unk_QueryInterface( &from_impl( (ID2D1EffectImpl *)iface )->IUnknown_iface, iid, out );
}
static ULONG WINAPI impl_AddRef( IUnknown *iface ) { return unk_AddRef( &from_impl( (ID2D1EffectImpl *)iface )->IUnknown_iface ); }
static ULONG WINAPI impl_Release( IUnknown *iface ) { return unk_Release( &from_impl( (ID2D1EffectImpl *)iface )->IUnknown_iface ); }
static HRESULT WINAPI impl_Initialize( ID2D1EffectImpl *iface, ID2D1EffectContext *context, ID2D1TransformGraph *graph )
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_10_0 };
    struct custom *c = from_impl( iface );
    IUnknown *context2;

    initialized = 1;
    if (SUCCEEDED( IUnknown_QueryInterface( (IUnknown *)context, &probe_IID_ID2D1EffectContext2, (void **)&context2 ) ))
    {
        IUnknown *context1;
        if (SUCCEEDED( IUnknown_QueryInterface( context2, &probe_IID_ID2D1EffectContext1, (void **)&context1 ) ))
        {
            has_context2 = 1;
            IUnknown_Release( context1 );
        }
        IUnknown_Release( context2 );
    }
    /* ID2D1EffectContext: IUnknown, GetDpi, CreateEffect, GetMaximumSupportedFeatureLevel */
    max_level_hr = ((HRESULT (WINAPI *)( void *, const D3D_FEATURE_LEVEL *, UINT32, D3D_FEATURE_LEVEL * ))
                    (*(void ***)context)[5])( context, levels, ARRAYSIZE(levels), &c->level );
    {
        /* shader model 5 needs 11_0 */
        static const D3D_FEATURE_LEVEL level11[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL got;
        level11_hr = SLOT( context, EFFECTCTX_MAX_FEATURE_LEVEL,
                           HRESULT (WINAPI *)( void *, const D3D_FEATURE_LEVEL *, UINT32, D3D_FEATURE_LEVEL * ) )(
                           context, level11, 1, &got );
    }
    {
        /* an effect as a node of this one's graph: a flood, no inputs like this one */
        ID2D1Effect *flood;
        IUnknown *node;
        effect_node = 0;
        if (SUCCEEDED( SLOT( context, EFFECTCTX_CREATE_EFFECT, HRESULT (WINAPI *)( void *, REFCLSID, ID2D1Effect ** ) )(
                           context, &probe_CLSID_Flood, &flood ) ))
        {
            if (SUCCEEDED( SLOT( context, EFFECTCTX_NODE_FROM_EFFECT, HRESULT (WINAPI *)( void *, ID2D1Effect *, IUnknown ** ) )(
                               context, flood, &node ) ))
            {
                effect_node = SUCCEEDED( SLOT( graph, GRAPH_SET_SINGLE_NODE, HRESULT (WINAPI *)( void *, IUnknown * ) )(
                                             graph, node ) );
                IUnknown_Release( node );
            }
            IUnknown_Release( (IUnknown *)flood );
        }
    }
    return S_OK;
}
static HRESULT WINAPI impl_PrepareForRender( ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE type ) { return S_OK; }
static HRESULT WINAPI impl_SetGraph( ID2D1EffectImpl *iface, ID2D1TransformGraph *graph ) { return E_NOTIMPL; }

static const ID2D1EffectImplVtbl impl_vtbl =
{
    { impl_QueryInterface, impl_AddRef, impl_Release }, impl_Initialize, impl_PrepareForRender, impl_SetGraph,
};

static HRESULT WINAPI custom_factory( IUnknown **out )
{
    struct custom *c = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*c) );
    if (!c) return E_OUTOFMEMORY;
    c->IUnknown_iface.lpVtbl = (IUnknownVtbl *)&unk_vtbl;
    c->ID2D1EffectImpl_iface.lpVtbl = &impl_vtbl;
    c->refcount = 1;
    *out = &c->IUnknown_iface;
    return S_OK;
}

/* the property getter gets the effect object; any of its interfaces will do */
static HRESULT WINAPI get_level( const IUnknown *effect, BYTE *data, UINT32 size, UINT32 *actual )
{
    struct custom *c;
    IUnknown *unk = (IUnknown *)effect;
    ID2D1EffectImpl *impl;

    if (actual) *actual = sizeof(UINT32);
    if (!data) return S_OK;
    if (size < sizeof(UINT32)) return E_INVALIDARG;
    if (FAILED( IUnknown_QueryInterface( unk, &IID_ID2D1EffectImpl, (void **)&impl ) )) return E_FAIL;
    c = from_impl( impl );
    *(UINT32 *)data = c->level;
    ID2D1EffectImpl_Release( impl );
    return S_OK;
}
static HRESULT WINAPI set_level( IUnknown *effect, const BYTE *data, UINT32 size ) { return S_OK; }

static const WCHAR custom_xml[] =
    L"<?xml version='1.0'?><Effect>"
    L"<Property name='DisplayName' type='string' value='Probe'/>"
    L"<Property name='Author' type='string' value='sg'/>"
    L"<Property name='Category' type='string' value='Test'/>"
    L"<Property name='Description' type='string' value='Probe'/>"
    L"<Inputs/>"
    L"<Property name='Level' type='enum'><Property name='DisplayName' type='string' value='Level'/>"
    L"<Property name='Default' type='enum' value='4096'/>"
    L"<Fields><Field name='Core' displayname='Core' index='4096'/><Field name='L91' displayname='L91' index='37120'/>"
    L"<Field name='L100' displayname='L100' index='40960'/></Fields></Property>"
    L"</Effect>";

/* UIAnimation: a variable along a storyboard, driven by the manager's Update */
static void check_animation( void )
{
    IUIAnimationManager *manager;
    IUIAnimationTransitionLibrary *library;
    IUIAnimationVariable *var;
    IUIAnimationStoryboard *sb;
    IUIAnimationTransition *tr;
    IUIAnimationTimer *timer;
    UI_ANIMATION_UPDATE_RESULT res;
    UI_ANIMATION_SCHEDULING_RESULT sched;
    UI_ANIMATION_MANAGER_STATUS status_mid = UI_ANIMATION_MANAGER_IDLE, status_end = UI_ANIMATION_MANAGER_BUSY;
    double mid = -1, end = -1;
    int ok = 0, timer_ok = 0;

    if (FAILED( CoCreateInstance( &CLSID_UIAnimationManager, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAnimationManager,
                                  (void **)&manager ) ))
    {
        printf( "animation=no-manager\n" );
        return;
    }
    if (SUCCEEDED( CoCreateInstance( &CLSID_UIAnimationTransitionLibrary, NULL, CLSCTX_INPROC_SERVER,
                                     &IID_IUIAnimationTransitionLibrary, (void **)&library ) ))
    {
        IUIAnimationManager_CreateAnimationVariable( manager, 0.0, &var );
        IUIAnimationManager_CreateStoryboard( manager, &sb );
        if (SUCCEEDED( IUIAnimationTransitionLibrary_CreateSmoothStopTransition( library, 1.0, 100.0, &tr ) ))
        {
            IUIAnimationStoryboard_AddTransition( sb, var, tr );
            IUIAnimationStoryboard_Schedule( sb, 10.0, &sched );
            IUIAnimationManager_Update( manager, 10.5, &res );
            IUIAnimationVariable_GetValue( var, &mid );
            IUIAnimationManager_GetStatus( manager, &status_mid );
            IUIAnimationManager_Update( manager, 11.5, &res );
            IUIAnimationVariable_GetValue( var, &end );
            IUIAnimationManager_GetStatus( manager, &status_end );
            ok = mid > 1.0 && mid < 99.0 && end == 100.0 && status_mid == UI_ANIMATION_MANAGER_BUSY
                 && status_end == UI_ANIMATION_MANAGER_IDLE;
            IUIAnimationTransition_Release( tr );
        }
        IUIAnimationStoryboard_Release( sb );
        IUIAnimationVariable_Release( var );
        IUIAnimationTransitionLibrary_Release( library );
    }
    printf( "animation=%d (mid %.1f end %.1f)\n", ok, mid, end );

    if (SUCCEEDED( CoCreateInstance( &CLSID_UIAnimationTimer, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAnimationTimer,
                                     (void **)&timer ) ))
    {
        UI_ANIMATION_SECONDS t1 = 0, t2 = 0;
        IUIAnimationTimer_GetTime( timer, &t1 );
        Sleep( 50 );
        IUIAnimationTimer_GetTime( timer, &t2 );
        timer_ok = t2 > t1 && IUIAnimationTimer_Enable( timer ) == S_OK && IUIAnimationTimer_IsEnabled( timer ) == S_OK;
        IUIAnimationTimer_Release( timer );
    }
    printf( "animation_timer=%d\n", timer_ok );
    IUIAnimationManager_Release( manager );
}

/* DXGI's WARP adapter, and shader reflection's minimum feature level */
static void check_dxgi_d3dcompiler( void )
{
    static const char ps[] = "float4 main(float4 p : SV_POSITION) : SV_TARGET { return p; }";
    IUnknown *factory = NULL, *adapter = NULL;
    ID3DBlob *code = NULL;
    IUnknown *reflection;
    D3D_FEATURE_LEVEL level = 0, level9 = 0;
    HRESULT (WINAPI *create_factory)( UINT, REFIID, void ** );
    HRESULT hr;

    create_factory = (void *)GetProcAddress( LoadLibraryA( "dxgi.dll" ), "CreateDXGIFactory2" );
    if (create_factory && SUCCEEDED( create_factory( 0, &probe_IID_IDXGIFactory4, (void **)&factory ) ))
    {
        /* IDXGIFactory4::EnumWarpAdapter */
        hr = SLOT( factory, DXGIFACTORY4_ENUM_WARP, HRESULT (WINAPI *)( void *, REFIID, void ** ) )( factory, &probe_IID_IDXGIAdapter,
                                                                                 (void **)&adapter );
        printf( "warp_adapter=%d\n", SUCCEEDED( hr ) && adapter );
        if (adapter)
        {
            /* its output says whether it composes in hardware (no: 0) */
            IUnknown *output = NULL, *output6 = NULL;
            UINT flags = 0xdead;
            if (SUCCEEDED( SLOT( adapter, DXGIADAPTER_ENUM_OUTPUTS, HRESULT (WINAPI *)( void *, UINT, IUnknown ** ) )(
                               adapter, 0, &output ) )
                && SUCCEEDED( IUnknown_QueryInterface( output, &probe_IID_IDXGIOutput6, (void **)&output6 ) ))
            {
                hr = SLOT( output6, DXGIOUTPUT6_HW_COMPOSITION, HRESULT (WINAPI *)( void *, UINT * ) )( output6, &flags );
                printf( "hw_composition=%d\n", hr == S_OK && flags == 0 );
                IUnknown_Release( output6 );
            }
            else printf( "hw_composition=no-output\n" );
            if (output) IUnknown_Release( output );
            IUnknown_Release( adapter );
        }
        IUnknown_Release( factory );
    }
    else printf( "warp_adapter=no-factory\n" );

    if (SUCCEEDED( D3DCompile( ps, sizeof(ps) - 1, NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &code, NULL ) )
        && SUCCEEDED( D3DReflect( ID3D10Blob_GetBufferPointer( code ), ID3D10Blob_GetBufferSize( code ),
                                  &probe_IID_ID3D11ShaderReflection, (void **)&reflection ) ))
    {
        SLOT( reflection, REFLECTION_MIN_FEATURE_LEVEL, HRESULT (WINAPI *)( void *, D3D_FEATURE_LEVEL * ) )(
                reflection, &level );
        IUnknown_Release( reflection );
    }
    if (code) ID3D10Blob_Release( code );
    code = NULL;
    if (SUCCEEDED( D3DCompile( ps, sizeof(ps) - 1, NULL, NULL, NULL, "main", "ps_4_0_level_9_1", 0, 0, &code, NULL ) )
        && SUCCEEDED( D3DReflect( ID3D10Blob_GetBufferPointer( code ), ID3D10Blob_GetBufferSize( code ),
                                  &probe_IID_ID3D11ShaderReflection, (void **)&reflection ) ))
    {
        SLOT( reflection, REFLECTION_MIN_FEATURE_LEVEL, HRESULT (WINAPI *)( void *, D3D_FEATURE_LEVEL * ) )(
                reflection, &level9 );
        IUnknown_Release( reflection );
    }
    if (code) ID3D10Blob_Release( code );
    printf( "min_feature_level=%#x,%#x\n", level, level9 );
}

/* a simplified geometry sink that adds up the area of what it is given */
struct area_sink
{
    const void *vtbl;
    double area, figure;
    D2D1_POINT_2F first, last;
};

static HRESULT WINAPI as_QueryInterface( void *iface, REFIID iid, void **out ) { *out = iface; return S_OK; }
static ULONG WINAPI as_AddRef( void *iface ) { return 2; }
static ULONG WINAPI as_Release( void *iface ) { return 1; }
static void WINAPI as_SetFillMode( void *iface, D2D1_FILL_MODE mode ) {}
static void WINAPI as_SetSegmentFlags( void *iface, D2D1_PATH_SEGMENT flags ) {}
static void WINAPI as_BeginFigure( struct area_sink *sink, D2D1_POINT_2F p, D2D1_FIGURE_BEGIN begin )
{
    sink->figure = 0;
    sink->first = sink->last = p;
}
static void WINAPI as_AddLines( struct area_sink *sink, const D2D1_POINT_2F *p, UINT32 count )
{
    UINT32 i;
    for (i = 0; i < count; i++)
    {
        sink->figure += (double)sink->last.x * p[i].y - (double)p[i].x * sink->last.y;
        sink->last = p[i];
    }
}
static void WINAPI as_AddBeziers( struct area_sink *sink, const D2D1_BEZIER_SEGMENT *b, UINT32 count )
{
    UINT32 i;
    for (i = 0; i < count; i++) as_AddLines( sink, &b[i].point3, 1 );
}
static void WINAPI as_EndFigure( struct area_sink *sink, D2D1_FIGURE_END end )
{
    as_AddLines( sink, &sink->first, 1 );
    sink->area += fabs( sink->figure ) / 2;
}
static HRESULT WINAPI as_Close( void *iface ) { return S_OK; }
static const void *area_sink_vtbl[] =
{
    as_QueryInterface, as_AddRef, as_Release, as_SetFillMode, as_SetSegmentFlags, as_BeginFigure, as_AddLines,
    as_AddBeziers, as_EndFigure, as_Close,
};

/* CombineWithGeometry and Widen: the areas of what they give */
static void check_geometry( ID2D1Factory *factory )
{
    D2D1_RECT_F ra = { 0, 0, 10, 10 }, rb = { 5, 0, 15, 10 };
    ID2D1RectangleGeometry *a, *b;
    static const double expect[4] = { 150, 50, 100, 50 };  /* union, intersect, xor, exclude */
    unsigned int mode, ok = 0;
    struct area_sink sink;

    if (FAILED( ID2D1Factory_CreateRectangleGeometry( factory, &ra, &a ) )
        || FAILED( ID2D1Factory_CreateRectangleGeometry( factory, &rb, &b ) ))
        return;
    for (mode = 0; mode < 4; mode++)
    {
        memset( &sink, 0, sizeof(sink) );
        sink.vtbl = area_sink_vtbl;
        if (SUCCEEDED( SLOT( a, GEOMETRY_COMBINE, HRESULT (WINAPI *)( void *, ID2D1Geometry *, D2D1_COMBINE_MODE,
                                                                      const D2D1_MATRIX_3X2_F *, float, void * ) )(
                           a, (ID2D1Geometry *)b, mode, NULL, 0.25f, &sink ) )
            && fabs( sink.area - expect[mode] ) < 0.01)
            ok++;
        else
            printf( "combine_mode_%u_area=%.2f\n", mode, sink.area );
    }
    printf( "combine=%u/4\n", ok );

    memset( &sink, 0, sizeof(sink) );
    sink.vtbl = area_sink_vtbl;
    /* a 10x10 square's 2-wide stroke with mitered corners: 12x12 - 8x8 */
    SLOT( a, GEOMETRY_WIDEN, HRESULT (WINAPI *)( void *, float, ID2D1StrokeStyle *, const D2D1_MATRIX_3X2_F *, float,
                                                 void * ) )( a, 2.0f, NULL, NULL, 0.25f, &sink );
    printf( "widen_area=%.1f\n", sink.area );
    ID2D1RectangleGeometry_Release( a );
    ID2D1RectangleGeometry_Release( b );
}

int main( void )
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D2D1_PROPERTY_BINDING binding = { L"Level", set_level, get_level };
    D2D1_BITMAP_PROPERTIES1 bitmap_props = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96,
                                             D2D1_BITMAP_OPTIONS_TARGET, NULL };
    D2D1_SIZE_U size = { 64, 64 };
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *ctx;
    ID2D1Effect *effect;
    ID2D1Properties *props;
    ID2D1Bitmap1 *target;
    unsigned int i, found = 0, created = 0;
    UINT32 level = 0;
    HRESULT hr;

    CoInitialize( NULL );
    if (FAILED( hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d, NULL, NULL ) ))
    {
        printf( "no_d3d11=%#lx\n", hr );
        return 1;
    }
    ID3D11Device_QueryInterface( d3d, &IID_IDXGIDevice, (void **)&dxgi );
    if (FAILED( hr = D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory ) ))
    {
        printf( "no_factory=%#lx\n", hr );
        return 1;
    }
    ID2D1Factory1_CreateDevice( factory, dxgi, &device );
    ID2D1Device_CreateDeviceContext( device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &ctx );

    for (i = 0; i < ARRAYSIZE(builtins); i++)
    {
        int ok_props = 0, ok_create = 0;
        if (SUCCEEDED( ID2D1Factory1_GetEffectProperties( factory, builtins[i].clsid, &props ) ))
        {
            ok_props = 1;
            IUnknown_Release( (IUnknown *)props );
        }
        if (SUCCEEDED( ID2D1DeviceContext_CreateEffect( ctx, builtins[i].clsid, &effect ) ))
        {
            ok_create = 1;
            IUnknown_Release( (IUnknown *)effect );
        }
        if (!ok_props || !ok_create) printf( "missing=%s props=%d create=%d\n", builtins[i].name, ok_props, ok_create );
        found += ok_props;
        created += ok_create;
    }
    printf( "builtin_props=%u/%u\n", found, (unsigned int)ARRAYSIZE(builtins) );
    printf( "builtin_create=%u/%u\n", created, (unsigned int)ARRAYSIZE(builtins) );

    /* the built-ins' properties: a blur's deviation is a float, 3 by default, and can be set;
     * color management's source is a color context */
    {
        float deviation = 0, set = 7.5f, back = 0;
        int typed = 0, settable = 0;
        if (SUCCEEDED( ID2D1DeviceContext_CreateEffect( ctx, &probe_CLSID_GaussianBlur, &effect ) ))
        {
            typed = SLOT( effect, EFFECT_GET_TYPE, D2D1_PROPERTY_TYPE (WINAPI *)( void *, UINT32 ) )( effect, 0 )
                    == D2D1_PROPERTY_TYPE_FLOAT;
            SLOT( effect, EFFECT_GET_VALUE, HRESULT (WINAPI *)( void *, UINT32, D2D1_PROPERTY_TYPE, BYTE *, UINT32 ) )(
                    effect, 0, D2D1_PROPERTY_TYPE_FLOAT, (BYTE *)&deviation, sizeof(deviation) );
            typed = typed && deviation == 3.0f;
            settable = SUCCEEDED( SLOT( effect, EFFECT_SET_VALUE,
                                        HRESULT (WINAPI *)( void *, UINT32, D2D1_PROPERTY_TYPE, const BYTE *, UINT32 ) )(
                                        effect, 0, D2D1_PROPERTY_TYPE_FLOAT, (const BYTE *)&set, sizeof(set) ) );
            SLOT( effect, EFFECT_GET_VALUE, HRESULT (WINAPI *)( void *, UINT32, D2D1_PROPERTY_TYPE, BYTE *, UINT32 ) )(
                    effect, 0, D2D1_PROPERTY_TYPE_FLOAT, (BYTE *)&back, sizeof(back) );
            settable = settable && back == set;
            IUnknown_Release( (IUnknown *)effect );
        }
        if (SUCCEEDED( ID2D1DeviceContext_CreateEffect( ctx, &probe_CLSID_ColorManagement, &effect ) ))
        {
            typed = typed && SLOT( effect, EFFECT_GET_TYPE, D2D1_PROPERTY_TYPE (WINAPI *)( void *, UINT32 ) )( effect, 0 )
                             == D2D1_PROPERTY_TYPE_COLOR_CONTEXT;
            IUnknown_Release( (IUnknown *)effect );
        }
        else typed = 0;
        printf( "builtin_typed=%d\n", typed );
        printf( "builtin_settable=%d\n", settable );
    }

    /* an effect's bounds: a blur of a 64x64 bitmap by a deviation of 3 grows by 9 each way */
    {
        D2D1_BITMAP_PROPERTIES1 plain = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96,
                                          D2D1_BITMAP_OPTIONS_NONE, NULL };
        D2D1_SIZE_U bsize = { 64, 64 };
        D2D1_RECT_F bounds = { 0 };
        ID2D1Bitmap1 *bitmap;
        ID2D1Image *output;
        HRESULT bhr = E_FAIL;

        if (SUCCEEDED( SLOT( ctx, DC_CREATE_BITMAP1, HRESULT (WINAPI *)( void *, D2D1_SIZE_U, const void *, UINT32,
                                                                          const D2D1_BITMAP_PROPERTIES1 *, ID2D1Bitmap1 ** ) )(
                           ctx, bsize, NULL, 0, &plain, &bitmap ) )
            && SUCCEEDED( ID2D1DeviceContext_CreateEffect( ctx, &probe_CLSID_GaussianBlur, &effect ) ))
        {
            SLOT( effect, EFFECT_SET_INPUT, void (WINAPI *)( void *, UINT32, ID2D1Image *, BOOL ) )(
                    effect, 0, (ID2D1Image *)bitmap, FALSE );
            SLOT( effect, EFFECT_GET_OUTPUT, void (WINAPI *)( void *, ID2D1Image ** ) )( effect, &output );
            bhr = SLOT( ctx, DC_GET_IMAGE_LOCAL_BOUNDS, HRESULT (WINAPI *)( void *, ID2D1Image *, D2D1_RECT_F * ) )(
                    ctx, output, &bounds );
            IUnknown_Release( (IUnknown *)output );
            IUnknown_Release( (IUnknown *)effect );
            IUnknown_Release( (IUnknown *)bitmap );
        }
        printf( "effect_bounds=%d (%.0f,%.0f,%.0f,%.0f)\n", bhr == S_OK && bounds.left == -9 && bounds.top == -9
                && bounds.right == 73 && bounds.bottom == 73, bounds.left, bounds.top, bounds.right, bounds.bottom );
    }

    hr = ID2D1Factory1_RegisterEffectFromString( factory, &probe_CLSID_Custom, custom_xml, &binding, 1, custom_factory );
    printf( "custom_registered=%d\n", SUCCEEDED( hr ) );
    hr = ID2D1DeviceContext_CreateEffect( ctx, &probe_CLSID_Custom, &effect );
    printf( "custom_created=%d\n", SUCCEEDED( hr ) );
    printf( "initialized_through_impl=%d\n", initialized && !trapped );
    printf( "context2=%d\n", has_context2 );
    printf( "max_level_ok=%d\n", max_level_hr == S_OK );
    printf( "effect_node=%d\n", effect_node );
    if (SUCCEEDED( hr ))
    {
        hr = SLOT( effect, EFFECT_GET_VALUE, HRESULT (WINAPI *)( void *, UINT32, D2D1_PROPERTY_TYPE, BYTE *, UINT32 ) )(
                effect, 0, D2D1_PROPERTY_TYPE_ENUM, (BYTE *)&level, sizeof(level) );
        printf( "getter_level=%#x\n", SUCCEEDED( hr ) ? level : 0 );
        IUnknown_Release( (IUnknown *)effect );
    }

    /* a WIC bitmap render target: its effects get a device with feature level 11 */
    {
        IWICImagingFactory *wic;
        IWICBitmap *wic_bitmap;
        ID2D1RenderTarget *rt;
        ID2D1DeviceContext *wic_ctx;
        D2D1_RENDER_TARGET_PROPERTIES rt_props = { D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                   { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 0, 0,
                                                   D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT };
        int ok = 0;

        if (SUCCEEDED( CoCreateInstance( &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                                         (void **)&wic ) )
            && SUCCEEDED( IWICImagingFactory_CreateBitmap( wic, 32, 32, &GUID_WICPixelFormat32bppPBGRA,
                                                           WICBitmapCacheOnLoad, &wic_bitmap ) ))
        {
            if (SUCCEEDED( ID2D1Factory_CreateWicBitmapRenderTarget( (ID2D1Factory *)factory, wic_bitmap, &rt_props, &rt ) )
                && SUCCEEDED( ID2D1RenderTarget_QueryInterface( rt, &IID_ID2D1DeviceContext, (void **)&wic_ctx ) ))
            {
                level11_hr = E_FAIL;
                if (SUCCEEDED( ID2D1DeviceContext_CreateEffect( wic_ctx, &probe_CLSID_Custom, &effect ) ))
                {
                    ok = level11_hr == S_OK;
                    IUnknown_Release( (IUnknown *)effect );
                }
                IUnknown_Release( (IUnknown *)wic_ctx );
                ID2D1RenderTarget_Release( rt );
            }
            IWICBitmap_Release( wic_bitmap );
            IWICImagingFactory_Release( wic );
        }
        printf( "wic_target_level11=%d\n", ok );
    }

    /* Flush tells whether the context is drawing; command lists close empty and record mid-frame */
    if (SUCCEEDED( SLOT( ctx, DC_CREATE_BITMAP1, HRESULT (WINAPI *)( void *, D2D1_SIZE_U, const void *, UINT32,
                                                               const D2D1_BITMAP_PROPERTIES1 *, ID2D1Bitmap1 ** ) )(
                       ctx, size, NULL, 0, &bitmap_props, &target ) ))
    {
        ID2D1CommandList *list;
        D2D1_COLOR_F red = { 1, 0, 0, 1 };
        ID2D1SolidColorBrush *brush;
        HRESULT outside, inside, close_empty = E_FAIL, close_recorded = E_FAIL;

        SET_TARGET( ctx, target );
        outside = ID2D1RenderTarget_Flush( RT(ctx), NULL, NULL );
        ID2D1RenderTarget_BeginDraw( RT(ctx) );
        inside = ID2D1RenderTarget_Flush( RT(ctx), NULL, NULL );
        if (SUCCEEDED( SLOT( ctx, DC_CREATE_COMMAND_LIST, HRESULT (WINAPI *)( void *, ID2D1CommandList ** ) )( ctx, &list ) ))
        {
            SET_TARGET( ctx, list );
            ID2D1RenderTarget_CreateSolidColorBrush( RT(ctx), &red, NULL, &brush );
            ID2D1RenderTarget_Clear( RT(ctx), &red );
            SET_TARGET( ctx, target );
            close_recorded = SLOT( list, CMDLIST_CLOSE, HRESULT (WINAPI *)( void * ) )( list );
            ID2D1SolidColorBrush_Release( brush );
            IUnknown_Release( (IUnknown *)list );
        }
        if (SUCCEEDED( SLOT( ctx, DC_CREATE_COMMAND_LIST, HRESULT (WINAPI *)( void *, ID2D1CommandList ** ) )( ctx, &list ) ))
        {
            close_empty = SLOT( list, CMDLIST_CLOSE, HRESULT (WINAPI *)( void * ) )( list );
            IUnknown_Release( (IUnknown *)list );
        }

        /* geometry realizations */
        {
            D2D1_RECT_F rect = { 4, 4, 20, 20 };
            ID2D1RectangleGeometry *geometry;
            IUnknown *realization = NULL;
            int ok = 0;

            ID2D1RenderTarget_CreateSolidColorBrush( RT(ctx), &red, NULL, &brush );
            if (SUCCEEDED( ID2D1Factory_CreateRectangleGeometry( (ID2D1Factory *)factory, &rect, &geometry ) ))
            {
                IUnknown *ctx1;
                if (SUCCEEDED( IUnknown_QueryInterface( (IUnknown *)ctx, &IID_ID2D1DeviceContext1, (void **)&ctx1 ) ))
                {
                    ok = SUCCEEDED( SLOT( ctx1, DC1_CREATE_FILLED_REALIZATION,
                                          HRESULT (WINAPI *)( void *, ID2D1Geometry *, float, IUnknown ** ) )(
                                          ctx1, (ID2D1Geometry *)geometry, 0.25f, &realization ) );
                    if (ok)
                    {
                        SLOT( ctx1, DC1_DRAW_REALIZATION, void (WINAPI *)( void *, IUnknown *, ID2D1Brush * ) )(
                                ctx1, realization, (ID2D1Brush *)brush );
                        IUnknown_Release( realization );
                    }
                    IUnknown_Release( ctx1 );
                }
                ID2D1RectangleGeometry_Release( geometry );
            }
            ID2D1SolidColorBrush_Release( brush );
            printf( "realization=%d\n", ok );
        }
        ID2D1RenderTarget_EndDraw( RT(ctx), NULL, NULL );

        printf( "flush_states=%d\n", outside == D2DERR_WRONG_STATE && inside == S_OK );
        printf( "cmdlist_close_empty=%d\n", close_empty == S_OK );
        printf( "cmdlist_record_midframe=%d\n", close_recorded == S_OK );
        IUnknown_Release( (IUnknown *)target );
    }

    /* an sRGB color context has an ICC profile; gradients are ID2D1GradientStopCollection1 */
    {
        ID2D1ColorContext *color;
        ID2D1GradientStopCollection *stops;
        D2D1_GRADIENT_STOP stop[2] = { { 0, { 0, 0, 0, 1 } }, { 1, { 1, 1, 1, 1 } } };
        IUnknown *stops1;
        int profile_ok = 0, gradient1 = 0;

        if (SUCCEEDED( SLOT( ctx, DC_CREATE_COLOR_CONTEXT, HRESULT (WINAPI *)( void *, D2D1_COLOR_SPACE, const BYTE *, UINT32,
                                                                     ID2D1ColorContext ** ) )(
                           ctx, D2D1_COLOR_SPACE_SRGB, NULL, 0, &color ) ))
        {
            UINT32 profile_size = SLOT( color, COLORCTX_GET_PROFILE_SIZE, UINT32 (WINAPI *)( void * ) )( color );
            BYTE *profile = profile_size ? malloc( profile_size ) : NULL;
            if (profile && SUCCEEDED( SLOT( color, COLORCTX_GET_PROFILE, HRESULT (WINAPI *)( void *, BYTE *, UINT32 ) )(
                                          color, profile, profile_size ) ))
                profile_ok = profile_size >= 128 && !memcmp( profile + 36, "acsp", 4 )
                             && !memcmp( profile + 16, "RGB ", 4 );
            free( profile );
            IUnknown_Release( (IUnknown *)color );
        }
        /* ID2D1RenderTarget::CreateGradientStopCollection (mingw's C macro takes the wrong arguments) */
        if (SUCCEEDED( SLOT( ctx, 9, HRESULT (WINAPI *)( void *, const D2D1_GRADIENT_STOP *, UINT32, D2D1_GAMMA,
                                                          D2D1_EXTEND_MODE, ID2D1GradientStopCollection ** ) )(
                           ctx, stop, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &stops ) ))
        {
            if (SUCCEEDED( ID2D1GradientStopCollection_QueryInterface( stops, &probe_IID_ID2D1GradientStopCollection1,
                                                                       (void **)&stops1 ) ))
            {
                gradient1 = 1;
                IUnknown_Release( stops1 );
            }
            ID2D1GradientStopCollection_Release( stops );
        }
        printf( "srgb_profile=%d\n", profile_ok );
        printf( "gradient1=%d\n", gradient1 );
    }

    check_geometry( (ID2D1Factory *)factory );
    check_dxgi_d3dcompiler();
    check_animation();
    printf( "done=1\n" );
    return 0;
}
