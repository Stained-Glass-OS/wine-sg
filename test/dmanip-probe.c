/* Direct Manipulation (patches/sg/1668), run by test/dmanip-gate.sh:
 * a viewport over a window, its status as event handlers hear it, its
 * primary content's transform through ZoomToRect (immediate, animated by
 * the update manager, within zoom boundaries), pans and pinches from
 * pointer messages (ProcessInput, and the activated window in automatic
 * mode) with inertia that settles on snap points, interaction events,
 * Disable, the update manager's wait handle callbacks and the compositor's
 * frame information. These were stubs (E_NOTIMPL, nothing kept). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <directmanipulation.h>
#include <stdio.h>
#include <math.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* the event handler, also an interaction handler */
struct handler
{
    IDirectManipulationViewportEventHandler iface;
    IDirectManipulationInteractionEventHandler interaction;
    LONG status_calls, content_calls, viewport_calls, begin, end;
    DIRECTMANIPULATION_STATUS current, previous;
};

static HRESULT WINAPI h_qi(IDirectManipulationViewportEventHandler *iface, REFIID riid, void **out)
{
    struct handler *h = CONTAINING_RECORD(iface, struct handler, iface);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectManipulationViewportEventHandler)) *out = iface;
    else if (IsEqualIID(riid, &IID_IDirectManipulationInteractionEventHandler)) *out = &h->interaction;
    else { *out = NULL; return E_NOINTERFACE; }
    return S_OK;
}
static ULONG WINAPI h_addref(IDirectManipulationViewportEventHandler *iface) { return 2; }
static ULONG WINAPI h_release(IDirectManipulationViewportEventHandler *iface) { return 1; }
static HRESULT WINAPI h_status(IDirectManipulationViewportEventHandler *iface, IDirectManipulationViewport *vp,
                               DIRECTMANIPULATION_STATUS current, DIRECTMANIPULATION_STATUS previous)
{
    struct handler *h = CONTAINING_RECORD(iface, struct handler, iface);
    h->status_calls++;
    h->current = current;
    h->previous = previous;
    return S_OK;
}
static HRESULT WINAPI h_viewport(IDirectManipulationViewportEventHandler *iface, IDirectManipulationViewport *vp)
{
    CONTAINING_RECORD(iface, struct handler, iface)->viewport_calls++;
    return S_OK;
}
static HRESULT WINAPI h_content(IDirectManipulationViewportEventHandler *iface, IDirectManipulationViewport *vp,
                                IDirectManipulationContent *content)
{
    CONTAINING_RECORD(iface, struct handler, iface)->content_calls++;
    return S_OK;
}
static IDirectManipulationViewportEventHandlerVtbl h_vtbl = { h_qi, h_addref, h_release, h_status, h_viewport, h_content };

static HRESULT WINAPI i_qi(IDirectManipulationInteractionEventHandler *iface, REFIID riid, void **out)
{
    struct handler *h = CONTAINING_RECORD(iface, struct handler, interaction);
    return h_qi(&h->iface, riid, out);
}
static ULONG WINAPI i_addref(IDirectManipulationInteractionEventHandler *iface) { return 2; }
static ULONG WINAPI i_release(IDirectManipulationInteractionEventHandler *iface) { return 1; }
static HRESULT WINAPI i_interaction(IDirectManipulationInteractionEventHandler *iface, IDirectManipulationViewport2 *vp,
                                    DIRECTMANIPULATION_INTERACTION_TYPE type)
{
    struct handler *h = CONTAINING_RECORD(iface, struct handler, interaction);
    if (type == DIRECTMANIPULATION_INTERACTION_BEGIN) h->begin++;
    if (type == DIRECTMANIPULATION_INTERACTION_END) h->end++;
    return S_OK;
}
static IDirectManipulationInteractionEventHandlerVtbl i_vtbl = { i_qi, i_addref, i_release, i_interaction };

/* an update handler for the wait handle */
struct update_handler { IDirectManipulationUpdateHandler iface; LONG calls; };
static HRESULT WINAPI u_qi(IDirectManipulationUpdateHandler *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI u_addref(IDirectManipulationUpdateHandler *iface) { return 2; }
static ULONG WINAPI u_release(IDirectManipulationUpdateHandler *iface) { return 1; }
static HRESULT WINAPI u_update(IDirectManipulationUpdateHandler *iface)
{
    InterlockedIncrement(&((struct update_handler *)iface)->calls);
    return S_OK;
}
static IDirectManipulationUpdateHandlerVtbl u_vtbl = { u_qi, u_addref, u_release, u_update };

static LONG app_pointer_messages;
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_POINTERUPDATE) app_pointer_messages++;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND hwnd;

static void get_xform(IDirectManipulationContent *content, float *m)
{
    IDirectManipulationContent_GetContentTransform(content, m, 6);
}

static BOOL close_to(float a, float b)
{
    return fabsf(a - b) < 0.5f;
}

static MSG pointer_msg(UINT message, UINT32 id, int x, int y)
{
    MSG msg = { hwnd, message, MAKEWPARAM(id, 0) };
    POINT pt = { x, y };
    ClientToScreen(hwnd, &pt);
    msg.lParam = MAKELPARAM(pt.x, pt.y);
    return msg;
}

static BOOL input(IDirectManipulationManager2 *mgr, UINT message, UINT32 id, int x, int y)
{
    MSG msg = pointer_msg(message, id, x, y);
    BOOL handled = FALSE;
    IDirectManipulationManager2_ProcessInput(mgr, &msg, &handled);
    return handled;
}

/* updates until the viewport settles (or a second goes by) */
static void settle(IDirectManipulationUpdateManager *update, IDirectManipulationViewport2 *vp)
{
    DIRECTMANIPULATION_STATUS status;
    int i;
    for (i = 0; i < 200; i++)
    {
        IDirectManipulationUpdateManager_Update(update, NULL);
        IDirectManipulationViewport2_GetStatus(vp, &status);
        if (status != DIRECTMANIPULATION_INERTIA && status != DIRECTMANIPULATION_RUNNING) break;
        Sleep(10);
    }
}

int main(void)
{
    struct handler handler = { { &h_vtbl }, { &i_vtbl } };
    struct update_handler uh = { { &u_vtbl } };
    IDirectManipulationManager2 *mgr;
    IDirectManipulationUpdateManager *update;
    IDirectManipulationViewport2 *vp;
    IDirectManipulationContent *content;
    IDirectManipulationPrimaryContent *primary;
    IDirectManipulationCompositor *compositor;
    IDirectManipulationFrameInfoProvider *frame;
    DIRECTMANIPULATION_STATUS status;
    DWORD cookie, ucookie;
    WNDCLASSW wc = {0};
    RECT rc = { 0, 0, 400, 300 }, content_rc = { 0, 0, 1600, 1200 };
    float m[6], end[6];
    ULONGLONG t, p, c;
    HANDLE event;
    HRESULT hr;
    int i;

    CoInitialize(NULL);
    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = L"dmanip_probe";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"dmanip_probe", L"dmanip", WS_POPUP | WS_VISIBLE, 50, 50, 400, 300, NULL, NULL, NULL, NULL);

    hr = CoCreateInstance(&CLSID_DirectManipulationManager, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IDirectManipulationManager2, (void **)&mgr);
    check(hr == S_OK, "the manager");
    if (hr != S_OK) goto done;
    check(IDirectManipulationManager2_Activate(mgr, hwnd) == S_OK, "Activate");
    check(IDirectManipulationManager2_GetUpdateManager(mgr, &IID_IDirectManipulationUpdateManager,
                                                       (void **)&update) == S_OK, "the update manager");
    hr = IDirectManipulationManager2_CreateViewport(mgr, NULL, hwnd, &IID_IDirectManipulationViewport2, (void **)&vp);
    check(hr == S_OK, "a viewport");
    if (hr != S_OK) goto done;
    check(IDirectManipulationViewport2_GetStatus(vp, &status) == S_OK && status == DIRECTMANIPULATION_BUILDING,
          "it is being built");
    check(IDirectManipulationViewport2_AddEventHandler(vp, hwnd, &handler.iface, &cookie) == S_OK, "an event handler");
    check(IDirectManipulationViewport2_ActivateConfiguration(vp, DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
          DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
          DIRECTMANIPULATION_CONFIGURATION_SCALING | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA) == S_OK,
          "a configuration");
    check(IDirectManipulationViewport2_ActivateConfiguration(vp, DIRECTMANIPULATION_CONFIGURATION_SCALING_INERTIA) ==
          E_INVALIDARG, "scaling inertia without scaling: E_INVALIDARG");
    IDirectManipulationViewport2_SetViewportRect(vp, &rc);
    IDirectManipulationViewport2_GetPrimaryContent(vp, &IID_IDirectManipulationContent, (void **)&content);
    IDirectManipulationContent_QueryInterface(content, &IID_IDirectManipulationPrimaryContent, (void **)&primary);
    check(content && primary && IDirectManipulationContent_SetContentRect(content, &content_rc) == S_OK,
          "the primary content");
    check(IDirectManipulationViewport2_Enable(vp) == S_OK && handler.status_calls == 1 &&
          handler.current == DIRECTMANIPULATION_ENABLED && handler.previous == DIRECTMANIPULATION_BUILDING,
          "Enable: told BUILDING -> ENABLED");

    /* ZoomToRect */
    handler.content_calls = handler.viewport_calls = 0;
    IDirectManipulationViewport2_ZoomToRect(vp, 400, 300, 800, 600, FALSE);
    get_xform(content, m);
    check(close_to(m[0], 1) && close_to(m[4], -400) && close_to(m[5], -300), "ZoomToRect: the rectangle fills the viewport");
    check(handler.content_calls == 1 && handler.viewport_calls == 1, "the content and viewport updates are told");
    IDirectManipulationViewport2_ZoomToRect(vp, 0, 0, 800, 600, FALSE);
    get_xform(content, m);
    check(close_to(m[0], 0.5f) && close_to(m[4], 0) && close_to(m[5], 0), "zoomed out to half");
    IDirectManipulationViewport2_ZoomToRect(vp, 0, 0, 400, 300, TRUE);
    IDirectManipulationViewport2_GetStatus(vp, &status);
    IDirectManipulationPrimaryContent_GetInertiaEndTransform(primary, end, 6);
    check(status == DIRECTMANIPULATION_INERTIA && close_to(end[0], 1), "animated: moving, towards scale 1");
    settle(update, vp);
    get_xform(content, m);
    IDirectManipulationViewport2_GetStatus(vp, &status);
    check(close_to(m[0], 1) && status == DIRECTMANIPULATION_READY, "the update manager brings it there; ready");
    check(IDirectManipulationPrimaryContent_SetZoomBoundaries(primary, 0.5f, 2.0f) == S_OK &&
          IDirectManipulationPrimaryContent_SetZoomBoundaries(primary, 3.0f, 2.0f) == E_INVALIDARG, "zoom boundaries");
    IDirectManipulationViewport2_ZoomToRect(vp, 0, 0, 100, 100, FALSE);
    get_xform(content, m);
    check(close_to(m[0], 2), "a zoom past the boundary stops at it");
    IDirectManipulationViewport2_ZoomToRect(vp, 0, 0, 400, 300, FALSE);

    /* a pan, by ProcessInput */
    IDirectManipulationViewport2_SetInputMode(vp, DIRECTMANIPULATION_INPUT_MODE_MANUAL);
    IDirectManipulationPrimaryContent_SetSnapInterval(primary, DIRECTMANIPULATION_MOTION_TRANSLATEX, 100, 0);
    check(IDirectManipulationViewport2_SetContact(vp, 1) == S_OK, "SetContact");
    check(input(mgr, WM_POINTERDOWN, 1, 300, 150), "ProcessInput takes the contact's pointer");
    check(!input(mgr, WM_POINTERUPDATE, 7, 300, 150), "but not another pointer");
    for (i = 1; i <= 10; i++)
    {
        input(mgr, WM_POINTERUPDATE, 1, 300 - i * 13, 150);
        Sleep(5);
    }
    IDirectManipulationViewport2_GetStatus(vp, &status);
    get_xform(content, m);
    check(status == DIRECTMANIPULATION_RUNNING && close_to(m[4], -130) && close_to(m[5], 0), "the pan moves the content");
    check(handler.begin == 1, "the interaction begins");
    input(mgr, WM_POINTERUP, 1, 170, 150);
    settle(update, vp);
    get_xform(content, m);
    IDirectManipulationViewport2_GetStatus(vp, &status);
    printf("      at rest at %.1f\n", m[4]);
    check(status == DIRECTMANIPULATION_READY && m[4] < -130 && close_to(fmodf(-m[4], 100), 0),
          "inertia carries it on, to a snap point");
    check(handler.end == 1, "the interaction ends");

    /* a pinch */
    IDirectManipulationPrimaryContent_SetSnapInterval(primary, DIRECTMANIPULATION_MOTION_TRANSLATEX, 0, 0);
    IDirectManipulationViewport2_ZoomToRect(vp, 0, 0, 400, 300, FALSE);
    IDirectManipulationViewport2_ActivateConfiguration(vp, DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
          DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
          DIRECTMANIPULATION_CONFIGURATION_SCALING);
    IDirectManipulationViewport2_SetContact(vp, 2);
    IDirectManipulationViewport2_SetContact(vp, 3);
    input(mgr, WM_POINTERDOWN, 2, 100, 150);
    input(mgr, WM_POINTERDOWN, 3, 300, 150);
    input(mgr, WM_POINTERUPDATE, 2, 50, 150);
    input(mgr, WM_POINTERUPDATE, 3, 350, 150);
    get_xform(content, m);
    check(close_to(m[0], 1.5f) && close_to(m[4], -100) && close_to(m[5], -75), "a pinch zooms about the fingers' centre");
    input(mgr, WM_POINTERUP, 2, 50, 150);
    input(mgr, WM_POINTERUP, 3, 350, 150);
    settle(update, vp);

    /* automatic input: the activated window's messages */
    IDirectManipulationViewport2_SetInputMode(vp, DIRECTMANIPULATION_INPUT_MODE_AUTOMATIC);
    IDirectManipulationViewport2_ZoomToRect(vp, 0, 0, 400, 300, FALSE);
    IDirectManipulationViewport2_SetContact(vp, 4);
    app_pointer_messages = 0;
    {
        MSG msg = pointer_msg(WM_POINTERDOWN, 4, 200, 200);
        SendMessageW(hwnd, msg.message, msg.wParam, msg.lParam);
        msg = pointer_msg(WM_POINTERUPDATE, 4, 200, 140);
        SendMessageW(hwnd, msg.message, msg.wParam, msg.lParam);
        msg = pointer_msg(WM_POINTERUPDATE, 9, 200, 140);
        SendMessageW(hwnd, msg.message, msg.wParam, msg.lParam);
    }
    get_xform(content, m);
    check(close_to(m[5], -60), "automatic input: the window's pointer messages move the content");
    check(app_pointer_messages == 1, "the window gets only the other pointer's");
    IDirectManipulationViewport2_ReleaseAllContacts(vp);
    settle(update, vp);

    /* Disable */
    check(IDirectManipulationViewport2_Disable(vp) == S_OK && handler.current == DIRECTMANIPULATION_DISABLED,
          "Disable: told");
    check(IDirectManipulationViewport2_SetContact(vp, 5) == E_INVALIDARG, "no contacts while disabled");
    check(IDirectManipulationViewport2_RemoveEventHandler(vp, cookie) == S_OK &&
          IDirectManipulationViewport2_RemoveEventHandler(vp, cookie) == E_INVALIDARG, "the handler is removed");

    /* the update manager's wait handles */
    event = CreateEventW(NULL, FALSE, FALSE, NULL);
    check(IDirectManipulationUpdateManager_RegisterWaitHandleCallback(update, event, &uh.iface, &ucookie) == S_OK,
          "a wait handle callback");
    SetEvent(event);
    for (i = 0; i < 100 && !uh.calls; i++) Sleep(20);
    check(uh.calls >= 1, "is called when the handle is signalled");
    check(IDirectManipulationUpdateManager_UnregisterWaitHandleCallback(update, ucookie) == S_OK, "and removed");

    /* the compositor */
    hr = CoCreateInstance(&CLSID_DCompManipulationCompositor, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IDirectManipulationCompositor, (void **)&compositor);
    check(hr == S_OK, "the compositor");
    if (hr == S_OK)
    {
        IDirectManipulationCompositor_QueryInterface(compositor, &IID_IDirectManipulationFrameInfoProvider, (void **)&frame);
        check(IDirectManipulationFrameInfoProvider_GetNextFrameInfo(frame, &t, &p, &c) == S_OK && t && c >= t,
              "its frame information");
        check(IDirectManipulationCompositor_AddContent(compositor, content, (IUnknown *)frame, (IUnknown *)frame,
                                                       (IUnknown *)frame) == S_OK &&
              IDirectManipulationCompositor_SetUpdateManager(compositor, update) == S_OK &&
              IDirectManipulationCompositor_Flush(compositor) == S_OK &&
              IDirectManipulationCompositor_RemoveContent(compositor, content) == S_OK, "contents, update manager, flush");
        IDirectManipulationFrameInfoProvider_Release(frame);
        IDirectManipulationCompositor_Release(compositor);
    }

    IDirectManipulationPrimaryContent_Release(primary);
    IDirectManipulationContent_Release(content);
    IDirectManipulationViewport2_Abandon(vp);
    IDirectManipulationViewport2_Release(vp);
    IDirectManipulationUpdateManager_Release(update);
    check(IDirectManipulationManager2_Deactivate(mgr, hwnd) == S_OK, "Deactivate");
    IDirectManipulationManager2_Release(mgr);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
