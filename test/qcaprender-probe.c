/* qcaprender-probe: ICaptureGraphBuilder2::RenderStream's source search (patches/sg/6516).
 *
 * RenderStream returned E_NOTIMPL for VBI pins ("Tee/Sink-to-Sink filter not supported") and
 * searched for the pin to render depth first. With filters of the probe's own (a source with a
 * VBI-category pin and two plain outputs, a middle filter, a two-input sink) it renders a VBI
 * stream, renders the source's own unconnected output before the outputs further down, fails
 * with E_FAIL when everything is connected, and does nothing (S_OK) for a media type nothing has.
 *
 * Prints name=value lines; see test/qcaprender-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <ks.h>
#include <ksproxy.h>
#include <stdio.h>
#include <string.h>

static const GUID probe_type = {0x5347aaaa, 0x1111, 0x2222, {0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};
static const GUID other_type = {0x5347bbbb, 0x1111, 0x2222, {0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};
DEFINE_GUID(probe_AMPROPSETID_Pin, 0x9b00f101, 0x1567, 0x11d1, 0xb3, 0xf1, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5);
DEFINE_GUID(probe_PIN_CATEGORY_VBI, 0xfb6c4284, 0x0353, 0x11d1, 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID(probe_IID_IKsPropertySet, 0x31efac30, 0x515c, 0x11d0, 0xa9, 0xaa, 0x00, 0xaa, 0x00, 0x61, 0xbe, 0x93);

struct filter;

struct pin
{
    IPin IPin_iface;
    IKsPropertySet IKsPropertySet_iface;
    struct filter *filter;
    PIN_DIRECTION dir;
    WCHAR name[16];
    const GUID *category;   /* NULL: none (no IKsPropertySet) */
    IPin *peer;
    AM_MEDIA_TYPE mt;
};

struct filter
{
    IBaseFilter IBaseFilter_iface;
    LONG ref;
    WCHAR name[16];
    IFilterGraph *graph;
    struct pin pins[3];
    int pin_count;
};

/* ---- IEnumMediaTypes over one type ---- */
struct enum_mt { IEnumMediaTypes iface; LONG ref; struct pin *pin; int pos; };
static HRESULT WINAPI emt_QueryInterface( IEnumMediaTypes *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IEnumMediaTypes )) { *out = iface; IEnumMediaTypes_AddRef( iface ); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI emt_AddRef( IEnumMediaTypes *iface ) { return InterlockedIncrement( &((struct enum_mt *)iface)->ref ); }
static ULONG WINAPI emt_Release( IEnumMediaTypes *iface )
{
    struct enum_mt *e = (struct enum_mt *)iface; ULONG ref = InterlockedDecrement( &e->ref );
    if (!ref) free( e );
    return ref;
}
static HRESULT WINAPI emt_Next( IEnumMediaTypes *iface, ULONG count, AM_MEDIA_TYPE **out, ULONG *fetched )
{
    struct enum_mt *e = (struct enum_mt *)iface;
    ULONG n = 0;
    if (count && !e->pos)
    {
        out[0] = CoTaskMemAlloc( sizeof(AM_MEDIA_TYPE) );
        *out[0] = e->pin->mt;
        e->pos++; n = 1;
    }
    if (fetched) *fetched = n;
    return n == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI emt_Skip( IEnumMediaTypes *iface, ULONG count ) { ((struct enum_mt *)iface)->pos += count; return S_OK; }
static HRESULT WINAPI emt_Reset( IEnumMediaTypes *iface ) { ((struct enum_mt *)iface)->pos = 0; return S_OK; }
static HRESULT WINAPI emt_Clone( IEnumMediaTypes *iface, IEnumMediaTypes **out ) { return E_NOTIMPL; }
static const IEnumMediaTypesVtbl enum_mt_vtbl = { emt_QueryInterface, emt_AddRef, emt_Release, emt_Next, emt_Skip, emt_Reset, emt_Clone };

/* ---- IEnumPins ---- */
struct enum_pins { IEnumPins iface; LONG ref; struct filter *filter; int pos; };
static HRESULT WINAPI ep_QueryInterface( IEnumPins *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IEnumPins )) { *out = iface; IEnumPins_AddRef( iface ); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ep_AddRef( IEnumPins *iface ) { return InterlockedIncrement( &((struct enum_pins *)iface)->ref ); }
static ULONG WINAPI ep_Release( IEnumPins *iface )
{
    struct enum_pins *e = (struct enum_pins *)iface; ULONG ref = InterlockedDecrement( &e->ref );
    if (!ref) { IBaseFilter_Release( &e->filter->IBaseFilter_iface ); free( e ); }
    return ref;
}
static HRESULT WINAPI ep_Next( IEnumPins *iface, ULONG count, IPin **out, ULONG *fetched )
{
    struct enum_pins *e = (struct enum_pins *)iface;
    ULONG n = 0;
    while (n < count && e->pos < e->filter->pin_count)
    {
        out[n] = &e->filter->pins[e->pos++].IPin_iface;
        IPin_AddRef( out[n] );
        n++;
    }
    if (fetched) *fetched = n;
    return n == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI ep_Skip( IEnumPins *iface, ULONG count ) { ((struct enum_pins *)iface)->pos += count; return S_OK; }
static HRESULT WINAPI ep_Reset( IEnumPins *iface ) { ((struct enum_pins *)iface)->pos = 0; return S_OK; }
static HRESULT WINAPI ep_Clone( IEnumPins *iface, IEnumPins **out ) { return E_NOTIMPL; }
static const IEnumPinsVtbl enum_pins_vtbl = { ep_QueryInterface, ep_AddRef, ep_Release, ep_Next, ep_Skip, ep_Reset, ep_Clone };

/* ---- IPin ---- */
static struct pin *impl_from_IPin( IPin *iface ) { return CONTAINING_RECORD( iface, struct pin, IPin_iface ); }

static HRESULT WINAPI pin_QueryInterface( IPin *iface, REFIID iid, void **out )
{
    struct pin *pin = impl_from_IPin( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IPin )) *out = iface;
    else if (IsEqualGUID( iid, &probe_IID_IKsPropertySet ) && pin->category) *out = &pin->IKsPropertySet_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    IUnknown_AddRef( (IUnknown *)*out );
    return S_OK;
}
static ULONG WINAPI pin_AddRef( IPin *iface ) { return IBaseFilter_AddRef( &impl_from_IPin( iface )->filter->IBaseFilter_iface ); }
static ULONG WINAPI pin_Release( IPin *iface ) { return IBaseFilter_Release( &impl_from_IPin( iface )->filter->IBaseFilter_iface ); }

static HRESULT WINAPI pin_Connect( IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt )
{
    struct pin *pin = impl_from_IPin( iface );
    HRESULT hr;

    if (pin->dir != PINDIR_OUTPUT) return E_UNEXPECTED;
    if (pin->peer) return VFW_E_ALREADY_CONNECTED;
    if (mt && !IsEqualGUID( &mt->majortype, &pin->mt.majortype ) && !IsEqualGUID( &mt->majortype, &GUID_NULL ))
        return VFW_E_TYPE_NOT_ACCEPTED;
    if (FAILED(hr = IPin_ReceiveConnection( peer, iface, &pin->mt ))) return hr;
    IPin_AddRef( pin->peer = peer );
    return S_OK;
}
static HRESULT WINAPI pin_ReceiveConnection( IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt )
{
    struct pin *pin = impl_from_IPin( iface );
    if (pin->dir != PINDIR_INPUT) return E_UNEXPECTED;
    if (pin->peer) return VFW_E_ALREADY_CONNECTED;
    if (!IsEqualGUID( &mt->majortype, &pin->mt.majortype )) return VFW_E_TYPE_NOT_ACCEPTED;
    IPin_AddRef( pin->peer = peer );
    return S_OK;
}
static HRESULT WINAPI pin_Disconnect( IPin *iface )
{
    struct pin *pin = impl_from_IPin( iface );
    if (!pin->peer) return S_FALSE;
    IPin_Release( pin->peer );
    pin->peer = NULL;
    return S_OK;
}
static HRESULT WINAPI pin_ConnectedTo( IPin *iface, IPin **peer )
{
    struct pin *pin = impl_from_IPin( iface );
    if (!(*peer = pin->peer)) return VFW_E_NOT_CONNECTED;
    IPin_AddRef( *peer );
    return S_OK;
}
static HRESULT WINAPI pin_ConnectionMediaType( IPin *iface, AM_MEDIA_TYPE *mt )
{
    struct pin *pin = impl_from_IPin( iface );
    if (!pin->peer) return VFW_E_NOT_CONNECTED;
    *mt = pin->mt;
    return S_OK;
}
static HRESULT WINAPI pin_QueryPinInfo( IPin *iface, PIN_INFO *info )
{
    struct pin *pin = impl_from_IPin( iface );
    info->pFilter = &pin->filter->IBaseFilter_iface;
    IBaseFilter_AddRef( info->pFilter );
    info->dir = pin->dir;
    lstrcpyW( info->achName, pin->name );
    return S_OK;
}
static HRESULT WINAPI pin_QueryDirection( IPin *iface, PIN_DIRECTION *dir ) { *dir = impl_from_IPin( iface )->dir; return S_OK; }
static HRESULT WINAPI pin_QueryId( IPin *iface, WCHAR **id )
{
    struct pin *pin = impl_from_IPin( iface );
    *id = CoTaskMemAlloc( (lstrlenW( pin->name ) + 1) * sizeof(WCHAR) );
    lstrcpyW( *id, pin->name );
    return S_OK;
}
static HRESULT WINAPI pin_QueryAccept( IPin *iface, const AM_MEDIA_TYPE *mt )
{
    return IsEqualGUID( &mt->majortype, &impl_from_IPin( iface )->mt.majortype ) ? S_OK : S_FALSE;
}
static HRESULT WINAPI pin_EnumMediaTypes( IPin *iface, IEnumMediaTypes **out )
{
    struct enum_mt *e = calloc( 1, sizeof(*e) );
    e->iface.lpVtbl = &enum_mt_vtbl; e->ref = 1; e->pin = impl_from_IPin( iface );
    *out = &e->iface;
    return S_OK;
}
static HRESULT WINAPI pin_QueryInternalConnections( IPin *iface, IPin **pins, ULONG *count ) { return E_NOTIMPL; }
static HRESULT WINAPI pin_EndOfStream( IPin *iface ) { return S_OK; }
static HRESULT WINAPI pin_BeginFlush( IPin *iface ) { return S_OK; }
static HRESULT WINAPI pin_EndFlush( IPin *iface ) { return S_OK; }
static HRESULT WINAPI pin_NewSegment( IPin *iface, REFERENCE_TIME start, REFERENCE_TIME stop, double rate ) { return S_OK; }
static const IPinVtbl pin_vtbl =
{
    pin_QueryInterface, pin_AddRef, pin_Release, pin_Connect, pin_ReceiveConnection, pin_Disconnect, pin_ConnectedTo,
    pin_ConnectionMediaType, pin_QueryPinInfo, pin_QueryDirection, pin_QueryId, pin_QueryAccept, pin_EnumMediaTypes,
    pin_QueryInternalConnections, pin_EndOfStream, pin_BeginFlush, pin_EndFlush, pin_NewSegment,
};

/* ---- IKsPropertySet: the pin category ---- */
static struct pin *impl_from_IKsPropertySet( IKsPropertySet *iface ) { return CONTAINING_RECORD( iface, struct pin, IKsPropertySet_iface ); }
static HRESULT WINAPI ks_QueryInterface( IKsPropertySet *iface, REFIID iid, void **out ) { return IPin_QueryInterface( &impl_from_IKsPropertySet( iface )->IPin_iface, iid, out ); }
static ULONG WINAPI ks_AddRef( IKsPropertySet *iface ) { return IPin_AddRef( &impl_from_IKsPropertySet( iface )->IPin_iface ); }
static ULONG WINAPI ks_Release( IKsPropertySet *iface ) { return IPin_Release( &impl_from_IKsPropertySet( iface )->IPin_iface ); }
static HRESULT WINAPI ks_Set( IKsPropertySet *iface, REFGUID set, DWORD id, void *instance, DWORD instance_size, void *data, DWORD size ) { return E_NOTIMPL; }
static HRESULT WINAPI ks_Get( IKsPropertySet *iface, REFGUID set, DWORD id, void *instance, DWORD instance_size, void *data, DWORD size, DWORD *ret )
{
    struct pin *pin = impl_from_IKsPropertySet( iface );
    if (!IsEqualGUID( set, &probe_AMPROPSETID_Pin ) || id != 0 /* AMPROPERTY_PIN_CATEGORY */ || size < sizeof(GUID)) return E_PROP_ID_UNSUPPORTED;
    memcpy( data, pin->category, sizeof(GUID) );
    if (ret) *ret = sizeof(GUID);
    return S_OK;
}
static HRESULT WINAPI ks_QuerySupported( IKsPropertySet *iface, REFGUID set, DWORD id, DWORD *support ) { return E_NOTIMPL; }
static const IKsPropertySetVtbl ks_vtbl = { ks_QueryInterface, ks_AddRef, ks_Release, ks_Set, ks_Get, ks_QuerySupported };

/* ---- IBaseFilter ---- */
static struct filter *impl_from_IBaseFilter( IBaseFilter *iface ) { return CONTAINING_RECORD( iface, struct filter, IBaseFilter_iface ); }
static HRESULT WINAPI filter_QueryInterface( IBaseFilter *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IPersist ) || IsEqualGUID( iid, &IID_IMediaFilter ) ||
        IsEqualGUID( iid, &IID_IBaseFilter )) { *out = iface; IBaseFilter_AddRef( iface ); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI filter_AddRef( IBaseFilter *iface ) { return InterlockedIncrement( &impl_from_IBaseFilter( iface )->ref ); }
static ULONG WINAPI filter_Release( IBaseFilter *iface ) { return InterlockedDecrement( &impl_from_IBaseFilter( iface )->ref ); }
static HRESULT WINAPI filter_GetClassID( IBaseFilter *iface, CLSID *clsid ) { memset( clsid, 0, sizeof(*clsid) ); return S_OK; }
static HRESULT WINAPI filter_Stop( IBaseFilter *iface ) { return S_OK; }
static HRESULT WINAPI filter_Pause( IBaseFilter *iface ) { return S_OK; }
static HRESULT WINAPI filter_Run( IBaseFilter *iface, REFERENCE_TIME start ) { return S_OK; }
static HRESULT WINAPI filter_GetState( IBaseFilter *iface, DWORD timeout, FILTER_STATE *state ) { *state = State_Stopped; return S_OK; }
static HRESULT WINAPI filter_SetSyncSource( IBaseFilter *iface, IReferenceClock *clock ) { return S_OK; }
static HRESULT WINAPI filter_GetSyncSource( IBaseFilter *iface, IReferenceClock **clock ) { *clock = NULL; return S_OK; }
static HRESULT WINAPI filter_EnumPins( IBaseFilter *iface, IEnumPins **out )
{
    struct enum_pins *e = calloc( 1, sizeof(*e) );
    e->iface.lpVtbl = &enum_pins_vtbl; e->ref = 1; e->filter = impl_from_IBaseFilter( iface );
    IBaseFilter_AddRef( iface );
    *out = &e->iface;
    return S_OK;
}
static HRESULT WINAPI filter_FindPin( IBaseFilter *iface, const WCHAR *id, IPin **pin )
{
    struct filter *filter = impl_from_IBaseFilter( iface );
    int i;
    for (i = 0; i < filter->pin_count; i++)
        if (!lstrcmpW( filter->pins[i].name, id )) { IPin_AddRef( *pin = &filter->pins[i].IPin_iface ); return S_OK; }
    *pin = NULL;
    return VFW_E_NOT_FOUND;
}
static HRESULT WINAPI filter_QueryFilterInfo( IBaseFilter *iface, FILTER_INFO *info )
{
    struct filter *filter = impl_from_IBaseFilter( iface );
    lstrcpyW( info->achName, filter->name );
    if ((info->pGraph = filter->graph)) IFilterGraph_AddRef( info->pGraph );
    return S_OK;
}
static HRESULT WINAPI filter_JoinFilterGraph( IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name )
{
    impl_from_IBaseFilter( iface )->graph = graph;
    return S_OK;
}
static HRESULT WINAPI filter_QueryVendorInfo( IBaseFilter *iface, WCHAR **info ) { return E_NOTIMPL; }
static const IBaseFilterVtbl filter_vtbl =
{
    filter_QueryInterface, filter_AddRef, filter_Release, filter_GetClassID, filter_Stop, filter_Pause, filter_Run,
    filter_GetState, filter_SetSyncSource, filter_GetSyncSource, filter_EnumPins, filter_FindPin, filter_QueryFilterInfo,
    filter_JoinFilterGraph, filter_QueryVendorInfo,
};

static void filter_init( struct filter *filter, const WCHAR *name )
{
    memset( filter, 0, sizeof(*filter) );
    filter->IBaseFilter_iface.lpVtbl = &filter_vtbl;
    filter->ref = 1;
    lstrcpyW( filter->name, name );
}

static void add_pin( struct filter *filter, const WCHAR *name, PIN_DIRECTION dir, const GUID *type, const GUID *category )
{
    struct pin *pin = &filter->pins[filter->pin_count++];
    pin->IPin_iface.lpVtbl = &pin_vtbl;
    pin->IKsPropertySet_iface.lpVtbl = &ks_vtbl;
    pin->filter = filter;
    pin->dir = dir;
    lstrcpyW( pin->name, name );
    pin->category = category;
    pin->mt.majortype = *type;
    pin->mt.subtype = GUID_NULL;
    pin->mt.formattype = GUID_NULL;
}

static ICaptureGraphBuilder2 *builder;
static IGraphBuilder *graph;

static void drop_graph( void )
{
    if (graph) IGraphBuilder_Release( graph );
    graph = NULL;
}

static void fresh_graph( struct filter **filters, int count )
{
    int i;
    drop_graph();
    CoCreateInstance( &CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph );
    ICaptureGraphBuilder2_SetFiltergraph( builder, graph );
    for (i = 0; i < count; i++) IGraphBuilder_AddFilter( graph, &filters[i]->IBaseFilter_iface, filters[i]->name );
}

int main( void )
{
    static struct filter source, middle, sink, vbi_source, sink_a, source_c, sink_c;
    struct filter *filters[3];
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    hr = CoCreateInstance( &CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, &IID_ICaptureGraphBuilder2, (void **)&builder );
    if (FAILED(hr)) { printf( "builder=0 hr=%#lx\n", hr ); return 1; }

    /* a VBI stream */
    filter_init( &vbi_source, L"vbi" );
    add_pin( &vbi_source, L"VBI", PINDIR_OUTPUT, &probe_type, &probe_PIN_CATEGORY_VBI );
    filter_init( &sink_a, L"sink" );
    add_pin( &sink_a, L"in1", PINDIR_INPUT, &probe_type, NULL );
    add_pin( &sink_a, L"in2", PINDIR_INPUT, &probe_type, NULL );
    filters[0] = &vbi_source; filters[1] = &sink_a;
    fresh_graph( filters, 2 );
    hr = ICaptureGraphBuilder2_RenderStream( builder, &probe_PIN_CATEGORY_VBI, NULL, (IUnknown *)&vbi_source.IBaseFilter_iface, NULL,
                                             &sink_a.IBaseFilter_iface );
    printf( "vbi=%d hr=%#lx connected=%d\n", hr == S_OK && vbi_source.pins[0].peer == &sink_a.pins[0].IPin_iface, hr,
            vbi_source.pins[0].peer != NULL );

    /* breadth first: the source's own unconnected output before the middle filter's */
    drop_graph();
    filter_init( &source, L"source" );
    add_pin( &source, L"out1", PINDIR_OUTPUT, &probe_type, NULL );
    add_pin( &source, L"out2", PINDIR_OUTPUT, &probe_type, NULL );
    filter_init( &middle, L"middle" );
    add_pin( &middle, L"in", PINDIR_INPUT, &probe_type, NULL );
    add_pin( &middle, L"out", PINDIR_OUTPUT, &probe_type, NULL );
    filter_init( &sink, L"sink" );
    add_pin( &sink, L"in1", PINDIR_INPUT, &probe_type, NULL );
    add_pin( &sink, L"in2", PINDIR_INPUT, &probe_type, NULL );
    filters[0] = &source; filters[1] = &middle; filters[2] = &sink;
    fresh_graph( filters, 3 );
    hr = IGraphBuilder_ConnectDirect( graph, &source.pins[0].IPin_iface, &middle.pins[0].IPin_iface, NULL );
    printf( "setup=%d hr=%#lx\n", hr == S_OK, hr );
    hr = ICaptureGraphBuilder2_RenderStream( builder, NULL, NULL, (IUnknown *)&source.IBaseFilter_iface, NULL, &sink.IBaseFilter_iface );
    printf( "own_first=%d hr=%#lx out2=%d middle_out=%d\n", hr == S_OK && source.pins[1].peer && !middle.pins[1].peer, hr,
            source.pins[1].peer != NULL, middle.pins[1].peer != NULL );
    hr = ICaptureGraphBuilder2_RenderStream( builder, NULL, NULL, (IUnknown *)&source.IBaseFilter_iface, NULL, &sink.IBaseFilter_iface );
    printf( "then_downstream=%d hr=%#lx middle_out=%d\n", hr == S_OK && middle.pins[1].peer != NULL, hr, middle.pins[1].peer != NULL );
    hr = ICaptureGraphBuilder2_RenderStream( builder, NULL, NULL, (IUnknown *)&source.IBaseFilter_iface, NULL, &sink.IBaseFilter_iface );
    printf( "all_connected=%d hr=%#lx\n", hr == E_FAIL, hr );

    /* a media type nothing has */
    drop_graph();
    filter_init( &source_c, L"source" );
    add_pin( &source_c, L"out1", PINDIR_OUTPUT, &probe_type, NULL );
    filter_init( &sink_c, L"sink" );
    add_pin( &sink_c, L"in1", PINDIR_INPUT, &probe_type, NULL );
    filters[0] = &source_c; filters[1] = &sink_c;
    fresh_graph( filters, 2 );
    hr = ICaptureGraphBuilder2_RenderStream( builder, NULL, &other_type, (IUnknown *)&source_c.IBaseFilter_iface, NULL, &sink_c.IBaseFilter_iface );
    printf( "no_such_type=%d hr=%#lx connected=%d\n", hr == S_OK && !source_c.pins[0].peer, hr, source_c.pins[0].peer != NULL );

    IGraphBuilder_Release( graph );
    ICaptureGraphBuilder2_Release( builder );
    printf( "done=1\n" );
    return 0;
}
