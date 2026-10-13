/* Probe for patches/sg/4578: the filter graph forwards IBasicAudio,
 * IBasicVideo, IBasicVideo2 and IVideoWindow to the filter that has them,
 * all four at once (its interface cache held three), and
 * IVideoFrameStep::Step without a filter that can step returns
 * VFW_E_FRAME_STEP_UNSUPPORTED. The filter is the probe's own: it has every
 * one of the four interfaces and no pins. Table driven; lines "ok"/"FAIL",
 * last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

struct filter
{
    IBaseFilter IBaseFilter_iface;
    IBasicAudio IBasicAudio_iface;
    IBasicVideo2 IBasicVideo2_iface;
    IVideoWindow IVideoWindow_iface;
    LONG ref;
    IFilterGraph *graph;
};
static struct filter filter;

static HRESULT WINAPI f_qi(IBaseFilter *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPersist) || IsEqualGUID(iid, &IID_IMediaFilter)
            || IsEqualGUID(iid, &IID_IBaseFilter)) *out = &filter.IBaseFilter_iface;
    else if (IsEqualGUID(iid, &IID_IBasicAudio)) *out = &filter.IBasicAudio_iface;
    else if (IsEqualGUID(iid, &IID_IBasicVideo) || IsEqualGUID(iid, &IID_IBasicVideo2)) *out = &filter.IBasicVideo2_iface;
    else if (IsEqualGUID(iid, &IID_IVideoWindow)) *out = &filter.IVideoWindow_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&filter.ref);
    return S_OK;
}
static ULONG WINAPI f_addref(IBaseFilter *iface) { return InterlockedIncrement(&filter.ref); }
static ULONG WINAPI f_release(IBaseFilter *iface) { return InterlockedDecrement(&filter.ref); }
static HRESULT WINAPI f_classid(IBaseFilter *iface, CLSID *clsid) { memset(clsid, 0, sizeof(*clsid)); return S_OK; }
static HRESULT WINAPI f_stop(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI f_pause(IBaseFilter *iface) { return S_OK; }
static HRESULT WINAPI f_run(IBaseFilter *iface, REFERENCE_TIME t) { return S_OK; }
static HRESULT WINAPI f_state(IBaseFilter *iface, DWORD ms, FILTER_STATE *s) { *s = State_Stopped; return S_OK; }
static HRESULT WINAPI f_setsync(IBaseFilter *iface, IReferenceClock *c) { return S_OK; }
static HRESULT WINAPI f_getsync(IBaseFilter *iface, IReferenceClock **c) { *c = NULL; return S_OK; }
static HRESULT WINAPI f_enumpins(IBaseFilter *iface, IEnumPins **e) { *e = NULL; return E_NOTIMPL; }
static HRESULT WINAPI f_findpin(IBaseFilter *iface, const WCHAR *id, IPin **p) { *p = NULL; return VFW_E_NOT_FOUND; }
static HRESULT WINAPI f_info(IBaseFilter *iface, FILTER_INFO *info)
{
    wcscpy(info->achName, L"probe");
    info->pGraph = filter.graph;
    if (info->pGraph) IFilterGraph_AddRef(info->pGraph);
    return S_OK;
}
static HRESULT WINAPI f_join(IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name) { filter.graph = graph; return S_OK; }
static HRESULT WINAPI f_vendor(IBaseFilter *iface, WCHAR **info) { return E_NOTIMPL; }
static IBaseFilterVtbl f_vtbl = {f_qi, f_addref, f_release, f_classid, f_stop, f_pause, f_run, f_state, f_setsync,
                                 f_getsync, f_enumpins, f_findpin, f_info, f_join, f_vendor};

/* only the members the graph is asked for here are filled in */
static HRESULT WINAPI ba_qi(IBasicAudio *iface, REFIID iid, void **out) { return f_qi(NULL, iid, out); }
static ULONG WINAPI ba_addref(IBasicAudio *iface) { return f_addref(NULL); }
static ULONG WINAPI ba_release(IBasicAudio *iface) { return f_release(NULL); }
static HRESULT WINAPI ba_get_volume(IBasicAudio *iface, LONG *v) { *v = -1234; return S_OK; }
static IBasicAudioVtbl ba_vtbl = {.QueryInterface = ba_qi, .AddRef = ba_addref, .Release = ba_release, .get_Volume = ba_get_volume};

static HRESULT WINAPI bv_qi(IBasicVideo2 *iface, REFIID iid, void **out) { return f_qi(NULL, iid, out); }
static ULONG WINAPI bv_addref(IBasicVideo2 *iface) { return f_addref(NULL); }
static ULONG WINAPI bv_release(IBasicVideo2 *iface) { return f_release(NULL); }
static HRESULT WINAPI bv_get_source_width(IBasicVideo2 *iface, LONG *w) { *w = 640; return S_OK; }
static HRESULT WINAPI bv_aspect(IBasicVideo2 *iface, LONG *x, LONG *y) { *x = 16; *y = 9; return S_OK; }
static IBasicVideo2Vtbl bv_vtbl = {.QueryInterface = bv_qi, .AddRef = bv_addref, .Release = bv_release,
                                   .get_SourceWidth = bv_get_source_width, .GetPreferredAspectRatio = bv_aspect};

static HRESULT WINAPI vw_qi(IVideoWindow *iface, REFIID iid, void **out) { return f_qi(NULL, iid, out); }
static ULONG WINAPI vw_addref(IVideoWindow *iface) { return f_addref(NULL); }
static ULONG WINAPI vw_release(IVideoWindow *iface) { return f_release(NULL); }
static HRESULT WINAPI vw_get_visible(IVideoWindow *iface, LONG *v) { *v = OATRUE; return S_OK; }
static IVideoWindowVtbl vw_vtbl = {.QueryInterface = vw_qi, .AddRef = vw_addref, .Release = vw_release, .get_Visible = vw_get_visible};

/* the four forwarded interfaces, asked in each order a program might use */
static void ask(IGraphBuilder *graph, int which)
{
    IBasicAudio *ba; IBasicVideo *bv; IBasicVideo2 *bv2; IVideoWindow *vw;
    LONG v = 0, x = 0, y = 0;
    HRESULT hr;

    switch (which)
    {
    case 0:
        IGraphBuilder_QueryInterface(graph, &IID_IBasicAudio, (void **)&ba);
        hr = IBasicAudio_get_Volume(ba, &v);
        check(hr == S_OK && v == -1234, "IBasicAudio::get_Volume forwarded: %#lx %ld", hr, v);
        IBasicAudio_Release(ba);
        break;
    case 1:
        IGraphBuilder_QueryInterface(graph, &IID_IBasicVideo, (void **)&bv);
        hr = IBasicVideo_get_SourceWidth(bv, &v);
        check(hr == S_OK && v == 640, "IBasicVideo::get_SourceWidth forwarded: %#lx %ld", hr, v);
        IBasicVideo_Release(bv);
        break;
    case 2:
        IGraphBuilder_QueryInterface(graph, &IID_IBasicVideo2, (void **)&bv2);
        hr = IBasicVideo2_GetPreferredAspectRatio(bv2, &x, &y);
        check(hr == S_OK && x == 16 && y == 9, "IBasicVideo2::GetPreferredAspectRatio forwarded: %#lx %ld:%ld", hr, x, y);
        IBasicVideo2_Release(bv2);
        break;
    case 3:
        IGraphBuilder_QueryInterface(graph, &IID_IVideoWindow, (void **)&vw);
        hr = IVideoWindow_get_Visible(vw, &v);
        check(hr == S_OK && v == OATRUE, "IVideoWindow::get_Visible forwarded: %#lx %ld", hr, v);
        IVideoWindow_Release(vw);
        break;
    }
}

int main(int argc, char **argv)
{
    static const int orders[][4] = {{0, 1, 2, 3}, {3, 2, 1, 0}, {2, 0, 3, 1}, {1, 3, 0, 2}};
    IGraphBuilder *graph;
    IVideoFrameStep *step;
    unsigned int i, j;
    HRESULT hr;

    CoInitialize(NULL);
    filter.IBaseFilter_iface.lpVtbl = &f_vtbl;
    filter.IBasicAudio_iface.lpVtbl = &ba_vtbl;
    filter.IBasicVideo2_iface.lpVtbl = &bv_vtbl;
    filter.IVideoWindow_iface.lpVtbl = &vw_vtbl;
    filter.ref = 1;

    for (i = 0; i < ARRAYSIZE(orders); i++)
    {
        hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
        check(hr == S_OK, "order %u: filter graph %#lx", i, hr);
        if (FAILED(hr)) break;
        hr = IGraphBuilder_AddFilter(graph, &filter.IBaseFilter_iface, L"probe");
        check(hr == S_OK, "AddFilter %#lx", hr);
        for (j = 0; j < 4; j++) ask(graph, orders[i][j]);
        for (j = 0; j < 4; j++) ask(graph, orders[i][j]);  /* from the cache */
        IGraphBuilder_Release(graph);
        filter.graph = NULL;
    }

    /* frame stepping without a filter that steps */
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    IGraphBuilder_QueryInterface(graph, &IID_IVideoFrameStep, (void **)&step);
    hr = IVideoFrameStep_Step(step, 1, NULL);
    check(hr == VFW_E_FRAME_STEP_UNSUPPORTED, "Step with nothing to step: %#lx", hr);
    hr = IVideoFrameStep_CanStep(step, 0, NULL);
    check(hr == S_FALSE, "CanStep with nothing to step: %#lx", hr);
    IGraphBuilder_AddFilter(graph, &filter.IBaseFilter_iface, L"probe");
    hr = IVideoFrameStep_Step(step, 2, NULL);
    check(hr == VFW_E_FRAME_STEP_UNSUPPORTED, "Step with a filter that cannot step: %#lx", hr);
    IVideoFrameStep_Release(step);
    IGraphBuilder_Release(graph);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
