/* colourconv-probe: the Color Space Converter filter (patches/sg/9106).
 *
 * Wine had no Color Space Converter (CLSID_Colour), so OpenCV's DirectShow capture -- a Sample
 * Grabber set to RGB24 behind the capture pin, built with ICaptureGraphBuilder2::RenderStream --
 * got VFW_E_CANNOT_CONNECT whenever the source gave RGB32 (a screen capture source, a virtual
 * camera). A push source of the probe's own stands in for the capture filter: one output pin
 * that offers MEDIASUBTYPE_RGB32 only, bottom-up frames of four coloured quarters. As OpenCV
 * builds its graph: the source, a Sample Grabber with a media type of RGB24 (then RGB565, RGB555,
 * then RGB32), a Null Renderer, RenderStream, Run. Checks the graph got a Color Space Converter
 * between the two (and none for RGB32, which connects directly), the grabbed frame's corners are
 * the right colours the right way up, a top-down source too, and the filter mapper has it as
 * Windows registers it.
 *
 * Prints name=value lines; see test/colourconv-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <qedit.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(p_CLSID_Colour, 0x1643e180, 0x90f5, 0x11ce, 0x97, 0xd5, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID(p_CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(p_CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);

#define W 64
#define H 48
#define FRAMES 5

static BOOL top_down;

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

/* the picture as seen: top left red, top right green, bottom left blue, bottom right white */
static DWORD quarter_colour(int x, int y_from_top)
{
    if (y_from_top < H / 2) return x < W / 2 ? 0xff0000 : 0x00ff00;
    return x < W / 2 ? 0x0000ff : 0xffffff;
}

/* ---- a push source: one RGB32 output pin ---- */
struct src
{
    IBaseFilter IBaseFilter_iface;
    IPin IPin_iface;
    IEnumMediaTypes IEnumMediaTypes_iface;
    IFilterGraph *graph;
    IPin *peer;
    IMemInputPin *input;
    IMemAllocator *allocator;
    AM_MEDIA_TYPE mt;
    VIDEOINFOHEADER vih;
    FILTER_STATE state;
    HANDLE thread;
    int enum_index;
};
static struct src source;

static void make_type(struct src *s)
{
    memset(&s->vih, 0, sizeof(s->vih));
    s->vih.AvgTimePerFrame = 333333;
    s->vih.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    s->vih.bmiHeader.biWidth = W;
    s->vih.bmiHeader.biHeight = top_down ? -H : H;
    s->vih.bmiHeader.biPlanes = 1;
    s->vih.bmiHeader.biBitCount = 32;
    s->vih.bmiHeader.biCompression = BI_RGB;
    s->vih.bmiHeader.biSizeImage = W * H * 4;
    memset(&s->mt, 0, sizeof(s->mt));
    s->mt.majortype = MEDIATYPE_Video;
    s->mt.subtype = MEDIASUBTYPE_RGB32;
    s->mt.bFixedSizeSamples = TRUE;
    s->mt.lSampleSize = W * H * 4;
    s->mt.formattype = FORMAT_VideoInfo;
    s->mt.cbFormat = sizeof(s->vih);
    s->mt.pbFormat = (BYTE *)&s->vih;
}

static AM_MEDIA_TYPE *dup_type(const AM_MEDIA_TYPE *mt)
{
    AM_MEDIA_TYPE *ret = CoTaskMemAlloc(sizeof(*ret));
    *ret = *mt;
    ret->pbFormat = CoTaskMemAlloc(mt->cbFormat);
    memcpy(ret->pbFormat, mt->pbFormat, mt->cbFormat);
    return ret;
}

static HRESULT WINAPI em_QueryInterface(IEnumMediaTypes *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI em_AddRef(IEnumMediaTypes *iface) { return 2; }
static ULONG WINAPI em_Release(IEnumMediaTypes *iface) { return 1; }
static HRESULT WINAPI em_Next(IEnumMediaTypes *iface, ULONG count, AM_MEDIA_TYPE **mts, ULONG *fetched)
{
    ULONG n = 0;
    if (count && !source.enum_index) { mts[n++] = dup_type(&source.mt); source.enum_index++; }
    if (fetched) *fetched = n;
    return n == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI em_Skip(IEnumMediaTypes *iface, ULONG count) { source.enum_index += count; return source.enum_index > 1 ? S_FALSE : S_OK; }
static HRESULT WINAPI em_Reset(IEnumMediaTypes *iface) { source.enum_index = 0; return S_OK; }
static HRESULT WINAPI em_Clone(IEnumMediaTypes *iface, IEnumMediaTypes **out) { *out = iface; return S_OK; }
static const IEnumMediaTypesVtbl em_vtbl = { em_QueryInterface, em_AddRef, em_Release, em_Next, em_Skip, em_Reset, em_Clone };

struct enum_pins { IEnumPins IEnumPins_iface; int index; };
static HRESULT WINAPI ep_QueryInterface(IEnumPins *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI ep_AddRef(IEnumPins *iface) { return 2; }
static ULONG WINAPI ep_Release(IEnumPins *iface) { return 1; }
static HRESULT WINAPI ep_Next(IEnumPins *iface, ULONG count, IPin **pins, ULONG *fetched)
{
    struct enum_pins *e = (struct enum_pins *)iface;
    ULONG n = 0;
    if (count && !e->index) { pins[n++] = &source.IPin_iface; e->index++; }
    if (fetched) *fetched = n;
    return n == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI ep_Skip(IEnumPins *iface, ULONG count) { ((struct enum_pins *)iface)->index += count; return S_OK; }
static HRESULT WINAPI ep_Reset(IEnumPins *iface) { ((struct enum_pins *)iface)->index = 0; return S_OK; }
static HRESULT WINAPI ep_Clone(IEnumPins *iface, IEnumPins **out) { return E_NOTIMPL; }
static const IEnumPinsVtbl ep_vtbl = { ep_QueryInterface, ep_AddRef, ep_Release, ep_Next, ep_Skip, ep_Reset, ep_Clone };
static struct enum_pins pin_enum = { { &ep_vtbl }, 0 };

static DWORD WINAPI push_thread(void *arg)
{
    int i, x, y;
    for (i = 0; i < FRAMES; i++)
    {
        REFERENCE_TIME start = i * 333333, stop = start + 333333;
        IMediaSample *sample;
        BYTE *data;
        if (FAILED(IMemAllocator_GetBuffer(source.allocator, &sample, NULL, NULL, 0))) break;
        IMediaSample_GetPointer(sample, &data);
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
            {
                DWORD c = quarter_colour(x, top_down ? y : H - 1 - y);
                memcpy(data + (y * W + x) * 4, &c, 4);
            }
        IMediaSample_SetActualDataLength(sample, W * H * 4);
        IMediaSample_SetTime(sample, &start, &stop);
        IMediaSample_SetSyncPoint(sample, TRUE);
        IMemInputPin_Receive(source.input, sample);
        IMediaSample_Release(sample);
    }
    IPin_EndOfStream(source.peer);
    return 0;
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
static HRESULT WINAPI f_Stop(IBaseFilter *iface)
{
    if (source.thread) { WaitForSingleObject(source.thread, 5000); CloseHandle(source.thread); source.thread = NULL; }
    if (source.allocator) IMemAllocator_Decommit(source.allocator);
    source.state = State_Stopped;
    return S_OK;
}
static HRESULT WINAPI f_Pause(IBaseFilter *iface)
{
    if (source.state == State_Stopped && source.allocator) IMemAllocator_Commit(source.allocator);
    source.state = State_Paused;
    /* frames go out from the pause on, as a file source's do: the renderer prerolls on the first */
    if (!source.thread && source.input) source.thread = CreateThread(NULL, 0, push_thread, NULL, 0, NULL);
    return S_OK;
}
static HRESULT WINAPI f_Run(IBaseFilter *iface, REFERENCE_TIME start)
{
    if (source.state == State_Stopped && source.allocator) IMemAllocator_Commit(source.allocator);
    source.state = State_Running;
    if (!source.thread && source.input) source.thread = CreateThread(NULL, 0, push_thread, NULL, 0, NULL);
    return S_OK;
}
static HRESULT WINAPI f_GetState(IBaseFilter *iface, DWORD ms, FILTER_STATE *state) { *state = source.state; return S_OK; }
static HRESULT WINAPI f_SetSyncSource(IBaseFilter *iface, IReferenceClock *clock) { return S_OK; }
static HRESULT WINAPI f_GetSyncSource(IBaseFilter *iface, IReferenceClock **clock) { *clock = NULL; return S_OK; }
static HRESULT WINAPI f_EnumPins(IBaseFilter *iface, IEnumPins **out) { pin_enum.index = 0; *out = &pin_enum.IEnumPins_iface; return S_OK; }
static HRESULT WINAPI f_FindPin(IBaseFilter *iface, const WCHAR *id, IPin **pin) { *pin = &source.IPin_iface; return S_OK; }
static HRESULT WINAPI f_QueryFilterInfo(IBaseFilter *iface, FILTER_INFO *info)
{
    wcscpy(info->achName, L"Probe Source");
    info->pGraph = source.graph;
    if (info->pGraph) IFilterGraph_AddRef(info->pGraph);
    return S_OK;
}
static HRESULT WINAPI f_JoinFilterGraph(IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name) { source.graph = graph; return S_OK; }
static HRESULT WINAPI f_QueryVendorInfo(IBaseFilter *iface, WCHAR **info) { return E_NOTIMPL; }
static const IBaseFilterVtbl filter_vtbl =
{
    f_QueryInterface, f_AddRef, f_Release, f_GetClassID, f_Stop, f_Pause, f_Run, f_GetState, f_SetSyncSource,
    f_GetSyncSource, f_EnumPins, f_FindPin, f_QueryFilterInfo, f_JoinFilterGraph, f_QueryVendorInfo,
};

static HRESULT WINAPI p_QueryInterface(IPin *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPin)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI p_AddRef(IPin *iface) { return 2; }
static ULONG WINAPI p_Release(IPin *iface) { return 1; }
static HRESULT WINAPI p_Connect(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    ALLOCATOR_PROPERTIES props = {4, W * H * 4, 1, 0}, actual;
    HRESULT hr;

    if (source.peer) return VFW_E_ALREADY_CONNECTED;
    /* the one type, or nothing */
    if (mt && mt->majortype.Data1 && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB32) && !IsEqualGUID(&mt->subtype, &GUID_NULL))
        return VFW_E_TYPE_NOT_ACCEPTED;
    if (FAILED(hr = IPin_ReceiveConnection(peer, iface, &source.mt))) return VFW_E_NO_ACCEPTABLE_TYPES;
    IPin_QueryInterface(peer, &IID_IMemInputPin, (void **)&source.input);
    if (FAILED(IMemInputPin_GetAllocator(source.input, &source.allocator)))
        CoCreateInstance(&CLSID_MemoryAllocator, NULL, CLSCTX_INPROC_SERVER, &IID_IMemAllocator, (void **)&source.allocator);
    IMemAllocator_SetProperties(source.allocator, &props, &actual);
    IMemInputPin_NotifyAllocator(source.input, source.allocator, FALSE);
    source.peer = peer;
    IPin_AddRef(peer);
    return S_OK;
}
static HRESULT WINAPI p_ReceiveConnection(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt) { return E_UNEXPECTED; }
static HRESULT WINAPI p_Disconnect(IPin *iface)
{
    if (!source.peer) return S_FALSE;
    IPin_Release(source.peer);
    source.peer = NULL;
    if (source.input) IMemInputPin_Release(source.input);
    source.input = NULL;
    if (source.allocator) IMemAllocator_Release(source.allocator);
    source.allocator = NULL;
    return S_OK;
}
static HRESULT WINAPI p_ConnectedTo(IPin *iface, IPin **peer)
{
    if (!(*peer = source.peer)) return VFW_E_NOT_CONNECTED;
    IPin_AddRef(*peer);
    return S_OK;
}
static HRESULT WINAPI p_ConnectionMediaType(IPin *iface, AM_MEDIA_TYPE *mt)
{
    AM_MEDIA_TYPE *copy;
    if (!source.peer) return VFW_E_NOT_CONNECTED;
    copy = dup_type(&source.mt);
    *mt = *copy;
    CoTaskMemFree(copy);
    return S_OK;
}
static HRESULT WINAPI p_QueryPinInfo(IPin *iface, PIN_INFO *info)
{
    info->pFilter = &source.IBaseFilter_iface;
    info->dir = PINDIR_OUTPUT;
    wcscpy(info->achName, L"Capture");
    return S_OK;
}
static HRESULT WINAPI p_QueryDirection(IPin *iface, PIN_DIRECTION *dir) { *dir = PINDIR_OUTPUT; return S_OK; }
static HRESULT WINAPI p_QueryId(IPin *iface, WCHAR **id)
{
    *id = CoTaskMemAlloc(16);
    wcscpy(*id, L"Capture");
    return S_OK;
}
static HRESULT WINAPI p_QueryAccept(IPin *iface, const AM_MEDIA_TYPE *mt) { return IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB32) ? S_OK : S_FALSE; }
static HRESULT WINAPI p_EnumMediaTypes(IPin *iface, IEnumMediaTypes **out)
{
    source.enum_index = 0;
    *out = &source.IEnumMediaTypes_iface;
    return S_OK;
}
static HRESULT WINAPI p_QueryInternalConnections(IPin *iface, IPin **pins, ULONG *count) { return E_NOTIMPL; }
static HRESULT WINAPI p_EndOfStream(IPin *iface) { return E_UNEXPECTED; }
static HRESULT WINAPI p_BeginFlush(IPin *iface) { return E_UNEXPECTED; }
static HRESULT WINAPI p_EndFlush(IPin *iface) { return E_UNEXPECTED; }
static HRESULT WINAPI p_NewSegment(IPin *iface, REFERENCE_TIME start, REFERENCE_TIME stop, double rate) { return S_OK; }
static const IPinVtbl pin_vtbl =
{
    p_QueryInterface, p_AddRef, p_Release, p_Connect, p_ReceiveConnection, p_Disconnect, p_ConnectedTo,
    p_ConnectionMediaType, p_QueryPinInfo, p_QueryDirection, p_QueryId, p_QueryAccept, p_EnumMediaTypes,
    p_QueryInternalConnections, p_EndOfStream, p_BeginFlush, p_EndFlush, p_NewSegment,
};

static void source_init(void)
{
    memset(&source, 0, sizeof(source));
    source.IBaseFilter_iface.lpVtbl = &filter_vtbl;
    source.IPin_iface.lpVtbl = &pin_vtbl;
    source.IEnumMediaTypes_iface.lpVtbl = &em_vtbl;
    source.state = State_Stopped;
    make_type(&source);
}

static BOOL graph_has(IGraphBuilder *graph, const GUID *clsid)
{
    IEnumFilters *filters;
    IBaseFilter *filter;
    BOOL found = FALSE;

    IGraphBuilder_EnumFilters(graph, &filters);
    while (IEnumFilters_Next(filters, 1, &filter, NULL) == S_OK)
    {
        CLSID id;
        if (SUCCEEDED(IBaseFilter_GetClassID(filter, &id)) && IsEqualGUID(&id, clsid)) found = TRUE;
        IBaseFilter_Release(filter);
    }
    IEnumFilters_Release(filters);
    return found;
}

/* a pixel of the grabbed frame (bottom-up DIB of the grabber's type) as 0xRRGGBB */
static BOOL grabbed_top_down;

static DWORD pixel(const BYTE *buf, const GUID *subtype, int x, int y_from_top)
{
    int y = grabbed_top_down ? y_from_top : H - 1 - y_from_top;
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB24))
    {
        const BYTE *p = buf + y * ((W * 3 + 3) & ~3) + x * 3;
        return (p[2] << 16) | (p[1] << 8) | p[0];
    }
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB565))
    {
        WORD v = *(const WORD *)(buf + y * ((W * 2 + 3) & ~3) + x * 2);
        return (((v >> 11) & 0x1f) << 19) | (((v >> 5) & 0x3f) << 10) | ((v & 0x1f) << 3);
    }
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB555))
    {
        WORD v = *(const WORD *)(buf + y * ((W * 2 + 3) & ~3) + x * 2);
        return (((v >> 10) & 0x1f) << 19) | (((v >> 5) & 0x1f) << 11) | ((v & 0x1f) << 3);
    }
    return *(const DWORD *)(buf + y * W * 4 + x * 4) & 0xffffff;
}

static BOOL close_colour(DWORD a, DWORD b)
{
    int i;
    for (i = 0; i < 24; i += 8)
        if (abs((int)((a >> i) & 0xff) - (int)((b >> i) & 0xff)) > 8) return FALSE;
    return TRUE;
}

static void run_case(const char *name, const GUID *subtype, BOOL want_converter)
{
    ICaptureGraphBuilder2 *builder;
    IBaseFilter *grabber_filter, *null;
    ISampleGrabber *grabber;
    IMediaControl *control;
    IMediaEvent *event;
    IGraphBuilder *graph;
    AM_MEDIA_TYPE mt = {0};
    static BYTE buffer[W * H * 4 + 64];
    LONG size = sizeof(buffer), code = 0;
    char key[64];
    HRESULT hr;
    BOOL converter, colours = FALSE;

    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    CoCreateInstance(&CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, &IID_ICaptureGraphBuilder2, (void **)&builder);
    ICaptureGraphBuilder2_SetFiltergraph(builder, graph);
    source_init();
    hr = IGraphBuilder_AddFilter(graph, &source.IBaseFilter_iface, L"Probe Source");
    CoCreateInstance(&p_CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&grabber_filter);
    IBaseFilter_QueryInterface(grabber_filter, &IID_ISampleGrabber, (void **)&grabber);
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = *subtype;
    ISampleGrabber_SetMediaType(grabber, &mt);
    ISampleGrabber_SetBufferSamples(grabber, TRUE);
    ISampleGrabber_SetOneShot(grabber, FALSE);
    IGraphBuilder_AddFilter(graph, grabber_filter, L"Sample Grabber");
    CoCreateInstance(&p_CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&null);
    IGraphBuilder_AddFilter(graph, null, L"Null Renderer");

    if (SUCCEEDED(hr))
        hr = ICaptureGraphBuilder2_RenderStream(builder, NULL, &MEDIATYPE_Video, (IUnknown *)&source.IBaseFilter_iface,
                grabber_filter, null);
    sprintf(key, "%s_connect", name);
    say(key, SUCCEEDED(hr), "RenderStream hr %#lx", hr);
    converter = graph_has(graph, &p_CLSID_Colour);
    sprintf(key, "%s_converter", name);
    say(key, converter == want_converter, "Color Space Converter in the graph: %s", converter ? "yes" : "no");

    if (SUCCEEDED(hr))
    {
        IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
        IGraphBuilder_QueryInterface(graph, &IID_IMediaEvent, (void **)&event);
        hr = IMediaControl_Run(control);
        IMediaEvent_WaitForCompletion(event, 5000, &code);
        hr = ISampleGrabber_GetCurrentBuffer(grabber, &size, (LONG *)buffer);
        {
            /* the frame's orientation is its type's: a negative biHeight is top-down */
            AM_MEDIA_TYPE connected;
            grabbed_top_down = FALSE;
            if (SUCCEEDED(ISampleGrabber_GetConnectedMediaType(grabber, &connected)))
            {
                if (IsEqualGUID(&connected.formattype, &FORMAT_VideoInfo) && connected.pbFormat)
                    grabbed_top_down = ((VIDEOINFOHEADER *)connected.pbFormat)->bmiHeader.biHeight < 0;
                CoTaskMemFree(connected.pbFormat);
            }
        }
        if (SUCCEEDED(hr))
        {
            DWORD tl = pixel(buffer, subtype, 2, 2), tr = pixel(buffer, subtype, W - 3, 2);
            DWORD bl = pixel(buffer, subtype, 2, H - 3), br = pixel(buffer, subtype, W - 3, H - 3);
            colours = close_colour(tl, 0xff0000) && close_colour(tr, 0x00ff00) && close_colour(bl, 0x0000ff)
                    && close_colour(br, 0xffffff);
            sprintf(key, "%s_frame", name);
            say(key, colours, "%ld bytes; corners tl %06lx tr %06lx bl %06lx br %06lx (want ff0000 00ff00 0000ff ffffff)",
                    size, tl, tr, bl, br);
        }
        else
        {
            sprintf(key, "%s_frame", name);
            say(key, FALSE, "GetCurrentBuffer hr %#lx (completion %#lx)", hr, code);
        }
        IMediaControl_Stop(control);
        IMediaControl_Release(control);
        IMediaEvent_Release(event);
    }
    else
    {
        sprintf(key, "%s_frame", name);
        say(key, FALSE, "no graph");
    }
    ISampleGrabber_Release(grabber);
    IBaseFilter_Release(grabber_filter);
    IBaseFilter_Release(null);
    ICaptureGraphBuilder2_Release(builder);
    IGraphBuilder_Release(graph);
}

/* ---- a sink of the probe's own that keeps the first frame it gets ---- */
static struct
{
    IBaseFilter IBaseFilter_iface;
    IPin IPin_iface;
    IMemInputPin IMemInputPin_iface;
    IPin *peer;
    IFilterGraph *graph;
    BYTE frame[W * H * 4];
    LONG frame_size;
    HANDLE got;
} cap;

static HRESULT WINAPI cf_QueryInterface(IBaseFilter *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPersist) || IsEqualGUID(iid, &IID_IMediaFilter)
            || IsEqualGUID(iid, &IID_IBaseFilter)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IBaseFilter *iface) { return 2; }
static ULONG WINAPI cf_Release(IBaseFilter *iface) { return 1; }
static HRESULT WINAPI cf_GetClassID(IBaseFilter *iface, CLSID *clsid) { *clsid = GUID_NULL; return S_OK; }
static HRESULT WINAPI cf_Stop(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI cf_Pause(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI cf_Run(IBaseFilter *iface, REFERENCE_TIME start) { return S_OK; }
static HRESULT WINAPI cf_GetState(IBaseFilter *iface, DWORD ms, FILTER_STATE *state) { *state = State_Stopped; return S_OK; }
static HRESULT WINAPI cf_SetSyncSource(IBaseFilter *iface, IReferenceClock *clock) { return S_OK; }
static HRESULT WINAPI cf_GetSyncSource(IBaseFilter *iface, IReferenceClock **clock) { *clock = NULL; return S_OK; }
static HRESULT WINAPI cep_QueryInterface(IEnumPins *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI cep_AddRef(IEnumPins *iface) { return 2; }
static ULONG WINAPI cep_Release(IEnumPins *iface) { return 1; }
static int cap_enum_index;
static HRESULT WINAPI cep_Next(IEnumPins *iface, ULONG count, IPin **pins, ULONG *fetched)
{
    ULONG n = 0;
    if (count && !cap_enum_index) { pins[n++] = &cap.IPin_iface; cap_enum_index++; }
    if (fetched) *fetched = n;
    return n == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI cep_Skip(IEnumPins *iface, ULONG count) { cap_enum_index += count; return S_OK; }
static HRESULT WINAPI cep_Reset(IEnumPins *iface) { cap_enum_index = 0; return S_OK; }
static HRESULT WINAPI cep_Clone(IEnumPins *iface, IEnumPins **out) { return E_NOTIMPL; }
static const IEnumPinsVtbl cap_enum_vtbl = { cep_QueryInterface, cep_AddRef, cep_Release, cep_Next, cep_Skip, cep_Reset, cep_Clone };
static IEnumPins cap_enum = { &cap_enum_vtbl };
static HRESULT WINAPI cf_EnumPins(IBaseFilter *iface, IEnumPins **out) { cap_enum_index = 0; *out = &cap_enum; return S_OK; }
static HRESULT WINAPI cf_FindPin(IBaseFilter *iface, const WCHAR *id, IPin **pin) { *pin = &cap.IPin_iface; return S_OK; }
static HRESULT WINAPI cf_QueryFilterInfo(IBaseFilter *iface, FILTER_INFO *info)
{
    wcscpy(info->achName, L"Probe Sink");
    if ((info->pGraph = cap.graph)) IFilterGraph_AddRef(info->pGraph);
    return S_OK;
}
static HRESULT WINAPI cf_JoinFilterGraph(IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name) { cap.graph = graph; return S_OK; }
static HRESULT WINAPI cf_QueryVendorInfo(IBaseFilter *iface, WCHAR **info) { return E_NOTIMPL; }
static const IBaseFilterVtbl cap_filter_vtbl =
{
    cf_QueryInterface, cf_AddRef, cf_Release, cf_GetClassID, cf_Stop, cf_Pause, cf_Run, cf_GetState, cf_SetSyncSource,
    cf_GetSyncSource, cf_EnumPins, cf_FindPin, cf_QueryFilterInfo, cf_JoinFilterGraph, cf_QueryVendorInfo,
};
static HRESULT WINAPI cp_QueryInterface(IPin *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPin)) { *out = iface; return S_OK; }
    if (IsEqualGUID(iid, &IID_IMemInputPin)) { *out = &cap.IMemInputPin_iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cp_AddRef(IPin *iface) { return 2; }
static ULONG WINAPI cp_Release(IPin *iface) { return 1; }
static HRESULT WINAPI cp_Connect(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt) { return E_UNEXPECTED; }
static HRESULT WINAPI cp_ReceiveConnection(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt) { cap.peer = peer; return S_OK; }
static HRESULT WINAPI cp_Disconnect(IPin *iface) { cap.peer = NULL; return S_OK; }
static HRESULT WINAPI cp_ConnectedTo(IPin *iface, IPin **peer) { if (!(*peer = cap.peer)) return VFW_E_NOT_CONNECTED; IPin_AddRef(*peer); return S_OK; }
static HRESULT WINAPI cp_ConnectionMediaType(IPin *iface, AM_MEDIA_TYPE *mt) { return E_NOTIMPL; }
static HRESULT WINAPI cp_QueryPinInfo(IPin *iface, PIN_INFO *info) { info->pFilter = &cap.IBaseFilter_iface; info->dir = PINDIR_INPUT; wcscpy(info->achName, L"In"); return S_OK; }
static HRESULT WINAPI cp_QueryDirection(IPin *iface, PIN_DIRECTION *dir) { *dir = PINDIR_INPUT; return S_OK; }
static HRESULT WINAPI cp_QueryId(IPin *iface, WCHAR **id) { return E_NOTIMPL; }
static HRESULT WINAPI cp_QueryAccept(IPin *iface, const AM_MEDIA_TYPE *mt) { return S_OK; }
static HRESULT WINAPI cp_EnumMediaTypes(IPin *iface, IEnumMediaTypes **out) { return E_NOTIMPL; }
static HRESULT WINAPI cp_QueryInternalConnections(IPin *iface, IPin **pins, ULONG *count) { return E_NOTIMPL; }
static HRESULT WINAPI cp_EndOfStream(IPin *iface) { return S_OK; }
static HRESULT WINAPI cp_BeginFlush(IPin *iface) { return S_OK; }
static HRESULT WINAPI cp_EndFlush(IPin *iface) { return S_OK; }
static HRESULT WINAPI cp_NewSegment(IPin *iface, REFERENCE_TIME start, REFERENCE_TIME stop, double rate) { return S_OK; }
static const IPinVtbl cap_pin_vtbl =
{
    cp_QueryInterface, cp_AddRef, cp_Release, cp_Connect, cp_ReceiveConnection, cp_Disconnect, cp_ConnectedTo,
    cp_ConnectionMediaType, cp_QueryPinInfo, cp_QueryDirection, cp_QueryId, cp_QueryAccept, cp_EnumMediaTypes,
    cp_QueryInternalConnections, cp_EndOfStream, cp_BeginFlush, cp_EndFlush, cp_NewSegment,
};
static HRESULT WINAPI cm_QueryInterface(IMemInputPin *iface, REFIID iid, void **out) { return IPin_QueryInterface(&cap.IPin_iface, iid, out); }
static ULONG WINAPI cm_AddRef(IMemInputPin *iface) { return 2; }
static ULONG WINAPI cm_Release(IMemInputPin *iface) { return 1; }
static HRESULT WINAPI cm_GetAllocator(IMemInputPin *iface, IMemAllocator **allocator) { return VFW_E_NO_ALLOCATOR; }
static HRESULT WINAPI cm_NotifyAllocator(IMemInputPin *iface, IMemAllocator *allocator, BOOL read_only) { return S_OK; }
static HRESULT WINAPI cm_GetAllocatorRequirements(IMemInputPin *iface, ALLOCATOR_PROPERTIES *props) { return E_NOTIMPL; }
static HRESULT WINAPI cm_Receive(IMemInputPin *iface, IMediaSample *sample)
{
    BYTE *data;
    if (!cap.frame_size && SUCCEEDED(IMediaSample_GetPointer(sample, &data)))
    {
        cap.frame_size = min(IMediaSample_GetActualDataLength(sample), (LONG)sizeof(cap.frame));
        memcpy(cap.frame, data, cap.frame_size);
        SetEvent(cap.got);
    }
    return S_OK;
}
static HRESULT WINAPI cm_ReceiveMultiple(IMemInputPin *iface, IMediaSample **samples, LONG count, LONG *processed) { *processed = count; return S_OK; }
static HRESULT WINAPI cm_ReceiveCanBlock(IMemInputPin *iface) { return S_FALSE; }
static const IMemInputPinVtbl cap_mem_vtbl =
{
    cm_QueryInterface, cm_AddRef, cm_Release, cm_GetAllocator, cm_NotifyAllocator, cm_GetAllocatorRequirements,
    cm_Receive, cm_ReceiveMultiple, cm_ReceiveCanBlock,
};

/* the converter itself, between a top-down RGB32 source and a bottom-up RGB24 type given to it
 * (ConnectDirect with a full format, as a renderer asks): the rows must be turned over */
static void run_flip_case(void)
{
    IBaseFilter *colour;
    IMediaControl *control;
    IGraphBuilder *graph;
    VIDEOINFOHEADER vih = {0};
    AM_MEDIA_TYPE mt = {0};
    IPin *in, *out;
    HRESULT hr;

    top_down = TRUE;
    memset(&cap, 0, sizeof(cap));
    cap.IBaseFilter_iface.lpVtbl = &cap_filter_vtbl;
    cap.IPin_iface.lpVtbl = &cap_pin_vtbl;
    cap.IMemInputPin_iface.lpVtbl = &cap_mem_vtbl;
    cap.got = CreateEventA(NULL, TRUE, FALSE, NULL);
    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    source_init();
    IGraphBuilder_AddFilter(graph, &source.IBaseFilter_iface, L"Probe Source");
    IGraphBuilder_AddFilter(graph, &cap.IBaseFilter_iface, L"Probe Sink");
    if (FAILED(hr = CoCreateInstance(&p_CLSID_Colour, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&colour)))
    {
        say("flip_connect", FALSE, "no converter: hr %#lx", hr);
        say("flip_frame", FALSE, "no graph");
        IGraphBuilder_Release(graph);
        return;
    }
    IGraphBuilder_AddFilter(graph, colour, L"Color Space Converter");

    vih.AvgTimePerFrame = 333333;
    vih.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih.bmiHeader.biWidth = W;
    vih.bmiHeader.biHeight = H;         /* bottom-up */
    vih.bmiHeader.biPlanes = 1;
    vih.bmiHeader.biBitCount = 24;
    vih.bmiHeader.biCompression = BI_RGB;
    vih.bmiHeader.biSizeImage = ((W * 3 + 3) & ~3) * H;
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    mt.bFixedSizeSamples = TRUE;
    mt.lSampleSize = vih.bmiHeader.biSizeImage;
    mt.formattype = FORMAT_VideoInfo;
    mt.cbFormat = sizeof(vih);
    mt.pbFormat = (BYTE *)&vih;

    IBaseFilter_FindPin(colour, L"In", &in);
    IBaseFilter_FindPin(colour, L"Out", &out);
    hr = IGraphBuilder_ConnectDirect(graph, &source.IPin_iface, in, NULL);
    if (SUCCEEDED(hr)) hr = IGraphBuilder_ConnectDirect(graph, out, &cap.IPin_iface, &mt);
    say("flip_connect", SUCCEEDED(hr), "top-down RGB32 in, bottom-up RGB24 out: hr %#lx", hr);
    if (SUCCEEDED(hr))
    {
        IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
        IMediaControl_Run(control);
        WaitForSingleObject(cap.got, 5000);
        if (cap.frame_size)
        {
            DWORD tl, bl;
            grabbed_top_down = FALSE;
            tl = pixel(cap.frame, &MEDIASUBTYPE_RGB24, 2, 2);
            bl = pixel(cap.frame, &MEDIASUBTYPE_RGB24, 2, H - 3);
            say("flip_frame", close_colour(tl, 0xff0000) && close_colour(bl, 0x0000ff),
                    "bottom-up frame: top left %06lx (want ff0000), bottom left %06lx (want 0000ff)", tl, bl);
        }
        else say("flip_frame", FALSE, "no frame");
        IMediaControl_Stop(control);
        IMediaControl_Release(control);
    }
    else say("flip_frame", FALSE, "no graph");
    IPin_Release(in);
    IPin_Release(out);
    IBaseFilter_Release(colour);
    IGraphBuilder_Release(graph);
}

int main(void)
{
    IFilterMapper2 *mapper;
    IEnumMoniker *monikers;
    IMoniker *moniker;
    IBaseFilter *filter;
    GUID types[4];
    BOOL listed = FALSE;
    HRESULT hr;

    CoInitialize(NULL);

    hr = CoCreateInstance(&p_CLSID_Colour, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
    say("create", hr == S_OK, "CoCreateInstance(CLSID_Colour) hr %#lx", hr);
    if (SUCCEEDED(hr)) IBaseFilter_Release(filter);

    /* the mapper finds it for RGB32 in, RGB24 out, at its merit */
    CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper2, (void **)&mapper);
    types[0] = MEDIATYPE_Video; types[1] = MEDIASUBTYPE_RGB32;
    types[2] = MEDIATYPE_Video; types[3] = MEDIASUBTYPE_RGB24;
    hr = IFilterMapper2_EnumMatchingFilters(mapper, &monikers, 0, TRUE, 0x00400000, TRUE, 1, types, NULL, NULL,
            FALSE, TRUE, 1, types + 2, NULL, NULL);
    if (SUCCEEDED(hr))
    {
        while (IEnumMoniker_Next(monikers, 1, &moniker, NULL) == S_OK)
        {
            IPropertyBag *bag;
            VARIANT var;
            if (SUCCEEDED(IMoniker_BindToStorage(moniker, NULL, NULL, &IID_IPropertyBag, (void **)&bag)))
            {
                VariantInit(&var);
                if (SUCCEEDED(IPropertyBag_Read(bag, L"FriendlyName", &var, NULL)))
                {
                    if (!wcscmp(V_BSTR(&var), L"Color Space Converter")) listed = TRUE;
                    VariantClear(&var);
                }
                IPropertyBag_Release(bag);
            }
            IMoniker_Release(moniker);
        }
        IEnumMoniker_Release(monikers);
    }
    say("registered", listed, "\"Color Space Converter\" matches RGB32 -> RGB24 at merit 0x400000 and up (hr %#lx)", hr);
    IFilterMapper2_Release(mapper);

    run_case("rgb24", &MEDIASUBTYPE_RGB24, TRUE);
    run_case("rgb565", &MEDIASUBTYPE_RGB565, TRUE);
    run_case("rgb555", &MEDIASUBTYPE_RGB555, TRUE);
    run_case("rgb32", &MEDIASUBTYPE_RGB32, FALSE);
    top_down = TRUE;
    run_case("topdown", &MEDIASUBTYPE_RGB24, TRUE);
    run_flip_case();
    printf("done=1\n");
    return 0;
}
