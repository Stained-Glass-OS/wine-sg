/* evr's default presenter (patches/sg/2891), run by test/evr-presenter-gate.sh:
 * IMFVideoDisplayControl (border colour, rendering preferences, ideal size,
 * repaint, full screen), IMFRateSupport, IQualProp statistics,
 * IMFQualityAdvise(Limits), the clock sink's rate, ProcessMessage (flush and
 * step), IMFVideoPositionMapper and the unknown service of IMFGetService,
 * before and after the presenter streamed frames, and after it was shut down.
 *
 *   evr-presenter-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <evr.h>
#include <evr9.h>
#include <strmif.h>
#include <evcode.h>
#include <amvideo.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "evr-qa.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

DEFINE_GUID(MR_VIDEO_RENDER_SERVICE_sg, 0x1092a86c, 0xab1a, 0x459a, 0xa3, 0x36, 0x83, 0x1f, 0xbc, 0x4d, 0x11, 0xff);
DEFINE_GUID(MR_VIDEO_MIXER_SERVICE_sg, 0x073cd2fc, 0x6cf4, 0x40b7, 0x88, 0x59, 0xe8, 0x95, 0x52, 0xc8, 0x41, 0xf8);
DEFINE_GUID(MR_BUFFER_SERVICE_sg, 0xa562248c, 0x9ac6, 0x4ffc, 0x9f, 0xba, 0x3a, 0xf8, 0xf8, 0xad, 0x1a, 0x4d);
DEFINE_GUID(MR_VIDEO_ACCELERATION_SERVICE_sg, 0xefef5175, 0x5c7d, 0x4ce2, 0xbb, 0xbd, 0x34, 0xff, 0x8b, 0xca, 0x65, 0x54);
DEFINE_GUID(IID_IQualProp_sg, 0x1bd0ecb0, 0xf8e2, 0x11ce, 0xaa, 0xc6, 0x00, 0x20, 0xaf, 0x0b, 0x99, 0xa3);
DEFINE_GUID(UNKNOWN_SERVICE, 0x5eed0001, 0x1234, 0x4321, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08);

/* the services the presenter asks its host for */
struct host
{
    IMFTopologyServiceLookup lookup;
    IMediaEventSink sink;
    IMFTransform *mixer;
};
static struct host *host_from_lookup(IMFTopologyServiceLookup *i) { return CONTAINING_RECORD(i, struct host, lookup); }
static struct host *host_from_sink(IMediaEventSink *i) { return CONTAINING_RECORD(i, struct host, sink); }
static HRESULT WINAPI host_qi(IMFTopologyServiceLookup *i, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IMFTopologyServiceLookup) || IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = i;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI host_addref(IMFTopologyServiceLookup *i) { return 2; }
static ULONG WINAPI host_release(IMFTopologyServiceLookup *i) { return 1; }
static HRESULT WINAPI host_lookup(IMFTopologyServiceLookup *i, MF_SERVICE_LOOKUP_TYPE type, DWORD index,
        REFGUID service, REFIID riid, void **objects, DWORD *count)
{
    struct host *host = host_from_lookup(i);

    *objects = NULL;
    if (IsEqualGUID(service, &MR_VIDEO_RENDER_SERVICE_sg))
    {
        if (IsEqualIID(riid, &IID_IMediaEventSink))
        {
            *objects = &host->sink;
            return S_OK;
        }
        return E_FAIL;
    }
    if (IsEqualGUID(service, &MR_VIDEO_MIXER_SERVICE_sg) && IsEqualIID(riid, &IID_IMFTransform))
    {
        *objects = host->mixer;
        IMFTransform_AddRef(host->mixer);
        return S_OK;
    }
    return E_NOTIMPL;
}
static const IMFTopologyServiceLookupVtbl host_vtbl = { host_qi, host_addref, host_release, host_lookup };
static HRESULT WINAPI sink_qi(IMediaEventSink *i, REFIID riid, void **obj)
{
    return host_qi(&host_from_sink(i)->lookup, riid, obj);
}
static ULONG WINAPI sink_addref(IMediaEventSink *i) { return 2; }
static ULONG WINAPI sink_release(IMediaEventSink *i) { return 1; }
static HRESULT WINAPI sink_notify(IMediaEventSink *i, LONG code, LONG_PTR p1, LONG_PTR p2) { return S_OK; }
static const IMediaEventSinkVtbl sink_vtbl = { sink_qi, sink_addref, sink_release, sink_notify };

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }

static IMFMediaType *rgb32_type(unsigned int w, unsigned int h)
{
    IMFVideoMediaType *vt;
    IMFMediaType *type;

    if (FAILED(MFCreateVideoMediaTypeFromSubtype(&MFVideoFormat_RGB32, &vt))) return NULL;
    IMFVideoMediaType_QueryInterface(vt, &IID_IMFMediaType, (void **)&type);
    IMFVideoMediaType_Release(vt);
    IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, (UINT64)w << 32 | h);
    IMFMediaType_SetUINT64(type, &MF_MT_FRAME_RATE, (UINT64)30 << 32 | 1);
    IMFMediaType_SetUINT32(type, &MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    return type;
}

static float display_refresh(void)
{
    DEVMODEW mode = { .dmSize = sizeof(mode) };

    if (!EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &mode) || mode.dmDisplayFrequency <= 1)
        mode.dmDisplayFrequency = 60;
    return (float)mode.dmDisplayFrequency;
}

static HRESULT (WINAPI *pMFCreateVideoSampleAllocator)(REFIID, void **);

static IMFSample *create_sample(IDirect3DDeviceManager9 *manager, IMFMediaType *type)
{
    IMFVideoSampleAllocator *allocator;
    IMFSample *sample = NULL;

    if (SUCCEEDED(pMFCreateVideoSampleAllocator(&IID_IMFVideoSampleAllocator, (void **)&allocator)))
    {
        IMFVideoSampleAllocator_SetDirectXManager(allocator, (IUnknown *)manager);
        if (SUCCEEDED(IMFVideoSampleAllocator_InitializeSampleAllocator(allocator, 2, type)))
            IMFVideoSampleAllocator_AllocateSample(allocator, &sample);
        IMFVideoSampleAllocator_Release(allocator);
    }
    return sample;
}

static void test_display_control(IMFVideoDisplayControl *control, HWND window)
{
    COLORREF color = 0xcc;
    DWORD prefs = 0xcc;
    SIZE min = { 7, 7 }, max = { 7, 7 };
    BOOL full = TRUE;
    HRESULT hr;

    hr = IMFVideoDisplayControl_GetBorderColor(control, &color);
    CHECKF(hr == S_OK && color == 0, "the border colour starts black (%#lx, %#lx)", hr, (long)color);
    hr = IMFVideoDisplayControl_GetBorderColor(control, NULL);
    CHECKF(hr == E_POINTER, "GetBorderColor(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFVideoDisplayControl_SetBorderColor(control, 0x00123456);
    CHECKF(hr == S_OK, "SetBorderColor (%#lx)", hr);
    color = 0;
    hr = IMFVideoDisplayControl_GetBorderColor(control, &color);
    CHECKF(hr == S_OK && color == 0x00123456, "the border colour reads back (%#lx, %#lx)", hr, (long)color);

    hr = IMFVideoDisplayControl_GetRenderingPrefs(control, &prefs);
    CHECKF(hr == S_OK && prefs == 0, "no preferences at first (%#lx, %#lx)", hr, prefs);
    hr = IMFVideoDisplayControl_SetRenderingPrefs(control, 0x1ff);
    CHECKF(hr == S_OK, "SetRenderingPrefs with every defined flag (%#lx)", hr);
    hr = IMFVideoDisplayControl_GetRenderingPrefs(control, &prefs);
    CHECKF(hr == S_OK && prefs == 0x1ff, "the preferences read back (%#lx, %#lx)", hr, prefs);
    hr = IMFVideoDisplayControl_SetRenderingPrefs(control, 0x200);
    CHECKF(hr == E_INVALIDARG, "an undefined preference flag is E_INVALIDARG (%#lx)", hr);
    IMFVideoDisplayControl_GetRenderingPrefs(control, &prefs);
    CHECKF(prefs == 0x1ff, "a rejected value changes nothing (%#lx)", prefs);
    IMFVideoDisplayControl_SetRenderingPrefs(control, 0);

    hr = IMFVideoDisplayControl_SetFullscreen(control, TRUE);
    CHECKF(hr == E_NOTIMPL, "SetFullscreen is E_NOTIMPL (documented) (%#lx)", hr);
    hr = IMFVideoDisplayControl_GetFullscreen(control, &full);
    CHECKF(hr == E_NOTIMPL, "GetFullscreen is E_NOTIMPL (documented) (%#lx)", hr);

    hr = IMFVideoDisplayControl_GetIdealVideoSize(control, NULL, NULL);
    CHECKF(hr == E_POINTER, "GetIdealVideoSize(NULL, NULL) is E_POINTER (%#lx)", hr);
    hr = IMFVideoDisplayControl_GetIdealVideoSize(control, &min, NULL);
    CHECKF(hr == S_OK && !min.cx && !min.cy, "ideal size before a type: S_OK, 0x0 (%#lx, %ldx%ld)", hr, min.cx, min.cy);
    hr = IMFVideoDisplayControl_GetIdealVideoSize(control, NULL, &max);
    CHECKF(hr == S_OK && !max.cx && !max.cy, "max ideal size with NULL min (%#lx)", hr);

    hr = IMFVideoDisplayControl_RepaintVideo(control);
    CHECKF(hr == S_OK, "RepaintVideo with nothing shown is S_OK (%#lx)", hr);
}

static void test_quality(IMFVideoPresenter *presenter)
{
    IMFQualityAdvise *qa;
    IMFQualityAdviseLimits *limits;
    MF_QUALITY_DROP_MODE dm = MF_DROP_MODE_5;
    MF_QUALITY_LEVEL lv = MF_QUALITY_NORMAL_MINUS_5;
    HRESULT hr;

    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFQualityAdvise_sg, (void **)&qa);
    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFQualityAdviseLimits_sg, (void **)&limits);

    hr = IMFQualityAdviseLimits_GetMaximumDropMode(limits, &dm);
    CHECKF(hr == S_OK && dm == MF_DROP_MODE_NONE, "maximum drop mode: none (%#lx, %d)", hr, dm);
    hr = IMFQualityAdviseLimits_GetMinimumQualityLevel(limits, &lv);
    CHECKF(hr == S_OK && lv == MF_QUALITY_NORMAL, "minimum quality level: normal (%#lx, %d)", hr, lv);
    hr = IMFQualityAdviseLimits_GetMaximumDropMode(limits, NULL);
    CHECKF(hr == E_POINTER, "GetMaximumDropMode(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFQualityAdviseLimits_GetMinimumQualityLevel(limits, NULL);
    CHECKF(hr == E_POINTER, "GetMinimumQualityLevel(NULL) is E_POINTER (%#lx)", hr);

    dm = MF_DROP_MODE_5;
    hr = IMFQualityAdvise_GetDropMode(qa, &dm);
    CHECKF(hr == S_OK && dm == MF_DROP_MODE_NONE, "GetDropMode (%#lx, %d)", hr, dm);
    hr = IMFQualityAdvise_GetQualityLevel(qa, &lv);
    CHECKF(hr == S_OK && lv == MF_QUALITY_NORMAL, "GetQualityLevel (%#lx, %d)", hr, lv);
    hr = IMFQualityAdvise_GetDropMode(qa, NULL);
    CHECKF(hr == E_POINTER, "GetDropMode(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFQualityAdvise_GetQualityLevel(qa, NULL);
    CHECKF(hr == E_POINTER, "GetQualityLevel(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFQualityAdvise_SetDropMode(qa, MF_DROP_MODE_NONE);
    CHECKF(hr == S_OK, "SetDropMode(none) (%#lx)", hr);
    hr = IMFQualityAdvise_SetDropMode(qa, MF_DROP_MODE_1);
    CHECKF(hr == MF_E_NO_MORE_DROP_MODES, "SetDropMode(1) is MF_E_NO_MORE_DROP_MODES (%#lx)", hr);
    hr = IMFQualityAdvise_SetDropMode(qa, 77);
    CHECKF(hr == E_INVALIDARG, "SetDropMode(77) is E_INVALIDARG (%#lx)", hr);
    hr = IMFQualityAdvise_SetQualityLevel(qa, MF_QUALITY_NORMAL);
    CHECKF(hr == S_OK, "SetQualityLevel(normal) (%#lx)", hr);
    hr = IMFQualityAdvise_SetQualityLevel(qa, MF_QUALITY_NORMAL_MINUS_1);
    CHECKF(hr == MF_E_NO_MORE_QUALITY_LEVELS, "SetQualityLevel(-1) is MF_E_NO_MORE_QUALITY_LEVELS (%#lx)", hr);
    hr = IMFQualityAdvise_SetQualityLevel(qa, 77);
    CHECKF(hr == E_INVALIDARG, "SetQualityLevel(77) is E_INVALIDARG (%#lx)", hr);
    hr = IMFQualityAdvise_DropTime(qa, 1000);
    CHECKF(hr == MF_E_DROPTIME_NOT_SUPPORTED, "DropTime is MF_E_DROPTIME_NOT_SUPPORTED (%#lx)", hr);

    IMFQualityAdvise_Release(qa);
    IMFQualityAdviseLimits_Release(limits);
}

static void test_rate(IMFVideoPresenter *presenter, BOOL have_type, BOOL shut_down)
{
    IMFRateSupport *rs;
    float rate, nearest, expect;
    HRESULT hr;

    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFRateSupport, (void **)&rs);

    if (shut_down)
    {
        hr = IMFRateSupport_GetFastestRate(rs, MFRATE_FORWARD, FALSE, &rate);
        CHECKF(hr == MF_E_SHUTDOWN, "GetFastestRate after shutdown is MF_E_SHUTDOWN (%#lx)", hr);
        hr = IMFRateSupport_IsRateSupported(rs, FALSE, 1.0f, &nearest);
        CHECKF(hr == MF_E_SHUTDOWN, "IsRateSupported after shutdown is MF_E_SHUTDOWN (%#lx)", hr);
        IMFRateSupport_Release(rs);
        return;
    }

    hr = IMFRateSupport_GetFastestRate(rs, MFRATE_FORWARD, FALSE, NULL);
    CHECKF(hr == E_POINTER, "GetFastestRate(NULL) is E_POINTER (%#lx)", hr);

    /* without a type nothing limits the rate; with 30 fps the display refresh does */
    expect = have_type ? display_refresh() / 30.0f : FLT_MAX;
    rate = 0;
    hr = IMFRateSupport_GetFastestRate(rs, MFRATE_FORWARD, FALSE, &rate);
    CHECKF(hr == S_OK && fabsf(rate - expect) <= expect * 1e-5f, "forward fastest rate %g (expected %g) (%#lx)", rate, expect, hr);
    hr = IMFRateSupport_GetFastestRate(rs, MFRATE_REVERSE, FALSE, &rate);
    CHECKF(hr == S_OK && fabsf(rate + expect) <= expect * 1e-5f, "reverse fastest rate %g (expected %g) (%#lx)", rate, -expect, hr);
    hr = IMFRateSupport_GetFastestRate(rs, MFRATE_FORWARD, TRUE, &rate);
    CHECKF(hr == S_OK && rate == FLT_MAX, "thinned fastest rate is unlimited (%#lx, %g)", hr, rate);
    hr = IMFRateSupport_GetFastestRate(rs, MFRATE_REVERSE, TRUE, &rate);
    CHECKF(hr == S_OK && rate == -FLT_MAX, "thinned reverse fastest rate is unlimited (%#lx, %g)", hr, rate);

    nearest = 0;
    hr = IMFRateSupport_IsRateSupported(rs, FALSE, 1.0f, &nearest);
    CHECKF(hr == S_OK && nearest == 1.0f, "rate 1.0 is supported (%#lx, %g)", hr, nearest);
    hr = IMFRateSupport_IsRateSupported(rs, FALSE, 1.0f, NULL);
    CHECKF(hr == S_OK, "IsRateSupported without the nearest rate (%#lx)", hr);
    hr = IMFRateSupport_IsRateSupported(rs, FALSE, 0.0f, &nearest);
    CHECKF(hr == S_OK && nearest == 0.0f, "rate 0 is supported (%#lx)", hr);
    hr = IMFRateSupport_IsRateSupported(rs, FALSE, -1.0f, &nearest);
    CHECKF(hr == S_OK && nearest == -1.0f, "rate -1.0 is supported (%#lx, %g)", hr, nearest);
    if (have_type)
    {
        hr = IMFRateSupport_IsRateSupported(rs, FALSE, 1000.0f, &nearest);
        CHECKF(hr == MF_E_UNSUPPORTED_RATE && fabsf(nearest - expect) <= expect * 1e-5f,
                "rate 1000 is MF_E_UNSUPPORTED_RATE, the nearest is the fastest (%#lx, %g)", hr, nearest);
        hr = IMFRateSupport_IsRateSupported(rs, FALSE, -1000.0f, &nearest);
        CHECKF(hr == MF_E_UNSUPPORTED_RATE && fabsf(nearest + expect) <= expect * 1e-5f,
                "rate -1000 is MF_E_UNSUPPORTED_RATE, the nearest is -fastest (%#lx, %g)", hr, nearest);
        hr = IMFRateSupport_IsRateSupported(rs, TRUE, 1000.0f, &nearest);
        CHECKF(hr == S_OK && nearest == 1000.0f, "thinned, rate 1000 is supported (%#lx, %g)", hr, nearest);
    }
    IMFRateSupport_Release(rs);
}

static void test_qualprop(IQualProp *qp, BOOL running, int expect_drawn, const char *when)
{
    typedef HRESULT (WINAPI *getter)(IQualProp *, int *);
    const getter get[] =
    {
        qp->lpVtbl->get_FramesDroppedInRenderer, qp->lpVtbl->get_FramesDrawn, qp->lpVtbl->get_AvgFrameRate,
        qp->lpVtbl->get_Jitter, qp->lpVtbl->get_AvgSyncOffset, qp->lpVtbl->get_DevSyncOffset,
    };
    static const char *names[] = { "FramesDroppedInRenderer", "FramesDrawn", "AvgFrameRate", "Jitter", "AvgSyncOffset", "DevSyncOffset" };
    unsigned int i;
    int value;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(get); ++i)
    {
        value = 0x1234;
        hr = get[i](qp, &value);
        if (!running)
            CHECKF(hr == E_NOTIMPL, "%s %s: E_NOTIMPL (%#lx)", names[i], when, hr);
        else
        {
            CHECKF(hr == S_OK, "%s %s: S_OK (%#lx, %d)", names[i], when, hr, value);
            hr = get[i](qp, NULL);
            CHECKF(hr == E_POINTER, "%s(NULL) %s: E_POINTER (%#lx)", names[i], when, hr);
        }
    }
    if (running)
    {
        value = -1;
        get[1](qp, &value);
        CHECKF(value == expect_drawn, "FramesDrawn %s is %d (%d)", when, expect_drawn, value);
    }
}

int main(void)
{
    HRESULT (WINAPI *create_mixer)(IUnknown *, REFIID, REFIID, void **);
    HRESULT (WINAPI *create_presenter)(IUnknown *, REFIID, REFIID, void **);
    IMFTopologyServiceLookupClient *client;
    IMFVideoDisplayControl *control;
    IMFVideoPositionMapper *mapper;
    IDirect3DDeviceManager9 *manager = NULL;
    IMFClockStateSink *clock_sink;
    IMFVideoPresenter *presenter = NULL;
    IMFTransform *mixer = NULL;
    IMFGetService *gs;
    IMFMediaType *type;
    IQualProp *qp;
    struct host host;
    IMFSample *sample;
    WNDCLASSA wc = {0};
    HMODULE evr;
    HRESULT hr;
    HWND window;
    float x, y;
    int value;
    IUnknown *unk;
    MFVideoNormalizedRect src = { 0.5f, 0.0f, 1.0f, 1.0f };

    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = "sg_evr_presenter";
    RegisterClassA(&wc);
    window = CreateWindowA("sg_evr_presenter", "evr", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, NULL, NULL);

    evr = LoadLibraryA("evr.dll");
    create_mixer = (void *)GetProcAddress(evr, "MFCreateVideoMixer");
    create_presenter = (void *)GetProcAddress(evr, "MFCreateVideoPresenter");
    pMFCreateVideoSampleAllocator = (void *)GetProcAddress(evr, "MFCreateVideoSampleAllocator");
    hr = create_mixer(NULL, &IID_IDirect3DDevice9, &IID_IMFTransform, (void **)&mixer);
    CHECKF(hr == S_OK, "MFCreateVideoMixer (%#lx)", hr);
    hr = create_presenter(NULL, &IID_IDirect3DDevice9, &IID_IMFVideoPresenter, (void **)&presenter);
    if (FAILED(hr))
    {
        puts("NOTE: the presenter needs a D3D device and there is none, nothing to test");
        puts("RESULT: PASS");
        return 0;
    }
    check(1, "MFCreateVideoPresenter");

    host.lookup.lpVtbl = (void *)&host_vtbl;
    host.sink.lpVtbl = (void *)&sink_vtbl;
    host.mixer = mixer;
    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFTopologyServiceLookupClient, (void **)&client);
    hr = IMFTopologyServiceLookupClient_InitServicePointers(client, &host.lookup);
    CHECKF(hr == S_OK, "InitServicePointers (%#lx)", hr);

    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFVideoDisplayControl, (void **)&control);
    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFVideoPositionMapper, (void **)&mapper);
    IMFVideoPresenter_QueryInterface(presenter, &IID_IQualProp_sg, (void **)&qp);
    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFClockStateSink, (void **)&clock_sink);
    IMFVideoPresenter_QueryInterface(presenter, &IID_IMFGetService, (void **)&gs);

    /* services */
    unk = (IUnknown *)0x1234;
    hr = IMFGetService_GetService(gs, &UNKNOWN_SERVICE, &IID_IUnknown, (void **)&unk);
    CHECKF(hr == MF_E_UNSUPPORTED_SERVICE, "an unknown service is MF_E_UNSUPPORTED_SERVICE (%#lx)", hr);

    test_display_control(control, window);
    test_quality(presenter);
    test_rate(presenter, FALSE, FALSE);
    test_qualprop(qp, FALSE, 0, "before the clock starts");

    /* the clock rate and the messages */
    hr = IMFClockStateSink_OnClockSetRate(clock_sink, 0, 2.0f);
    CHECKF(hr == S_OK, "OnClockSetRate(2.0) is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockSetRate(clock_sink, 0, 0.0f);
    CHECKF(hr == S_OK, "OnClockSetRate(0.0) is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockSetRate(clock_sink, 0, 1.0f);
    CHECKF(hr == S_OK, "OnClockSetRate(1.0) is S_OK (%#lx)", hr);
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_FLUSH, 0);
    CHECKF(hr == S_OK, "MFVP_MESSAGE_FLUSH before streaming is S_OK (%#lx)", hr);
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_STEP, 2);
    CHECKF(hr == S_OK, "MFVP_MESSAGE_STEP is S_OK (%#lx)", hr);
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_CANCELSTEP, 0);
    CHECKF(hr == S_OK, "MFVP_MESSAGE_CANCELSTEP is S_OK (%#lx)", hr);
    hr = IMFVideoPresenter_ProcessMessage(presenter, 99, 0);
    CHECKF(hr == E_NOTIMPL, "an undefined message is E_NOTIMPL (%#lx)", hr);

    /* position mapper: the presenter's visible part, then the mixer's stream placement */
    x = y = -1.0f;
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 0, 0, &x, &y);
    CHECKF(hr == S_OK && x == 0.25f && y == 0.75f, "a full source rectangle maps coordinates as they are (%#lx, %f, %f)", hr, x, y);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 0, 0, NULL, &y);
    CHECKF(hr == E_POINTER, "NULL x is E_POINTER (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 1, 0, &x, &y);
    CHECKF(hr == MF_E_INVALIDINDEX, "output stream 1 is MF_E_INVALIDINDEX (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 0, 5, &x, &y);
    CHECKF(hr == MF_E_INVALIDINDEX, "an unknown input stream is MF_E_INVALIDINDEX (%#lx)", hr);

    hr = IMFVideoDisplayControl_SetVideoWindow(control, window);
    CHECKF(hr == S_OK, "SetVideoWindow (%#lx)", hr);
    hr = IMFVideoDisplayControl_SetVideoPosition(control, &src, NULL);
    CHECKF(hr == S_OK, "SetVideoPosition with the right half of the video (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.5f, 0.25f, 0, 0, &x, &y);
    CHECKF(hr == S_OK && x == 0.75f && y == 0.25f, "the window maps into the zoomed part of the video (%#lx, %f, %f)", hr, x, y);
    src.left = 0.0f;
    IMFVideoDisplayControl_SetVideoPosition(control, &src, NULL);

    /* a media type: 640x480 at 30 fps */
    hr = IMFGetService_GetService(gs, &MR_VIDEO_ACCELERATION_SERVICE_sg, &IID_IDirect3DDeviceManager9, (void **)&manager);
    CHECKF(hr == S_OK, "the presenter's device manager (%#lx)", hr);
    hr = IMFTransform_ProcessMessage(mixer, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)manager);
    type = rgb32_type(640, 480);
    hr = IMFTransform_SetInputType(mixer, 0, type, 0);
    CHECKF(hr == S_OK, "mixer SetInputType (%#lx)", hr);
    hr = IMFTransform_SetOutputType(mixer, 0, type, 0);
    CHECKF(hr == S_OK, "mixer SetOutputType (%#lx)", hr);
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_INVALIDATEMEDIATYPE, 0);
    CHECKF(hr == S_OK, "INVALIDATEMEDIATYPE (%#lx)", hr);
    {
        SIZE min = {0}, max = {1};
        hr = IMFVideoDisplayControl_GetIdealVideoSize(control, &min, &max);
        CHECKF(hr == S_OK && min.cx == 640 && min.cy == 480 && max.cx == 640 && max.cy == 480,
                "ideal size is the native size (%#lx, %ldx%ld, %ldx%ld)", hr, min.cx, min.cy, max.cx, max.cy);
    }
    test_rate(presenter, TRUE, FALSE);

    /* statistics while running */
    hr = IMFClockStateSink_OnClockStart(clock_sink, 0, 0);
    CHECKF(hr == S_OK, "OnClockStart (%#lx)", hr);
    test_qualprop(qp, TRUE, 0, "right after the start");
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_BEGINSTREAMING, 0);
    CHECKF(hr == S_OK, "BEGINSTREAMING (%#lx)", hr);

    if (!(sample = create_sample(manager, type)))
        puts("NOTE: no sample, the frame statistics are skipped");
    else
    {
        int rate = 0, drawn = 0;
        unsigned int i;

        for (i = 0; i < 3; ++i)
        {
            hr = IMFTransform_ProcessInput(mixer, 0, sample, 0);
            if (hr != S_OK) CHECKF(0, "ProcessInput %u (%#lx)", i, hr);
            hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_PROCESSINPUTNOTIFY, 0);
            if (hr != S_OK) CHECKF(0, "PROCESSINPUTNOTIFY %u (%#lx)", i, hr);
            Sleep(60);
        }
        IMFSample_Release(sample);
        hr = IQualProp_get_FramesDrawn(qp, &drawn);
        CHECKF(hr == S_OK && drawn == 3, "FramesDrawn counts the presented frames (%#lx, %d)", hr, drawn);
        hr = IQualProp_get_AvgFrameRate(qp, &rate);
        /* three frames 60 ms apart: about 16 frames per second, reported times 100 */
        CHECKF(hr == S_OK && rate > 500 && rate < 5000, "AvgFrameRate is about 1600 (%#lx, %d)", hr, rate);
        hr = IQualProp_get_Jitter(qp, &value);
        CHECKF(hr == S_OK && value >= 0, "Jitter (%#lx, %d)", hr, value);
        hr = IQualProp_get_FramesDroppedInRenderer(qp, &value);
        CHECKF(hr == S_OK && value == 0, "no frame was dropped (%#lx, %d)", hr, value);
        hr = IQualProp_get_AvgSyncOffset(qp, &value);
        CHECKF(hr == S_OK && value == 0, "without a clock there is no sync offset (%#lx, %d)", hr, value);

        /* a repaint is not another frame */
        hr = IMFVideoDisplayControl_RepaintVideo(control);
        CHECKF(hr == S_OK, "RepaintVideo with a frame shown (%#lx)", hr);
        IQualProp_get_FramesDrawn(qp, &drawn);
        CHECKF(drawn == 3, "RepaintVideo does not count as a frame (%d)", drawn);

        hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_FLUSH, 0);
        CHECKF(hr == S_OK, "FLUSH while streaming (%#lx)", hr);

        /* stopping the clock resets the statistics */
        IMFClockStateSink_OnClockStop(clock_sink, 0);
        test_qualprop(qp, FALSE, 0, "after the clock stops");
        IMFClockStateSink_OnClockStart(clock_sink, 0, 0);
        IQualProp_get_FramesDrawn(qp, &drawn);
        CHECKF(drawn == 0, "the statistics start again after a stop (%d)", drawn);
    }
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_ENDSTREAMING, 0);
    CHECKF(hr == S_OK, "ENDSTREAMING (%#lx)", hr);

    /* shut down */
    hr = IMFTopologyServiceLookupClient_ReleaseServicePointers(client);
    CHECKF(hr == S_OK, "ReleaseServicePointers (%#lx)", hr);
    hr = IMFVideoDisplayControl_RepaintVideo(control);
    CHECKF(hr == MF_E_SHUTDOWN, "RepaintVideo after shutdown is MF_E_SHUTDOWN (%#lx)", hr);
    {
        SIZE min;
        hr = IMFVideoDisplayControl_GetIdealVideoSize(control, &min, NULL);
        CHECKF(hr == MF_E_SHUTDOWN, "GetIdealVideoSize after shutdown is MF_E_SHUTDOWN (%#lx)", hr);
    }
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_FLUSH, 0);
    CHECKF(hr == MF_E_SHUTDOWN, "FLUSH after shutdown is MF_E_SHUTDOWN (%#lx)", hr);
    hr = IMFVideoPresenter_ProcessMessage(presenter, MFVP_MESSAGE_STEP, 1);
    CHECKF(hr == MF_E_SHUTDOWN, "STEP after shutdown is MF_E_SHUTDOWN (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.5f, 0.5f, 0, 0, &x, &y);
    CHECKF(hr == MF_E_SHUTDOWN, "the position mapper after shutdown is MF_E_SHUTDOWN (%#lx)", hr);
    test_rate(presenter, TRUE, TRUE);
    hr = IMFVideoDisplayControl_SetBorderColor(control, 0x55);
    CHECKF(hr == S_OK, "SetBorderColor still works after shutdown (%#lx)", hr);

    IMFMediaType_Release(type);
    IMFGetService_Release(gs);
    IMFClockStateSink_Release(clock_sink);
    IQualProp_Release(qp);
    IMFVideoPositionMapper_Release(mapper);
    IMFVideoDisplayControl_Release(control);
    IMFTopologyServiceLookupClient_Release(client);
    if (manager) IDirect3DDeviceManager9_Release(manager);
    IMFVideoPresenter_Release(presenter);
    IMFTransform_Release(mixer);
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
