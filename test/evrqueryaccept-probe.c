/* A format check on the connected EVR input pin keeps the presenter's
 * services (patches/sg/6548). A decoder asks QueryAccept while streaming
 * before a dynamic format change (LAV Video with 4:4:4 H.264, RGB32 out); the
 * EVR filter brought its services up for the check and released them after
 * it, so the presenter got ReleaseServicePointers mid-stream -- MPC-HC's
 * presenter stops its threads there, and the picture stayed black.
 * The probe gives the EVR a presenter that counts InitServicePointers and
 * ReleaseServicePointers (wrapping MFCreateVideoPresenter's), renders an
 * uncompressed RGB32 AVI into it, then calls QueryAccept on the connected
 * input pin with the connection's own type. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <dshow.h>
#include <d3d9.h>
#include <evr.h>
#include <mfidl.h>
#include <stdio.h>

DEFINE_GUID(CLSID_EnhancedVideoRenderer_, 0xfa10746c, 0x9b63, 0x4b6c, 0xbc, 0x49, 0xfc, 0x30, 0x0e, 0xa5, 0xf2, 0x56);

struct wrap
{
    IMFVideoPresenter IMFVideoPresenter_iface;
    IMFTopologyServiceLookupClient IMFTopologyServiceLookupClient_iface;
    LONG ref;
    IMFVideoPresenter *inner;
    IMFTopologyServiceLookupClient *inner_lookup;
    LONG inits, releases;
};
static struct wrap w;

static HRESULT WINAPI p_QueryInterface(IMFVideoPresenter *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IMFVideoPresenter) || IsEqualGUID(riid, &IID_IMFClockStateSink))
        *out = &w.IMFVideoPresenter_iface;
    else if (IsEqualGUID(riid, &IID_IMFTopologyServiceLookupClient))
        *out = &w.IMFTopologyServiceLookupClient_iface;
    else
        return IMFVideoPresenter_QueryInterface(w.inner, riid, out);   /* the rest: the real presenter's */
    InterlockedIncrement(&w.ref);
    return S_OK;
}
static ULONG WINAPI p_AddRef(IMFVideoPresenter *iface) { return InterlockedIncrement(&w.ref); }
static ULONG WINAPI p_Release(IMFVideoPresenter *iface) { return InterlockedDecrement(&w.ref); }
static HRESULT WINAPI p_OnClockStart(IMFVideoPresenter *i, MFTIME t, LONGLONG o) { return IMFVideoPresenter_OnClockStart(w.inner, t, o); }
static HRESULT WINAPI p_OnClockStop(IMFVideoPresenter *i, MFTIME t) { return IMFVideoPresenter_OnClockStop(w.inner, t); }
static HRESULT WINAPI p_OnClockPause(IMFVideoPresenter *i, MFTIME t) { return IMFVideoPresenter_OnClockPause(w.inner, t); }
static HRESULT WINAPI p_OnClockRestart(IMFVideoPresenter *i, MFTIME t) { return IMFVideoPresenter_OnClockRestart(w.inner, t); }
static HRESULT WINAPI p_OnClockSetRate(IMFVideoPresenter *i, MFTIME t, float r) { return IMFVideoPresenter_OnClockSetRate(w.inner, t, r); }
static HRESULT WINAPI p_ProcessMessage(IMFVideoPresenter *i, MFVP_MESSAGE_TYPE m, ULONG_PTR p) { return IMFVideoPresenter_ProcessMessage(w.inner, m, p); }
static HRESULT WINAPI p_GetCurrentMediaType(IMFVideoPresenter *i, IMFVideoMediaType **t) { return IMFVideoPresenter_GetCurrentMediaType(w.inner, t); }

static const IMFVideoPresenterVtbl p_vtbl =
{
    p_QueryInterface, p_AddRef, p_Release, p_OnClockStart, p_OnClockStop, p_OnClockPause, p_OnClockRestart,
    p_OnClockSetRate, p_ProcessMessage, p_GetCurrentMediaType,
};

static HRESULT WINAPI l_QueryInterface(IMFTopologyServiceLookupClient *i, REFIID riid, void **out) { return p_QueryInterface(NULL, riid, out); }
static ULONG WINAPI l_AddRef(IMFTopologyServiceLookupClient *i) { return InterlockedIncrement(&w.ref); }
static ULONG WINAPI l_Release(IMFTopologyServiceLookupClient *i) { return InterlockedDecrement(&w.ref); }
static HRESULT WINAPI l_InitServicePointers(IMFTopologyServiceLookupClient *i, IMFTopologyServiceLookup *lookup)
{
    InterlockedIncrement(&w.inits);
    return IMFTopologyServiceLookupClient_InitServicePointers(w.inner_lookup, lookup);
}
static HRESULT WINAPI l_ReleaseServicePointers(IMFTopologyServiceLookupClient *i)
{
    InterlockedIncrement(&w.releases);
    return IMFTopologyServiceLookupClient_ReleaseServicePointers(w.inner_lookup);
}
static const IMFTopologyServiceLookupClientVtbl l_vtbl =
{
    l_QueryInterface, l_AddRef, l_Release, l_InitServicePointers, l_ReleaseServicePointers,
};

static IPin *input_pin(IBaseFilter *filter)
{
    IEnumPins *e; IPin *pin = NULL; PIN_DIRECTION dir;
    IBaseFilter_EnumPins(filter, &e);
    while (IEnumPins_Next(e, 1, &pin, NULL) == S_OK)
    {
        IPin_QueryDirection(pin, &dir);
        if (dir == PINDIR_INPUT) break;
        IPin_Release(pin); pin = NULL;
    }
    IEnumPins_Release(e);
    return pin;
}

int main(int argc, char **argv)
{
    IGraphBuilder *graph; IBaseFilter *evr; IMFVideoRenderer *renderer; IPin *pin, *peer = NULL;
    AM_MEDIA_TYPE mt; HRESULT hr; LONG inits, releases; WCHAR file[MAX_PATH];

    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, file, MAX_PATH);
    CoInitialize(NULL);
    w.IMFVideoPresenter_iface.lpVtbl = &p_vtbl;
    w.IMFTopologyServiceLookupClient_iface.lpVtbl = &l_vtbl;
    w.ref = 1;
    if (FAILED(hr = MFCreateVideoPresenter(NULL, &IID_IDirect3DDevice9, &IID_IMFVideoPresenter, (void **)&w.inner)))
    { printf("no presenter %#lx\n", hr); return 1; }
    IMFVideoPresenter_QueryInterface(w.inner, &IID_IMFTopologyServiceLookupClient, (void **)&w.inner_lookup);

    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    if (FAILED(hr = CoCreateInstance(&CLSID_EnhancedVideoRenderer_, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&evr)))
    { printf("no evr %#lx\n", hr); return 1; }
    IBaseFilter_QueryInterface(evr, &IID_IMFVideoRenderer, (void **)&renderer);
    hr = IMFVideoRenderer_InitializeRenderer(renderer, NULL, &w.IMFVideoPresenter_iface);
    printf("initialize %#lx\n", hr);
    IGraphBuilder_AddFilter(graph, evr, L"EVR");
    hr = IGraphBuilder_RenderFile(graph, file, NULL);
    pin = input_pin(evr);
    IPin_ConnectedTo(pin, &peer);
    printf("render %#lx connected=%d\n", hr, peer != NULL);
    if (!peer) return 1;

    inits = w.inits; releases = w.releases;
    printf("before: inits %ld releases %ld\n", inits, releases);
    IPin_ConnectionMediaType(pin, &mt);
    hr = IPin_QueryAccept(pin, &mt);
    printf("queryaccept %#lx\n", hr);
    printf("after: inits %ld releases %ld\n", w.inits, w.releases);
    printf("accepted=%d\n", hr == S_OK);
    printf("services_kept=%d\n", w.releases == releases);
    printf("not_reinited=%d\n", w.inits == inits);
    printf("done=1\n");
    fflush(stdout);
    ExitProcess(0);
}
