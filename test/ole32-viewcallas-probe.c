/* Probe for patches/sg/5032: the call_as proxies and stubs of IViewObject
 * (Draw with its continue callback carried as an IContinue, GetColorSet,
 * Freeze, GetAdvise with NULL out pointers) and
 * IOleInPlaceActiveObject::ResizeBorder forward to the object across
 * apartments (they returned E_NOTIMPL). Objects in an STA thread, calls from
 * the MTA.  Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG continue_calls, draw_calls;
static RECTL draw_bounds;
static RECT border;
static BOOL border_frame;

static HRESULT WINAPI vo_QueryInterface(IViewObject *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IViewObject)) { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI vo_AddRef(IViewObject *iface) { return 2; }
static ULONG WINAPI vo_Release(IViewObject *iface) { return 1; }
static HRESULT WINAPI vo_Draw(IViewObject *iface, DWORD aspect, LONG lindex, void *pvaspect, DVTARGETDEVICE *ptd,
        HDC target, HDC hdc, const RECTL *bounds, const RECTL *wbounds, BOOL (STDMETHODCALLTYPE *cont)(ULONG_PTR), ULONG_PTR param)
{
    BOOL go = TRUE;
    draw_calls++;
    if (bounds) draw_bounds = *bounds;
    if (cont) go = cont(param);
    return go ? S_OK : VIEW_E_DRAW;
}
static HRESULT WINAPI vo_GetColorSet(IViewObject *iface, DWORD aspect, LONG lindex, void *pvaspect, DVTARGETDEVICE *ptd,
        HDC hic, LOGPALETTE **set)
{
    *set = NULL;
    return S_FALSE;
}
static HRESULT WINAPI vo_Freeze(IViewObject *iface, DWORD aspect, LONG lindex, void *pvaspect, DWORD *freeze)
{
    *freeze = aspect * 100 + 7;
    return S_OK;
}
static HRESULT WINAPI vo_Unfreeze(IViewObject *iface, DWORD freeze) { return S_OK; }
static HRESULT WINAPI vo_SetAdvise(IViewObject *iface, DWORD aspects, DWORD advf, IAdviseSink *sink) { return S_OK; }
static HRESULT WINAPI vo_GetAdvise(IViewObject *iface, DWORD *aspects, DWORD *advf, IAdviseSink **sink)
{
    if (aspects) *aspects = DVASPECT_ICON;
    if (advf) *advf = ADVF_PRIMEFIRST;
    if (sink) *sink = NULL;
    return S_OK;
}
static IViewObjectVtbl vo_vtbl = {vo_QueryInterface, vo_AddRef, vo_Release, vo_Draw, vo_GetColorSet, vo_Freeze, vo_Unfreeze,
                                  vo_SetAdvise, vo_GetAdvise};
static IViewObject view = {&vo_vtbl};

static HRESULT WINAPI ao_QueryInterface(IOleInPlaceActiveObject *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleWindow) || IsEqualIID(riid, &IID_IOleInPlaceActiveObject))
    { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ao_AddRef(IOleInPlaceActiveObject *iface) { return 2; }
static ULONG WINAPI ao_Release(IOleInPlaceActiveObject *iface) { return 1; }
static HRESULT WINAPI ao_GetWindow(IOleInPlaceActiveObject *iface, HWND *hwnd) { return E_NOTIMPL; }
static HRESULT WINAPI ao_ContextSensitiveHelp(IOleInPlaceActiveObject *iface, BOOL enter) { return E_NOTIMPL; }
static HRESULT WINAPI ao_TranslateAccelerator(IOleInPlaceActiveObject *iface, MSG *msg) { return S_FALSE; }
static HRESULT WINAPI ao_OnFrameWindowActivate(IOleInPlaceActiveObject *iface, BOOL activate) { return S_OK; }
static HRESULT WINAPI ao_OnDocWindowActivate(IOleInPlaceActiveObject *iface, BOOL activate) { return S_OK; }
static HRESULT WINAPI ao_ResizeBorder(IOleInPlaceActiveObject *iface, const RECT *rect, IOleInPlaceUIWindow *win, BOOL frame)
{
    if (rect) border = *rect;
    border_frame = frame;
    return win ? E_UNEXPECTED : S_OK;
}
static HRESULT WINAPI ao_EnableModeless(IOleInPlaceActiveObject *iface, BOOL enable) { return S_OK; }
static IOleInPlaceActiveObjectVtbl ao_vtbl = {ao_QueryInterface, ao_AddRef, ao_Release, ao_GetWindow, ao_ContextSensitiveHelp,
    ao_TranslateAccelerator, ao_OnFrameWindowActivate, ao_OnDocWindowActivate, ao_ResizeBorder, ao_EnableModeless};
static IOleInPlaceActiveObject active = {&ao_vtbl};

static BOOL STDMETHODCALLTYPE continue_cb(ULONG_PTR param)
{
    InterlockedIncrement(&continue_calls);
    return param == 1;
}

static IStream *streams[2];
static HANDLE ready, quit;

static DWORD WINAPI sta_thread(void *arg)
{
    MSG msg;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    CoMarshalInterThreadInterfaceInStream(&IID_IViewObject, (IUnknown *)&view, &streams[0]);
    CoMarshalInterThreadInterfaceInStream(&IID_IOleInPlaceActiveObject, (IUnknown *)&active, &streams[1]);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &quit, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    CoUninitialize();
    return 0;
}

int main(void)
{
    static const struct { const char *what; BOOL use_cb; ULONG_PTR param; HRESULT want; LONG calls; } draws[] =
    {
        {"no callback", FALSE, 0, S_OK, 0},
        {"a callback that goes on", TRUE, 1, S_OK, 1},
        {"a callback that stops", TRUE, 0, VIEW_E_DRAW, 1},
    };
    IViewObject *vp = NULL;
    IOleInPlaceActiveObject *ap = NULL;
    RECTL bounds = {1, 2, 30, 40};
    RECT rc = {5, 6, 7, 8};
    LOGPALETTE *pal = (void *)0xdeadbeef;
    DWORD freeze = 0, aspects = 0;
    IAdviseSink *sink = (void *)0xdeadbeef;
    HANDLE thread;
    unsigned int i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    quit = CreateEventA(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    CoGetInterfaceAndReleaseStream(streams[0], &IID_IViewObject, (void **)&vp);
    CoGetInterfaceAndReleaseStream(streams[1], &IID_IOleInPlaceActiveObject, (void **)&ap);
    check(vp && ap && vp != &view && ap != &active, "proxies made (%p %p)", vp, ap);
    if (!vp || !ap) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(draws); i++)
    {
        draw_calls = continue_calls = 0;
        memset(&draw_bounds, 0, sizeof(draw_bounds));
        hr = IViewObject_Draw(vp, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &bounds, NULL,
                              draws[i].use_cb ? continue_cb : NULL, draws[i].param);
        check(hr == draws[i].want && draw_calls == 1 && continue_calls == draws[i].calls && draw_bounds.right == 30
              && draw_bounds.bottom == 40, "Draw, %s: %#lx, %ld draws, %ld callbacks", draws[i].what, hr, draw_calls,
              continue_calls);
    }
    hr = IViewObject_GetColorSet(vp, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal);
    check(hr == S_FALSE && pal == NULL, "GetColorSet: %#lx %p", hr, pal);
    hr = IViewObject_Freeze(vp, DVASPECT_ICON, -1, NULL, &freeze);
    check(hr == S_OK && freeze == DVASPECT_ICON * 100 + 7, "Freeze: %#lx %lu", hr, freeze);
    hr = IViewObject_GetAdvise(vp, &aspects, NULL, &sink);
    check(hr == S_OK && aspects == DVASPECT_ICON && sink == NULL, "GetAdvise with a NULL advf: %#lx %lu %p", hr, aspects, sink);

    hr = IOleInPlaceActiveObject_ResizeBorder(ap, &rc, NULL, TRUE);
    check(hr == S_OK && border.left == 5 && border.bottom == 8 && border_frame, "ResizeBorder: %#lx (%ld,%ld) %d", hr,
          border.left, border.bottom, border_frame);

    IViewObject_Release(vp);
    IOleInPlaceActiveObject_Release(ap);
    SetEvent(quit);
    WaitForSingleObject(thread, 10000);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
