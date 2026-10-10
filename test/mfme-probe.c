/* mfmediaengine's IMFMediaEngine/IMFMediaEngineEx members that were stubs
 * (patches/sg/2870), run by test/mfme-gate.sh: an engine in the idle state, an
 * engine with a loaded AVI file and an engine after Shutdown. The loaded part
 * needs a working media source (it prints a note when the file cannot be
 * loaded).
 *
 *   mfme-probe.exe <wavefile> */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static HANDLE loaded_event;

static HRESULT WINAPI notify_QI(IMFMediaEngineNotify *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMFMediaEngineNotify))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI notify_AddRef(IMFMediaEngineNotify *iface) { return 2; }
static ULONG WINAPI notify_Release(IMFMediaEngineNotify *iface) { return 1; }
static HRESULT WINAPI notify_EventNotify(IMFMediaEngineNotify *iface, DWORD event, DWORD_PTR p1, DWORD p2)
{
    if (event == MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA || event == MF_MEDIA_ENGINE_EVENT_ERROR)
        SetEvent(loaded_event);
    return S_OK;
}
static IMFMediaEngineNotifyVtbl notify_vtbl = { notify_QI, notify_AddRef, notify_Release, notify_EventNotify };
static IMFMediaEngineNotify notify = { &notify_vtbl };

/* IMFMediaEngineSrcElements over a fixed list of URLs */
struct src_elements { IMFMediaEngineSrcElements iface; const WCHAR *urls[4]; DWORD count; };
static HRESULT WINAPI se_QI(IMFMediaEngineSrcElements *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMFMediaEngineSrcElements))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI se_AddRef(IMFMediaEngineSrcElements *iface) { return 2; }
static ULONG WINAPI se_Release(IMFMediaEngineSrcElements *iface) { return 1; }
static DWORD WINAPI se_GetLength(IMFMediaEngineSrcElements *iface)
{
    return ((struct src_elements *)iface)->count;
}
static HRESULT WINAPI se_GetURL(IMFMediaEngineSrcElements *iface, DWORD i, BSTR *url)
{
    struct src_elements *s = (struct src_elements *)iface;
    *url = i < s->count ? SysAllocString(s->urls[i]) : NULL;
    return i < s->count ? S_OK : E_INVALIDARG;
}
static HRESULT WINAPI se_GetType(IMFMediaEngineSrcElements *iface, DWORD i, BSTR *type) { *type = NULL; return S_OK; }
static HRESULT WINAPI se_GetMedia(IMFMediaEngineSrcElements *iface, DWORD i, BSTR *media) { *media = NULL; return S_OK; }
static HRESULT WINAPI se_AddElement(IMFMediaEngineSrcElements *iface, BSTR u, BSTR t, BSTR m) { return E_NOTIMPL; }
static HRESULT WINAPI se_RemoveAll(IMFMediaEngineSrcElements *iface) { return E_NOTIMPL; }
static IMFMediaEngineSrcElementsVtbl se_vtbl = { se_QI, se_AddRef, se_Release, se_GetLength, se_GetURL,
        se_GetType, se_GetMedia, se_AddElement, se_RemoveAll };

static IMFMediaEngineEx *create_engine(void)
{
    IMFMediaEngineClassFactory *factory;
    IMFMediaEngine *engine = NULL;
    IMFMediaEngineEx *ex = NULL;
    IMFAttributes *attrs;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFMediaEngineClassFactory, (void **)&factory);
    if (FAILED(hr)) return NULL;
    MFCreateAttributes(&attrs, 1);
    IMFAttributes_SetUnknown(attrs, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)&notify);
    hr = IMFMediaEngineClassFactory_CreateInstance(factory, MF_MEDIA_ENGINE_AUDIOONLY, attrs, &engine);
    if (SUCCEEDED(hr))
    {
        IMFMediaEngine_QueryInterface(engine, &IID_IMFMediaEngineEx, (void **)&ex);
        IMFMediaEngine_Release(engine);
    }
    IMFAttributes_Release(attrs);
    IMFMediaEngineClassFactory_Release(factory);
    return ex;
}

static int is_nan(double d) { return d != d; }

static void test_idle(IMFMediaEngineEx *e)
{
    IMFMediaTimeRange *range;
    PROPVARIANT pv;
    HRESULT hr;
    BOOL b;
    double d;
    DWORD n;
    int i;
    static const struct { int id; VARTYPE vt; } stats[] = {
        {MF_MEDIA_ENGINE_STATISTIC_FRAMES_RENDERED, VT_UI4}, {MF_MEDIA_ENGINE_STATISTIC_FRAMES_DROPPED, VT_UI4},
        {MF_MEDIA_ENGINE_STATISTIC_BYTES_DOWNLOADED, VT_UI8}, {MF_MEDIA_ENGINE_STATISTIC_BUFFER_PROGRESS, VT_UI4},
        {MF_MEDIA_ENGINE_STATISTIC_FRAMES_PER_SECOND, VT_R4}, {MF_MEDIA_ENGINE_STATISTIC_PLAYBACK_JITTER, VT_I4},
        {MF_MEDIA_ENGINE_STATISTIC_FRAMES_CORRUPTED, VT_UI4}, {MF_MEDIA_ENGINE_STATISTIC_TOTAL_FRAME_DELAY, VT_UI4},
    };
    static const struct { double v; HRESULT hr; } balances[] = {
        {0.5, S_OK}, {-1.0, S_OK}, {1.0, S_OK}, {1.5, E_INVALIDARG}, {-1.25, E_INVALIDARG}, {0.0 / 0.0, E_INVALIDARG},
    };
    static const struct { int v; HRESULT hr; } packing[] = {{1, S_OK}, {2, S_OK}, {0, S_OK}, {3, E_INVALIDARG}, {-1, E_INVALIDARG}};
    static const struct { int v; HRESULT hr; } rendermode[] = {{1, S_OK}, {0, S_OK}, {2, E_INVALIDARG}};

    /* Load / IsSeeking / GetStartTime / GetPlayed */
    check(IMFMediaEngineEx_IsSeeking(e) == FALSE, "idle: IsSeeking is FALSE");
    check(IMFMediaEngineEx_GetStartTime(e) == 0.0, "idle: GetStartTime is 0");
    hr = IMFMediaEngineEx_Load(e);
    CHECKF(hr == S_OK, "idle: Load with no source is S_OK (%#lx)", hr);
    range = (void *)1;
    hr = IMFMediaEngineEx_GetPlayed(e, &range);
    CHECKF(hr == S_OK && range && IMFMediaTimeRange_GetLength(range) == 0, "idle: GetPlayed is an empty range (%#lx)", hr);
    if (hr == S_OK && range) IMFMediaTimeRange_Release(range);
    hr = IMFMediaEngineEx_GetPlayed(e, NULL);
    CHECKF(hr == E_POINTER, "GetPlayed(NULL) is E_POINTER (%#lx)", hr);

    /* auto play, loop */
    for (i = 0; i < 2; i++)
    {
        b = i == 0;
        hr = IMFMediaEngineEx_SetAutoPlay(e, b);
        CHECKF(hr == S_OK && IMFMediaEngineEx_GetAutoPlay(e) == b, "SetAutoPlay(%d) round-trips (%#lx)", b, hr);
        hr = IMFMediaEngineEx_SetLoop(e, b);
        CHECKF(hr == S_OK && IMFMediaEngineEx_GetLoop(e) == b, "SetLoop(%d) round-trips (%#lx)", b, hr);
    }

    /* balance */
    d = IMFMediaEngineEx_GetBalance(e);
    CHECKF(d == 0.0, "GetBalance default is 0 (%f)", d);
    for (i = 0; i < ARRAY_SIZE(balances); i++)
    {
        double before = IMFMediaEngineEx_GetBalance(e);
        hr = IMFMediaEngineEx_SetBalance(e, balances[i].v);
        d = IMFMediaEngineEx_GetBalance(e);
        CHECKF(hr == balances[i].hr && d == (hr == S_OK ? balances[i].v : before),
               "SetBalance(%f) is %#lx and Get is %f (%#lx)", balances[i].v, balances[i].hr, d, hr);
    }

    /* statistics */
    for (i = 0; i < ARRAY_SIZE(stats); i++)
    {
        memset(&pv, 0xcc, sizeof(pv));
        hr = IMFMediaEngineEx_GetStatistics(e, stats[i].id, &pv);
        CHECKF(hr == S_OK && pv.vt == stats[i].vt && pv.uhVal.QuadPart == 0,
               "GetStatistics(%d) is VT %d, zero (%#lx, vt %d)", stats[i].id, stats[i].vt, hr, pv.vt);
    }
    hr = IMFMediaEngineEx_GetStatistics(e, 99, &pv);
    CHECKF(hr == E_INVALIDARG, "GetStatistics(99) is E_INVALIDARG (%#lx)", hr);
    hr = IMFMediaEngineEx_GetStatistics(e, 0, NULL);
    CHECKF(hr == E_POINTER, "GetStatistics(NULL) is E_POINTER (%#lx)", hr);

    /* UpdateVideoStream */
    {
        MFVideoNormalizedRect good = {0.0f, 0.0f, 1.0f, 0.5f}, bad = {0.5f, 0.0f, 0.25f, 1.0f}, big = {0.0f, 0.0f, 2.0f, 1.0f};
        RECT dst = {0, 0, 64, 64}, empty = {10, 10, 10, 20};
        MFARGB border = {0, 0, 0, 255};

        hr = IMFMediaEngineEx_UpdateVideoStream(e, NULL, NULL, NULL);
        CHECKF(hr == E_INVALIDARG, "UpdateVideoStream with nothing is E_INVALIDARG (%#lx)", hr);
        hr = IMFMediaEngineEx_UpdateVideoStream(e, &bad, NULL, NULL);
        CHECKF(hr == E_INVALIDARG, "UpdateVideoStream with an inverted source is E_INVALIDARG (%#lx)", hr);
        hr = IMFMediaEngineEx_UpdateVideoStream(e, &big, NULL, NULL);
        CHECKF(hr == E_INVALIDARG, "UpdateVideoStream with a source beyond 1.0 is E_INVALIDARG (%#lx)", hr);
        hr = IMFMediaEngineEx_UpdateVideoStream(e, NULL, &empty, NULL);
        CHECKF(hr == E_INVALIDARG, "UpdateVideoStream with an empty destination is E_INVALIDARG (%#lx)", hr);
        hr = IMFMediaEngineEx_UpdateVideoStream(e, &good, &dst, &border);
        CHECKF(hr == S_OK, "UpdateVideoStream with good rectangles is S_OK (%#lx)", hr);
    }

    /* rate, frame step */
    check(IMFMediaEngineEx_IsPlaybackRateSupported(e, 1.0) == FALSE, "idle: IsPlaybackRateSupported(1.0) is FALSE");
    hr = IMFMediaEngineEx_FrameStep(e, TRUE);
    CHECKF(hr == MF_E_INVALIDREQUEST, "idle: FrameStep is MF_E_INVALIDREQUEST (%#lx)", hr);

    /* stream selection, protection */
    hr = IMFMediaEngineEx_GetStreamSelection(e, 0, &b);
    CHECKF(hr == MF_E_NOT_INITIALIZED, "idle: GetStreamSelection is MF_E_NOT_INITIALIZED (%#lx)", hr);
    hr = IMFMediaEngineEx_SetStreamSelection(e, 0, TRUE);
    CHECKF(hr == MF_E_NOT_INITIALIZED, "idle: SetStreamSelection is MF_E_NOT_INITIALIZED (%#lx)", hr);
    hr = IMFMediaEngineEx_ApplyStreamSelections(e);
    CHECKF(hr == MF_E_NOT_INITIALIZED, "idle: ApplyStreamSelections is MF_E_NOT_INITIALIZED (%#lx)", hr);
    hr = IMFMediaEngineEx_GetStreamSelection(e, 0, NULL);
    CHECKF(hr == E_POINTER, "GetStreamSelection(NULL) is E_POINTER (%#lx)", hr);
    b = TRUE;
    hr = IMFMediaEngineEx_IsProtected(e, &b);
    CHECKF(hr == S_OK && b == FALSE, "idle: IsProtected is S_OK, FALSE (%#lx)", hr);
    hr = IMFMediaEngineEx_IsProtected(e, NULL);
    CHECKF(hr == E_POINTER, "IsProtected(NULL) is E_POINTER (%#lx)", hr);

    /* timeline marker timer */
    d = 0.0;
    hr = IMFMediaEngineEx_GetTimelineMarkerTimer(e, &d);
    CHECKF(hr == S_OK && is_nan(d), "timeline marker default is NaN (%#lx, %f)", hr, d);
    hr = IMFMediaEngineEx_SetTimelineMarkerTimer(e, 2.5);
    d = 0.0;
    IMFMediaEngineEx_GetTimelineMarkerTimer(e, &d);
    CHECKF(hr == S_OK && d == 2.5, "SetTimelineMarkerTimer(2.5) round-trips (%#lx, %f)", hr, d);
    hr = IMFMediaEngineEx_CancelTimelineMarkerTimer(e);
    IMFMediaEngineEx_GetTimelineMarkerTimer(e, &d);
    CHECKF(hr == S_OK && is_nan(d), "CancelTimelineMarkerTimer resets to NaN (%#lx)", hr);
    hr = IMFMediaEngineEx_GetTimelineMarkerTimer(e, NULL);
    CHECKF(hr == E_POINTER, "GetTimelineMarkerTimer(NULL) is E_POINTER (%#lx)", hr);

    /* stereo 3D */
    check(IMFMediaEngineEx_IsStereo3D(e) == FALSE, "IsStereo3D is FALSE");
    {
        MF_MEDIA_ENGINE_S3D_PACKING_MODE pm = 9;
        MF3DVideoOutputType rm = 9;

        hr = IMFMediaEngineEx_GetStereo3DFramePackingMode(e, &pm);
        CHECKF(hr == S_OK && pm == 0, "packing mode default is NONE (%#lx, %d)", hr, pm);
        for (i = 0; i < ARRAY_SIZE(packing); i++)
        {
            MF_MEDIA_ENGINE_S3D_PACKING_MODE old = pm;
            hr = IMFMediaEngineEx_SetStereo3DFramePackingMode(e, packing[i].v);
            IMFMediaEngineEx_GetStereo3DFramePackingMode(e, &pm);
            CHECKF(hr == packing[i].hr && pm == (hr == S_OK ? packing[i].v : old),
                   "SetStereo3DFramePackingMode(%d) is %#lx, Get %d (%#lx)", packing[i].v, packing[i].hr, pm, hr);
        }
        hr = IMFMediaEngineEx_GetStereo3DRenderMode(e, &rm);
        CHECKF(hr == S_OK && rm == MF3DVideoOutputType_BaseView, "render mode default is BaseView (%#lx, %d)", hr, rm);
        for (i = 0; i < ARRAY_SIZE(rendermode); i++)
        {
            MF3DVideoOutputType old = rm;
            hr = IMFMediaEngineEx_SetStereo3DRenderMode(e, rendermode[i].v);
            IMFMediaEngineEx_GetStereo3DRenderMode(e, &rm);
            CHECKF(hr == rendermode[i].hr && rm == (hr == S_OK ? rendermode[i].v : old),
                   "SetStereo3DRenderMode(%d) is %#lx, Get %d (%#lx)", rendermode[i].v, rendermode[i].hr, rm, hr);
        }
        hr = IMFMediaEngineEx_GetStereo3DFramePackingMode(e, NULL);
        CHECKF(hr == E_POINTER, "GetStereo3DFramePackingMode(NULL) is E_POINTER (%#lx)", hr);
        hr = IMFMediaEngineEx_GetStereo3DRenderMode(e, NULL);
        CHECKF(hr == E_POINTER, "GetStereo3DRenderMode(NULL) is E_POINTER (%#lx)", hr);
    }

    /* swap chain, mirror, timer */
    {
        HANDLE h = (HANDLE)1;
        hr = IMFMediaEngineEx_GetVideoSwapchainHandle(e, &h);
        CHECKF(hr == MF_E_INVALIDREQUEST && h == NULL, "GetVideoSwapchainHandle without windowless mode is MF_E_INVALIDREQUEST (%#lx)", hr);
        hr = IMFMediaEngineEx_GetVideoSwapchainHandle(e, NULL);
        CHECKF(hr == E_POINTER, "GetVideoSwapchainHandle(NULL) is E_POINTER (%#lx)", hr);
        hr = IMFMediaEngineEx_EnableWindowlessSwapchainMode(e, TRUE);
        CHECKF(hr == S_OK, "EnableWindowlessSwapchainMode(TRUE) is S_OK (%#lx)", hr);
        hr = IMFMediaEngineEx_EnableWindowlessSwapchainMode(e, FALSE);
        CHECKF(hr == S_OK, "EnableWindowlessSwapchainMode(FALSE) is S_OK (%#lx)", hr);
        hr = IMFMediaEngineEx_EnableHorizontalMirrorMode(e, TRUE);
        CHECKF(hr == S_OK, "EnableHorizontalMirrorMode is S_OK (%#lx)", hr);
        hr = IMFMediaEngineEx_EnableTimeUpdateTimer(e, TRUE);
        CHECKF(hr == S_OK, "EnableTimeUpdateTimer is S_OK (%#lx)", hr);
    }

    /* GetService */
    {
        IMFGetService *gs;
        GUID unknown = {0x5eed0002, 0x1234, 0x4321, {1, 2, 3, 4, 5, 6, 7, 8}};
        void *obj = (void *)1;

        hr = IMFMediaEngineEx_QueryInterface(e, &IID_IMFGetService, (void **)&gs);
        if (hr == S_OK)
        {
            hr = IMFGetService_GetService(gs, &unknown, &IID_IUnknown, &obj);
            CHECKF(hr == MF_E_UNSUPPORTED_SERVICE && obj == NULL, "GetService(unknown) is MF_E_UNSUPPORTED_SERVICE (%#lx)", hr);
            hr = IMFGetService_GetService(gs, &unknown, &IID_IUnknown, NULL);
            CHECKF(hr == E_POINTER, "GetService(NULL out) is E_POINTER (%#lx)", hr);
            IMFGetService_Release(gs);
        }
        else
            check(0, "the engine has IMFGetService");
    }

    /* SetSourceElements */
    {
        struct src_elements se = {{&se_vtbl}, {L"c:\\no-such-1.wav", L"c:\\no-such-2.wav"}, 2};
        struct src_elements empty = {{&se_vtbl}, {0}, 0};
        BSTR url = NULL;

        hr = IMFMediaEngineEx_SetSourceElements(e, NULL);
        CHECKF(hr == E_POINTER, "SetSourceElements(NULL) is E_POINTER (%#lx)", hr);
        hr = IMFMediaEngineEx_SetSourceElements(e, &se.iface);
        CHECKF(hr == S_OK, "SetSourceElements(two URLs) is S_OK (%#lx)", hr);
        hr = IMFMediaEngineEx_GetCurrentSource(e, &url);
        CHECKF(hr == S_OK && url && !wcscmp(url, L"c:\\no-such-1.wav"), "the first element became the current source (%#lx, %ls)", hr, url ? url : L"(null)");
        SysFreeString(url);
        hr = IMFMediaEngineEx_Load(e);
        url = NULL;
        IMFMediaEngineEx_GetCurrentSource(e, &url);
        CHECKF(hr == S_OK && url && !wcscmp(url, L"c:\\no-such-1.wav"), "Load keeps the current source (%#lx)", hr);
        SysFreeString(url);
        hr = IMFMediaEngineEx_SetSourceElements(e, &empty.iface);
        url = (BSTR)1;
        IMFMediaEngineEx_GetCurrentSource(e, &url);
        CHECKF(hr == S_OK && url == NULL, "SetSourceElements(empty list) clears the source (%#lx)", hr);
    }
}

static void test_loaded(const WCHAR *file)
{
    IMFMediaEngineEx *e = create_engine();
    DWORD n = 0, i;
    BOOL b = FALSE, prot = TRUE, sel, initial;
    BSTR url;
    HRESULT hr;

    if (!e) { check(0, "loaded: engine created"); return; }
    loaded_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    url = SysAllocString(file);
    hr = IMFMediaEngineEx_SetSource(e, url);
    SysFreeString(url);
    CHECKF(hr == S_OK, "loaded: SetSource(AVI file) is S_OK (%#lx)", hr);
    WaitForSingleObject(loaded_event, 30000);
    hr = IMFMediaEngineEx_GetNumberOfStreams(e, &n);
    if (FAILED(hr) || !n)
    {
        printf("NOTE  the AVI file did not load (%#lx): the loaded-source checks are skipped\n", hr);
        IMFMediaEngineEx_Shutdown(e);
        IMFMediaEngineEx_Release(e);
        return;
    }
    hr = IMFMediaEngineEx_IsProtected(e, &prot);
    CHECKF(hr == S_OK && prot == FALSE, "loaded: IsProtected is FALSE (%#lx)", hr);
    hr = IMFMediaEngineEx_GetStreamSelection(e, 0, &sel);
    CHECKF(hr == S_OK, "loaded: GetStreamSelection(0) is S_OK (%#lx)", hr);
    initial = sel;
    hr = IMFMediaEngineEx_GetStreamSelection(e, n, &sel);
    CHECKF(hr == E_INVALIDARG || hr == MF_E_INVALIDSTREAMNUMBER, "loaded: GetStreamSelection(out of range) fails (%#lx)", hr);
    hr = IMFMediaEngineEx_SetStreamSelection(e, n, TRUE);
    CHECKF(hr == E_INVALIDARG || hr == MF_E_INVALIDSTREAMNUMBER, "loaded: SetStreamSelection(out of range) fails (%#lx)", hr);
    hr = IMFMediaEngineEx_SetStreamSelection(e, 0, !initial);
    CHECKF(hr == S_OK, "loaded: SetStreamSelection(0, %d) is S_OK (%#lx)", !initial, hr);
    sel = initial;
    IMFMediaEngineEx_GetStreamSelection(e, 0, &sel);
    CHECKF(sel == !initial, "loaded: the selection round-trips before Apply (%d)", sel);
    hr = IMFMediaEngineEx_ApplyStreamSelections(e);
    CHECKF(hr == S_OK, "loaded: ApplyStreamSelections is S_OK (%#lx)", hr);
    sel = initial;
    IMFMediaEngineEx_GetStreamSelection(e, 0, &sel);
    CHECKF(sel == !initial, "loaded: the selection stays after Apply (%d)", sel);
    IMFMediaEngineEx_SetStreamSelection(e, 0, initial);
    IMFMediaEngineEx_ApplyStreamSelections(e);
    sel = !initial;
    IMFMediaEngineEx_GetStreamSelection(e, 0, &sel);
    CHECKF(sel == initial, "loaded: the stream goes back to its first selection (%d)", sel);
    check(IMFMediaEngineEx_IsPlaybackRateSupported(e, 1.0) == TRUE, "loaded: IsPlaybackRateSupported(1.0) is TRUE");
    hr = IMFMediaEngineEx_FrameStep(e, TRUE);
    CHECKF(hr == MF_E_INVALIDREQUEST || hr == E_NOTIMPL, "loaded, paused: FrameStep is MF_E_INVALIDREQUEST (audio only) or E_NOTIMPL (%#lx)", hr);
    check(IMFMediaEngineEx_IsSeeking(e) == FALSE, "loaded: IsSeeking is FALSE");
    IMFMediaEngineEx_Shutdown(e);
    IMFMediaEngineEx_Release(e);
}

static void test_shutdown(IMFMediaEngineEx *e)
{
    IMFMediaTimeRange *range = NULL;
    MF_MEDIA_ENGINE_S3D_PACKING_MODE pm;
    MF3DVideoOutputType rm;
    PROPVARIANT pv;
    HRESULT hr;
    BOOL b;
    double d;
    HANDLE h;
    MFVideoNormalizedRect src = {0, 0, 1, 1};
    struct src_elements se = {{&se_vtbl}, {L"c:\\x.wav"}, 1};
    int i;
    struct { const char *name; HRESULT hr; } r[] = {
        {"Load", IMFMediaEngineEx_Load(e)},
        {"GetPlayed", IMFMediaEngineEx_GetPlayed(e, &range)},
        {"SetSourceElements", IMFMediaEngineEx_SetSourceElements(e, &se.iface)},
        {"SetBalance", IMFMediaEngineEx_SetBalance(e, 0.25)},
        {"GetStatistics", IMFMediaEngineEx_GetStatistics(e, 0, &pv)},
        {"UpdateVideoStream", IMFMediaEngineEx_UpdateVideoStream(e, &src, NULL, NULL)},
        {"FrameStep", IMFMediaEngineEx_FrameStep(e, TRUE)},
        {"GetStreamSelection", IMFMediaEngineEx_GetStreamSelection(e, 0, &b)},
        {"SetStreamSelection", IMFMediaEngineEx_SetStreamSelection(e, 0, TRUE)},
        {"ApplyStreamSelections", IMFMediaEngineEx_ApplyStreamSelections(e)},
        {"IsProtected", IMFMediaEngineEx_IsProtected(e, &b)},
        {"SetTimelineMarkerTimer", IMFMediaEngineEx_SetTimelineMarkerTimer(e, 1.0)},
        {"GetTimelineMarkerTimer", IMFMediaEngineEx_GetTimelineMarkerTimer(e, &d)},
        {"CancelTimelineMarkerTimer", IMFMediaEngineEx_CancelTimelineMarkerTimer(e)},
        {"GetStereo3DFramePackingMode", IMFMediaEngineEx_GetStereo3DFramePackingMode(e, &pm)},
        {"SetStereo3DFramePackingMode", IMFMediaEngineEx_SetStereo3DFramePackingMode(e, 1)},
        {"GetStereo3DRenderMode", IMFMediaEngineEx_GetStereo3DRenderMode(e, &rm)},
        {"SetStereo3DRenderMode", IMFMediaEngineEx_SetStereo3DRenderMode(e, 1)},
        {"EnableWindowlessSwapchainMode", IMFMediaEngineEx_EnableWindowlessSwapchainMode(e, TRUE)},
        {"GetVideoSwapchainHandle", IMFMediaEngineEx_GetVideoSwapchainHandle(e, &h)},
        {"EnableHorizontalMirrorMode", IMFMediaEngineEx_EnableHorizontalMirrorMode(e, TRUE)},
        {"EnableTimeUpdateTimer", IMFMediaEngineEx_EnableTimeUpdateTimer(e, TRUE)},
    };
    IMFGetService *gs;
    GUID unknown = {0x5eed0003, 0x1234, 0x4321, {1, 2, 3, 4, 5, 6, 7, 8}};
    void *obj;

    for (i = 0; i < ARRAY_SIZE(r); i++)
        CHECKF(r[i].hr == MF_E_SHUTDOWN, "after Shutdown: %s is MF_E_SHUTDOWN (%#lx)", r[i].name, r[i].hr);
    check(IMFMediaEngineEx_IsSeeking(e) == FALSE, "after Shutdown: IsSeeking is FALSE");
    check(IMFMediaEngineEx_GetStartTime(e) == 0.0, "after Shutdown: GetStartTime is 0");
    check(IMFMediaEngineEx_IsPlaybackRateSupported(e, 1.0) == FALSE, "after Shutdown: IsPlaybackRateSupported is FALSE");
    /* accessors that stay available */
    check(IMFMediaEngineEx_SetAutoPlay(e, TRUE) == S_OK && IMFMediaEngineEx_GetAutoPlay(e), "after Shutdown: autoplay still round-trips");
    check(IMFMediaEngineEx_SetLoop(e, TRUE) == S_OK && IMFMediaEngineEx_GetLoop(e), "after Shutdown: loop still round-trips");
    d = IMFMediaEngineEx_GetBalance(e);
    CHECKF(d == 1.0 || d == 0.0 || d == -1.0 || d == 0.5, "after Shutdown: GetBalance still answers (%f)", d);
    if (SUCCEEDED(IMFMediaEngineEx_QueryInterface(e, &IID_IMFGetService, (void **)&gs)))
    {
        HRESULT hr2 = IMFGetService_GetService(gs, &unknown, &IID_IUnknown, &obj);
        CHECKF(hr2 == MF_E_SHUTDOWN, "after Shutdown: GetService is MF_E_SHUTDOWN (%#lx)", hr2);
        IMFGetService_Release(gs);
    }
}

static void test_factory(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    IClassFactory *factory = NULL;
    HMODULE mod = LoadLibraryA("mfmediaengine.dll");
    HRESULT hr;

    get_class_object = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
    if (!get_class_object) { check(0, "mfmediaengine exports DllGetClassObject"); return; }
    hr = get_class_object(&CLSID_MFMediaEngineClassFactory, &IID_IClassFactory, (void **)&factory);
    if (hr != S_OK) { CHECKF(0, "class factory (%#lx)", hr); return; }
    hr = IClassFactory_LockServer(factory, TRUE);
    CHECKF(hr == S_OK, "LockServer(TRUE) is S_OK (%#lx)", hr);
    hr = IClassFactory_LockServer(factory, FALSE);
    CHECKF(hr == S_OK, "LockServer(FALSE) is S_OK (%#lx)", hr);
    IClassFactory_Release(factory);
}

int main(int argc, char **argv)
{
    IMFMediaEngineEx *e;
    WCHAR file[MAX_PATH];

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    MultiByteToWideChar(CP_ACP, 0, argc > 1 ? argv[1] : "clip.avi", -1, file, MAX_PATH);

    test_factory();
    e = create_engine();
    if (!e) { puts("FAIL  engine created"); return 1; }
    test_idle(e);
    IMFMediaEngineEx_Shutdown(e);
    test_shutdown(e);
    IMFMediaEngineEx_Release(e);
    test_loaded(file);

    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
