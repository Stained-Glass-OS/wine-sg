/* mf leftovers (patches/sg/2873), run by test/mfrest2-gate.sh: the audio
 * renderer's clock check, IMFRateSupport and stream IMFGetService, the cached
 * activation object after ShutdownObject (audio renderer) versus the sample
 * grabber's MF_E_SHUTDOWN, Stop/Pause of a session without topology and the
 * topology loader's error order. The audio part needs a render device. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
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

static HRESULT WINAPI cb_qi(IMFSampleGrabberSinkCallback *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMFSampleGrabberSinkCallback))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cb_addref(IMFSampleGrabberSinkCallback *iface) { return 2; }
static ULONG WINAPI cb_release(IMFSampleGrabberSinkCallback *iface) { return 1; }
static HRESULT WINAPI cb_clock_start(IMFSampleGrabberSinkCallback *iface, MFTIME t, LONGLONG o) { return S_OK; }
static HRESULT WINAPI cb_clock_stop(IMFSampleGrabberSinkCallback *iface, MFTIME t) { return S_OK; }
static HRESULT WINAPI cb_clock_pause(IMFSampleGrabberSinkCallback *iface, MFTIME t) { return S_OK; }
static HRESULT WINAPI cb_clock_restart(IMFSampleGrabberSinkCallback *iface, MFTIME t) { return S_OK; }
static HRESULT WINAPI cb_clock_rate(IMFSampleGrabberSinkCallback *iface, MFTIME t, float r) { return S_OK; }
static HRESULT WINAPI cb_setclock(IMFSampleGrabberSinkCallback *iface, IMFPresentationClock *c) { return S_OK; }
static HRESULT WINAPI cb_sample(IMFSampleGrabberSinkCallback *iface, REFGUID id, DWORD f, LONGLONG t, LONGLONG d,
        const BYTE *b, DWORD n) { return S_OK; }
static HRESULT WINAPI cb_shutdown(IMFSampleGrabberSinkCallback *iface) { return S_OK; }
static IMFSampleGrabberSinkCallbackVtbl cb_vtbl =
{
    cb_qi, cb_addref, cb_release, cb_clock_start, cb_clock_stop, cb_clock_pause, cb_clock_restart, cb_clock_rate,
    cb_setclock, cb_sample, cb_shutdown,
};
static IMFSampleGrabberSinkCallback grabber_cb = { &cb_vtbl };

static void test_sar(void)
{
    IMFPresentationClock *clock, *clock2;
    IMFPresentationTimeSource *source;
    IMFMediaTypeHandler *handler;
    IMFMediaType *type, *type2;
    IMFRateSupport *rs, *rs2;
    IMFStreamSink *stream;
    IMFGetService *gs;
    IMFMediaSink *sink, *sink2;
    IMFActivate *activate;
    float rate, nearest;
    DWORD flags;
    HRESULT hr;
    int i;

    hr = MFCreateAudioRenderer(NULL, &sink);
    if (FAILED(hr))
    {
        printf("NOTE  no audio render device (hr %#lx), audio renderer checks skipped\n", hr);
        return;
    }

    hr = MFCreatePresentationClock(&clock);
    CHECKF(hr == S_OK, "clock create %#lx", hr);
    hr = IMFMediaSink_SetPresentationClock(sink, clock);
    CHECKF(hr == MF_E_CLOCK_NO_TIME_SOURCE, "SetPresentationClock without time source: %#lx", hr);
    hr = IMFMediaSink_GetPresentationClock(sink, &clock2);
    CHECKF(hr == MF_E_NO_CLOCK, "clock not kept after failure: %#lx", hr);
    hr = IMFMediaSink_SetPresentationClock(sink, NULL);
    CHECKF(hr == S_OK, "SetPresentationClock NULL %#lx", hr);
    hr = MFCreateSystemTimeSource(&source);
    hr = IMFPresentationClock_SetTimeSource(clock, source);
    IMFPresentationTimeSource_Release(source);
    hr = IMFMediaSink_SetPresentationClock(sink, clock);
    CHECKF(hr == S_OK, "SetPresentationClock with time source %#lx", hr);
    hr = IMFMediaSink_GetPresentationClock(sink, &clock2);
    CHECKF(hr == S_OK && clock2 == clock, "clock kept %#lx", hr);
    if (SUCCEEDED(hr)) IMFPresentationClock_Release(clock2);
    hr = IMFMediaSink_SetPresentationClock(sink, NULL);
    IMFPresentationClock_Release(clock);

    /* rate support */
    hr = IMFMediaSink_QueryInterface(sink, &IID_IMFRateSupport, (void **)&rs);
    CHECKF(hr == S_OK, "sink IMFRateSupport %#lx", hr);
    hr = IMFMediaSink_QueryInterface(sink, &IID_IMFGetService, (void **)&gs);
    CHECKF(hr == S_OK, "sink IMFGetService %#lx", hr);
    hr = IMFGetService_GetService(gs, &MF_RATE_CONTROL_SERVICE, &IID_IMFRateSupport, (void **)&rs2);
    CHECKF(hr == S_OK && rs2 == rs, "rate control service gives IMFRateSupport %#lx", hr);
    if (SUCCEEDED(hr)) IMFRateSupport_Release(rs2);
    hr = IMFGetService_GetService(gs, &MF_RATE_CONTROL_SERVICE, &IID_IMFRateControl, (void **)&rs2);
    CHECKF(hr == E_NOINTERFACE, "no IMFRateControl %#lx", hr);
    IMFGetService_Release(gs);

    hr = IMFMediaSink_GetStreamSinkByIndex(sink, 0, &stream);
    CHECKF(hr == S_OK, "stream %#lx", hr);
    hr = IMFStreamSink_QueryInterface(stream, &IID_IMFGetService, (void **)&gs);
    CHECKF(hr == S_OK, "stream IMFGetService %#lx", hr);
    hr = IMFGetService_GetService(gs, &MF_RATE_CONTROL_SERVICE, &IID_IMFRateSupport, (void **)&rs2);
    CHECKF(hr == S_OK, "stream rate service %#lx", hr);
    if (SUCCEEDED(hr)) IMFRateSupport_Release(rs2);
    IMFGetService_Release(gs);

    hr = IMFRateSupport_GetSlowestRate(rs, MFRATE_FORWARD, FALSE, NULL);
    CHECKF(hr == E_POINTER, "slowest NULL %#lx", hr);
    rate = 5.0f;
    hr = IMFRateSupport_GetSlowestRate(rs, MFRATE_FORWARD, FALSE, &rate);
    CHECKF(hr == MF_E_NOT_INITIALIZED, "slowest without type %#lx", hr);
    hr = IMFRateSupport_GetFastestRate(rs, MFRATE_REVERSE, TRUE, &rate);
    CHECKF(hr == MF_E_NOT_INITIALIZED, "fastest without type %#lx", hr);

    hr = IMFStreamSink_GetMediaTypeHandler(stream, &handler);
    hr = IMFMediaTypeHandler_GetMediaTypeByIndex(handler, 0, &type);
    CHECKF(hr == S_OK, "supported type %#lx", hr);
    if (SUCCEEDED(hr))
    {
        hr = IMFMediaTypeHandler_SetCurrentMediaType(handler, type);
        CHECKF(hr == S_OK, "set type %#lx", hr);
        for (i = 0; i < 4; ++i)
        {
            MFRATE_DIRECTION dir = i & 1 ? MFRATE_REVERSE : MFRATE_FORWARD;
            BOOL thin = i >> 1;
            rate = 9.0f;
            hr = IMFRateSupport_GetSlowestRate(rs, dir, thin, &rate);
            CHECKF(hr == S_OK && rate == 0.0f, "slowest dir %d thin %d = %f (%#lx)", dir, thin, rate, hr);
            rate = 9.0f;
            hr = IMFRateSupport_GetFastestRate(rs, dir, thin, &rate);
            CHECKF(hr == S_OK && rate == (dir == MFRATE_REVERSE ? -2.0f : 2.0f), "fastest dir %d thin %d = %f (%#lx)",
                    dir, thin, rate, hr);
        }
        hr = IMFRateSupport_IsRateSupported(rs, FALSE, 1.0f, &nearest);
        CHECKF(hr == S_OK && nearest == 1.0f, "rate 1.0 supported %#lx %f", hr, nearest);
        hr = IMFRateSupport_IsRateSupported(rs, FALSE, 3.0f, &nearest);
        CHECKF(hr == MF_E_UNSUPPORTED_RATE && nearest == 2.0f, "rate 3.0 unsupported, nearest %f (%#lx)", nearest, hr);
        IMFMediaType_Release(type);
    }
    IMFMediaTypeHandler_Release(handler);
    IMFStreamSink_Release(stream);

    hr = IMFMediaSink_Shutdown(sink);
    hr = IMFRateSupport_GetSlowestRate(rs, MFRATE_FORWARD, FALSE, NULL);
    CHECKF(hr == E_POINTER, "after shutdown slowest NULL %#lx", hr);
    hr = IMFRateSupport_GetSlowestRate(rs, MFRATE_FORWARD, FALSE, &rate);
    CHECKF(hr == MF_E_SHUTDOWN, "after shutdown slowest %#lx", hr);
    hr = IMFRateSupport_GetFastestRate(rs, MFRATE_FORWARD, FALSE, &rate);
    CHECKF(hr == MF_E_SHUTDOWN, "after shutdown fastest %#lx", hr);
    IMFRateSupport_Release(rs);
    IMFMediaSink_Release(sink);

    /* activation keeps the shut down object */
    hr = MFCreateAudioRendererActivate(&activate);
    hr = IMFActivate_ActivateObject(activate, &IID_IMFMediaSink, (void **)&sink);
    CHECKF(hr == S_OK, "activate %#lx", hr);
    hr = IMFActivate_ActivateObject(activate, &IID_IMFMediaSink, (void **)&sink2);
    CHECKF(hr == S_OK && sink == sink2, "same instance twice");
    IMFMediaSink_Release(sink2);
    hr = IMFActivate_ShutdownObject(activate);
    CHECKF(hr == S_OK, "ShutdownObject %#lx", hr);
    hr = IMFMediaSink_GetCharacteristics(sink, &flags);
    CHECKF(hr == MF_E_SHUTDOWN, "sink shut down %#lx", hr);
    hr = IMFActivate_ActivateObject(activate, &IID_IMFMediaSink, (void **)&sink2);
    CHECKF(hr == S_OK && sink == sink2, "activate again returns the shut down sink %#lx", hr);
    if (SUCCEEDED(hr)) IMFMediaSink_Release(sink2);
    hr = IMFMediaSink_GetCharacteristics(sink, &flags);
    CHECKF(hr == MF_E_SHUTDOWN, "still shut down %#lx", hr);
    hr = IMFActivate_DetachObject(activate);
    CHECKF(hr == E_NOTIMPL, "DetachObject stays E_NOTIMPL %#lx", hr);
    IMFActivate_Release(activate);
    IMFMediaSink_Release(sink);
}

static HRESULT wait_event(IMFMediaSession *session, MediaEventType type, HRESULT *status)
{
    IMFMediaEvent *event;
    MediaEventType t;
    int i;

    for (i = 0; i < 200; ++i)
    {
        if (IMFMediaSession_GetEvent(session, MF_EVENT_FLAG_NO_WAIT, &event) == S_OK)
        {
            IMFMediaEvent_GetType(event, &t);
            if (t == type)
            {
                IMFMediaEvent_GetStatus(event, status);
                IMFMediaEvent_Release(event);
                return S_OK;
            }
            IMFMediaEvent_Release(event);
        }
        else
            Sleep(10);
    }
    return E_FAIL;
}

static void test_session(void)
{
    IMFMediaSession *session;
    PROPVARIANT var;
    HRESULT hr, status;

    hr = MFCreateMediaSession(NULL, &session);
    CHECKF(hr == S_OK, "session %#lx", hr);
    var.vt = VT_EMPTY;
    hr = IMFMediaSession_Start(session, &GUID_NULL, &var);
    status = 0;
    CHECKF(hr == S_OK && wait_event(session, MESessionStarted, &status) == S_OK && status == MF_E_INVALIDREQUEST,
            "Start without topology event status %#lx", status);
    hr = IMFMediaSession_Stop(session);
    status = 0;
    CHECKF(hr == S_OK && wait_event(session, MESessionStopped, &status) == S_OK && status == MF_E_INVALIDREQUEST,
            "Stop without topology event status %#lx", status);
    hr = IMFMediaSession_Pause(session);
    status = 0;
    CHECKF(hr == S_OK && wait_event(session, MESessionPaused, &status) == S_OK && status == MF_E_INVALIDREQUEST,
            "Pause without topology event status %#lx", status);
    IMFMediaSession_Shutdown(session);
    IMFMediaSession_Release(session);
}

static void test_sample_grabber_activate(void)
{
    IMFSampleGrabberSinkCallback *callback = &grabber_cb;
    IMFActivate *activate;
    IMFMediaSink *sink;
    IMFMediaType *type;
    HRESULT hr;

    /* the grabber callback is only needed by the activate object, never called here */
    hr = MFCreateMediaType(&type);
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    hr = MFCreateSampleGrabberSinkActivate(type, callback, &activate);
    if (FAILED(hr)) { CHECKF(0, "grabber activate %#lx", hr); IMFMediaType_Release(type); return; }
    hr = IMFActivate_ActivateObject(activate, &IID_IMFMediaSink, (void **)&sink);
    CHECKF(hr == S_OK, "grabber activate object %#lx", hr);
    hr = IMFActivate_ShutdownObject(activate);
    CHECKF(hr == S_OK, "grabber ShutdownObject %#lx", hr);
    IMFMediaSink_Shutdown(sink);
    IMFMediaSink_Release(sink);
    hr = IMFActivate_ActivateObject(activate, &IID_IMFMediaSink, (void **)&sink);
    CHECKF(hr == MF_E_SHUTDOWN, "grabber activate after shutdown %#lx", hr);
    IMFActivate_Release(activate);
    IMFMediaType_Release(type);
}

static void test_loader(void)
{
    IMFTopologyNode *src, *dst;
    IMFPresentationDescriptor *pd;
    IMFStreamDescriptor *sd;
    IMFTopology *topology, *full;
    IMFMediaType *type;
    IMFMediaTypeHandler *handler;
    IMFActivate *activate;
    IMFTopoLoader *loader;
    HRESULT hr;

    hr = MFCreateTopoLoader(&loader);
    hr = MFCreateTopology(&topology);
    hr = IMFTopoLoader_Load(loader, topology, &full, NULL);
    CHECKF(hr == MF_E_TOPO_UNSUPPORTED, "empty topology %#lx", hr);

    MFCreateMediaType(&type);
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    MFCreateStreamDescriptor(0, 1, &type, &sd);
    IMFStreamDescriptor_GetMediaTypeHandler(sd, &handler);
    IMFMediaTypeHandler_SetCurrentMediaType(handler, type);
    IMFMediaTypeHandler_Release(handler);
    MFCreatePresentationDescriptor(1, &sd, &pd);

    MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &src);
    IMFTopologyNode_SetUnknown(src, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)sd);
    IMFTopologyNode_SetUnknown(src, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pd);
    IMFTopology_AddNode(topology, src);
    hr = IMFTopoLoader_Load(loader, topology, &full, NULL);
    CHECKF(hr == MF_E_TOPO_UNSUPPORTED, "source node only %#lx", hr);

    MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &dst);
    MFCreateSampleGrabberSinkActivate(type, &grabber_cb, &activate);
    IMFTopologyNode_SetObject(dst, (IUnknown *)activate);
    IMFTopology_AddNode(topology, dst);
    hr = IMFTopoLoader_Load(loader, topology, &full, NULL);
    CHECKF(hr == MF_E_TOPO_UNSUPPORTED, "unconnected unresolved sink %#lx", hr);
    IMFTopologyNode_ConnectOutput(src, 0, dst, 0);
    hr = IMFTopoLoader_Load(loader, topology, &full, NULL);
    CHECKF(hr == MF_E_TOPO_SINK_ACTIVATES_UNSUPPORTED, "connected unresolved sink %#lx", hr);

    IMFTopologyNode_SetObject(dst, NULL);
    IMFActivate_ShutdownObject(activate);
    IMFActivate_Release(activate);
    IMFTopologyNode_Release(dst);
    IMFTopologyNode_Release(src);
    IMFPresentationDescriptor_Release(pd);
    IMFStreamDescriptor_Release(sd);
    IMFMediaType_Release(type);
    IMFTopology_Release(topology);
    IMFTopoLoader_Release(loader);
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    CHECKF(hr == S_OK, "MFStartup %#lx", hr);
    test_sar();
    test_session();
    test_sample_grabber_activate();
    test_loader();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
