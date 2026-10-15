/* Probe for patches/sg/5122: CoIsHandlerConnected (docs: TRUE if the object is
 * not remote or still connected, FALSE if remote and no longer connected).
 * Rows: a local object; a proxy in the MTA to a server in the main STA, asked
 * from both sides, before and after CoDisconnectObject; a proxy whose
 * apartment was uninitialized (leaked on purpose). */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HRESULT WINAPI cf_qi(IClassFactory *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref(IClassFactory *i) { return 2; }
static ULONG WINAPI cf_release(IClassFactory *i) { return 1; }
static HRESULT WINAPI cf_create(IClassFactory *i, IUnknown *o, REFIID r, void **p) { return E_NOTIMPL; }
static HRESULT WINAPI cf_lock(IClassFactory *i, BOOL l) { return S_OK; }
static const IClassFactoryVtbl cfvtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static IClassFactory cf = { (IClassFactoryVtbl *)&cfvtbl };

static HANDLE ev_ready, ev_go, ev_done;
static IStream *stream;
static IClassFactory *proxy;
static BOOL mta_before, mta_after, mta_unknown_before, mta_unknown_after;

static DWORD WINAPI worker(void *arg)
{
    IUnknown *unk;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoUnmarshalInterface(stream, &IID_IClassFactory, (void **)&proxy);
    IClassFactory_QueryInterface(proxy, &IID_IUnknown, (void **)&unk);
    mta_before = CoIsHandlerConnected((IUnknown *)proxy);
    mta_unknown_before = CoIsHandlerConnected(unk);
    SetEvent(ev_ready);
    WaitForSingleObject(ev_go, 20000);
    mta_after = CoIsHandlerConnected((IUnknown *)proxy);
    mta_unknown_after = CoIsHandlerConnected(unk);
    IUnknown_Release(unk);
    IClassFactory_Release(proxy);
    SetEvent(ev_done);
    CoUninitialize();
    return 0;
}

static void pump_until(HANDLE h)
{
    DWORD end = GetTickCount() + 20000;
    while (GetTickCount() < end)
    {
        MSG msg;
        if (MsgWaitForMultipleObjects(1, &h, FALSE, 100, QS_ALLINPUT) == WAIT_OBJECT_0) return;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
}

static IClassFactory *leaked;
static DWORD WINAPI leaker(void *arg)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    CoUnmarshalInterface(stream, &IID_IClassFactory, (void **)&leaked);
    CoUninitialize(); /* the proxy is not released: its apartment ends under it */
    return 0;
}

int main(void)
{
    HANDLE thread;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    check(hr == S_OK, "CoInitializeEx %#lx", hr);

    check(CoIsHandlerConnected((IUnknown *)&cf) == TRUE, "a local object that was never marshaled is connected");

    ev_ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    ev_go = CreateEventA(NULL, FALSE, FALSE, NULL);
    ev_done = CreateEventA(NULL, FALSE, FALSE, NULL);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = CoMarshalInterface(stream, &IID_IClassFactory, (IUnknown *)&cf, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    check(hr == S_OK, "CoMarshalInterface %#lx", hr);
    IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    pump_until(ev_ready);
    check(mta_before == TRUE, "proxy (interface pointer) connected, asked from the MTA");
    check(mta_unknown_before == TRUE, "proxy (IUnknown) connected, asked from the MTA");
    check(CoIsHandlerConnected((IUnknown *)&cf) == TRUE, "the server object is connected");

    hr = CoDisconnectObject((IUnknown *)&cf, 0);
    check(hr == S_OK, "CoDisconnectObject %#lx", hr);
    SetEvent(ev_go);
    pump_until(ev_done);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    check(mta_after == FALSE, "proxy (interface pointer) disconnected after CoDisconnectObject");
    check(mta_unknown_after == FALSE, "proxy (IUnknown) disconnected after CoDisconnectObject");
    check(CoIsHandlerConnected((IUnknown *)&cf) == TRUE, "the server object itself is not remote: connected");
    IStream_Release(stream);

    /* a second proxy, whose apartment is torn down */
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = CoMarshalInterface(stream, &IID_IClassFactory, (IUnknown *)&cf, MSHCTX_INPROC, NULL, MSHLFLAGS_TABLESTRONG);
    check(hr == S_OK, "CoMarshalInterface (table) %#lx", hr);
    IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
    thread = CreateThread(NULL, 0, leaker, NULL, 0, NULL);
    pump_until(thread);
    CloseHandle(thread);
    check(leaked != NULL, "proxy unmarshaled in the second apartment");
    if (leaked) check(CoIsHandlerConnected((IUnknown *)leaked) == FALSE, "proxy whose apartment was uninitialized is disconnected");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
