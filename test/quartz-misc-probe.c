/* Probe for patches 2808-2809: quartz AmpFactorToDB / DBToAmpFactor,
 * IFilterMapper2::RegisterFilter REGFILTER2 versions, the filter graph's
 * IMediaSeeking time formats and the default EC_REPAINT handling. */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <stdio.h>

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_HR(hr, exp, what) CHECK((hr) == (exp), "%s: got %#lx, expected %#lx", what, (long)(hr), (long)(exp))

static void test_amp(void)
{
    typedef int (WINAPI *conv_t)(int);
    HMODULE q = LoadLibraryA("quartz.dll");
    conv_t to_db = (conv_t)GetProcAddress(q, "AmpFactorToDB");
    conv_t to_amp = (conv_t)GetProcAddress(q, "DBToAmpFactor");
    static const struct { int amp, db; } a2d[] = {
        {100, 0}, {200, 602}, {50, -602}, {10, -2000}, {1, -4000}, {1000, 2000}, {0, -10000}, {-5, -10000}, {141, 298},
    };
    static const struct { int db, amp; } d2a[] = {
        {0, 100}, {-2000, 10}, {-602, 50}, {602, 200}, {2000, 1000}, {-10000, 0}, {-20000, 0}, {-6000, 0}, {-4000, 1},
    };
    unsigned i;

    CHECK(to_db && to_amp, "exports missing");
    if (!to_db || !to_amp) return;
    for (i = 0; i < sizeof(a2d)/sizeof(a2d[0]); i++)
        CHECK(to_db(a2d[i].amp) == a2d[i].db, "AmpFactorToDB(%d) = %d, expected %d", a2d[i].amp, to_db(a2d[i].amp), a2d[i].db);
    for (i = 0; i < sizeof(d2a)/sizeof(d2a[0]); i++)
        CHECK(to_amp(d2a[i].db) == d2a[i].amp, "DBToAmpFactor(%d) = %d, expected %d", d2a[i].db, to_amp(d2a[i].db), d2a[i].amp);
}

static void test_register(void)
{
    static const GUID clsid = {0x5a8d1e00, 0x1234, 0x4abc, {1,2,3,4,5,6,7,8}};
    static const GUID cat = {0x5a8d1e01, 0x1234, 0x4abc, {1,2,3,4,5,6,7,8}};
    IFilterMapper2 *mapper;
    REGFILTER2 rf;
    REGFILTERPINS2 pin;
    REGPINTYPES type = { &MEDIATYPE_Video, &MEDIASUBTYPE_NULL };
    REGFILTERPINS pin1;
    IMoniker *mon;
    HRESULT hr;
    unsigned i;
    static const DWORD bad_versions[] = {0, 3, 4, 0xffffffff};

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper2, (void **)&mapper);
    CHECK_HR(hr, S_OK, "create FilterMapper2");
    if (hr != S_OK) return;

    memset(&pin, 0, sizeof(pin));
    pin.nMediaTypes = 1; pin.lpMediaType = &type; pin.cInstances = 1;
    memset(&rf, 0, sizeof(rf));
    rf.dwVersion = 2; rf.dwMerit = MERIT_NORMAL; rf.cPins2 = 1; rf.rgPins2 = &pin;
    hr = IFilterMapper2_RegisterFilter(mapper, &clsid, L"SG probe v2", &mon, &cat, NULL, &rf);
    CHECK_HR(hr, S_OK, "RegisterFilter v2");
    if (hr == S_OK) { IMoniker_Release(mon); hr = IFilterMapper2_UnregisterFilter(mapper, &cat, NULL, &clsid); CHECK_HR(hr, S_OK, "UnregisterFilter v2"); }

    memset(&pin1, 0, sizeof(pin1));
    pin1.strName = (WCHAR *)L"in"; pin1.nMediaTypes = 1; pin1.lpMediaType = &type;
    memset(&rf, 0, sizeof(rf));
    rf.dwVersion = 1; rf.dwMerit = MERIT_NORMAL; rf.cPins = 1; rf.rgPins = &pin1;
    hr = IFilterMapper2_RegisterFilter(mapper, &clsid, L"SG probe v1", NULL, &cat, NULL, &rf);
    CHECK_HR(hr, S_OK, "RegisterFilter v1");
    if (hr == S_OK) { hr = IFilterMapper2_UnregisterFilter(mapper, &cat, NULL, &clsid); CHECK_HR(hr, S_OK, "UnregisterFilter v1"); }

    for (i = 0; i < sizeof(bad_versions)/sizeof(bad_versions[0]); i++)
    {
        memset(&rf, 0, sizeof(rf));
        rf.dwVersion = bad_versions[i]; rf.cPins = 1; rf.rgPins = &pin1;
        mon = (IMoniker *)0x1234;
        hr = IFilterMapper2_RegisterFilter(mapper, &clsid, L"SG probe bad", &mon, &cat, NULL, &rf);
        CHECK(hr == E_INVALIDARG, "RegisterFilter version %lu: got %#lx", (long)bad_versions[i], (long)hr);
        CHECK(mon == (IMoniker *)0x1234, "moniker written for version %lu", (long)bad_versions[i]);
    }
    IFilterMapper2_Release(mapper);
}

/* fake pins for EC_REPAINT routing */
static IMediaEventSink sink;
static IPin fake_pin, peer_pin;
static volatile LONG notified; static volatile LONG notify_code; static volatile LONG_PTR notify_p1;
static HRESULT WINAPI sink_qi(IMediaEventSink *i, REFIID r, void **o) { if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IMediaEventSink)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI sink_ar(IMediaEventSink *i) { return 2; }
static ULONG WINAPI sink_rel(IMediaEventSink *i) { return 1; }
static HRESULT WINAPI sink_notify(IMediaEventSink *i, LONG c, LONG_PTR p1, LONG_PTR p2) { notify_code = c; notify_p1 = p1; InterlockedIncrement(&notified); return S_OK; }
static IMediaEventSinkVtbl sink_vtbl = { sink_qi, sink_ar, sink_rel, sink_notify };
static HRESULT WINAPI pin_qi(IPin *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IPin)) { *o = i; return S_OK; }
    if (i == &peer_pin && IsEqualGUID(r, &IID_IMediaEventSink)) { *o = &sink; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI pin_ar(IPin *i) { return 2; }
static ULONG WINAPI pin_rel(IPin *i) { return 1; }
static HRESULT WINAPI pin_connectedto(IPin *i, IPin **p) { if (i == &fake_pin) { *p = &peer_pin; return S_OK; } *p = NULL; return VFW_E_NOT_CONNECTED; }
static IPinVtbl pin_vtbl = { pin_qi, pin_ar, pin_rel };

static void test_graph(void)
{
    static const GUID *unsupported[] = { &TIME_FORMAT_FRAME, &TIME_FORMAT_SAMPLE, &TIME_FORMAT_FIELD, &TIME_FORMAT_BYTE };
    IGraphBuilder *graph;
    IMediaSeeking *seeking;
    IMediaEventSink *es;
    IMediaEventEx *ev;
    LONGLONG t;
    GUID f;
    HRESULT hr;
    LONG code; LONG_PTR p1, p2;
    unsigned i;

    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    CHECK_HR(hr, S_OK, "create graph");
    if (hr != S_OK) return;
    IGraphBuilder_QueryInterface(graph, &IID_IMediaSeeking, (void **)&seeking);

    hr = IMediaSeeking_SetTimeFormat(seeking, &TIME_FORMAT_MEDIA_TIME); CHECK_HR(hr, S_OK, "SetTimeFormat(media time)");
    hr = IMediaSeeking_SetTimeFormat(seeking, &TIME_FORMAT_NONE); CHECK_HR(hr, S_OK, "SetTimeFormat(none)");
    for (i = 0; i < 4; i++)
    {
        hr = IMediaSeeking_SetTimeFormat(seeking, unsupported[i]); CHECK_HR(hr, E_NOTIMPL, "SetTimeFormat(unsupported)");
    }
    hr = IMediaSeeking_SetTimeFormat(seeking, NULL); CHECK_HR(hr, E_POINTER, "SetTimeFormat(NULL)");
    hr = IMediaSeeking_GetTimeFormat(seeking, &f);
    CHECK(hr == S_OK && IsEqualGUID(&f, &TIME_FORMAT_MEDIA_TIME), "time format changed");

    t = 0xdeadbeef;
    hr = IMediaSeeking_ConvertTimeFormat(seeking, &t, NULL, 0x123456789a, NULL);
    CHECK(hr == S_OK && t == 0x123456789a, "convert media->media: %#lx %s", (long)hr, "");
    for (i = 0; i < 4; i++)
    {
        t = 0xdeadbeef;
        hr = IMediaSeeking_ConvertTimeFormat(seeking, &t, unsupported[i], 5, &TIME_FORMAT_MEDIA_TIME);
        CHECK(hr == E_NOTIMPL && t == 0xdeadbeef, "convert media->other: %#lx", (long)hr);
        hr = IMediaSeeking_ConvertTimeFormat(seeking, &t, NULL, 5, unsupported[i]);
        CHECK(hr == E_NOTIMPL && t == 0xdeadbeef, "convert other->media: %#lx", (long)hr);
    }
    hr = IMediaSeeking_ConvertTimeFormat(seeking, &t, &TIME_FORMAT_NONE, 5, NULL);
    CHECK_HR(hr, E_NOTIMPL, "convert to none");

    /* EC_REPAINT: routed to the upstream pin's IMediaEventSink, and not given to the application */
    fake_pin.lpVtbl = &pin_vtbl; peer_pin.lpVtbl = &pin_vtbl; sink.lpVtbl = &sink_vtbl;
    pin_vtbl.ConnectedTo = pin_connectedto;
    IGraphBuilder_QueryInterface(graph, &IID_IMediaEventSink, (void **)&es);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaEventEx, (void **)&ev);
    hr = IMediaEventSink_Notify(es, EC_REPAINT, (LONG_PTR)&fake_pin, 0);
    CHECK_HR(hr, S_OK, "Notify(EC_REPAINT)");
    for (i = 0; i < 100 && !notified; i++) Sleep(20);
    CHECK(notified == 1, "upstream sink notified %ld times", (long)notified);
    CHECK(notify_code == EC_REPAINT && notify_p1 == (LONG_PTR)&fake_pin, "sink got %#lx %#Ix", (long)notify_code, notify_p1);
    hr = IMediaEventEx_GetEvent(ev, &code, &p1, &p2, 300);
    CHECK(hr != S_OK, "EC_REPAINT reached the application (code %#lx)", (long)code);
    if (hr == S_OK) IMediaEventEx_FreeEventParams(ev, code, p1, p2);

    /* a pin that is not connected: the graph's own handling (stopped, no filters) still eats the event */
    hr = IMediaEventSink_Notify(es, EC_REPAINT, (LONG_PTR)&peer_pin, 0);
    CHECK_HR(hr, S_OK, "Notify(EC_REPAINT, unconnected pin)");
    Sleep(200);
    CHECK(notified == 1, "sink notified for an unconnected pin");
    /* the Pause done by the default handling may raise EC_CLOCK_CHANGED; EC_REPAINT must not appear */
    while ((hr = IMediaEventEx_GetEvent(ev, &code, &p1, &p2, 300)) == S_OK)
    {
        CHECK(code != EC_REPAINT, "EC_REPAINT (unconnected) reached the application");
        IMediaEventEx_FreeEventParams(ev, code, p1, p2);
    }

    /* once the default handling is cancelled the application gets it */
    hr = IMediaEventEx_CancelDefaultHandling(ev, EC_REPAINT);
    CHECK_HR(hr, S_OK, "CancelDefaultHandling");
    hr = IMediaEventSink_Notify(es, EC_REPAINT, 0, 0);
    hr = IMediaEventEx_GetEvent(ev, &code, &p1, &p2, 1000);
    CHECK(hr == S_OK && code == EC_REPAINT, "cancelled EC_REPAINT: %#lx", (long)hr);

    IMediaEventEx_Release(ev); IMediaEventSink_Release(es); IMediaSeeking_Release(seeking); IGraphBuilder_Release(graph);
}

int main(void)
{
    CoInitialize(NULL);
    test_amp();
    test_register();
    test_graph();
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    CoUninitialize();
    return failures != 0;
}
