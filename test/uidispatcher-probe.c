/* Windows.UI for XAML Islands (patches/sg/6175): the thread CoreWindow's CoreDispatcher (HasThreadAccess;
 * RunAsync from the thread and from another one runs the callback on the CoreWindow's thread, its
 * IAsyncAction completing then), Windows.UI.Colors and ColorHelper, AccessibilitySettings (HighContrast,
 * a weak reference of it) -- what Windows Terminal and WinUI 2 controls ask of it.
 * Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_CoreWindowStatic, 0x4d239005, 0x3c2a, 0x41b1, 0x90, 0x22, 0x53, 0x6b, 0xb9, 0xcf, 0x93, 0xb1);
DEFINE_GUID(IID_AsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_ColorsStatics, 0xcff52e04, 0xcca6, 0x4614, 0xa1, 0x7e, 0x75, 0x49, 0x10, 0xc8, 0x4a, 0x99);
DEFINE_GUID(IID_ColorHelperStatics, 0x8504dbea, 0xfb6a, 0x4144, 0xa6, 0xc2, 0x33, 0x49, 0x9c, 0x92, 0x84, 0xf5);
DEFINE_GUID(IID_ColorHelperStatics2, 0x24d9af02, 0x6eb0, 0x4b94, 0x85, 0x5c, 0xfc, 0xf0, 0x81, 0x8d, 0x9a, 0x16);
DEFINE_GUID(IID_Accessibility, 0xfe0e8147, 0xc4c0, 0x4562, 0xb9, 0x62, 0x13, 0x27, 0xb5, 0x2a, 0xd5, 0xb9);
DEFINE_GUID(IID_WeakSource, 0x00000038, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

/* a DispatchedHandler: Invoke() */
struct handler { void **vtbl; LONG ref; int calls; DWORD thread; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h) { h->calls++; h->thread = GetCurrentThreadId(); return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };
/* an AsyncActionCompletedHandler: Invoke( action, status ) */
static int completed_status = -1;
static HRESULT WINAPI c_invoke(struct handler *h, void *action, int status) { h->calls++; completed_status = status; return S_OK; }
static void *c_vtbl[] = { h_qi, h_addref, h_release, c_invoke };

static obj *dispatcher;
static struct handler from_thread = { h_vtbl, 1 };
static DWORD WINAPI other_thread(void *arg)
{
    obj *action = NULL;
    HRESULT hr = ((HRESULT (WINAPI *)(obj *, int, void *, obj **))SLOT(dispatcher, 8))(dispatcher, 0, &from_thread, &action);
    boolean access = TRUE;
    ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(dispatcher, 6))(dispatcher, &access);
    printf("other=%08lx,%d\n", hr, access);
    return 0;
}

int main(void)
{
    struct handler run = { h_vtbl, 1 }, completed = { c_vtbl, 1 };
    obj *ms, *mgr = NULL, *cws, *cw = NULL, *action = NULL, *info, *colors, *helper, *helper2, *acc = NULL, *ws, *weak = NULL, *back = NULL;
    boolean access = FALSE, hc = TRUE;
    BYTE c[4] = { 0 };
    HSTRING name = NULL;
    int status = -1;
    HANDLE thread;
    HRESULT hr;

    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    if (ms) ((get_fn)SLOT(ms, 6))(ms, &mgr);
    cws = factory(L"Windows.UI.Core.CoreWindow", &IID_CoreWindowStatic);
    if (cws) ((get_fn)SLOT(cws, 6))(cws, &cw);
    /* ICoreWindow.Dispatcher */
    if (cw) ((get_fn)SLOT(cw, 9))(cw, &dispatcher);
    if (!dispatcher) { printf("no dispatcher\n"); return 1; }
    ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(dispatcher, 6))(dispatcher, &access);
    hr = ((HRESULT (WINAPI *)(obj *, int, void *, obj **))SLOT(dispatcher, 8))(dispatcher, 0, &run, &action);
    printf("run=%08lx,%d,%d\n", hr, access, run.calls);   /* not run before the loop runs it */
    if (action) ((HRESULT (WINAPI *)(obj *, void *))SLOT(action, 6))(action, &completed);
    pump(200);
    info = as(action, &IID_AsyncInfo);
    if (info) ((HRESULT (WINAPI *)(obj *, int *))SLOT(info, 7))(info, &status);
    printf("ran=%d,%d,%d,%d\n", run.calls, run.thread == GetCurrentThreadId(), status, completed.calls == 1 && completed_status == 1);
    thread = CreateThread(NULL, 0, other_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, 5000);
    pump(200);
    printf("posted=%d,%d\n", from_thread.calls, from_thread.thread == GetCurrentThreadId());

    colors = factory(L"Windows.UI.Colors", &IID_ColorsStatics);
    if (colors)
    {
        ((HRESULT (WINAPI *)(obj *, BYTE *))SLOT(colors, 119))(colors, c);
        printf("red=%02x%02x%02x%02x\n", c[0], c[1], c[2], c[3]);
        ((HRESULT (WINAPI *)(obj *, BYTE *))SLOT(colors, 139))(colors, c);
        printf("transparent=%02x%02x%02x%02x\n", c[0], c[1], c[2], c[3]);
        ((HRESULT (WINAPI *)(obj *, BYTE *))SLOT(colors, 23))(colors, c);
        printf("cornflower=%02x%02x%02x%02x\n", c[0], c[1], c[2], c[3]);
    }
    helper = factory(L"Windows.UI.ColorHelper", &IID_ColorHelperStatics);
    if (helper)
    {
        ((HRESULT (WINAPI *)(obj *, BYTE, BYTE, BYTE, BYTE, BYTE *))SLOT(helper, 6))(helper, 0x80, 1, 2, 3, c);
        printf("argb=%02x%02x%02x%02x\n", c[0], c[1], c[2], c[3]);
        if ((helper2 = as(helper, &IID_ColorHelperStatics2)))
        {
            UINT32 red = 0x0000ffff;   /* a b g r in memory: A=ff R=ff G=00 B=00 */
            BYTE rc[4] = { 0xff, 0xff, 0, 0 };
            memcpy(&red, rc, 4);
            ((HRESULT (WINAPI *)(obj *, UINT32, HSTRING *))SLOT(helper2, 6))(helper2, red, &name);
            printf("name=%ls\n", WindowsGetStringRawBuffer(name, NULL));
        }
    }

    RoActivateInstance(hs(L"Windows.UI.ViewManagement.AccessibilitySettings"), (IInspectable **)&acc);
    if (acc && as(acc, &IID_Accessibility)) ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(as(acc, &IID_Accessibility), 6))(as(acc, &IID_Accessibility), &hc);
    ws = as(acc, &IID_WeakSource);
    if (ws) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(ws, 3))(ws, &weak);
    if (weak) ((HRESULT (WINAPI *)(obj *, REFIID, obj **))SLOT(weak, 3))(weak, &IID_Accessibility, &back);
    printf("accessibility=%d,%d,%d,%d\n", acc != NULL, hc, weak != NULL, back != NULL);
    if (back) ((ULONG (WINAPI *)(obj *))SLOT(back, 2))(back);
    if (ws) ((ULONG (WINAPI *)(obj *))SLOT(ws, 2))(ws);
    while (acc && ((ULONG (WINAPI *)(obj *))SLOT(acc, 2))(acc)) ;
    back = (obj *)1;
    if (weak) ((HRESULT (WINAPI *)(obj *, REFIID, obj **))SLOT(weak, 3))(weak, &IID_Accessibility, &back);
    printf("gone=%d\n", back == NULL);
    return 0;
}
