/* evr's DirectShow filter (patches/sg/2892), run by test/evr-filter-gate.sh:
 * IEVRFilterConfig (the number of input streams: default, limits, refused while
 * connected is not reachable without a source filter, handed to the mixer) and
 * the filter's IMediaEventSink: the mixer and presenter bookkeeping events stay
 * inside the filter, other events reach the filter graph's event queue.
 *
 *   evr-filter-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <evr.h>
#include <evcode.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

DEFINE_GUID(CLSID_EVR_sg, 0xfa10746c, 0x9b63, 0x4b6c, 0xbc, 0x49, 0xfc, 0x30, 0x0e, 0xa5, 0xf2, 0x56);
DEFINE_GUID(MR_VIDEO_MIXER_SERVICE_sg, 0x073cd2fc, 0x6cf4, 0x40b7, 0x88, 0x59, 0xe8, 0x95, 0x52, 0xc8, 0x41, 0xf8);

static DWORD mixer_streams(IBaseFilter *filter)
{
    IMFGetService *gs = NULL;
    IMFTransform *mixer = NULL;
    DWORD inputs = 0, outputs = 0;

    if (SUCCEEDED(IBaseFilter_QueryInterface(filter, &IID_IMFGetService, (void **)&gs)))
    {
        if (SUCCEEDED(IMFGetService_GetService(gs, &MR_VIDEO_MIXER_SERVICE_sg, &IID_IMFTransform, (void **)&mixer)))
        {
            IMFTransform_GetStreamCount(mixer, &inputs, &outputs);
            IMFTransform_Release(mixer);
        }
        IMFGetService_Release(gs);
    }
    return inputs;
}

static IBaseFilter *create_evr(IEVRFilterConfig **config)
{
    IBaseFilter *filter = NULL;

    if (FAILED(CoCreateInstance(&CLSID_EVR_sg, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter)))
        return NULL;
    IBaseFilter_QueryInterface(filter, &IID_IEVRFilterConfig, (void **)config);
    return filter;
}

static void test_config(void)
{
    static const struct { DWORD count; HRESULT hr; } table[] =
    {
        { 0, E_INVALIDARG }, { 17, E_INVALIDARG }, { 100, E_INVALIDARG }, { 1, S_OK }, { 2, S_OK }, { 16, S_OK }, { 3, S_OK },
    };
    IEVRFilterConfig *config;
    IBaseFilter *filter;
    IMFVideoRenderer *renderer;
    DWORD count;
    HRESULT hr;
    unsigned int i;

    if (!(filter = create_evr(&config)))
    {
        puts("NOTE: the EVR filter cannot be created, the filter tests are skipped");
        return;
    }

    hr = IEVRFilterConfig_GetNumberOfStreams(config, NULL);
    CHECKF(hr == E_POINTER, "GetNumberOfStreams(NULL) is E_POINTER (%#lx)", hr);
    count = 0;
    hr = IEVRFilterConfig_GetNumberOfStreams(config, &count);
    CHECKF(hr == S_OK && count == 1, "a new filter has one stream (%#lx, %lu)", hr, count);

    for (i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
    {
        DWORD before = 99;

        IEVRFilterConfig_GetNumberOfStreams(config, &before);
        hr = IEVRFilterConfig_SetNumberOfStreams(config, table[i].count);
        CHECKF(hr == table[i].hr, "SetNumberOfStreams(%lu) is %#lx (%#lx)", table[i].count, (long)table[i].hr, hr);
        count = 0;
        IEVRFilterConfig_GetNumberOfStreams(config, &count);
        CHECKF(count == (table[i].hr == S_OK ? table[i].count : before), "the count afterwards is %lu (%lu)",
                table[i].hr == S_OK ? table[i].count : before, count);
    }

    /* the mixer follows: set before it exists ... */
    IBaseFilter_QueryInterface(filter, &IID_IMFVideoRenderer, (void **)&renderer);
    hr = IMFVideoRenderer_InitializeRenderer(renderer, NULL, NULL);
    CHECKF(hr == S_OK, "InitializeRenderer (%#lx)", hr);
    CHECKF(mixer_streams(filter) == 3, "a mixer made after SetNumberOfStreams(3) has 3 input streams (%lu)", mixer_streams(filter));
    /* ... and after */
    hr = IEVRFilterConfig_SetNumberOfStreams(config, 5);
    CHECKF(hr == S_OK, "SetNumberOfStreams(5) with a mixer (%#lx)", hr);
    CHECKF(mixer_streams(filter) == 5, "the mixer grows to 5 input streams (%lu)", mixer_streams(filter));
    IMFVideoRenderer_Release(renderer);

    IEVRFilterConfig_Release(config);
    IBaseFilter_Release(filter);
}

static void test_notify(void)
{
    IMediaEventSink *sink;
    IGraphBuilder *graph;
    IMediaEvent *events;
    IBaseFilter *filter;
    IEVRFilterConfig *config;
    LONG code;
    LONG_PTR p1, p2;
    HRESULT hr;

    if (!(filter = create_evr(&config)))
        return;
    IEVRFilterConfig_Release(config);
    IBaseFilter_QueryInterface(filter, &IID_IMediaEventSink, (void **)&sink);

    /* no graph: the filter has nowhere to forward to */
    hr = IMediaEventSink_Notify(sink, EC_SAMPLE_NEEDED, 0, 0);
    CHECKF(hr == S_OK, "EC_SAMPLE_NEEDED without a graph is S_OK (%#lx)", hr);
    hr = IMediaEventSink_Notify(sink, EC_STEP_COMPLETE, 0, 0);
    CHECKF(hr == S_OK, "EC_STEP_COMPLETE without a graph is S_OK (%#lx)", hr);

    if (FAILED(CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph)))
    {
        puts("NOTE: no filter graph, the forwarding tests are skipped");
    }
    else
    {
        IGraphBuilder_QueryInterface(graph, &IID_IMediaEvent, (void **)&events);
        hr = IGraphBuilder_AddFilter(graph, filter, L"evr");
        CHECKF(hr == S_OK, "AddFilter (%#lx)", hr);

        hr = IMediaEventSink_Notify(sink, EC_SAMPLE_NEEDED, 0, 0);
        CHECKF(hr == S_OK, "EC_SAMPLE_NEEDED with a graph is S_OK (%#lx)", hr);
        hr = IMediaEventSink_Notify(sink, EC_PROCESSING_LATENCY, 0, 0);
        CHECKF(hr == S_OK, "EC_PROCESSING_LATENCY is S_OK (%#lx)", hr);
        hr = IMediaEvent_GetEvent(events, &code, &p1, &p2, 200);
        CHECKF(hr == E_ABORT, "mixer and presenter bookkeeping never reaches the graph (%#lx, %#lx)", hr, code);

        hr = IMediaEventSink_Notify(sink, EC_USER + 1, 123, 456);
        CHECKF(hr == S_OK, "a graph event is S_OK (%#lx)", hr);
        code = 0; p1 = p2 = 0;
        hr = IMediaEvent_GetEvent(events, &code, &p1, &p2, 2000);
        CHECKF(hr == S_OK && code == EC_USER + 1 && p1 == 123 && p2 == 456, "the event reaches the graph with its parameters (%#lx, %#lx, %Id, %Id)",
                hr, code, p1, p2);
        if (hr == S_OK) IMediaEvent_FreeEventParams(events, code, p1, p2);

        IBaseFilter_Release(filter);
        IMediaEvent_Release(events);
        IGraphBuilder_Release(graph);
        IMediaEventSink_Release(sink);
        return;
    }
    IMediaEventSink_Release(sink);
    IBaseFilter_Release(filter);
}

int main(void)
{
    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    test_config();
    test_notify();
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
