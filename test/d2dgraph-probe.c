/* d2dgraph-probe: Direct2D effect graphs as Paint.NET builds them
 * (patches/sg/0341).
 *
 *  - one effect input feeds two transform nodes; the output node is the one
 *    that is drawn (Paint.NET's "Convert Alpha": premultiply and
 *    un-premultiply nodes, one chosen as output);
 *  - an effect that rebuilds its graph in PrepareForRender: its draw
 *    transform is told its draw info when it is added, and sets its pixel
 *    shader then (Paint.NET's affine transform);
 *  - DrawImage with an image rectangle puts that rectangle's top left at the
 *    target offset, for effects as for bitmaps (Paint.NET's canvas).
 *
 * The effects' pixel shader swaps red and green: a red bitmap drawn through
 * them comes out green.
 *
 * Prints name=value lines; see test/d2dgraph-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effectauthor.h>
#include <wincodec.h>
#include <stdio.h>

HRESULT WINAPI D3DCompile( const void *data, SIZE_T size, const char *name, const void *defines, void *include,
                           const char *entry, const char *target, UINT flags1, UINT flags2, ID3DBlob **code,
                           ID3DBlob **errors );

DEFINE_GUID(probe_IID_ID2D1TransformNode, 0xb2efe1e7,0x729f,0x4102,0x94,0x9f,0x50,0x5f,0xa2,0x1b,0xf6,0x66);
DEFINE_GUID(probe_IID_ID2D1Transform,     0xef1a287d,0x342a,0x4f76,0x8f,0xdb,0xda,0x0d,0x6e,0xa9,0xf9,0x2b);
DEFINE_GUID(probe_IID_ID2D1DrawTransform, 0x36bfdcb6,0x9739,0x435d,0xa3,0x0d,0xa6,0x53,0xbe,0xff,0x6a,0x6f);
DEFINE_GUID(probe_CLSID_Pick,             0x5e8f3a1c,0x7d2b,0x4c9e,0x9a,0x10,0x53,0x47,0x46,0x58,0x45,0x01);
DEFINE_GUID(probe_CLSID_Rebuild,          0x5e8f3a1c,0x7d2b,0x4c9e,0x9a,0x10,0x53,0x47,0x46,0x58,0x45,0x02);
DEFINE_GUID(probe_shader_swap,            0x5e8f3a1c,0x7d2b,0x4c9e,0x9a,0x10,0x53,0x47,0x46,0x58,0x45,0x03);
DEFINE_GUID(probe_shader_black,           0x5e8f3a1c,0x7d2b,0x4c9e,0x9a,0x10,0x53,0x47,0x46,0x58,0x45,0x04);

/* vtable slots of what the C headers lack (from the interfaces' definitions) */
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
#define GRAPH_SET_SINGLE_NODE         4
#define GRAPH_ADD_NODE                5
#define GRAPH_SET_OUTPUT_NODE         7
#define GRAPH_CONNECT_TO_EFFECT_INPUT 9
#define GRAPH_CLEAR                   10
#define EFFECT_SET_INPUT              14
#define EFFECT_GET_OUTPUT             18
#define DC_CREATE_BITMAP_FROM_SURFACE 62
#define DC_CREATE_BITMAP1             57
#define DC_DRAW_IMAGE                 83
#define DC_SET_TARGET                 74
#define EFFECTCTX_LOAD_PIXEL_SHADER   11
#define DRAWINFO_SET_PIXEL_SHADER     10
#define RT(ctx) ((ID2D1RenderTarget *)(ctx))
#define SET_TARGET(ctx, t) SLOT( ctx, DC_SET_TARGET, void (WINAPI *)( void *, IUnknown * ) )( ctx, (IUnknown *)(t) )

static const char swap_ps[] =
    "Texture2D t0 : register(t0); SamplerState s0 : register(s0);\n"
    "float4 main(float4 pos : SV_POSITION, float4 scene : SCENE_POSITION, float4 uv0 : TEXCOORD0) : SV_TARGET\n"
    "{ float4 c = t0.Sample(s0, uv0.xy); return float4(c.g, c.r, c.b, c.a); }\n";
static const char black_ps[] =
    "Texture2D t0 : register(t0); SamplerState s0 : register(s0);\n"
    "float4 main(float4 pos : SV_POSITION, float4 scene : SCENE_POSITION, float4 uv0 : TEXCOORD0) : SV_TARGET\n"
    "{ float4 c = t0.Sample(s0, uv0.xy); return float4(0, 0, 0.5 * c.a, c.a); }\n";

/* a one-input draw transform running a given shader */
struct transform
{
    const void *vtbl;
    LONG ref;
    const GUID *shader;
    int told;                           /* SetDrawInfo called */
};
static HRESULT WINAPI t_QueryInterface( struct transform *t, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &probe_IID_ID2D1TransformNode )
        || IsEqualGUID( iid, &probe_IID_ID2D1Transform ) || IsEqualGUID( iid, &probe_IID_ID2D1DrawTransform ))
    {
        *out = t;
        InterlockedIncrement( &t->ref );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI t_AddRef( struct transform *t ) { return InterlockedIncrement( &t->ref ); }
static ULONG WINAPI t_Release( struct transform *t ) { return InterlockedDecrement( &t->ref ); }
static UINT32 WINAPI t_GetInputCount( struct transform *t ) { return 1; }
static HRESULT WINAPI t_MapOutputRectToInputRects( struct transform *t, const D2D1_RECT_L *out, D2D1_RECT_L *in,
                                                   UINT32 count )
{
    if (count) in[0] = *out;
    return S_OK;
}
static HRESULT WINAPI t_MapInputRectsToOutputRect( struct transform *t, const D2D1_RECT_L *in,
                                                   const D2D1_RECT_L *opaque_in, UINT32 count, D2D1_RECT_L *out,
                                                   D2D1_RECT_L *opaque_out )
{
    *out = count ? in[0] : (D2D1_RECT_L){ 0 };
    memset( opaque_out, 0, sizeof(*opaque_out) );
    return S_OK;
}
static HRESULT WINAPI t_MapInvalidRect( struct transform *t, UINT32 index, D2D1_RECT_L in, D2D1_RECT_L *out )
{
    *out = in;
    return S_OK;
}
static HRESULT WINAPI t_SetDrawInfo( struct transform *t, IUnknown *info )
{
    t->told++;
    return SLOT( info, DRAWINFO_SET_PIXEL_SHADER, HRESULT (WINAPI *)( void *, REFGUID, UINT32 ) )(
            info, t->shader, 0 );
}
static const void *transform_vtbl[] =
{
    t_QueryInterface, t_AddRef, t_Release, t_GetInputCount, t_MapOutputRectToInputRects,
    t_MapInputRectsToOutputRect, t_MapInvalidRect, t_SetDrawInfo,
};

static struct transform swap_node = { transform_vtbl, 1, &probe_shader_swap };
static struct transform black_node = { transform_vtbl, 1, &probe_shader_black };
static struct transform rebuilt_node = { transform_vtbl, 1, &probe_shader_swap };

static HRESULT load_shader( ID2D1EffectContext *context, const GUID *id, const char *src, SIZE_T len )
{
    ID3DBlob *code = NULL;
    HRESULT hr;

    if (FAILED( hr = D3DCompile( src, len, NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &code, NULL ) ))
        return hr;
    hr = SLOT( context, EFFECTCTX_LOAD_PIXEL_SHADER, HRESULT (WINAPI *)( void *, REFGUID, const BYTE *, UINT32 ) )(
            context, id, ID3D10Blob_GetBufferPointer( code ), ID3D10Blob_GetBufferSize( code ) );
    ID3D10Blob_Release( code );
    return hr;
}

struct effect
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG ref;
    int rebuild;
    ID2D1TransformGraph *graph;
};
static HRESULT WINAPI e_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_ID2D1EffectImpl ))
    {
        *out = iface;
        IUnknown_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI e_AddRef( IUnknown *iface ) { return InterlockedIncrement( &((struct effect *)iface)->ref ); }
static ULONG WINAPI e_Release( IUnknown *iface )
{
    struct effect *e = (struct effect *)iface;
    ULONG ref = InterlockedDecrement( &e->ref );
    if (!ref)
    {
        if (e->graph) IUnknown_Release( (IUnknown *)e->graph );
        HeapFree( GetProcessHeap(), 0, e );
    }
    return ref;
}
static HRESULT WINAPI e_Initialize( ID2D1EffectImpl *iface, ID2D1EffectContext *context, ID2D1TransformGraph *graph )
{
    struct effect *e = (struct effect *)iface;
    HRESULT hr;

    if (FAILED( hr = load_shader( context, &probe_shader_swap, swap_ps, sizeof(swap_ps) - 1 ) )
            || FAILED( hr = load_shader( context, &probe_shader_black, black_ps, sizeof(black_ps) - 1 ) ))
        return hr;
    if (e->rebuild)
    {
        /* the graph is made when it is drawn */
        e->graph = graph;
        IUnknown_AddRef( (IUnknown *)graph );
        return S_OK;
    }
    /* input 0 feeds both nodes; the swap node is the output */
    SLOT( graph, GRAPH_ADD_NODE, HRESULT (WINAPI *)( void *, void * ) )( graph, &swap_node );
    SLOT( graph, GRAPH_ADD_NODE, HRESULT (WINAPI *)( void *, void * ) )( graph, &black_node );
    SLOT( graph, GRAPH_CONNECT_TO_EFFECT_INPUT, HRESULT (WINAPI *)( void *, UINT32, void *, UINT32 ) )(
            graph, 0, &swap_node, 0 );
    SLOT( graph, GRAPH_CONNECT_TO_EFFECT_INPUT, HRESULT (WINAPI *)( void *, UINT32, void *, UINT32 ) )(
            graph, 0, &black_node, 0 );
    return SLOT( graph, GRAPH_SET_OUTPUT_NODE, HRESULT (WINAPI *)( void *, void * ) )( graph, &swap_node );
}
static HRESULT WINAPI e_PrepareForRender( ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE type )
{
    struct effect *e = (struct effect *)iface;

    if (!e->rebuild) return S_OK;
    SLOT( e->graph, GRAPH_CLEAR, void (WINAPI *)( void * ) )( e->graph );
    return SLOT( e->graph, GRAPH_SET_SINGLE_NODE, HRESULT (WINAPI *)( void *, void * ) )( e->graph, &rebuilt_node );
}
static HRESULT WINAPI e_SetGraph( ID2D1EffectImpl *iface, ID2D1TransformGraph *graph ) { return E_NOTIMPL; }
static const ID2D1EffectImplVtbl effect_vtbl =
{
    { e_QueryInterface, e_AddRef, e_Release }, e_Initialize, e_PrepareForRender, e_SetGraph,
};
static HRESULT make_effect( IUnknown **out, int rebuild )
{
    struct effect *e = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*e) );
    e->ID2D1EffectImpl_iface.lpVtbl = &effect_vtbl;
    e->ref = 1;
    e->rebuild = rebuild;
    *out = (IUnknown *)&e->ID2D1EffectImpl_iface;
    return S_OK;
}
static HRESULT WINAPI pick_factory( IUnknown **out ) { return make_effect( out, 0 ); }
static HRESULT WINAPI rebuild_factory( IUnknown **out ) { return make_effect( out, 1 ); }

static const WCHAR effect_xml[] =
    L"<?xml version='1.0'?><Effect>"
    L"<Property name='DisplayName' type='string' value='Probe'/>"
    L"<Property name='Author' type='string' value='sg'/>"
    L"<Property name='Category' type='string' value='Test'/>"
    L"<Property name='Description' type='string' value='Probe'/>"
    L"<Inputs><Input name='Source'/></Inputs></Effect>";

/* draw an image with an offset and an optional image rectangle on a black 64x64 target; read a pixel */
static DWORD draw_and_read( ID3D11Device *d3d, ID2D1DeviceContext *ctx, ID2D1Image *image, float x, float y,
                            const D2D1_RECT_F *image_rect, unsigned int px, unsigned int py )
{
    D3D11_TEXTURE2D_DESC desc = { 64, 64, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT,
                                  D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    D2D1_BITMAP_PROPERTIES1 props = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96,
                                      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, NULL };
    ID3D11Texture2D *texture, *staging;
    ID3D11DeviceContext *dc;
    D3D11_MAPPED_SUBRESOURCE map;
    IDXGISurface *surface;
    ID2D1Bitmap1 *target;
    D2D1_COLOR_F black = { 0, 0, 0, 1 };
    D2D1_POINT_2F offset = { x, y };
    DWORD pixel = 0xdeadbeef;

    if (FAILED( ID3D11Device_CreateTexture2D( d3d, &desc, NULL, &texture ) )) return pixel;
    ID3D11Texture2D_QueryInterface( texture, &IID_IDXGISurface, (void **)&surface );
    if (SUCCEEDED( SLOT( ctx, DC_CREATE_BITMAP_FROM_SURFACE, HRESULT (WINAPI *)( void *, IDXGISurface *,
                                                                               const D2D1_BITMAP_PROPERTIES1 *,
                                                                               ID2D1Bitmap1 ** ) )(
                       ctx, surface, &props, &target ) ))
    {
        SET_TARGET( ctx, target );
        ID2D1RenderTarget_BeginDraw( RT(ctx) );
        ID2D1RenderTarget_Clear( RT(ctx), &black );
        SLOT( ctx, DC_DRAW_IMAGE, void (WINAPI *)( void *, ID2D1Image *, const D2D1_POINT_2F *, const D2D1_RECT_F *,
                                                  UINT32, UINT32 ) )( ctx, image, &offset, image_rect, 0, 0 );
        ID2D1RenderTarget_EndDraw( RT(ctx), NULL, NULL );
        SET_TARGET( ctx, NULL );
        IUnknown_Release( (IUnknown *)target );

        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Device_CreateTexture2D( d3d, &desc, NULL, &staging );
        ID3D11Device_GetImmediateContext( d3d, &dc );
        ID3D11DeviceContext_CopyResource( dc, (ID3D11Resource *)staging, (ID3D11Resource *)texture );
        if (SUCCEEDED( ID3D11DeviceContext_Map( dc, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map ) ))
        {
            pixel = ((DWORD *)((BYTE *)map.pData + py * map.RowPitch))[px];
            ID3D11DeviceContext_Unmap( dc, (ID3D11Resource *)staging, 0 );
        }
        ID3D11DeviceContext_Release( dc );
        ID3D11Texture2D_Release( staging );
    }
    IDXGISurface_Release( surface );
    ID3D11Texture2D_Release( texture );
    return pixel;
}

static ID2D1Image *effect_output( ID2D1DeviceContext *ctx, const GUID *clsid, ID2D1Image *input,
                                  ID2D1Effect **effect )
{
    ID2D1Image *output = NULL;

    if (FAILED( ID2D1DeviceContext_CreateEffect( ctx, clsid, effect ) )) return NULL;
    SLOT( *effect, EFFECT_SET_INPUT, void (WINAPI *)( void *, UINT32, ID2D1Image *, BOOL ) )( *effect, 0, input, FALSE );
    SLOT( *effect, EFFECT_GET_OUTPUT, void (WINAPI *)( void *, ID2D1Image ** ) )( *effect, &output );
    return output;
}

int main( void )
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D2D1_BITMAP_PROPERTIES1 plain = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96,
                                      D2D1_BITMAP_OPTIONS_NONE, NULL };
    D2D1_SIZE_U size = { 16, 16 };
    ID3D11Device *d3d;
    IDXGIDevice *dxgi;
    ID2D1Factory1 *factory;
    ID2D1Device *device;
    ID2D1DeviceContext *ctx;
    ID2D1Bitmap1 *bitmap;
    ID2D1Effect *effect;
    ID2D1Image *output;
    DWORD red[256], a, b, c;
    unsigned int i;
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
    ID2D1Factory1_RegisterEffectFromString( factory, &probe_CLSID_Pick, effect_xml, NULL, 0, pick_factory );
    ID2D1Factory1_RegisterEffectFromString( factory, &probe_CLSID_Rebuild, effect_xml, NULL, 0, rebuild_factory );

    for (i = 0; i < 256; i++) red[i] = 0xffff0000;
    if (FAILED( hr = SLOT( ctx, DC_CREATE_BITMAP1, HRESULT (WINAPI *)( void *, D2D1_SIZE_U, const void *, UINT32,
                                                                   const D2D1_BITMAP_PROPERTIES1 *, ID2D1Bitmap1 ** ) )(
                    ctx, size, red, 64, &plain, &bitmap ) ))
    {
        printf( "no_bitmap=%#lx\n", hr );
        return 1;
    }

    /* one input, two nodes: the output node draws the input */
    if ((output = effect_output( ctx, &probe_CLSID_Pick, (ID2D1Image *)bitmap, &effect )))
    {
        a = draw_and_read( d3d, ctx, output, 20, 20, NULL, 28, 28 );
        printf( "shared_input=%d (%08lx)\n", a == 0xff00ff00, a );
        IUnknown_Release( (IUnknown *)output );
        IUnknown_Release( (IUnknown *)effect );
    }
    else printf( "shared_input=0 (no effect)\n" );

    /* a graph made in PrepareForRender: its transform is told its draw info and draws */
    if ((output = effect_output( ctx, &probe_CLSID_Rebuild, (ID2D1Image *)bitmap, &effect )))
    {
        a = draw_and_read( d3d, ctx, output, 20, 20, NULL, 28, 28 );
        printf( "rebuilt_graph=%d (%08lx told %d)\n", a == 0xff00ff00, a, rebuilt_node.told );
        IUnknown_Release( (IUnknown *)output );
        IUnknown_Release( (IUnknown *)effect );
    }
    else printf( "rebuilt_graph=0 (no effect)\n" );

    /* an image rectangle's top left goes to the target offset: the effect's (8,8)-(16,16) at (20,20) */
    if ((output = effect_output( ctx, &probe_CLSID_Pick, (ID2D1Image *)bitmap, &effect )))
    {
        D2D1_RECT_F part = { 8, 8, 16, 16 };
        a = draw_and_read( d3d, ctx, output, 20, 20, &part, 21, 21 );
        b = draw_and_read( d3d, ctx, output, 20, 20, &part, 27, 27 );
        c = draw_and_read( d3d, ctx, output, 20, 20, &part, 30, 30 );
        printf( "image_rect_effect=%d (%08lx %08lx %08lx)\n", a == 0xff00ff00 && b == 0xff00ff00 && c == 0xff000000,
                a, b, c );
        IUnknown_Release( (IUnknown *)output );
        IUnknown_Release( (IUnknown *)effect );
    }
    else printf( "image_rect_effect=0 (no effect)\n" );

    /* and a bitmap's likewise (as before) */
    {
        D2D1_RECT_F part = { 8, 8, 16, 16 };
        a = draw_and_read( d3d, ctx, (ID2D1Image *)bitmap, 20, 20, &part, 21, 21 );
        c = draw_and_read( d3d, ctx, (ID2D1Image *)bitmap, 20, 20, &part, 30, 30 );
        printf( "image_rect_bitmap=%d (%08lx %08lx)\n", a == 0xffff0000 && c == 0xff000000, a, c );
    }

    printf( "done=1\n" );
    return 0;
}
