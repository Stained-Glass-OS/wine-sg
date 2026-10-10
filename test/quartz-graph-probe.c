/* quartz's filter graph stubs (patches/sg/2940), run by test/quartz-graph-gate.sh:
 * IMediaSeeking rate / preroll / available, IMediaPosition preroll and CanSeek*,
 * IMediaFilter::GetClassID, IGraphConfig (filter cache, filter flags, start time,
 * Reconnect, PushThroughData, RemoveFilterEx), IVideoFrameStep forwarding and the
 * IMediaControl collections. The graph is driven with small hand written filters.
 *
 *   quartz-graph-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

#ifndef REMFILTERF_LEAVECONNECTED
#define REMFILTERF_LEAVECONNECTED 1
#endif

struct tfilter;
struct tpin
{
    IPin IPin_iface;
    struct tfilter *filter;
    PIN_DIRECTION dir;
    IPin *peer;
    HANDLE eos_event;
};

struct tfilter
{
    IBaseFilter IBaseFilter_iface;
    IMediaSeeking IMediaSeeking_iface;
    IVideoFrameStep IVideoFrameStep_iface;
    LONG ref;
    IFilterGraph *graph;
    struct tpin pins[2];
    int pin_count;
    BOOL seeking, stepping;
    double rate;
    HRESULT rate_hr;
    LONGLONG preroll, avail_early, avail_late;
    DWORD caps;
    int steps_forwarded, can_step_calls, cancels;
    DWORD last_step;
};

static struct tfilter *tf_from_filter(IBaseFilter *iface) { return CONTAINING_RECORD(iface, struct tfilter, IBaseFilter_iface); }
static struct tfilter *tf_from_seek(IMediaSeeking *iface) { return CONTAINING_RECORD(iface, struct tfilter, IMediaSeeking_iface); }
static struct tfilter *tf_from_step(IVideoFrameStep *iface) { return CONTAINING_RECORD(iface, struct tfilter, IVideoFrameStep_iface); }
static struct tpin *tp_from_pin(IPin *iface) { return CONTAINING_RECORD(iface, struct tpin, IPin_iface); }

/* ---- pin enumerator ---- */
struct tenum
{
    IEnumPins IEnumPins_iface;
    LONG ref;
    struct tfilter *filter;
    int pos;
};
static struct tenum *te_from(IEnumPins *iface) { return CONTAINING_RECORD(iface, struct tenum, IEnumPins_iface); }
static HRESULT WINAPI te_QI(IEnumPins *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IEnumPins))
    {
        *out = iface;
        IEnumPins_AddRef(iface);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI te_AddRef(IEnumPins *iface) { return InterlockedIncrement(&te_from(iface)->ref); }
static ULONG WINAPI te_Release(IEnumPins *iface)
{
    struct tenum *e = te_from(iface);
    ULONG r = InterlockedDecrement(&e->ref);
    if (!r) HeapFree(GetProcessHeap(), 0, e);
    return r;
}
static HRESULT WINAPI te_Next(IEnumPins *iface, ULONG count, IPin **pins, ULONG *fetched)
{
    struct tenum *e = te_from(iface);
    ULONG i = 0;
    while (i < count && e->pos < e->filter->pin_count)
    {
        pins[i] = &e->filter->pins[e->pos++].IPin_iface;
        IPin_AddRef(pins[i]);
        i++;
    }
    if (fetched) *fetched = i;
    return i == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI te_Skip(IEnumPins *iface, ULONG count) { te_from(iface)->pos += count; return S_OK; }
static HRESULT WINAPI te_Reset(IEnumPins *iface) { te_from(iface)->pos = 0; return S_OK; }
static HRESULT WINAPI te_Clone(IEnumPins *iface, IEnumPins **out) { return E_NOTIMPL; }
static const IEnumPinsVtbl te_vtbl = { te_QI, te_AddRef, te_Release, te_Next, te_Skip, te_Reset, te_Clone };

/* ---- pins ---- */
static HRESULT WINAPI tp_QI(IPin *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPin))
    {
        *out = iface;
        IPin_AddRef(iface);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI tp_AddRef(IPin *iface) { return IBaseFilter_AddRef(&tp_from_pin(iface)->filter->IBaseFilter_iface); }
static ULONG WINAPI tp_Release(IPin *iface) { return IBaseFilter_Release(&tp_from_pin(iface)->filter->IBaseFilter_iface); }
static HRESULT WINAPI tp_Connect(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct tpin *pin = tp_from_pin(iface);
    HRESULT hr;
    if (pin->peer) return VFW_E_ALREADY_CONNECTED;
    if (FAILED(hr = IPin_ReceiveConnection(peer, iface, mt))) return hr;
    IPin_AddRef(pin->peer = peer);
    return S_OK;
}
static HRESULT WINAPI tp_ReceiveConnection(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct tpin *pin = tp_from_pin(iface);
    if (pin->peer) return VFW_E_ALREADY_CONNECTED;
    IPin_AddRef(pin->peer = peer);
    return S_OK;
}
static HRESULT WINAPI tp_Disconnect(IPin *iface)
{
    struct tpin *pin = tp_from_pin(iface);
    if (!pin->peer) return S_FALSE;
    IPin_Release(pin->peer);
    pin->peer = NULL;
    return S_OK;
}
static HRESULT WINAPI tp_ConnectedTo(IPin *iface, IPin **peer)
{
    struct tpin *pin = tp_from_pin(iface);
    if (!pin->peer) { *peer = NULL; return VFW_E_NOT_CONNECTED; }
    IPin_AddRef(*peer = pin->peer);
    return S_OK;
}
static HRESULT WINAPI tp_ConnectionMediaType(IPin *iface, AM_MEDIA_TYPE *mt) { return VFW_E_NOT_CONNECTED; }
static HRESULT WINAPI tp_QueryPinInfo(IPin *iface, PIN_INFO *info)
{
    struct tpin *pin = tp_from_pin(iface);
    memset(info, 0, sizeof(*info));
    IBaseFilter_AddRef(info->pFilter = &pin->filter->IBaseFilter_iface);
    info->dir = pin->dir;
    wcscpy(info->achName, pin->dir == PINDIR_INPUT ? L"in" : L"out");
    return S_OK;
}
static HRESULT WINAPI tp_QueryDirection(IPin *iface, PIN_DIRECTION *dir) { *dir = tp_from_pin(iface)->dir; return S_OK; }
static HRESULT WINAPI tp_QueryId(IPin *iface, WCHAR **id)
{
    *id = CoTaskMemAlloc(8);
    wcscpy(*id, L"pin");
    return S_OK;
}
static HRESULT WINAPI tp_QueryAccept(IPin *iface, const AM_MEDIA_TYPE *mt) { return S_OK; }
static HRESULT WINAPI tp_EnumMediaTypes(IPin *iface, IEnumMediaTypes **e) { return E_NOTIMPL; }
static HRESULT WINAPI tp_QueryInternalConnections(IPin *iface, IPin **pins, ULONG *count) { return E_NOTIMPL; }
static HRESULT WINAPI tp_EndOfStream(IPin *iface) { return S_OK; }
static HRESULT WINAPI tp_BeginFlush(IPin *iface) { return S_OK; }
static HRESULT WINAPI tp_EndFlush(IPin *iface) { return S_OK; }
static HRESULT WINAPI tp_NewSegment(IPin *iface, REFERENCE_TIME a, REFERENCE_TIME b, double r) { return S_OK; }
static const IPinVtbl tp_vtbl =
{
    tp_QI, tp_AddRef, tp_Release, tp_Connect, tp_ReceiveConnection, tp_Disconnect, tp_ConnectedTo,
    tp_ConnectionMediaType, tp_QueryPinInfo, tp_QueryDirection, tp_QueryId, tp_QueryAccept,
    tp_EnumMediaTypes, tp_QueryInternalConnections, tp_EndOfStream, tp_BeginFlush, tp_EndFlush, tp_NewSegment
};

/* ---- IPinConnection on the sink of the PushThroughData test ---- */
static IPinConnectionVtbl pc_vtbl;
struct tconn { IPinConnection IPinConnection_iface; HANDLE event; HRESULT hr; };
static HRESULT WINAPI pc_QI(IPinConnection *iface, REFIID iid, void **out) { *out = NULL; return E_NOINTERFACE; }
static ULONG WINAPI pc_AddRef(IPinConnection *iface) { return 2; }
static ULONG WINAPI pc_Release(IPinConnection *iface) { return 1; }
static HRESULT WINAPI pc_DynamicQueryAccept(IPinConnection *iface, const AM_MEDIA_TYPE *mt) { return S_OK; }
static HRESULT WINAPI pc_NotifyEndOfStream(IPinConnection *iface, HANDLE event)
{
    struct tconn *c = CONTAINING_RECORD(iface, struct tconn, IPinConnection_iface);
    c->event = event;
    return c->hr;
}
static HRESULT WINAPI pc_IsEndPin(IPinConnection *iface) { return S_OK; }
static HRESULT WINAPI pc_DynamicDisconnect(IPinConnection *iface) { return S_OK; }

/* ---- IMediaSeeking ---- */
static HRESULT WINAPI ts_QI(IMediaSeeking *iface, REFIID iid, void **out) { return IBaseFilter_QueryInterface(&tf_from_seek(iface)->IBaseFilter_iface, iid, out); }
static ULONG WINAPI ts_AddRef(IMediaSeeking *iface) { return IBaseFilter_AddRef(&tf_from_seek(iface)->IBaseFilter_iface); }
static ULONG WINAPI ts_Release(IMediaSeeking *iface) { return IBaseFilter_Release(&tf_from_seek(iface)->IBaseFilter_iface); }
static HRESULT WINAPI ts_GetCapabilities(IMediaSeeking *iface, DWORD *caps) { *caps = tf_from_seek(iface)->caps; return S_OK; }
static HRESULT WINAPI ts_CheckCapabilities(IMediaSeeking *iface, DWORD *caps) { return S_OK; }
static HRESULT WINAPI ts_IsFormatSupported(IMediaSeeking *iface, const GUID *f) { return IsEqualGUID(f, &TIME_FORMAT_MEDIA_TIME) ? S_OK : S_FALSE; }
static HRESULT WINAPI ts_QueryPreferredFormat(IMediaSeeking *iface, GUID *f) { *f = TIME_FORMAT_MEDIA_TIME; return S_OK; }
static HRESULT WINAPI ts_GetTimeFormat(IMediaSeeking *iface, GUID *f) { *f = TIME_FORMAT_MEDIA_TIME; return S_OK; }
static HRESULT WINAPI ts_IsUsingTimeFormat(IMediaSeeking *iface, const GUID *f) { return S_OK; }
static HRESULT WINAPI ts_SetTimeFormat(IMediaSeeking *iface, const GUID *f) { return S_OK; }
static HRESULT WINAPI ts_GetDuration(IMediaSeeking *iface, LONGLONG *d) { *d = 100; return S_OK; }
static HRESULT WINAPI ts_GetStopPosition(IMediaSeeking *iface, LONGLONG *d) { *d = 100; return S_OK; }
static HRESULT WINAPI ts_GetCurrentPosition(IMediaSeeking *iface, LONGLONG *d) { *d = 0; return S_OK; }
static HRESULT WINAPI ts_ConvertTimeFormat(IMediaSeeking *iface, LONGLONG *t, const GUID *tf, LONGLONG s, const GUID *sf) { *t = s; return S_OK; }
static HRESULT WINAPI ts_SetPositions(IMediaSeeking *iface, LONGLONG *c, DWORD cf, LONGLONG *s, DWORD sf) { return S_OK; }
static HRESULT WINAPI ts_GetPositions(IMediaSeeking *iface, LONGLONG *c, LONGLONG *s) { return S_OK; }
static HRESULT WINAPI ts_GetAvailable(IMediaSeeking *iface, LONGLONG *e, LONGLONG *l)
{
    struct tfilter *f = tf_from_seek(iface);
    *e = f->avail_early;
    *l = f->avail_late;
    return f->avail_late ? S_OK : E_NOTIMPL;
}
static HRESULT WINAPI ts_SetRate(IMediaSeeking *iface, double rate)
{
    struct tfilter *f = tf_from_seek(iface);
    if (SUCCEEDED(f->rate_hr)) f->rate = rate;
    return f->rate_hr;
}
static HRESULT WINAPI ts_GetRate(IMediaSeeking *iface, double *rate) { *rate = tf_from_seek(iface)->rate; return S_OK; }
static HRESULT WINAPI ts_GetPreroll(IMediaSeeking *iface, LONGLONG *p)
{
    struct tfilter *f = tf_from_seek(iface);
    *p = f->preroll;
    return f->preroll ? S_OK : E_NOTIMPL;
}
static const IMediaSeekingVtbl ts_vtbl =
{
    ts_QI, ts_AddRef, ts_Release, ts_GetCapabilities, ts_CheckCapabilities, ts_IsFormatSupported,
    ts_QueryPreferredFormat, ts_GetTimeFormat, ts_IsUsingTimeFormat, ts_SetTimeFormat, ts_GetDuration,
    ts_GetStopPosition, ts_GetCurrentPosition, ts_ConvertTimeFormat, ts_SetPositions, ts_GetPositions,
    ts_GetAvailable, ts_SetRate, ts_GetRate, ts_GetPreroll
};

/* ---- IVideoFrameStep ---- */
static HRESULT WINAPI tv_QI(IVideoFrameStep *iface, REFIID iid, void **out) { return IBaseFilter_QueryInterface(&tf_from_step(iface)->IBaseFilter_iface, iid, out); }
static ULONG WINAPI tv_AddRef(IVideoFrameStep *iface) { return IBaseFilter_AddRef(&tf_from_step(iface)->IBaseFilter_iface); }
static ULONG WINAPI tv_Release(IVideoFrameStep *iface) { return IBaseFilter_Release(&tf_from_step(iface)->IBaseFilter_iface); }
static HRESULT WINAPI tv_Step(IVideoFrameStep *iface, DWORD n, IUnknown *o)
{
    struct tfilter *f = tf_from_step(iface);
    f->steps_forwarded++;
    f->last_step = n;
    return 0x1234;
}
static HRESULT WINAPI tv_CanStep(IVideoFrameStep *iface, LONG multiple, IUnknown *o)
{
    tf_from_step(iface)->can_step_calls++;
    return multiple ? S_FALSE : S_OK;
}
static HRESULT WINAPI tv_CancelStep(IVideoFrameStep *iface) { tf_from_step(iface)->cancels++; return S_OK; }
static const IVideoFrameStepVtbl tv_vtbl = { tv_QI, tv_AddRef, tv_Release, tv_Step, tv_CanStep, tv_CancelStep };

/* ---- the filter ---- */
static HRESULT WINAPI tf_QI(IBaseFilter *iface, REFIID iid, void **out)
{
    struct tfilter *f = tf_from_filter(iface);
    *out = NULL;
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPersist) || IsEqualGUID(iid, &IID_IMediaFilter)
            || IsEqualGUID(iid, &IID_IBaseFilter))
        *out = &f->IBaseFilter_iface;
    else if (f->seeking && IsEqualGUID(iid, &IID_IMediaSeeking))
        *out = &f->IMediaSeeking_iface;
    else if (f->stepping && IsEqualGUID(iid, &IID_IVideoFrameStep))
        *out = &f->IVideoFrameStep_iface;
    if (!*out) return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}
static ULONG WINAPI tf_AddRef(IBaseFilter *iface) { return InterlockedIncrement(&tf_from_filter(iface)->ref); }
static ULONG WINAPI tf_Release(IBaseFilter *iface) { return InterlockedDecrement(&tf_from_filter(iface)->ref); }
static HRESULT WINAPI tf_GetClassID(IBaseFilter *iface, CLSID *clsid) { return E_NOTIMPL; }
static HRESULT WINAPI tf_Stop(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI tf_Pause(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI tf_Run(IBaseFilter *iface, REFERENCE_TIME t) { return S_OK; }
static HRESULT WINAPI tf_GetState(IBaseFilter *iface, DWORD ms, FILTER_STATE *s) { *s = State_Stopped; return S_OK; }
static HRESULT WINAPI tf_SetSyncSource(IBaseFilter *iface, IReferenceClock *c) { return S_OK; }
static HRESULT WINAPI tf_GetSyncSource(IBaseFilter *iface, IReferenceClock **c) { *c = NULL; return S_OK; }
static HRESULT WINAPI tf_EnumPins(IBaseFilter *iface, IEnumPins **out)
{
    struct tenum *e = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*e));
    e->IEnumPins_iface.lpVtbl = &te_vtbl;
    e->ref = 1;
    e->filter = tf_from_filter(iface);
    *out = &e->IEnumPins_iface;
    return S_OK;
}
static HRESULT WINAPI tf_FindPin(IBaseFilter *iface, const WCHAR *id, IPin **pin) { return VFW_E_NOT_FOUND; }
static HRESULT WINAPI tf_QueryFilterInfo(IBaseFilter *iface, FILTER_INFO *info)
{
    struct tfilter *f = tf_from_filter(iface);
    memset(info, 0, sizeof(*info));
    wcscpy(info->achName, L"tfilter");
    if ((info->pGraph = f->graph)) IFilterGraph_AddRef(info->pGraph);
    return S_OK;
}
static HRESULT WINAPI tf_JoinFilterGraph(IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name)
{
    tf_from_filter(iface)->graph = graph;
    return S_OK;
}
static HRESULT WINAPI tf_QueryVendorInfo(IBaseFilter *iface, WCHAR **v) { return E_NOTIMPL; }
static const IBaseFilterVtbl tf_vtbl =
{
    tf_QI, tf_AddRef, tf_Release, tf_GetClassID, tf_Stop, tf_Pause, tf_Run, tf_GetState,
    tf_SetSyncSource, tf_GetSyncSource, tf_EnumPins, tf_FindPin, tf_QueryFilterInfo,
    tf_JoinFilterGraph, tf_QueryVendorInfo
};

static void tf_init(struct tfilter *f, int in, int out)
{
    int i = 0;
    memset(f, 0, sizeof(*f));
    f->IBaseFilter_iface.lpVtbl = &tf_vtbl;
    f->IMediaSeeking_iface.lpVtbl = &ts_vtbl;
    f->IVideoFrameStep_iface.lpVtbl = &tv_vtbl;
    f->ref = 1;
    f->rate = 1.0;
    f->rate_hr = S_OK;
    if (in) { f->pins[i].IPin_iface.lpVtbl = &tp_vtbl; f->pins[i].filter = f; f->pins[i++].dir = PINDIR_INPUT; }
    if (out) { f->pins[i].IPin_iface.lpVtbl = &tp_vtbl; f->pins[i].filter = f; f->pins[i++].dir = PINDIR_OUTPUT; }
    f->pin_count = i;
}

static IGraphBuilder *new_graph(const CLSID *clsid)
{
    IGraphBuilder *graph = NULL;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    CHECKF(hr == S_OK, "graph created (%#lx)", hr);
    return graph;
}

static void test_seeking(void)
{
    struct tfilter f1, f2;
    IGraphBuilder *graph = new_graph(&CLSID_FilterGraph);
    IMediaSeeking *seeking;
    IMediaPosition *position;
    LONGLONG earliest, latest, preroll;
    double rate, t;
    LONG can;
    HRESULT hr;
    GUID format;
    CLSID clsid;
    IMediaFilter *mf;

    IGraphBuilder_QueryInterface(graph, &IID_IMediaSeeking, (void **)&seeking);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaPosition, (void **)&position);

    hr = IMediaSeeking_GetAvailable(seeking, &earliest, &latest);
    CHECKF(hr == E_NOTIMPL, "GetAvailable without filters is E_NOTIMPL (%#lx)", hr);
    hr = IMediaSeeking_SetRate(seeking, 2.0);
    CHECKF(hr == E_NOTIMPL, "SetRate(2) without filters is E_NOTIMPL (%#lx)", hr);
    hr = IMediaSeeking_SetRate(seeking, 1.0);
    CHECKF(hr == S_OK, "SetRate(1) without filters is S_OK (%#lx)", hr);
    rate = 0;
    hr = IMediaSeeking_GetRate(seeking, &rate);
    CHECKF(hr == S_OK && rate == 1.0, "GetRate is 1 (%#lx, %g)", hr, rate);
    hr = IMediaSeeking_GetRate(seeking, NULL);
    CHECKF(hr == E_POINTER, "GetRate(NULL) is E_POINTER (%#lx)", hr);
    hr = IMediaSeeking_GetPreroll(seeking, &preroll);
    CHECKF(hr == E_NOTIMPL, "GetPreroll without filters is E_NOTIMPL (%#lx)", hr);
    hr = IMediaSeeking_QueryPreferredFormat(seeking, &format);
    CHECKF(hr == S_OK && IsEqualGUID(&format, &TIME_FORMAT_MEDIA_TIME), "preferred format is media time (%#lx)", hr);
    hr = IMediaPosition_get_PrerollTime(position, &t);
    CHECKF(hr == E_NOTIMPL, "get_PrerollTime without filters is E_NOTIMPL (%#lx)", hr);
    hr = IMediaPosition_put_PrerollTime(position, 1.0);
    CHECKF(hr == E_NOTIMPL, "put_PrerollTime is E_NOTIMPL (%#lx)", hr);

    tf_init(&f1, 1, 0);
    tf_init(&f2, 1, 0);
    f1.seeking = f2.seeking = TRUE;
    f1.preroll = 0x1234; f2.preroll = 0x2345;
    f1.avail_early = 5; f1.avail_late = 9;
    f2.avail_early = 3; f2.avail_late = 8;
    f1.caps = AM_SEEKING_CanSeekForwards | AM_SEEKING_CanSeekAbsolute;
    f2.caps = AM_SEEKING_CanSeekForwards | AM_SEEKING_CanSeekBackwards;
    IGraphBuilder_AddFilter(graph, &f1.IBaseFilter_iface, L"f1");
    IGraphBuilder_AddFilter(graph, &f2.IBaseFilter_iface, L"f2");

    hr = IMediaSeeking_SetRate(seeking, 2.5);
    CHECKF(hr == S_OK, "SetRate(2.5) with filters is S_OK (%#lx)", hr);
    CHECKF(f1.rate == 2.5 && f2.rate == 2.5, "SetRate reached both filters (%g, %g)", f1.rate, f2.rate);
    hr = IMediaSeeking_GetRate(seeking, &rate);
    CHECKF(hr == S_OK && rate == 2.5, "GetRate returns the rate set (%#lx, %g)", hr, rate);
    f2.rate_hr = E_FAIL;
    hr = IMediaSeeking_SetRate(seeking, 3.0);
    CHECKF(hr == E_FAIL, "SetRate returns a filter's failure (%#lx)", hr);
    IMediaSeeking_GetRate(seeking, &rate);
    CHECKF(rate == 2.5, "a failed SetRate keeps the rate (%g)", rate);
    f2.rate_hr = E_NOTIMPL;
    hr = IMediaSeeking_SetRate(seeking, 4.0);
    CHECKF(hr == S_OK && f1.rate == 4.0, "E_NOTIMPL from one filter is skipped (%#lx)", hr);
    f1.rate_hr = E_NOTIMPL;
    hr = IMediaSeeking_SetRate(seeking, 5.0);
    CHECKF(hr == E_NOTIMPL, "all filters E_NOTIMPL gives E_NOTIMPL (%#lx)", hr);
    f1.rate_hr = f2.rate_hr = S_OK;
    IMediaSeeking_SetRate(seeking, -1.0);
    IMediaSeeking_GetRate(seeking, &rate);
    CHECKF(rate == -1.0, "negative rate is kept (%g)", rate);
    IMediaSeeking_SetRate(seeking, 1.0);

    preroll = 0;
    hr = IMediaSeeking_GetPreroll(seeking, &preroll);
    CHECKF(hr == S_OK && preroll == 0x2345, "GetPreroll is the largest preroll (%#lx, %#I64x)", hr, preroll);
    hr = IMediaSeeking_GetPreroll(seeking, NULL);
    CHECKF(hr == E_POINTER, "GetPreroll(NULL) is E_POINTER (%#lx)", hr);
    t = 0;
    hr = IMediaPosition_get_PrerollTime(position, &t);
    CHECKF(hr == S_OK && t == (double)0x2345 / 10000000, "get_PrerollTime in seconds (%#lx, %g)", hr, t);
    hr = IMediaPosition_get_PrerollTime(position, NULL);
    CHECKF(hr == E_POINTER, "get_PrerollTime(NULL) is E_POINTER (%#lx)", hr);

    earliest = latest = -1;
    hr = IMediaSeeking_GetAvailable(seeking, &earliest, &latest);
    CHECKF(hr == S_OK && earliest == 5 && latest == 8, "GetAvailable is the common range (%#lx, %I64d, %I64d)", hr, earliest, latest);

    can = 0x55;
    hr = IMediaPosition_CanSeekForward(position, &can);
    CHECKF(hr == S_OK && can == OATRUE, "CanSeekForward true (%#lx, %ld)", hr, can);
    can = 0x55;
    hr = IMediaPosition_CanSeekBackward(position, &can);
    CHECKF(hr == S_OK && can == OAFALSE, "CanSeekBackward false when one filter cannot (%#lx, %ld)", hr, can);
    hr = IMediaPosition_CanSeekForward(position, NULL);
    CHECKF(hr == E_POINTER, "CanSeekForward(NULL) is E_POINTER (%#lx)", hr);
    f1.caps |= AM_SEEKING_CanSeekBackwards;
    IGraphBuilder_RemoveFilter(graph, &f1.IBaseFilter_iface);
    IGraphBuilder_AddFilter(graph, &f1.IBaseFilter_iface, L"f1");
    can = 0x55;
    hr = IMediaPosition_CanSeekBackward(position, &can);
    CHECKF(hr == S_OK && can == OATRUE, "CanSeekBackward true when every filter can (%#lx, %ld)", hr, can);

    hr = IGraphBuilder_QueryInterface(graph, &IID_IMediaFilter, (void **)&mf);
    memset(&clsid, 0, sizeof(clsid));
    hr = IMediaFilter_GetClassID(mf, &clsid);
    CHECKF(hr == S_OK && IsEqualGUID(&clsid, &CLSID_FilterGraph), "GetClassID of the threaded graph (%#lx, %s)", hr, "");
    hr = IMediaFilter_GetClassID(mf, NULL);
    CHECKF(hr == E_POINTER, "GetClassID(NULL) is E_POINTER (%#lx)", hr);
    IMediaFilter_Release(mf);

    IMediaSeeking_Release(seeking);
    IMediaPosition_Release(position);
    IGraphBuilder_Release(graph);

    graph = new_graph(&CLSID_FilterGraphNoThread);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaFilter, (void **)&mf);
    memset(&clsid, 0, sizeof(clsid));
    hr = IMediaFilter_GetClassID(mf, &clsid);
    CHECKF(hr == S_OK && IsEqualGUID(&clsid, &CLSID_FilterGraphNoThread), "GetClassID of the no-thread graph (%#lx)", hr);
    IMediaFilter_Release(mf);
    IGraphBuilder_Release(graph);
}

static int count_enum(IEnumFilters *en, IBaseFilter **first)
{
    IBaseFilter *f;
    int n = 0;
    if (first) *first = NULL;
    while (IEnumFilters_Next(en, 1, &f, NULL) == S_OK)
    {
        if (!n && first) *first = f;
        n++;
        if (!first || n > 1) IBaseFilter_Release(f);
    }
    return n;
}

static void test_graph_config(void)
{
    struct tfilter src, mid, dst, other;
    IGraphBuilder *graph = new_graph(&CLSID_FilterGraph);
    IGraphConfig *config;
    IFilterGraph2 *graph2;
    IEnumFilters *en, *en2;
    IBaseFilter *first;
    REFERENCE_TIME start;
    HANDLE event = CreateEventW(NULL, TRUE, FALSE, NULL);
    struct tconn conn;
    DWORD flags;
    HRESULT hr;
    IPin *peer;

    IGraphBuilder_QueryInterface(graph, &IID_IGraphConfig, (void **)&config);
    IGraphBuilder_QueryInterface(graph, &IID_IFilterGraph2, (void **)&graph2);
    tf_init(&src, 0, 1);
    tf_init(&mid, 1, 1);
    tf_init(&dst, 1, 0);
    tf_init(&other, 1, 0);

    /* filter cache */
    hr = IGraphConfig_EnumCacheFilter(config, &en);
    CHECKF(hr == S_OK && count_enum(en, NULL) == 0, "empty cache enumerates nothing (%#lx)", hr);
    IEnumFilters_Release(en);
    hr = IGraphConfig_EnumCacheFilter(config, NULL);
    CHECKF(hr == E_POINTER, "EnumCacheFilter(NULL) is E_POINTER (%#lx)", hr);
    hr = IGraphConfig_AddFilterToCache(config, NULL);
    CHECKF(hr == E_POINTER, "AddFilterToCache(NULL) is E_POINTER (%#lx)", hr);
    hr = IGraphConfig_AddFilterToCache(config, &mid.IBaseFilter_iface);
    CHECKF(hr == S_OK, "AddFilterToCache (%#lx)", hr);
    CHECKF(mid.ref == 2, "the cache holds a reference (%ld)", mid.ref);
    hr = IGraphConfig_AddFilterToCache(config, &mid.IBaseFilter_iface);
    CHECKF(hr == S_OK && mid.ref == 2, "caching twice keeps one entry (%#lx, %ld)", hr, mid.ref);
    IGraphConfig_AddFilterToCache(config, &other.IBaseFilter_iface);
    hr = IGraphConfig_EnumCacheFilter(config, &en);
    CHECKF(hr == S_OK && count_enum(en, NULL) == 2, "two cached filters enumerate (%#lx)", hr);
    IEnumFilters_Reset(en);
    count_enum(en, &first);
    CHECKF(first == &mid.IBaseFilter_iface, "the first cached filter is first");
    if (first) IBaseFilter_Release(first);
    IEnumFilters_Clone(en, &en2);
    IEnumFilters_Release(en2);
    hr = IGraphConfig_RemoveFilterFromCache(config, &other.IBaseFilter_iface);
    CHECKF(hr == S_OK && other.ref == 1, "RemoveFilterFromCache releases it (%#lx, %ld)", hr, other.ref);
    hr = IEnumFilters_Next(en, 1, &first, NULL);
    CHECKF(hr == VFW_E_ENUM_OUT_OF_SYNC, "an enumerator is out of sync after a change (%#lx)", hr);
    IEnumFilters_Release(en);
    hr = IGraphConfig_RemoveFilterFromCache(config, &other.IBaseFilter_iface);
    CHECKF(hr == VFW_E_NOT_FOUND, "removing an uncached filter is VFW_E_NOT_FOUND (%#lx)", hr);

    /* flags and start time */
    hr = IGraphConfig_GetFilterFlags(config, &mid.IBaseFilter_iface, &flags);
    CHECKF(hr == VFW_E_NOT_FOUND, "GetFilterFlags of a filter not in the graph (%#lx)", hr);
    IGraphBuilder_AddFilter(graph, &src.IBaseFilter_iface, L"src");
    IGraphBuilder_AddFilter(graph, &mid.IBaseFilter_iface, L"mid");
    IGraphBuilder_AddFilter(graph, &dst.IBaseFilter_iface, L"dst");
    flags = 0xdead;
    hr = IGraphConfig_GetFilterFlags(config, &mid.IBaseFilter_iface, &flags);
    CHECKF(hr == S_OK && flags == 0, "flags default to 0 (%#lx, %#lx)", hr, flags);
    hr = IGraphConfig_SetFilterFlags(config, &mid.IBaseFilter_iface, AM_FILTER_FLAGS_REMOVABLE);
    CHECKF(hr == S_OK, "SetFilterFlags(REMOVABLE) (%#lx)", hr);
    flags = 0;
    hr = IGraphConfig_GetFilterFlags(config, &mid.IBaseFilter_iface, &flags);
    CHECKF(hr == S_OK && flags == AM_FILTER_FLAGS_REMOVABLE, "flags read back (%#lx, %#lx)", hr, flags);
    flags = 0xdead;
    IGraphConfig_GetFilterFlags(config, &src.IBaseFilter_iface, &flags);
    CHECKF(flags == 0, "flags are per filter (%#lx)", flags);
    hr = IGraphConfig_SetFilterFlags(config, &mid.IBaseFilter_iface, 0x10);
    CHECKF(hr == E_INVALIDARG, "SetFilterFlags with an unknown flag is E_INVALIDARG (%#lx)", hr);
    hr = IGraphConfig_SetFilterFlags(config, &other.IBaseFilter_iface, 0);
    CHECKF(hr == VFW_E_NOT_FOUND, "SetFilterFlags of a filter not in the graph (%#lx)", hr);
    hr = IGraphConfig_GetFilterFlags(config, &mid.IBaseFilter_iface, NULL);
    CHECKF(hr == E_POINTER, "GetFilterFlags(NULL) is E_POINTER (%#lx)", hr);
    hr = IGraphConfig_GetStartTime(config, NULL);
    CHECKF(hr == E_POINTER, "GetStartTime(NULL) is E_POINTER (%#lx)", hr);
    start = 0x1234;
    hr = IGraphConfig_GetStartTime(config, &start);
    CHECKF(hr == S_OK && start == 0, "start time of a never run graph is 0 (%#lx, %I64d)", hr, start);

    /* Reconnect */
    hr = IGraphConfig_Reconnect(config, NULL, NULL, NULL, NULL, NULL, 0);
    CHECKF(hr == E_INVALIDARG, "Reconnect without pins is E_INVALIDARG (%#lx)", hr);
    hr = IGraphConfig_Reconnect(config, &src.pins[0].IPin_iface, &dst.pins[0].IPin_iface, NULL, NULL, NULL, 0x80);
    CHECKF(hr == E_INVALIDARG, "Reconnect with unknown flags is E_INVALIDARG (%#lx)", hr);
    hr = IGraphConfig_Reconnect(config, &src.pins[0].IPin_iface, NULL, NULL, NULL, NULL, 0);
    CHECKF(hr == VFW_E_NOT_CONNECTED, "Reconnect of an unconnected pin is VFW_E_NOT_CONNECTED (%#lx)", hr);

    hr = IGraphBuilder_ConnectDirect(graph, &src.pins[0].IPin_iface, &mid.pins[0].IPin_iface, NULL);
    CHECKF(hr == S_OK, "ConnectDirect src to mid (%#lx)", hr);
    hr = IGraphBuilder_ConnectDirect(graph, &mid.pins[1].IPin_iface, &dst.pins[0].IPin_iface, NULL);
    CHECKF(hr == S_OK, "ConnectDirect mid to dst (%#lx)", hr);

    SetEvent(event);
    hr = IGraphConfig_Reconnect(config, &mid.pins[1].IPin_iface, &dst.pins[0].IPin_iface, NULL, NULL, event, 0);
    CHECKF(hr == E_ABORT, "Reconnect with a signalled abort event is E_ABORT (%#lx)", hr);
    CHECKF(mid.pins[1].peer == &dst.pins[0].IPin_iface, "an aborted Reconnect leaves the connection");
    ResetEvent(event);

    hr = IGraphConfig_Reconnect(config, &mid.pins[1].IPin_iface, NULL, NULL, NULL, event, AM_GRAPH_CONFIG_RECONNECT_DIRECTCONNECT);
    CHECKF(hr == S_OK && mid.pins[1].peer == &dst.pins[0].IPin_iface && dst.pins[0].peer == &mid.pins[1].IPin_iface,
            "Reconnect of the output pin alone reconnects it to its peer (%#lx)", hr);
    hr = IGraphConfig_Reconnect(config, NULL, &dst.pins[0].IPin_iface, NULL, NULL, event, 0);
    CHECKF(hr == S_OK && mid.pins[1].peer == &dst.pins[0].IPin_iface,
            "Reconnect of the input pin alone reconnects it to its peer (%#lx)", hr);
    /* put a different sink in */
    IGraphBuilder_AddFilter(graph, &other.IBaseFilter_iface, L"other");
    hr = IGraphConfig_Reconnect(config, &mid.pins[1].IPin_iface, &other.pins[0].IPin_iface, NULL, NULL, NULL, AM_GRAPH_CONFIG_RECONNECT_DIRECTCONNECT);
    CHECKF(hr == S_OK && mid.pins[1].peer == &other.pins[0].IPin_iface && !dst.pins[0].peer,
            "Reconnect moves the connection to the new sink (%#lx)", hr);
    hr = IGraphConfig_Reconnect(config, &mid.pins[1].IPin_iface, &dst.pins[0].IPin_iface, NULL, NULL, NULL, AM_GRAPH_CONFIG_RECONNECT_DIRECTCONNECT);
    CHECKF(hr == S_OK && mid.pins[1].peer == &dst.pins[0].IPin_iface, "Reconnect back to dst (%#lx)", hr);

    /* Reconnect through a filter */
    {
        struct tfilter via;
        tf_init(&via, 1, 1);
        hr = IGraphConfig_Reconnect(config, &mid.pins[1].IPin_iface, &dst.pins[0].IPin_iface, NULL, &via.IBaseFilter_iface, NULL, 0);
        CHECKF(hr == S_OK && mid.pins[1].peer == &via.pins[0].IPin_iface && via.pins[1].peer == &dst.pins[0].IPin_iface,
                "Reconnect through a filter chains it between the pins (%#lx)", hr);
        hr = IGraphConfig_Reconnect(config, &mid.pins[1].IPin_iface, &dst.pins[0].IPin_iface, NULL, &via.IBaseFilter_iface, NULL, AM_GRAPH_CONFIG_RECONNECT_DIRECTCONNECT);
        CHECKF(hr == E_INVALIDARG, "DIRECTCONNECT with a filter is E_INVALIDARG (%#lx)", hr);

        /* RemoveFilterEx */
        hr = IGraphConfig_RemoveFilterEx(config, &via.IBaseFilter_iface, 0x10);
        CHECKF(hr == E_INVALIDARG, "RemoveFilterEx with an unknown flag is E_INVALIDARG (%#lx)", hr);
        hr = IGraphConfig_RemoveFilterEx(config, &via.IBaseFilter_iface, REMFILTERF_LEAVECONNECTED);
        CHECKF(hr == S_OK && !via.graph && mid.pins[1].peer == &dst.pins[0].IPin_iface
                && dst.pins[0].peer == &mid.pins[1].IPin_iface,
                "RemoveFilterEx(LEAVECONNECTED) joins the neighbours (%#lx)", hr);
    }
    hr = IGraphConfig_RemoveFilterEx(config, &mid.IBaseFilter_iface, 0);
    CHECKF(hr == S_OK && !mid.graph && !src.pins[0].peer && !dst.pins[0].peer,
            "RemoveFilterEx(0) removes and disconnects (%#lx)", hr);
    hr = IGraphConfig_RemoveFilterEx(config, &mid.IBaseFilter_iface, 0);
    CHECKF(hr == E_FAIL, "RemoveFilterEx of a filter not in the graph fails (%#lx)", hr);

    /* PushThroughData */
    conn.IPinConnection_iface.lpVtbl = &pc_vtbl;
    conn.event = NULL;
    conn.hr = S_OK;
    hr = IGraphConfig_PushThroughData(config, NULL, &conn.IPinConnection_iface, event);
    CHECKF(hr == E_POINTER, "PushThroughData without an output pin is E_POINTER (%#lx)", hr);
    hr = IGraphConfig_PushThroughData(config, &src.pins[0].IPin_iface, NULL, event);
    CHECKF(hr == E_POINTER, "PushThroughData without a connection is E_POINTER (%#lx)", hr);
    hr = IGraphConfig_PushThroughData(config, &dst.pins[0].IPin_iface, &conn.IPinConnection_iface, event);
    CHECKF(hr == E_INVALIDARG, "PushThroughData on an input pin is E_INVALIDARG (%#lx)", hr);
    hr = IGraphConfig_PushThroughData(config, &src.pins[0].IPin_iface, &conn.IPinConnection_iface, event);
    CHECKF(hr == S_OK && conn.event == event, "PushThroughData asks the sink to notify (%#lx)", hr);
    conn.hr = 0x1235;
    hr = IGraphConfig_PushThroughData(config, &src.pins[0].IPin_iface, &conn.IPinConnection_iface, event);
    CHECKF(hr == 0x1235, "PushThroughData returns the sink's result (%#lx)", hr);
    SetEvent(event);
    hr = IGraphConfig_PushThroughData(config, &src.pins[0].IPin_iface, &conn.IPinConnection_iface, event);
    CHECKF(hr == E_ABORT, "PushThroughData with a signalled abort event is E_ABORT (%#lx)", hr);

    (void)peer;
    CloseHandle(event);
    IFilterGraph2_Release(graph2);
    IGraphConfig_Release(config);
    IGraphBuilder_Release(graph);
    CHECKF(mid.ref == 1, "the graph released the cached filter (%ld)", mid.ref);
}

static void test_frame_step(void)
{
    struct tfilter f1, f2;
    IGraphBuilder *graph = new_graph(&CLSID_FilterGraph);
    IVideoFrameStep *step;
    HRESULT hr;

    hr = IGraphBuilder_QueryInterface(graph, &IID_IVideoFrameStep, (void **)&step);
    CHECKF(hr == S_OK, "graph has IVideoFrameStep (%#lx)", hr);

    hr = IVideoFrameStep_CanStep(step, 0, NULL);
    CHECKF(hr == S_FALSE, "CanStep without a stepping filter is S_FALSE (%#lx)", hr);
    hr = IVideoFrameStep_Step(step, 1, NULL);
    CHECKF(hr == E_NOTIMPL, "Step without a stepping filter is E_NOTIMPL (%#lx)", hr);
    hr = IVideoFrameStep_CancelStep(step);
    CHECKF(hr == S_OK, "CancelStep with nothing to cancel is S_OK (%#lx)", hr);

    tf_init(&f1, 1, 0);
    tf_init(&f2, 1, 0);
    f2.stepping = TRUE;
    IGraphBuilder_AddFilter(graph, &f1.IBaseFilter_iface, L"f1");
    IGraphBuilder_AddFilter(graph, &f2.IBaseFilter_iface, L"f2");

    hr = IVideoFrameStep_Step(step, 7, NULL);
    CHECKF(hr == 0x1234 && f2.steps_forwarded == 1 && f2.last_step == 7, "Step is forwarded with the count (%#lx, %lu)", hr, f2.last_step);
    hr = IVideoFrameStep_CanStep(step, 0, NULL);
    CHECKF(hr == S_OK && f2.can_step_calls == 1, "CanStep is forwarded (%#lx)", hr);
    hr = IVideoFrameStep_CanStep(step, 1, NULL);
    CHECKF(hr == S_FALSE, "CanStep(multiple) returns the filter's answer (%#lx)", hr);
    hr = IVideoFrameStep_CancelStep(step);
    CHECKF(hr == S_OK && f2.cancels == 1, "CancelStep is forwarded (%#lx)", hr);
    hr = IVideoFrameStep_Step(step, 2, (IUnknown *)&f2.IBaseFilter_iface);
    CHECKF(hr == 0x1234 && f2.steps_forwarded == 2, "Step with an explicit filter (%#lx)", hr);
    hr = IVideoFrameStep_Step(step, 2, (IUnknown *)&f2.pins[0].IPin_iface);
    CHECKF(hr == 0x1234 && f2.steps_forwarded == 3, "Step with an explicit pin (%#lx)", hr);
    hr = IVideoFrameStep_Step(step, 2, (IUnknown *)&f1.IBaseFilter_iface);
    CHECKF(hr == E_NOTIMPL && f2.steps_forwarded == 3, "Step with a filter that cannot step (%#lx)", hr);

    IVideoFrameStep_Release(step);
    IGraphBuilder_Release(graph);
}

static void test_control_collections(void)
{
    IGraphBuilder *graph = new_graph(&CLSID_FilterGraph);
    IMediaControl *control;
    IDispatch *disp;
    HRESULT hr;
    BSTR name = SysAllocString(L"nothing.avi");

    IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    disp = (IDispatch *)0xdeadbeef;
    hr = IMediaControl_get_FilterCollection(control, &disp);
    CHECKF(hr == E_NOTIMPL && !disp, "FilterCollection: no objects, no fake success (%#lx, %p)", hr, disp);
    disp = (IDispatch *)0xdeadbeef;
    hr = IMediaControl_get_RegFilterCollection(control, &disp);
    CHECKF(hr == E_NOTIMPL && !disp, "RegFilterCollection: no objects, no fake success (%#lx, %p)", hr, disp);
    disp = (IDispatch *)0xdeadbeef;
    hr = IMediaControl_AddSourceFilter(control, name, &disp);
    CHECKF(hr == E_NOTIMPL && !disp, "AddSourceFilter: no objects, no fake success (%#lx, %p)", hr, disp);
    hr = IMediaControl_AddSourceFilter(control, name, NULL);
    CHECKF(hr == E_POINTER, "AddSourceFilter(NULL out) is E_POINTER (%#lx)", hr);
    SysFreeString(name);
    IMediaControl_Release(control);
    IGraphBuilder_Release(graph);
}

int main(void)
{
    pc_vtbl = (IPinConnectionVtbl){ pc_QI, pc_AddRef, pc_Release, pc_DynamicQueryAccept,
            pc_NotifyEndOfStream, pc_IsEndPin, pc_DynamicDisconnect };
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_seeking();
    test_graph_config();
    test_frame_step();
    test_control_collections();
    CoUninitialize();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
