/* strictpins-probe: the filter graph asks a filter's pin enumerator for a count (patches/sg/9108).
 *
 * AviSynth's DirectShowSource adds its own sink filter to a graph and calls RenderFile, expecting
 * Intelligent Connect to end the stream at that filter. Its IEnumPins::Next refuses a NULL
 * pcFetched (E_POINTER), which Windows' filter graph manager never passes; Wine's passed NULL in
 * every pin walk, so the filter looked pinless and RenderFile built a renderer of its own:
 * "DirectShowSource: RenderFile, the filter graph manager won't talk to me". A sink filter of the
 * probe's own with such an enumerator (it also refuses a NULL count from IEnumMediaTypes for more
 * than one) is added to a graph; then RenderFile of a WAV file, IGraphBuilder::Connect,
 * ICaptureGraphBuilder2::RenderStream and FindPin must all find and use its pin.
 *
 * Prints name=value lines; see test/strictpins-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <stdio.h>
#include <string.h>

static const WCHAR wav[] = L"C:\\tone.wav";

static void say(const char *name, int ok, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s=%d %s\n", name, ok ? 1 : 0, buf);
    fflush(stdout);
}

static BOOL write_wav(void)
{
    static BYTE file[44 + 44100];
    DWORD data = 44100, riff = 36 + data, rate = 22050, bytes = 44100, fmt = 16, written;
    WORD pcm = 1, ch = 1, align = 2, bits = 16;
    HANDLE h;
    BYTE *p = file;
#define PUT(v) (memcpy(p, &(v), sizeof(v)), p += sizeof(v))
    memcpy(p, "RIFF", 4); p += 4; PUT(riff); memcpy(p, "WAVEfmt ", 8); p += 8; PUT(fmt);
    PUT(pcm); PUT(ch); PUT(rate); PUT(bytes); PUT(align); PUT(bits);
    memcpy(p, "data", 4); p += 4; PUT(data);
    h = CreateFileW(wav, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    WriteFile(h, file, sizeof(file), &written, NULL);
    CloseHandle(h);
    return written == sizeof(file);
}

/* ---- the strict sink: one input pin, PCM audio ---- */
static struct
{
    IBaseFilter IBaseFilter_iface;
    IPin IPin_iface;
    IMemInputPin IMemInputPin_iface;
    IFilterGraph *graph;
    IPin *peer;
    AM_MEDIA_TYPE mt;
    LONG null_fetched_pins, null_fetched_types;
} sink;

struct strict_enum { IEnumPins IEnumPins_iface; LONG ref; int pos; };
static HRESULT WINAPI ep_QueryInterface(IEnumPins *iface, REFIID iid, void **out) { *out = iface; IEnumPins_AddRef(iface); return S_OK; }
static ULONG WINAPI ep_AddRef(IEnumPins *iface) { return InterlockedIncrement(&((struct strict_enum *)iface)->ref); }
static ULONG WINAPI ep_Release(IEnumPins *iface)
{
    struct strict_enum *e = (struct strict_enum *)iface;
    ULONG ref = InterlockedDecrement(&e->ref);
    if (!ref) free(e);
    return ref;
}
/* as DirectShowSource's GetSampleEnumPins::Next: a count is required */
static HRESULT WINAPI ep_Next(IEnumPins *iface, ULONG count, IPin **pins, ULONG *fetched)
{
    struct strict_enum *e = (struct strict_enum *)iface;
    int copy;
    if (!pins || !fetched) { InterlockedIncrement(&sink.null_fetched_pins); return E_POINTER; }
    copy = *fetched = min((int)count, 1 - e->pos);
    if (copy > 0) { *pins = &sink.IPin_iface; IPin_AddRef(*pins); }
    e->pos += copy;
    return (int)count > copy ? S_FALSE : S_OK;
}
static HRESULT WINAPI ep_Skip(IEnumPins *iface, ULONG count) { ((struct strict_enum *)iface)->pos += count; return S_OK; }
static HRESULT WINAPI ep_Reset(IEnumPins *iface) { ((struct strict_enum *)iface)->pos = 0; return S_OK; }
static HRESULT WINAPI ep_Clone(IEnumPins *iface, IEnumPins **out) { return E_NOTIMPL; }
static const IEnumPinsVtbl ep_vtbl = { ep_QueryInterface, ep_AddRef, ep_Release, ep_Next, ep_Skip, ep_Reset, ep_Clone };

static BOOL pcm_type(const AM_MEDIA_TYPE *mt)
{
    return IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio) && IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_PCM);
}

static HRESULT WINAPI f_QueryInterface(IBaseFilter *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPersist) || IsEqualGUID(iid, &IID_IMediaFilter)
            || IsEqualGUID(iid, &IID_IBaseFilter)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI f_AddRef(IBaseFilter *iface) { return 2; }
static ULONG WINAPI f_Release(IBaseFilter *iface) { return 1; }
static HRESULT WINAPI f_GetClassID(IBaseFilter *iface, CLSID *clsid) { *clsid = GUID_NULL; return S_OK; }
static HRESULT WINAPI f_Stop(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI f_Pause(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI f_Run(IBaseFilter *iface, REFERENCE_TIME start) { return S_OK; }
static HRESULT WINAPI f_GetState(IBaseFilter *iface, DWORD ms, FILTER_STATE *state) { *state = State_Stopped; return S_OK; }
static HRESULT WINAPI f_SetSyncSource(IBaseFilter *iface, IReferenceClock *clock) { return S_OK; }
static HRESULT WINAPI f_GetSyncSource(IBaseFilter *iface, IReferenceClock **clock) { *clock = NULL; return S_OK; }
static HRESULT WINAPI f_EnumPins(IBaseFilter *iface, IEnumPins **out)
{
    struct strict_enum *e = calloc(1, sizeof(*e));
    e->IEnumPins_iface.lpVtbl = &ep_vtbl;
    e->ref = 1;
    *out = &e->IEnumPins_iface;
    return S_OK;
}
static HRESULT WINAPI f_FindPin(IBaseFilter *iface, const WCHAR *id, IPin **pin)
{
    if (wcscmp(id, L"In")) { *pin = NULL; return VFW_E_NOT_FOUND; }
    *pin = &sink.IPin_iface;
    return S_OK;
}
static HRESULT WINAPI f_QueryFilterInfo(IBaseFilter *iface, FILTER_INFO *info)
{
    wcscpy(info->achName, L"Strict Sink");
    if ((info->pGraph = sink.graph)) IFilterGraph_AddRef(info->pGraph);
    return S_OK;
}
static HRESULT WINAPI f_JoinFilterGraph(IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name) { sink.graph = graph; return S_OK; }
static HRESULT WINAPI f_QueryVendorInfo(IBaseFilter *iface, WCHAR **info) { return E_NOTIMPL; }
static const IBaseFilterVtbl filter_vtbl =
{
    f_QueryInterface, f_AddRef, f_Release, f_GetClassID, f_Stop, f_Pause, f_Run, f_GetState, f_SetSyncSource,
    f_GetSyncSource, f_EnumPins, f_FindPin, f_QueryFilterInfo, f_JoinFilterGraph, f_QueryVendorInfo,
};

static HRESULT WINAPI p_QueryInterface(IPin *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPin)) { *out = iface; return S_OK; }
    if (IsEqualGUID(iid, &IID_IMemInputPin)) { *out = &sink.IMemInputPin_iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI p_AddRef(IPin *iface) { return 2; }
static ULONG WINAPI p_Release(IPin *iface) { return 1; }
static HRESULT WINAPI p_Connect(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt) { return E_UNEXPECTED; }
static HRESULT WINAPI p_ReceiveConnection(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    if (sink.peer) return VFW_E_ALREADY_CONNECTED;
    if (!pcm_type(mt)) return VFW_E_TYPE_NOT_ACCEPTED;
    sink.peer = peer;
    IPin_AddRef(peer);
    sink.mt = *mt;
    sink.mt.pbFormat = NULL;
    sink.mt.cbFormat = 0;
    return S_OK;
}
static HRESULT WINAPI p_Disconnect(IPin *iface)
{
    if (!sink.peer) return S_FALSE;
    IPin_Release(sink.peer);
    sink.peer = NULL;
    return S_OK;
}
static HRESULT WINAPI p_ConnectedTo(IPin *iface, IPin **peer)
{
    if (!(*peer = sink.peer)) return VFW_E_NOT_CONNECTED;
    IPin_AddRef(*peer);
    return S_OK;
}
static HRESULT WINAPI p_ConnectionMediaType(IPin *iface, AM_MEDIA_TYPE *mt)
{
    if (!sink.peer) return VFW_E_NOT_CONNECTED;
    *mt = sink.mt;
    return S_OK;
}
static HRESULT WINAPI p_QueryPinInfo(IPin *iface, PIN_INFO *info)
{
    info->pFilter = &sink.IBaseFilter_iface;
    info->dir = PINDIR_INPUT;
    wcscpy(info->achName, L"In");
    return S_OK;
}
static HRESULT WINAPI p_QueryDirection(IPin *iface, PIN_DIRECTION *dir) { *dir = PINDIR_INPUT; return S_OK; }
static HRESULT WINAPI p_QueryId(IPin *iface, WCHAR **id) { *id = CoTaskMemAlloc(8); wcscpy(*id, L"In"); return S_OK; }
static HRESULT WINAPI p_QueryAccept(IPin *iface, const AM_MEDIA_TYPE *mt) { return pcm_type(mt) ? S_OK : S_FALSE; }
static HRESULT WINAPI p_EnumMediaTypes(IPin *iface, IEnumMediaTypes **out) { return E_NOTIMPL; }
static HRESULT WINAPI p_QueryInternalConnections(IPin *iface, IPin **pins, ULONG *count) { return E_NOTIMPL; }
static HRESULT WINAPI p_EndOfStream(IPin *iface) { return S_OK; }
static HRESULT WINAPI p_BeginFlush(IPin *iface) { return S_OK; }
static HRESULT WINAPI p_EndFlush(IPin *iface) { return S_OK; }
static HRESULT WINAPI p_NewSegment(IPin *iface, REFERENCE_TIME start, REFERENCE_TIME stop, double rate) { return S_OK; }
static const IPinVtbl pin_vtbl =
{
    p_QueryInterface, p_AddRef, p_Release, p_Connect, p_ReceiveConnection, p_Disconnect, p_ConnectedTo,
    p_ConnectionMediaType, p_QueryPinInfo, p_QueryDirection, p_QueryId, p_QueryAccept, p_EnumMediaTypes,
    p_QueryInternalConnections, p_EndOfStream, p_BeginFlush, p_EndFlush, p_NewSegment,
};

static HRESULT WINAPI m_QueryInterface(IMemInputPin *iface, REFIID iid, void **out) { return IPin_QueryInterface(&sink.IPin_iface, iid, out); }
static ULONG WINAPI m_AddRef(IMemInputPin *iface) { return 2; }
static ULONG WINAPI m_Release(IMemInputPin *iface) { return 1; }
static HRESULT WINAPI m_GetAllocator(IMemInputPin *iface, IMemAllocator **allocator) { return VFW_E_NO_ALLOCATOR; }
static HRESULT WINAPI m_NotifyAllocator(IMemInputPin *iface, IMemAllocator *allocator, BOOL read_only) { return S_OK; }
static HRESULT WINAPI m_GetAllocatorRequirements(IMemInputPin *iface, ALLOCATOR_PROPERTIES *props) { return E_NOTIMPL; }
static HRESULT WINAPI m_Receive(IMemInputPin *iface, IMediaSample *sample) { return S_OK; }
static HRESULT WINAPI m_ReceiveMultiple(IMemInputPin *iface, IMediaSample **samples, LONG count, LONG *processed) { *processed = count; return S_OK; }
static HRESULT WINAPI m_ReceiveCanBlock(IMemInputPin *iface) { return S_FALSE; }
static const IMemInputPinVtbl mem_vtbl =
{
    m_QueryInterface, m_AddRef, m_Release, m_GetAllocator, m_NotifyAllocator, m_GetAllocatorRequirements,
    m_Receive, m_ReceiveMultiple, m_ReceiveCanBlock,
};

static void sink_reset(void)
{
    if (sink.peer) IPin_Release(sink.peer);
    memset(&sink, 0, sizeof(sink));
    sink.IBaseFilter_iface.lpVtbl = &filter_vtbl;
    sink.IPin_iface.lpVtbl = &pin_vtbl;
    sink.IMemInputPin_iface.lpVtbl = &mem_vtbl;
}

static IGraphBuilder *new_graph(void)
{
    IGraphBuilder *graph;
    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    sink_reset();
    IGraphBuilder_AddFilter(graph, &sink.IBaseFilter_iface, L"Strict Sink");
    return graph;
}

static BOOL graph_has_renderer(IGraphBuilder *graph)
{
    IBaseFilter *filter;
    BOOL found = IGraphBuilder_FindFilterByName(graph, L"Default DirectSound Device", &filter) == S_OK;
    if (found) IBaseFilter_Release(filter);
    if (IGraphBuilder_FindFilterByName(graph, L"DirectSound Audio Renderer", &filter) == S_OK)
    {
        found = TRUE;
        IBaseFilter_Release(filter);
    }
    return found;
}

int main(void)
{
    ICaptureGraphBuilder2 *builder;
    IBaseFilter *source;
    IGraphBuilder *graph;
    IPin *pin;
    HRESULT hr;

    CoInitialize(NULL);
    say("wav", write_wav(), "a PCM WAV file");

    /* RenderFile ends at the filter already in the graph */
    graph = new_graph();
    hr = IGraphBuilder_RenderFile(graph, wav, NULL);
    say("renderfile", SUCCEEDED(hr) && sink.peer, "RenderFile hr %#lx, the strict sink %s", hr,
            sink.peer ? "connected" : "NOT connected");
    say("no_extra_renderer", !graph_has_renderer(graph), "no audio renderer of the graph's own");
    say("renderfile_counts", !sink.null_fetched_pins, "IEnumPins::Next with a NULL count: %ld times", sink.null_fetched_pins);
    IGraphBuilder_Release(graph);

    /* Connect from the source filter's output, through the parser */
    graph = new_graph();
    hr = IGraphBuilder_AddSourceFilter(graph, wav, L"Source", &source);
    if (SUCCEEDED(hr) && SUCCEEDED(hr = IBaseFilter_FindPin(source, L"Output", &pin)))
    {
        hr = IGraphBuilder_Connect(graph, pin, &sink.IPin_iface);
        IPin_Release(pin);
    }
    say("connect", SUCCEEDED(hr) && sink.peer, "Connect hr %#lx", hr);
    IBaseFilter_Release(source);
    IGraphBuilder_Release(graph);

    /* the capture graph builder: RenderStream to the sink, FindPin on it */
    graph = new_graph();
    CoCreateInstance(&CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, &IID_ICaptureGraphBuilder2, (void **)&builder);
    ICaptureGraphBuilder2_SetFiltergraph(builder, graph);
    hr = IGraphBuilder_AddSourceFilter(graph, wav, L"Source", &source);
    if (SUCCEEDED(hr))
        hr = ICaptureGraphBuilder2_RenderStream(builder, NULL, NULL, (IUnknown *)source, NULL, &sink.IBaseFilter_iface);
    say("renderstream", SUCCEEDED(hr) && sink.peer, "RenderStream hr %#lx", hr);
    pin = NULL;
    hr = ICaptureGraphBuilder2_FindPin(builder, (IUnknown *)&sink.IBaseFilter_iface, PINDIR_INPUT, NULL, NULL, FALSE, 0, &pin);
    say("findpin", hr == S_OK && pin == &sink.IPin_iface, "FindPin hr %#lx", hr);
    say("builder_counts", !sink.null_fetched_pins, "IEnumPins::Next with a NULL count: %ld times", sink.null_fetched_pins);
    IBaseFilter_Release(source);
    ICaptureGraphBuilder2_Release(builder);
    IGraphBuilder_Release(graph);

    DeleteFileW(wav);
    printf("done=1\n");
    return 0;
}
