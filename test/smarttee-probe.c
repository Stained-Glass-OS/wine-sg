/* smarttee-probe: the Smart Tee does not wait for itself (patches/sg/9109).
 *
 * Wine's Smart Tee copied each sample for both its pins into buffers of the source's own allocator,
 * waiting for one under the filter's lock. A source with one buffer (or two, held downstream) made
 * the streaming thread wait for itself, nothing reached the preview pin, and stopping the graph --
 * which takes the same lock -- hung for ever: OpenCV's DirectShow capture graph (the capture graph
 * builder puts a Smart Tee in for the preview it renders) froze in cv2.VideoCapture.release(). As on
 * Windows, the capture pin now passes the sample on and the preview pin gets a copy in a buffer of
 * its own when one is free. A push source with a one-buffer allocator feeds a Smart Tee whose
 * preview pin feeds a sink of the probe's own (then the capture pin, then both); frames must arrive
 * and the graph must stop.
 *
 * Prints name=value lines; see test/smarttee-gate.sh.
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
    ALLOCATOR_PROPERTIES props = {1, W * H * 4, 1, 0}, actual;
    HRESULT hr;

    if (source.peer) return VFW_E_ALREADY_CONNECTED;
    /* the one type, or nothing */
    if (mt && mt->majortype.Data1 && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB32) && !IsEqualGUID(&mt->subtype, &GUID_NULL))
        return VFW_E_TYPE_NOT_ACCEPTED;
    if (FAILED(hr = IPin_ReceiveConnection(peer, iface, &source.mt))) return VFW_E_NO_ACCEPTABLE_TYPES;
    IPin_QueryInterface(peer, &IID_IMemInputPin, (void **)&source.input);
    /* an allocator of its own with one buffer, as push sources often have */
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

/* ---- a sink of the probe's own that keeps the first frame it gets ---- */
static struct
{
    IBaseFilter IBaseFilter_iface;
    IPin IPin_iface;
    IMemInputPin IMemInputPin_iface;
    IPin *peer;
    IFilterGraph *graph;
    BYTE frame[W * H * 4];
    LONG frame_size, frames;
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
    InterlockedIncrement(&cap.frames);
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


DEFINE_GUID(p_CLSID_SmartTee, 0xcc58e280, 0x8aa1, 0x11d1, 0xb3, 0xf1, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5);

static DWORD WINAPI stop_thread(void *control)
{
    IMediaControl_Stop((IMediaControl *)control);
    return 0;
}

static void run(const char *name, BOOL capture, BOOL preview)
{
    IBaseFilter *tee;
    IMediaControl *control;
    IGraphBuilder *graph;
    IPin *in, *out_capture, *out_preview;
    HANDLE thread;
    char key[64];
    HRESULT hr;
    DWORD wait;

    top_down = FALSE;
    memset(&cap, 0, sizeof(cap));
    cap.IBaseFilter_iface.lpVtbl = &cap_filter_vtbl;
    cap.IPin_iface.lpVtbl = &cap_pin_vtbl;
    cap.IMemInputPin_iface.lpVtbl = &cap_mem_vtbl;
    cap.got = CreateEventA(NULL, TRUE, FALSE, NULL);
    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    source_init();
    IGraphBuilder_AddFilter(graph, &source.IBaseFilter_iface, L"Probe Source");
    IGraphBuilder_AddFilter(graph, &cap.IBaseFilter_iface, L"Probe Sink");
    CoCreateInstance(&p_CLSID_SmartTee, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&tee);
    IGraphBuilder_AddFilter(graph, tee, L"Smart Tee");
    IBaseFilter_FindPin(tee, L"Input", &in);
    IBaseFilter_FindPin(tee, L"Capture", &out_capture);
    IBaseFilter_FindPin(tee, L"Preview", &out_preview);
    hr = IGraphBuilder_ConnectDirect(graph, &source.IPin_iface, in, NULL);
    if (SUCCEEDED(hr)) hr = IGraphBuilder_ConnectDirect(graph, capture ? out_capture : out_preview, &cap.IPin_iface, &source.mt);
    sprintf(key, "%s_connect", name);
    say(key, SUCCEEDED(hr), "source -> Smart Tee -> %s pin -> sink: hr %#lx", capture ? "capture" : "preview", hr);
    if (SUCCEEDED(hr))
    {
        IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
        IMediaControl_Run(control);
        WaitForSingleObject(cap.got, 5000);
        {
            int i;
            for (i = 0; i < 50 && cap.frames < FRAMES; i++) Sleep(20);
        }
        sprintf(key, "%s_frames", name);
        say(key, cap.frames > 0, "%ld of %d frames reached the sink", cap.frames, FRAMES);
        if (cap.frame_size)
        {
            DWORD tl = pixel(cap.frame, &MEDIASUBTYPE_RGB32, 2, 2), bl = pixel(cap.frame, &MEDIASUBTYPE_RGB32, 2, H - 3);
            sprintf(key, "%s_data", name);
            say(key, close_colour(tl, 0xff0000) && close_colour(bl, 0x0000ff), "the frame: %06lx %06lx", tl, bl);
        }
        else
        {
            sprintf(key, "%s_data", name);
            say(key, FALSE, "no frame");
        }
        thread = CreateThread(NULL, 0, stop_thread, control, 0, NULL);
        wait = WaitForSingleObject(thread, 10000);
        sprintf(key, "%s_stop", name);
        say(key, wait == WAIT_OBJECT_0, "IMediaControl::Stop %s", wait == WAIT_OBJECT_0 ? "returned" : "HUNG (10 s)");
        if (wait != WAIT_OBJECT_0) { printf("done=1\n"); fflush(stdout); ExitProcess(0); }
        CloseHandle(thread);
        IMediaControl_Release(control);
    }
    IPin_Release(in);
    IPin_Release(out_capture);
    IPin_Release(out_preview);
    IBaseFilter_Release(tee);
    IGraphBuilder_Release(graph);
}

int main(void)
{
    CoInitialize(NULL);
    run("preview", FALSE, TRUE);
    run("capture", TRUE, FALSE);
    printf("done=1\n");
    return 0;
}
