/* Native probe for amstream.dll: the IMediaStreamFilter seeking interface,
 * IAMMultiMediaStream::GetInformation / OpenMoniker / Render, the audio and
 * DirectDraw media streams (SetSameFormat, AllocateSample, CreateSharedSample,
 * SendEndOfStream, ReceiveMultiple) and their samples (SetSampleTimes, SetRect,
 * Update with an event or an APC, CompletionStatus). A mock source filter
 * stands in for a real upstream filter. */
#define __USE_MINGW_ANSI_STDIO 1
#define CONST_VTABLE
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <amstream.h>
#include <ddraw.h>
#include <ddstream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks, last_line;
#define CHECK(cond, ...) do { last_line = __LINE__; checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_HR(got, want) CHECK((got) == (want), "%s: hr %#lx, expected %#lx", #got, (unsigned long)(got), (unsigned long)(want))

/* ---------------------------------------------------------------- mock source */

struct src
{
    IBaseFilter IBaseFilter_iface;
    IPin IPin_iface;
    IMediaSeeking IMediaSeeking_iface;
    IFilterGraph *graph;
    IPin *peer;
    AM_MEDIA_TYPE mt;
    IMemAllocator *allocator;
    LONGLONG current, stop, duration;
    DWORD caps;
};

static struct src *src_from_filter(IBaseFilter *iface) { return CONTAINING_RECORD(iface, struct src, IBaseFilter_iface); }
static struct src *src_from_pin(IPin *iface) { return CONTAINING_RECORD(iface, struct src, IPin_iface); }
static struct src *src_from_seeking(IMediaSeeking *iface) { return CONTAINING_RECORD(iface, struct src, IMediaSeeking_iface); }

/* enumerator over the single pin */
struct enum_pins { IEnumPins IEnumPins_iface; LONG ref; struct src *src; int index; };
static HRESULT WINAPI ep_QueryInterface(IEnumPins *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IEnumPins)) { *out = iface; IEnumPins_AddRef(iface); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ep_AddRef(IEnumPins *iface) { struct enum_pins *e = CONTAINING_RECORD(iface, struct enum_pins, IEnumPins_iface); return InterlockedIncrement(&e->ref); }
static ULONG WINAPI ep_Release(IEnumPins *iface)
{
    struct enum_pins *e = CONTAINING_RECORD(iface, struct enum_pins, IEnumPins_iface);
    ULONG ref = InterlockedDecrement(&e->ref);
    if (!ref) free(e);
    return ref;
}
static HRESULT WINAPI ep_Next(IEnumPins *iface, ULONG count, IPin **pins, ULONG *fetched)
{
    struct enum_pins *e = CONTAINING_RECORD(iface, struct enum_pins, IEnumPins_iface);
    ULONG n = 0;
    if (count && e->index == 0) { pins[0] = &e->src->IPin_iface; IPin_AddRef(pins[0]); e->index = 1; n = 1; }
    if (fetched) *fetched = n;
    return n == count ? S_OK : S_FALSE;
}
static HRESULT WINAPI ep_Skip(IEnumPins *iface, ULONG count) { return E_NOTIMPL; }
static HRESULT WINAPI ep_Reset(IEnumPins *iface) { struct enum_pins *e = CONTAINING_RECORD(iface, struct enum_pins, IEnumPins_iface); e->index = 0; return S_OK; }
static HRESULT WINAPI ep_Clone(IEnumPins *iface, IEnumPins **out) { return E_NOTIMPL; }
static const IEnumPinsVtbl ep_vtbl = { ep_QueryInterface, ep_AddRef, ep_Release, ep_Next, ep_Skip, ep_Reset, ep_Clone };

static HRESULT WINAPI f_QueryInterface(IBaseFilter *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPersist) || IsEqualGUID(iid, &IID_IMediaFilter)
            || IsEqualGUID(iid, &IID_IBaseFilter))
    { *out = iface; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_AddRef(IBaseFilter *iface) { return 2; }
static ULONG WINAPI f_Release(IBaseFilter *iface) { return 1; }
static HRESULT WINAPI f_GetClassID(IBaseFilter *iface, CLSID *clsid) { *clsid = GUID_NULL; return S_OK; }
static HRESULT WINAPI f_Stop(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI f_Pause(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI f_Run(IBaseFilter *iface, REFERENCE_TIME start) { return S_OK; }
static HRESULT WINAPI f_GetState(IBaseFilter *iface, DWORD ms, FILTER_STATE *state) { *state = State_Running; return S_OK; }
static HRESULT WINAPI f_SetSyncSource(IBaseFilter *iface, IReferenceClock *clock) { return S_OK; }
static HRESULT WINAPI f_GetSyncSource(IBaseFilter *iface, IReferenceClock **clock) { *clock = NULL; return S_OK; }
static HRESULT WINAPI f_EnumPins(IBaseFilter *iface, IEnumPins **out)
{
    struct enum_pins *e = calloc(1, sizeof(*e));
    e->IEnumPins_iface.lpVtbl = &ep_vtbl; e->ref = 1; e->src = src_from_filter(iface);
    *out = &e->IEnumPins_iface;
    return S_OK;
}
static HRESULT WINAPI f_FindPin(IBaseFilter *iface, const WCHAR *id, IPin **pin)
{
    *pin = &src_from_filter(iface)->IPin_iface; IPin_AddRef(*pin); return S_OK;
}
static HRESULT WINAPI f_QueryFilterInfo(IBaseFilter *iface, FILTER_INFO *info)
{
    struct src *s = src_from_filter(iface);
    wcscpy(info->achName, L"mock source");
    info->pGraph = s->graph;
    if (s->graph) IFilterGraph_AddRef(s->graph);
    return S_OK;
}
static HRESULT WINAPI f_JoinFilterGraph(IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name) { src_from_filter(iface)->graph = graph; return S_OK; }
static HRESULT WINAPI f_QueryVendorInfo(IBaseFilter *iface, WCHAR **info) { return E_NOTIMPL; }
static const IBaseFilterVtbl filter_vtbl =
{
    f_QueryInterface, f_AddRef, f_Release, f_GetClassID, f_Stop, f_Pause, f_Run, f_GetState, f_SetSyncSource,
    f_GetSyncSource, f_EnumPins, f_FindPin, f_QueryFilterInfo, f_JoinFilterGraph, f_QueryVendorInfo,
};

static HRESULT WINAPI p_QueryInterface(IPin *iface, REFIID iid, void **out)
{
    struct src *s = src_from_pin(iface);
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPin)) *out = iface;
    else if (IsEqualGUID(iid, &IID_IMediaSeeking)) *out = &s->IMediaSeeking_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    return S_OK;
}
static ULONG WINAPI p_AddRef(IPin *iface) { return 2; }
static ULONG WINAPI p_Release(IPin *iface) { return 1; }
static HRESULT WINAPI p_Connect(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct src *s = src_from_pin(iface);
    HRESULT hr = IPin_ReceiveConnection(peer, iface, mt);
    if (SUCCEEDED(hr)) { s->peer = peer; IPin_AddRef(peer); s->mt = *mt; }
    return hr;
}
static HRESULT WINAPI p_ReceiveConnection(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt) { return E_UNEXPECTED; }
static HRESULT WINAPI p_Disconnect(IPin *iface)
{
    struct src *s = src_from_pin(iface);
    if (!s->peer) return S_FALSE;
    IPin_Release(s->peer); s->peer = NULL;
    return S_OK;
}
static HRESULT WINAPI p_ConnectedTo(IPin *iface, IPin **peer)
{
    struct src *s = src_from_pin(iface);
    if (!s->peer) { *peer = NULL; return VFW_E_NOT_CONNECTED; }
    *peer = s->peer; IPin_AddRef(*peer);
    return S_OK;
}
static HRESULT WINAPI p_ConnectionMediaType(IPin *iface, AM_MEDIA_TYPE *mt) { *mt = src_from_pin(iface)->mt; return S_OK; }
static HRESULT WINAPI p_QueryPinInfo(IPin *iface, PIN_INFO *info)
{
    struct src *s = src_from_pin(iface);
    info->pFilter = &s->IBaseFilter_iface;
    IBaseFilter_AddRef(info->pFilter);
    info->dir = PINDIR_OUTPUT;
    wcscpy(info->achName, L"out");
    return S_OK;
}
static HRESULT WINAPI p_QueryDirection(IPin *iface, PIN_DIRECTION *dir) { *dir = PINDIR_OUTPUT; return S_OK; }
static HRESULT WINAPI p_QueryId(IPin *iface, WCHAR **id)
{
    *id = CoTaskMemAlloc(8); wcscpy(*id, L"out"); return S_OK;
}
static HRESULT WINAPI p_QueryAccept(IPin *iface, const AM_MEDIA_TYPE *mt) { return S_OK; }
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

static HRESULT WINAPI s_QueryInterface(IMediaSeeking *iface, REFIID iid, void **out)
{
    return IPin_QueryInterface(&src_from_seeking(iface)->IPin_iface, iid, out);
}
static ULONG WINAPI s_AddRef(IMediaSeeking *iface) { return 2; }
static ULONG WINAPI s_Release(IMediaSeeking *iface) { return 1; }
static HRESULT WINAPI s_GetCapabilities(IMediaSeeking *iface, DWORD *caps) { *caps = src_from_seeking(iface)->caps; return S_OK; }
static HRESULT WINAPI s_CheckCapabilities(IMediaSeeking *iface, DWORD *caps) { return E_NOTIMPL; }
static HRESULT WINAPI s_IsFormatSupported(IMediaSeeking *iface, const GUID *format) { return S_OK; }
static HRESULT WINAPI s_QueryPreferredFormat(IMediaSeeking *iface, GUID *format) { return E_NOTIMPL; }
static HRESULT WINAPI s_GetTimeFormat(IMediaSeeking *iface, GUID *format) { return E_NOTIMPL; }
static HRESULT WINAPI s_IsUsingTimeFormat(IMediaSeeking *iface, const GUID *format) { return E_NOTIMPL; }
static HRESULT WINAPI s_SetTimeFormat(IMediaSeeking *iface, const GUID *format) { return E_NOTIMPL; }
static HRESULT WINAPI s_GetDuration(IMediaSeeking *iface, LONGLONG *d) { *d = src_from_seeking(iface)->duration; return S_OK; }
static HRESULT WINAPI s_GetStopPosition(IMediaSeeking *iface, LONGLONG *d) { *d = src_from_seeking(iface)->stop; return S_OK; }
static HRESULT WINAPI s_GetCurrentPosition(IMediaSeeking *iface, LONGLONG *d) { *d = src_from_seeking(iface)->current; return S_OK; }
static HRESULT WINAPI s_ConvertTimeFormat(IMediaSeeking *iface, LONGLONG *t, const GUID *tf, LONGLONG s, const GUID *sf) { return E_NOTIMPL; }
static HRESULT WINAPI s_SetPositions(IMediaSeeking *iface, LONGLONG *c, DWORD cf, LONGLONG *st, DWORD sf) { return S_OK; }
static HRESULT WINAPI s_GetPositions(IMediaSeeking *iface, LONGLONG *c, LONGLONG *st) { return E_NOTIMPL; }
static HRESULT WINAPI s_GetAvailable(IMediaSeeking *iface, LONGLONG *e, LONGLONG *l) { return E_NOTIMPL; }
static HRESULT WINAPI s_SetRate(IMediaSeeking *iface, double r) { return E_NOTIMPL; }
static HRESULT WINAPI s_GetRate(IMediaSeeking *iface, double *r) { return E_NOTIMPL; }
static HRESULT WINAPI s_GetPreroll(IMediaSeeking *iface, LONGLONG *p) { return E_NOTIMPL; }
static const IMediaSeekingVtbl seeking_vtbl =
{
    s_QueryInterface, s_AddRef, s_Release, s_GetCapabilities, s_CheckCapabilities, s_IsFormatSupported,
    s_QueryPreferredFormat, s_GetTimeFormat, s_IsUsingTimeFormat, s_SetTimeFormat, s_GetDuration,
    s_GetStopPosition, s_GetCurrentPosition, s_ConvertTimeFormat, s_SetPositions, s_GetPositions,
    s_GetAvailable, s_SetRate, s_GetRate, s_GetPreroll,
};

static void src_init(struct src *s)
{
    memset(s, 0, sizeof(*s));
    s->IBaseFilter_iface.lpVtbl = &filter_vtbl;
    s->IPin_iface.lpVtbl = &pin_vtbl;
    s->IMediaSeeking_iface.lpVtbl = &seeking_vtbl;
    s->current = 12345;
    s->stop = 4000000;
    s->duration = 5000000;
    s->caps = AM_SEEKING_CanSeekAbsolute | AM_SEEKING_CanGetDuration | AM_SEEKING_CanGetCurrentPos;
    CoCreateInstance(&CLSID_MemoryAllocator, NULL, CLSCTX_INPROC_SERVER, &IID_IMemAllocator, (void **)&s->allocator);
}

static IMediaSample *get_sample(struct src *s, const BYTE *data, DWORD size)
{
    IMediaSample *sample = NULL;
    BYTE *ptr;
    HRESULT hr = IMemAllocator_GetBuffer(s->allocator, &sample, NULL, NULL, 0);
    CHECK_HR(hr, S_OK);
    IMediaSample_GetPointer(sample, &ptr);
    IMediaSample_SetActualDataLength(sample, size);
    memcpy(ptr, data, size);
    return sample;
}

static void alloc_commit(struct src *s, LONG count, LONG size)
{
    ALLOCATOR_PROPERTIES req = {count, size, 1, 0}, got;
    IMemAllocator_SetProperties(s->allocator, &req, &got);
    IMemAllocator_Commit(s->allocator);
}

/* ---------------------------------------------------------------- formats */

static WAVEFORMATEX wf_a = {WAVE_FORMAT_PCM, 1, 11025, 11025, 1, 8, 0};
static WAVEFORMATEX wf_b = {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};

static AM_MEDIA_TYPE audio_mt(WAVEFORMATEX *wf)
{
    AM_MEDIA_TYPE mt = {{0}};
    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_PCM;
    mt.formattype = FORMAT_WaveFormatEx;
    mt.cbFormat = sizeof(*wf);
    mt.pbFormat = (BYTE *)wf;
    return mt;
}

#define VW 16
#define VH 8
static VIDEOINFO rgb32_info;
static AM_MEDIA_TYPE rgb32_mt(void)
{
    AM_MEDIA_TYPE mt = {{0}};
    memset(&rgb32_info, 0, sizeof(rgb32_info));
    rgb32_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    rgb32_info.bmiHeader.biWidth = VW;
    rgb32_info.bmiHeader.biHeight = -VH;
    rgb32_info.bmiHeader.biPlanes = 1;
    rgb32_info.bmiHeader.biBitCount = 32;
    rgb32_info.bmiHeader.biCompression = BI_RGB;
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB32;
    mt.formattype = FORMAT_VideoInfo;
    mt.cbFormat = sizeof(rgb32_info);
    mt.pbFormat = (BYTE *)&rgb32_info;
    return mt;
}

static IAMMultiMediaStream *create_mm(void)
{
    IAMMultiMediaStream *mm = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_AMMultiMediaStream, NULL, CLSCTX_INPROC_SERVER, &IID_IAMMultiMediaStream, (void **)&mm);
    CHECK_HR(hr, S_OK);
    return mm;
}

/* ---------------------------------------------------------------- tests */

static void test_filter_seeking(void)
{
    IAMMultiMediaStream *mm = create_mm();
    IMediaStreamFilter *filter;
    IMediaSeeking *seeking;
    IMediaStream *stream;
    IGraphBuilder *graph;
    struct src src;
    AM_MEDIA_TYPE mt = audio_mt(&wf_a);
    LONGLONG a, b;
    DWORD caps;
    double rate;
    GUID guid;
    IPin *pin;
    HRESULT hr;

    IAMMultiMediaStream_Initialize(mm, STREAMTYPE_READ, 0, NULL);
    IAMMultiMediaStream_GetFilter(mm, &filter);
    IAMMultiMediaStream_AddMediaStream(mm, NULL, &MSPID_PrimaryAudio, 0, &stream);
    IMediaStream_QueryInterface(stream, &IID_IPin, (void **)&pin);
    IAMMultiMediaStream_GetFilterGraph(mm, &graph);

    src_init(&src);
    IGraphBuilder_AddFilter(graph, &src.IBaseFilter_iface, NULL);
    hr = IGraphBuilder_ConnectDirect(graph, &src.IPin_iface, pin, &mt);
    CHECK_HR(hr, S_OK);
    seeking = (IMediaSeeking *)0xdead;
    CHECK_HR(IMediaStreamFilter_QueryInterface(filter, &IID_IMediaSeeking, (void **)&seeking), E_NOINTERFACE);
    CHECK_HR(IMediaStreamFilter_SupportSeeking(filter, TRUE), S_OK);
    CHECK_HR(IMediaStreamFilter_QueryInterface(filter, &IID_IMediaSeeking, (void **)&seeking), S_OK);

    /* answers that do not depend on an upstream filter */
    memset(&guid, 0xcc, sizeof(guid));
    CHECK_HR(IMediaSeeking_QueryPreferredFormat(seeking, &guid), S_OK);
    CHECK(IsEqualGUID(&guid, &TIME_FORMAT_MEDIA_TIME), "preferred format");
    memset(&guid, 0xcc, sizeof(guid));
    CHECK_HR(IMediaSeeking_GetTimeFormat(seeking, &guid), S_OK);
    CHECK(IsEqualGUID(&guid, &TIME_FORMAT_MEDIA_TIME), "time format");
    CHECK_HR(IMediaSeeking_QueryPreferredFormat(seeking, NULL), E_POINTER);
    CHECK_HR(IMediaSeeking_GetTimeFormat(seeking, NULL), E_POINTER);
    CHECK_HR(IMediaSeeking_IsUsingTimeFormat(seeking, &TIME_FORMAT_MEDIA_TIME), S_OK);
    CHECK_HR(IMediaSeeking_IsUsingTimeFormat(seeking, &TIME_FORMAT_FRAME), S_FALSE);
    CHECK_HR(IMediaSeeking_SetTimeFormat(seeking, &TIME_FORMAT_MEDIA_TIME), S_OK);
    CHECK_HR(IMediaSeeking_SetTimeFormat(seeking, &TIME_FORMAT_FRAME), E_INVALIDARG);
    CHECK_HR(IMediaSeeking_SetTimeFormat(seeking, NULL), E_POINTER);
    b = 0;
    CHECK_HR(IMediaSeeking_ConvertTimeFormat(seeking, &b, NULL, 777, &TIME_FORMAT_MEDIA_TIME), S_OK);
    CHECK(b == 777, "converted time %lld", b);
    CHECK_HR(IMediaSeeking_ConvertTimeFormat(seeking, &b, &TIME_FORMAT_FRAME, 777, NULL), E_INVALIDARG);
    CHECK_HR(IMediaSeeking_ConvertTimeFormat(seeking, NULL, NULL, 777, NULL), E_POINTER);
    a = 99;
    CHECK_HR(IMediaSeeking_GetPreroll(seeking, &a), S_OK);
    CHECK(a == 0, "preroll %lld", a);
    CHECK_HR(IMediaSeeking_GetPreroll(seeking, NULL), E_POINTER);
    rate = 0.0;
    CHECK_HR(IMediaSeeking_GetRate(seeking, &rate), S_OK);
    CHECK(rate == 1.0, "default rate %f", rate);
    CHECK_HR(IMediaSeeking_SetRate(seeking, 0.0), E_INVALIDARG);
    CHECK_HR(IMediaSeeking_SetRate(seeking, -1.0), E_INVALIDARG);
    CHECK_HR(IMediaSeeking_SetRate(seeking, 2.5), S_OK);
    IMediaSeeking_GetRate(seeking, &rate);
    CHECK(rate == 2.5, "rate %f", rate);
    CHECK_HR(IMediaSeeking_GetRate(seeking, NULL), E_POINTER);
    IMediaSeeking_SetRate(seeking, 1.0);


    CHECK_HR(IMediaSeeking_GetCurrentPosition(seeking, &a), S_OK);
    CHECK(a == 12345, "current position %lld", a);
    CHECK_HR(IMediaSeeking_GetCurrentPosition(seeking, NULL), E_POINTER);
    caps = 0;
    CHECK_HR(IMediaSeeking_GetCapabilities(seeking, &caps), S_OK);
    CHECK(caps == src.caps, "capabilities %#lx", caps);
    CHECK_HR(IMediaSeeking_GetCapabilities(seeking, NULL), E_POINTER);

    caps = AM_SEEKING_CanSeekAbsolute | AM_SEEKING_CanGetDuration;
    CHECK_HR(IMediaSeeking_CheckCapabilities(seeking, &caps), S_OK);
    CHECK(caps == (AM_SEEKING_CanSeekAbsolute | AM_SEEKING_CanGetDuration), "checked caps %#lx", caps);
    caps = AM_SEEKING_CanSeekAbsolute | AM_SEEKING_CanSeekBackwards;
    CHECK_HR(IMediaSeeking_CheckCapabilities(seeking, &caps), S_FALSE);
    CHECK(caps == AM_SEEKING_CanSeekAbsolute, "partially available caps %#lx", caps);
    caps = AM_SEEKING_CanSeekBackwards;
    CHECK_HR(IMediaSeeking_CheckCapabilities(seeking, &caps), E_FAIL);
    CHECK(caps == 0, "unavailable caps %#lx", caps);
    CHECK_HR(IMediaSeeking_CheckCapabilities(seeking, NULL), E_POINTER);

    a = b = 0;
    CHECK_HR(IMediaSeeking_GetPositions(seeking, &a, &b), S_OK);
    CHECK(a == 12345 && b == 4000000, "positions %lld, %lld", a, b);
    a = b = 0;
    CHECK_HR(IMediaSeeking_GetAvailable(seeking, &a, &b), S_OK);
    CHECK(a == 0 && b == 5000000, "available %lld, %lld", a, b);

    IGraphBuilder_Disconnect(graph, pin);
    IGraphBuilder_Disconnect(graph, &src.IPin_iface);
    IGraphBuilder_RemoveFilter(graph, &src.IBaseFilter_iface);
    IMediaSeeking_Release(seeking);
    IGraphBuilder_Release(graph);
    IPin_Release(pin);
    IMediaStream_Release(stream);
    IMediaStreamFilter_Release(filter);
    IAMMultiMediaStream_Release(mm);
    if (src.allocator) IMemAllocator_Release(src.allocator);
}

static void test_multimedia(void)
{
    IAMMultiMediaStream *mm = create_mm();
    IMoniker *moniker = NULL;
    DWORD flags = 0xdead;
    STREAM_TYPE type = 77;
    HRESULT hr;

    CHECK_HR(IAMMultiMediaStream_Initialize(mm, STREAMTYPE_READ, 0, NULL), S_OK);
    CHECK_HR(IAMMultiMediaStream_GetInformation(mm, &flags, &type), S_OK);
    CHECK(flags == 0 && type == STREAMTYPE_READ, "information %#lx %d", flags, type);
    flags = 0xdead;
    CHECK_HR(IAMMultiMediaStream_GetInformation(mm, &flags, NULL), S_OK);
    CHECK(flags == 0, "flags only %#lx", flags);
    type = 77;
    CHECK_HR(IAMMultiMediaStream_GetInformation(mm, NULL, &type), S_OK);
    CHECK(type == STREAMTYPE_READ, "type only %d", type);
    CHECK_HR(IAMMultiMediaStream_GetInformation(mm, NULL, NULL), S_OK);

    CHECK_HR(IAMMultiMediaStream_OpenMoniker(mm, NULL, NULL, 0), E_POINTER);
    hr = CreateFileMoniker(L"C:\\no such directory\\no such file.avi", &moniker);
    CHECK_HR(hr, S_OK);
    hr = IAMMultiMediaStream_OpenMoniker(mm, NULL, moniker, AMMSF_NORENDER);
    CHECK(FAILED(hr) && hr != E_NOTIMPL, "OpenMoniker on a missing file gives %#lx", (unsigned long)hr);
    IMoniker_Release(moniker);

    CHECK_HR(IAMMultiMediaStream_Render(mm, AMMSF_NOCLOCK), E_UNEXPECTED);
    CHECK_HR(IAMMultiMediaStream_Render(mm, 0), E_UNEXPECTED);
    IAMMultiMediaStream_Release(mm);
}

static volatile LONG apc_calls;
static volatile ULONG_PTR apc_arg;
static VOID CALLBACK apc_func(ULONG_PTR arg) { apc_arg = arg; InterlockedIncrement(&apc_calls); }

struct feed { struct src *src; IMemInputPin *input; const BYTE *data; DWORD size; DWORD delay; };
static DWORD WINAPI feed_thread(void *arg)
{
    struct feed *f = arg;
    IMediaSample *sample;
    Sleep(f->delay);
    sample = get_sample(f->src, f->data, f->size);
    IMemInputPin_Receive(f->input, sample);
    IMediaSample_Release(sample);
    return 0;
}

static void test_audio(void)
{
    static const BYTE data[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    IAMMultiMediaStream *mm = create_mm();
    IAMMultiMediaStream *mm_a = create_mm(), *mm_b = create_mm();
    IAudioStreamSample *sample1, *sample2;
    IAudioMediaStream *audio_stream;
    IMemInputPin *input;
    IMediaStream *stream, *stream_a, *stream_b, *video_stream;
    IGraphBuilder *graph, *graph_a, *graph_b;
    IAudioData *ad1, *ad2;
    IMediaSample *media[2];
    struct src src, src_a, src_b;
    AM_MEDIA_TYPE mt = audio_mt(&wf_a), mt_a = audio_mt(&wf_a), mt_b = audio_mt(&wf_b);
    STREAM_TIME start, end, current;
    IStreamSample *ss = (IStreamSample *)0xdead;
    HANDLE event, thread;
    struct feed feed;
    LONG processed;
    BYTE *ptr;
    DWORD len;
    IPin *pin, *pin_a, *pin_b;
    HRESULT hr;

    /* ---- sample updates */
    IAMMultiMediaStream_Initialize(mm, STREAMTYPE_READ, 0, NULL);
    IAMMultiMediaStream_AddMediaStream(mm, NULL, &MSPID_PrimaryAudio, 0, &stream);
    IMediaStream_QueryInterface(stream, &IID_IAudioMediaStream, (void **)&audio_stream);
    IMediaStream_QueryInterface(stream, &IID_IPin, (void **)&pin);
    IMediaStream_QueryInterface(stream, &IID_IMemInputPin, (void **)&input);
    IAMMultiMediaStream_GetFilterGraph(mm, &graph);
    src_init(&src);
    alloc_commit(&src, 4, 64);
    IGraphBuilder_AddFilter(graph, &src.IBaseFilter_iface, NULL);
    CoCreateInstance(&CLSID_AMAudioData, NULL, CLSCTX_INPROC_SERVER, &IID_IAudioData, (void **)&ad1);
    CoCreateInstance(&CLSID_AMAudioData, NULL, CLSCTX_INPROC_SERVER, &IID_IAudioData, (void **)&ad2);
    IAudioData_SetBuffer(ad1, 6, NULL, 0);
    IAudioData_SetBuffer(ad2, 6, NULL, 0);
    CHECK_HR(IAudioMediaStream_CreateSample(audio_stream, ad1, 0, &sample1), S_OK);
    CHECK_HR(IAudioMediaStream_CreateSample(audio_stream, ad2, 0, &sample2), S_OK);
    CHECK_HR(IGraphBuilder_ConnectDirect(graph, &src.IPin_iface, pin, &mt), S_OK);
    CHECK_HR(IAMMultiMediaStream_SetState(mm, STREAMSTATE_RUN), S_OK);

    /* sample times can be set and read back */
    CHECK_HR(IAudioStreamSample_SetSampleTimes(sample1, &(STREAM_TIME){1000}, &(STREAM_TIME){2500}), S_OK);
    start = end = current = 0;
    CHECK_HR(IAudioStreamSample_GetSampleTimes(sample1, &start, &end, &current), S_OK);
    CHECK(start == 1000 && end == 2500, "sample times %lld %lld", start, end);
    CHECK_HR(IAudioStreamSample_SetSampleTimes(sample1, NULL, &(STREAM_TIME){3000}), S_OK);
    IAudioStreamSample_GetSampleTimes(sample1, &start, &end, &current);
    CHECK(start == 1000 && end == 3000, "sample times after partial set %lld %lld", start, end);

    /* event and APC cannot be combined */
    event = CreateEventW(NULL, FALSE, FALSE, NULL);
    CHECK_HR(IAudioStreamSample_Update(sample1, 0, event, apc_func, 1), E_INVALIDARG);

    /* update with an event: pending until the data arrives */
    CHECK_HR(IAudioStreamSample_Update(sample1, 0, event, NULL, 0), MS_S_PENDING);
    CHECK(WaitForSingleObject(event, 0) == WAIT_TIMEOUT, "event signalled early");
    media[0] = get_sample(&src, data, 6);
    CHECK_HR(IMemInputPin_Receive(input, media[0]), S_OK);
    IMediaSample_Release(media[0]);
    CHECK(WaitForSingleObject(event, 1000) == WAIT_OBJECT_0, "event not signalled");
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, 0, 0), S_OK);
    IAudioData_GetInfo(ad1, &len, &ptr, NULL);
    CHECK(!memcmp(ptr, data, 6), "update data");

    /* update with an APC: runs in this thread when it waits alertably */
    apc_calls = 0; apc_arg = 0;
    CHECK_HR(IAudioStreamSample_Update(sample1, 0, NULL, apc_func, 0x1234), MS_S_PENDING);
    SleepEx(0, TRUE);
    CHECK(apc_calls == 0, "APC ran before the data arrived");
    media[0] = get_sample(&src, data + 6, 6);
    CHECK_HR(IMemInputPin_Receive(input, media[0]), S_OK);
    IMediaSample_Release(media[0]);
    SleepEx(0, TRUE);
    CHECK(apc_calls == 1 && apc_arg == 0x1234, "APC calls %ld arg %#llx", apc_calls, (unsigned long long)apc_arg);
    SleepEx(0, TRUE);
    CHECK(apc_calls == 1, "APC ran twice");
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, 0, 0), S_OK);
    IAudioData_GetInfo(ad1, &len, &ptr, NULL);
    CHECK(!memcmp(ptr, data + 6, 6), "update data for the APC update");

    /* abort a pending update */
    CHECK_HR(IAudioStreamSample_Update(sample1, SSUPDATE_ASYNC, NULL, NULL, 0), MS_S_PENDING);
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, 0, 0), MS_S_PENDING);
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, COMPSTAT_ABORT, 0), MS_S_NOUPDATE);
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, 0, 0), MS_S_NOUPDATE);
    CHECK_HR(IAudioStreamSample_Update(sample1, SSUPDATE_ASYNC, NULL, NULL, 0), MS_S_PENDING);
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, COMPSTAT_NOUPDATEOK, 0), MS_S_NOUPDATE);

    /* wait for a pending update */
    CHECK_HR(IAudioStreamSample_Update(sample1, SSUPDATE_ASYNC, NULL, NULL, 0), MS_S_PENDING);
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, COMPSTAT_WAIT, 50), MS_S_PENDING);
    feed.src = &src; feed.input = input; feed.data = data; feed.size = 6; feed.delay = 150;
    thread = CreateThread(NULL, 0, feed_thread, &feed, 0, NULL);
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, COMPSTAT_WAIT, 5000), S_OK);
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);

    /* ReceiveMultiple delivers every sample it is given */
    CHECK_HR(IAudioStreamSample_Update(sample1, SSUPDATE_ASYNC, NULL, NULL, 0), MS_S_PENDING);
    media[0] = get_sample(&src, data, 3);
    media[1] = get_sample(&src, data + 3, 3);
    CHECK_HR(IMemInputPin_ReceiveMultiple(input, NULL, 2, &processed), E_POINTER);
    CHECK_HR(IMemInputPin_ReceiveMultiple(input, media, 2, NULL), E_POINTER);
    processed = 0;
    CHECK_HR(IMemInputPin_ReceiveMultiple(input, media, 2, &processed), S_OK);
    CHECK(processed == 2, "processed %ld", processed);
    IMediaSample_Release(media[0]);
    IMediaSample_Release(media[1]);
    CHECK_HR(IAudioStreamSample_CompletionStatus(sample1, 0, 0), S_OK);
    IAudioData_GetInfo(ad1, &len, &ptr, NULL);
    CHECK(!memcmp(ptr, data, 6), "ReceiveMultiple data");

    /* IMediaStream members that have no meaning for this stream */
    CHECK_HR(IMediaStream_AllocateSample(stream, 0, &ss), E_NOTIMPL);
    CHECK(ss == NULL, "AllocateSample output");
    ss = (IStreamSample *)0xdead;
    CHECK_HR(IMediaStream_CreateSharedSample(stream, (IStreamSample *)sample1, 0, &ss), E_NOTIMPL);
    CHECK(ss == NULL, "CreateSharedSample output");
    CHECK_HR(IMediaStream_AllocateSample(stream, 0, NULL), E_POINTER);
    CHECK_HR(IMediaStream_SendEndOfStream(stream, 0), MS_E_INVALIDSTREAMTYPE);

    IAMMultiMediaStream_SetState(mm, STREAMSTATE_STOP);
    IGraphBuilder_Disconnect(graph, pin);
    IGraphBuilder_Disconnect(graph, &src.IPin_iface);
    CloseHandle(event);

    /* ---- SetSameFormat copies the format of another audio stream */
    IAMMultiMediaStream_Initialize(mm_a, STREAMTYPE_READ, 0, NULL);
    IAMMultiMediaStream_Initialize(mm_b, STREAMTYPE_READ, 0, NULL);
    IAMMultiMediaStream_AddMediaStream(mm_a, NULL, &MSPID_PrimaryAudio, 0, &stream_a);
    IAMMultiMediaStream_AddMediaStream(mm_b, NULL, &MSPID_PrimaryAudio, 0, &stream_b);
    IMediaStream_QueryInterface(stream_a, &IID_IPin, (void **)&pin_a);
    IMediaStream_QueryInterface(stream_b, &IID_IPin, (void **)&pin_b);
    IAMMultiMediaStream_GetFilterGraph(mm_a, &graph_a);
    IAMMultiMediaStream_GetFilterGraph(mm_b, &graph_b);
    src_init(&src_a);
    src_init(&src_b);
    IGraphBuilder_AddFilter(graph_a, &src_a.IBaseFilter_iface, NULL);
    IGraphBuilder_AddFilter(graph_b, &src_b.IBaseFilter_iface, NULL);

    CHECK_HR(IMediaStream_SetSameFormat(stream_a, NULL, 0), E_POINTER);
    CHECK_HR(IMediaStream_SetSameFormat(stream_a, stream_b, 0), MS_E_NOSTREAM);
    {
        IAMMultiMediaStream *mmv = create_mm();
        IAMMultiMediaStream_Initialize(mmv, STREAMTYPE_READ, 0, NULL);
        IAMMultiMediaStream_AddMediaStream(mmv, NULL, &MSPID_PrimaryVideo, 0, &video_stream);
        CHECK_HR(IMediaStream_SetSameFormat(stream_a, video_stream, 0), MS_E_INCOMPATIBLE);
        IMediaStream_Release(video_stream);
        IAMMultiMediaStream_Release(mmv);
    }
    CHECK_HR(IGraphBuilder_ConnectDirect(graph_b, &src_b.IPin_iface, pin_b, &mt_b), S_OK);
    CHECK_HR(IMediaStream_SetSameFormat(stream_a, stream_b, 0), S_OK);
    hr = IGraphBuilder_ConnectDirect(graph_a, &src_a.IPin_iface, pin_a, &mt_a);
    CHECK(FAILED(hr), "a connection with another format is accepted: %#lx", (unsigned long)hr);
    hr = IGraphBuilder_ConnectDirect(graph_a, &src_a.IPin_iface, pin_a, &mt_b);
    CHECK_HR(hr, S_OK);
    IGraphBuilder_Disconnect(graph_a, pin_a);
    IGraphBuilder_Disconnect(graph_a, &src_a.IPin_iface);
    IGraphBuilder_Disconnect(graph_b, pin_b);
    IGraphBuilder_Disconnect(graph_b, &src_b.IPin_iface);

    IPin_Release(pin_a); IPin_Release(pin_b);
    IGraphBuilder_Release(graph_a); IGraphBuilder_Release(graph_b);
    IMediaStream_Release(stream_a); IMediaStream_Release(stream_b);
    IAMMultiMediaStream_Release(mm_a); IAMMultiMediaStream_Release(mm_b);
    IAudioStreamSample_Release(sample1);
    IAudioStreamSample_Release(sample2);
    IAudioData_Release(ad1);
    IAudioData_Release(ad2);
    IMemInputPin_Release(input);
    IPin_Release(pin);
    IGraphBuilder_Release(graph);
    IAudioMediaStream_Release(audio_stream);
    IMediaStream_Release(stream);
    IAMMultiMediaStream_Release(mm);
}

static void test_ddraw(void)
{
    IAMMultiMediaStream *mm = create_mm(), *mm_a = create_mm(), *mm_b = create_mm();
    IDirectDrawMediaStream *dds, *dds_a, *dds_b;
    IDirectDrawStreamSample *sample1, *sample2;
    IMediaStream *stream, *stream_a, *stream_b, *audio;
    IGraphBuilder *graph, *graph_b;
    IDirectDrawSurface *surface;
    IMemInputPin *input;
    struct src src, src_b;
    AM_MEDIA_TYPE mt = rgb32_mt();
    STREAM_TIME start, end, current;
    DDSURFACEDESC desc, cur;
    IDirectDraw *ddraw;
    IMediaSample *media[2];
    IStreamSample *ss = (IStreamSample *)0xdead;
    RECT rect;
    IPin *pin, *pin_b;
    BYTE frame[VW * VH * 4];
    HRESULT hr;

    hr = DirectDrawCreate(NULL, &ddraw, NULL);
    if (FAILED(hr))
    {
        printf("SKIP ddraw not available (%#lx)\n", (unsigned long)hr);
        return;
    }
    IDirectDraw_SetCooperativeLevel(ddraw, NULL, DDSCL_NORMAL);

    IAMMultiMediaStream_Initialize(mm, STREAMTYPE_READ, 0, NULL);
    IAMMultiMediaStream_AddMediaStream(mm, (IUnknown *)ddraw, &MSPID_PrimaryVideo, 0, &stream);
    IMediaStream_QueryInterface(stream, &IID_IDirectDrawMediaStream, (void **)&dds);
    IMediaStream_QueryInterface(stream, &IID_IPin, (void **)&pin);
    IMediaStream_QueryInterface(stream, &IID_IMemInputPin, (void **)&input);
    IAMMultiMediaStream_GetFilterGraph(mm, &graph);
    src_init(&src);
    alloc_commit(&src, 4, sizeof(frame));
    IGraphBuilder_AddFilter(graph, &src.IBaseFilter_iface, NULL);

    /* ---- stream members without a meaning here */
    CHECK_HR(IMediaStream_AllocateSample(stream, 0, &ss), E_NOTIMPL);
    CHECK(ss == NULL, "AllocateSample output");
    ss = (IStreamSample *)0xdead;
    CHECK_HR(IMediaStream_CreateSharedSample(stream, NULL, 0, &ss), E_NOTIMPL);
    CHECK(ss == NULL, "CreateSharedSample output");
    CHECK_HR(IMediaStream_SendEndOfStream(stream, 0), MS_E_INVALIDSTREAMTYPE);

    /* ---- Update with an APC and ReceiveMultiple, on a connected stream */
    CHECK_HR(IGraphBuilder_ConnectDirect(graph, &src.IPin_iface, pin, &mt), S_OK);
    CHECK_HR(IDirectDrawMediaStream_CreateSample(dds, NULL, NULL, 0, &sample1), S_OK);
    CHECK_HR(IDirectDrawMediaStream_CreateSample(dds, NULL, NULL, 0, &sample2), S_OK);
    CHECK_HR(IAMMultiMediaStream_SetState(mm, STREAMSTATE_RUN), S_OK);

    CHECK_HR(IDirectDrawStreamSample_Update(sample1, 0, CreateEventW(NULL, 0, 0, NULL), apc_func, 1), E_INVALIDARG);
    apc_calls = 0; apc_arg = 0;
    CHECK_HR(IDirectDrawStreamSample_Update(sample1, 0, NULL, apc_func, 0x4321), MS_S_PENDING);
    SleepEx(0, TRUE);
    CHECK(apc_calls == 0, "APC ran too early");
    memset(frame, 0x11, sizeof(frame));
    media[0] = get_sample(&src, frame, sizeof(frame));
    CHECK_HR(IMemInputPin_Receive(input, media[0]), S_OK);
    IMediaSample_Release(media[0]);
    SleepEx(0, TRUE);
    CHECK(apc_calls == 1 && apc_arg == 0x4321, "APC calls %ld arg %#llx", apc_calls, (unsigned long long)apc_arg);
    CHECK_HR(IDirectDrawStreamSample_CompletionStatus(sample1, 0, 0), S_OK);

    CHECK_HR(IDirectDrawStreamSample_Update(sample1, SSUPDATE_ASYNC, NULL, NULL, 0), MS_S_PENDING);
    CHECK_HR(IDirectDrawStreamSample_Update(sample2, SSUPDATE_ASYNC, NULL, NULL, 0), MS_S_PENDING);
    memset(frame, 0x22, sizeof(frame));
    media[0] = get_sample(&src, frame, sizeof(frame));
    memset(frame, 0x33, sizeof(frame));
    media[1] = get_sample(&src, frame, sizeof(frame));
    {
        LONG processed = 0;
        CHECK_HR(IMemInputPin_ReceiveMultiple(input, NULL, 2, &processed), E_POINTER);
        CHECK_HR(IMemInputPin_ReceiveMultiple(input, media, 2, &processed), S_OK);
        CHECK(processed == 2, "processed %ld", processed);
    }
    IMediaSample_Release(media[0]);
    IMediaSample_Release(media[1]);
    CHECK_HR(IDirectDrawStreamSample_CompletionStatus(sample1, 0, 0), S_OK);
    CHECK_HR(IDirectDrawStreamSample_CompletionStatus(sample2, 0, 0), S_OK);
    {
        IDirectDrawStreamSample *samples[2] = {sample1, sample2};
        BYTE want[2] = {0x22, 0x33};
        int i;
        for (i = 0; i < 2; i++)
        {
            surface = NULL;
            IDirectDrawStreamSample_GetSurface(samples[i], &surface, NULL);
            memset(&desc, 0, sizeof(desc));
            desc.dwSize = sizeof(desc);
            if (surface && SUCCEEDED(IDirectDrawSurface_Lock(surface, NULL, &desc, DDLOCK_WAIT, NULL)))
            {
                CHECK(((BYTE *)desc.lpSurface)[0] == want[i], "surface %d holds %02x", i, ((BYTE *)desc.lpSurface)[0]);
                IDirectDrawSurface_Unlock(surface, desc.lpSurface);
            }
            else
                CHECK(0, "cannot lock surface %d", i);
            if (surface) IDirectDrawSurface_Release(surface);
        }
    }
    IAMMultiMediaStream_SetState(mm, STREAMSTATE_STOP);
    IGraphBuilder_Disconnect(graph, pin);
    IGraphBuilder_Disconnect(graph, &src.IPin_iface);

    /* ---- sample rectangle */
    IDirectDrawStreamSample_Release(sample1);
    CHECK_HR(IDirectDrawMediaStream_CreateSample(dds, NULL, NULL, 0, &sample1), S_OK);
    CHECK_HR(IDirectDrawStreamSample_SetRect(sample1, NULL), S_OK);
    SetRect(&rect, 2, 1, 10, 6);
    CHECK_HR(IDirectDrawStreamSample_SetRect(sample1, &rect), S_OK);
    {
        RECT got = {0};
        CHECK_HR(IDirectDrawStreamSample_GetSurface(sample1, NULL, &got), S_OK);
        CHECK(EqualRect(&got, &rect), "rect %ld %ld %ld %ld", got.left, got.top, got.right, got.bottom);
    }
    SetRect(&rect, 0, 0, 17, 4);
    CHECK_HR(IDirectDrawStreamSample_SetRect(sample1, &rect), E_INVALIDARG);
    SetRect(&rect, 4, 4, 4, 6);
    CHECK_HR(IDirectDrawStreamSample_SetRect(sample1, &rect), E_INVALIDARG);
    SetRect(&rect, -1, 0, 5, 5);
    CHECK_HR(IDirectDrawStreamSample_SetRect(sample1, &rect), E_INVALIDARG);
    SetRect(&rect, 8, 6, 4, 7);
    CHECK_HR(IDirectDrawStreamSample_SetRect(sample1, &rect), E_INVALIDARG);
    IDirectDrawStreamSample_Release(sample1);

    /* ---- sample times */
    CHECK_HR(IDirectDrawMediaStream_CreateSample(dds, NULL, NULL, 0, &sample1), S_OK);
    CHECK_HR(IDirectDrawStreamSample_SetSampleTimes(sample1, &(STREAM_TIME){500}, &(STREAM_TIME){900}), S_OK);
    start = end = current = 0;
    CHECK_HR(IDirectDrawStreamSample_GetSampleTimes(sample1, &start, &end, &current), S_OK);
    CHECK(start == 500 && end == 900, "sample times %lld %lld", start, end);
    CHECK_HR(IDirectDrawStreamSample_SetSampleTimes(sample1, &(STREAM_TIME){700}, NULL), S_OK);
    IDirectDrawStreamSample_GetSampleTimes(sample1, &start, &end, &current);
    CHECK(start == 700 && end == 900, "sample times after partial set %lld %lld", start, end);
    IDirectDrawStreamSample_Release(sample1);

    /* ---- SetSameFormat: a new stream takes over the size of a connected one */
    IAMMultiMediaStream_Initialize(mm_a, STREAMTYPE_READ, 0, NULL);
    IAMMultiMediaStream_Initialize(mm_b, STREAMTYPE_READ, 0, NULL);
    IAMMultiMediaStream_AddMediaStream(mm_a, (IUnknown *)ddraw, &MSPID_PrimaryVideo, 0, &stream_a);
    IAMMultiMediaStream_AddMediaStream(mm_b, (IUnknown *)ddraw, &MSPID_PrimaryVideo, 0, &stream_b);
    IMediaStream_QueryInterface(stream_a, &IID_IDirectDrawMediaStream, (void **)&dds_a);
    IMediaStream_QueryInterface(stream_b, &IID_IDirectDrawMediaStream, (void **)&dds_b);
    IMediaStream_QueryInterface(stream_b, &IID_IPin, (void **)&pin_b);
    IAMMultiMediaStream_GetFilterGraph(mm_b, &graph_b);
    src_init(&src_b);
    IGraphBuilder_AddFilter(graph_b, &src_b.IBaseFilter_iface, NULL);

    CHECK_HR(IMediaStream_SetSameFormat(stream_a, NULL, 0), E_POINTER);
    CHECK_HR(IMediaStream_SetSameFormat(stream_a, stream_b, 0), MS_E_NOSTREAM);
    {
        IAMMultiMediaStream *mma = create_mm();
        IAMMultiMediaStream_Initialize(mma, STREAMTYPE_READ, 0, NULL);
        IAMMultiMediaStream_AddMediaStream(mma, NULL, &MSPID_PrimaryAudio, 0, &audio);
        CHECK_HR(IMediaStream_SetSameFormat(stream_a, audio, 0), MS_E_INCOMPATIBLE);
        IMediaStream_Release(audio);
        IAMMultiMediaStream_Release(mma);
    }
    CHECK_HR(IGraphBuilder_ConnectDirect(graph_b, &src_b.IPin_iface, pin_b, &mt), S_OK);
    CHECK_HR(IMediaStream_SetSameFormat(stream_a, stream_b, 0), S_OK);
    CHECK_HR(IDirectDrawMediaStream_CreateSample(dds_a, NULL, NULL, 0, &sample1), S_OK);
    {
        IDirectDrawSurface *s = NULL;
        DDSURFACEDESC d = {0};
        d.dwSize = sizeof(d);
        IDirectDrawStreamSample_GetSurface(sample1, &s, NULL);
        if (s)
        {
            IDirectDrawSurface_GetSurfaceDesc(s, &d);
            CHECK(d.dwWidth == VW && d.dwHeight == VH, "sample surface %lux%lu after SetSameFormat", d.dwWidth, d.dwHeight);
            IDirectDrawSurface_Release(s);
        }
        else
            CHECK(0, "no surface");
    }
    memset(&cur, 0, sizeof(cur));
    cur.dwSize = sizeof(cur);
    (void)cur;
    IDirectDrawStreamSample_Release(sample1);
    IGraphBuilder_Disconnect(graph_b, pin_b);
    IGraphBuilder_Disconnect(graph_b, &src_b.IPin_iface);

    IDirectDrawStreamSample_Release(sample2);
    IGraphBuilder_Release(graph_b);
    IPin_Release(pin_b);
    IDirectDrawMediaStream_Release(dds_a);
    IDirectDrawMediaStream_Release(dds_b);
    IMediaStream_Release(stream_a);
    IMediaStream_Release(stream_b);
    IAMMultiMediaStream_Release(mm_a);
    IAMMultiMediaStream_Release(mm_b);
    IMemInputPin_Release(input);
    IPin_Release(pin);
    IGraphBuilder_Release(graph);
    IDirectDrawMediaStream_Release(dds);
    IMediaStream_Release(stream);
    IAMMultiMediaStream_Release(mm);
    IDirectDraw_Release(ddraw);
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *info)
{
    printf("FAIL  crashed after the check on line %d\n", last_line);
    printf("RESULT: FAIL\n");
    ExitProcess(1);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(void)
{
    SetUnhandledExceptionFilter(crash_filter);
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    printf("# seeking\n"); test_filter_seeking();
    printf("# multimedia\n"); test_multimedia();
    printf("# audio\n"); test_audio();
    printf("# ddraw\n"); test_ddraw();
    CoUninitialize();
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
