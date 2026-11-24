/* Windows.UI.Composition ICompositor5::RequestCommitAsync (patches/sg/9012): as on Windows the action is
 * Started until the next frame, then Completed on the compositor's thread. Wine's was done at once and ran
 * its Completed handler from inside put_Completed, so Avalonia's render loop (the handler asks for the next
 * commit and sets the same handler on it) recursed until the stack overflowed: ShareX 22.4 and ImageGlass 10
 * died at start. The handler here does what Avalonia's does, ten times. Interface identity per windows-rs. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <stdarg.h>

DEFINE_GUID(IID_Compositor5, 0x48ea31ad, 0x7fcd, 0x4076, 0xa7, 0x9c, 0x90, 0xcc, 0x4b, 0x85, 0x2c, 0x9b);
DEFINE_GUID(IID_AsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_AsyncActionCompletedHandler, 0xa4ed5c81, 0x76c9, 0x40bd, 0x8b, 0xe6, 0xb1, 0xd9, 0x0f, 0xb2, 0x0a, 0xe7);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);

static int failures, checks;
static void check(int ok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s  ", ok ? "PASS" : "FAIL");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    checks++;
    if (!ok) failures++;
}

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }

static obj *c5;
static HRESULT request(obj **action) { return ((HRESULT (WINAPI *)(obj *, obj **))SLOT(c5, 25))(c5, action); }
static HRESULT put_completed(obj *action, void *handler) { return ((HRESULT (WINAPI *)(obj *, void *))SLOT(action, 6))(action, handler); }
static int status_of(obj *action)
{
    obj *info = as(action, &IID_AsyncInfo);
    int st = -1;
    if (info) { ((HRESULT (WINAPI *)(obj *, int *))SLOT(info, 7))(info, &st); ((rel_fn)SLOT(info, 2))(info); }
    return st;
}

/* the handler: Avalonia's RunLoopHandler (tick, ask for the next commit, the same handler on it) */
struct handler { const void *const *vtbl; LONG ref; int calls, depth, max_depth, wrong_thread, last_status, limit; DWORD tid; DWORD t0, t_last; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IAgileObject) || IsEqualGUID(iid, &IID_AsyncActionCompletedHandler))
    { *out = h; InterlockedIncrement(&h->ref); return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI h_addref(struct handler *h) { return InterlockedIncrement(&h->ref); }
static ULONG WINAPI h_release(struct handler *h) { return InterlockedDecrement(&h->ref); }
static HRESULT WINAPI h_invoke(struct handler *h, obj *action, int status)
{
    obj *next = NULL;
    h->calls++;
    h->last_status = status;
    if (GetCurrentThreadId() != h->tid) h->wrong_thread++;
    if (++h->depth > h->max_depth) h->max_depth = h->depth;
    h->t_last = GetTickCount();
    if (h->calls < h->limit && SUCCEEDED(request(&next)) && next)
    {
        put_completed(next, h);
        ((rel_fn)SLOT(next, 2))(next);
    }
    h->depth--;
    return S_OK;
}
static const void *const handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

static void pump_until(int (*done)(void *), void *arg, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (!done(arg) && GetTickCount() < end)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 20, QS_ALLINPUT);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
}
static int ten_calls(void *arg) { return ((struct handler *)arg)->calls >= ((struct handler *)arg)->limit; }
static int is_done(void *arg) { return status_of(arg) == 1; }

int main(void)
{
    obj *comp = NULL, *action = NULL, *action2 = NULL;
    struct handler h = { handler_vtbl, 1 }, h2 = { handler_vtbl, 1 };
    HRESULT hr;
    int st;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    hr = RoActivateInstance(hs(L"Windows.UI.Composition.Compositor"), (IInspectable **)&comp);
    c5 = as(comp, &IID_Compositor5);
    check(c5 != NULL, "a compositor with ICompositor5 (%08lx)", hr);
    if (!c5) { printf("RESULT: FAIL\n"); return 1; }

    hr = request(&action);
    st = status_of(action);
    check(hr == S_OK && st == 0, "RequestCommitAsync: Started until the next frame (%08lx, status %d)", hr, st);

    h.tid = GetCurrentThreadId();
    h.limit = 10;
    h.t0 = GetTickCount();
    hr = put_completed(action, &h);
    check(hr == S_OK && h.calls == 0, "put_Completed returns before the handler runs (%08lx, %d calls)", hr, h.calls);
    ((rel_fn)SLOT(action, 2))(action);

    pump_until(ten_calls, &h, 5000);
    check(h.calls == 10, "the render loop's handler ran 10 times, one per frame (%d)", h.calls);
    check(h.max_depth == 1, "never from inside itself (deepest %d)", h.max_depth);
    check(!h.wrong_thread, "on the compositor's thread (%d calls elsewhere)", h.wrong_thread);
    check(h.last_status == 1, "with status Completed (%d)", h.last_status);
    check(h.t_last - h.t0 >= 90, "paced by frames: 10 took %lu ms", h.t_last - h.t0);

    /* finished before a handler is set: the handler runs at once */
    request(&action2);
    pump_until(is_done, action2, 2000);
    st = status_of(action2);
    check(st == 1, "an action left alone completes at the next frame (status %d)", st);
    h2.tid = GetCurrentThreadId();
    h2.limit = 1;
    put_completed(action2, &h2);
    check(h2.calls == 1 && h2.last_status == 1, "a handler set on a completed action runs at once (%d calls)", h2.calls);
    hr = put_completed(action2, &h2);
    check(hr == (HRESULT)0x80000018, "a second handler is E_ILLEGAL_DELEGATE_ASSIGNMENT (%08lx)", hr);
    ((rel_fn)SLOT(action2, 2))(action2);

    if (failures) printf("RESULT: FAIL (%d of %d)\n", failures, checks);
    else printf("RESULT: PASS (%d checks)\n", checks);
    return 0;
}
