/* Probe for patches/sg/5121: the IMarshal that CoGetStandardMarshal makes for an
 * object has a working DisconnectObject (docs: "Releases all connections to an
 * object"; afterwards a proxy answers RPC_E_DISCONNECTED / CO_E_OBJNOTCONNECTED).
 * The object lives in the main STA (which pumps messages); a worker thread in
 * the MTA holds the proxy. Rows: calls before and after, for the marshaler with
 * an object and one without (the latter has nothing to disconnect: S_OK, proxy
 * stays connected). */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG locks;
static HRESULT WINAPI cf_qi(IClassFactory *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref(IClassFactory *i) { return 2; }
static ULONG WINAPI cf_release(IClassFactory *i) { return 1; }
static HRESULT WINAPI cf_create(IClassFactory *i, IUnknown *o, REFIID r, void **p) { return E_NOTIMPL; }
static HRESULT WINAPI cf_lock(IClassFactory *i, BOOL l) { InterlockedExchangeAdd(&locks, l ? 1 : -1); return S_OK; }
static const IClassFactoryVtbl cfvtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static IClassFactory cf = { (IClassFactoryVtbl *)&cfvtbl };

static HANDLE ev_proxy_ready, ev_disconnected, ev_done;
static IStream *stream;
static HRESULT hr_before, hr_after, hr_unmarshal;

static DWORD WINAPI worker(void *arg)
{
    IClassFactory *proxy = NULL;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr_unmarshal = CoUnmarshalInterface(stream, &IID_IClassFactory, (void **)&proxy);
    if (hr_unmarshal == S_OK) hr_before = IClassFactory_LockServer(proxy, TRUE);
    SetEvent(ev_proxy_ready);
    WaitForSingleObject(ev_disconnected, 20000);
    if (proxy)
    {
        hr_after = IClassFactory_LockServer(proxy, TRUE);
        IClassFactory_Release(proxy);
    }
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
        DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, 100, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
}

static HRESULT last_after;
static void run(const char *name, int mode)
{
    IMarshal *m = NULL;
    HANDLE thread;
    HRESULT hr;

    locks = 0; hr_before = hr_after = hr_unmarshal = E_FAIL;
    ev_proxy_ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    ev_disconnected = CreateEventA(NULL, FALSE, FALSE, NULL);
    ev_done = CreateEventA(NULL, FALSE, FALSE, NULL);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = CoMarshalInterface(stream, &IID_IClassFactory, (IUnknown *)&cf, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    check(hr == S_OK, "%s: CoMarshalInterface %#lx", name, hr);
    IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);

    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    pump_until(ev_proxy_ready);
    check(hr_unmarshal == S_OK && hr_before == S_OK && locks == 1, "%s: before: unmarshal %#lx, call %#lx, server saw %ld",
          name, hr_unmarshal, hr_before, locks);

    if (mode == 2)
    {
        hr = CoDisconnectObject((IUnknown *)&cf, 0);
        check(hr == S_OK, "%s: CoDisconnectObject %#lx", name, hr);
    }
    else
    {
        hr = CoGetStandardMarshal(&IID_IClassFactory, mode == 0 ? (IUnknown *)&cf : NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &m);
        check(hr == S_OK, "%s: CoGetStandardMarshal %#lx", name, hr);
        hr = IMarshal_DisconnectObject(m, 0);
        check(hr == S_OK, "%s: DisconnectObject %#lx", name, hr);
        IMarshal_Release(m);
    }
    SetEvent(ev_disconnected);
    pump_until(ev_done);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    if (mode == 2)
    {
        last_after = hr_after;
        check(hr_after != S_OK && locks == 1, "%s: call after disconnect fails %#lx, server saw %ld", name, hr_after, locks);
    }
    else if (mode == 0)
        check(hr_after != S_OK && hr_after == last_after && locks == 1,
              "%s: call after disconnect %#lx fails like after CoDisconnectObject (%#lx), server saw %ld", name, hr_after, last_after, locks);
    else
        check(hr_after == S_OK && locks == 2, "%s: call after a no-op disconnect %#lx, server saw %ld", name, hr_after, locks);
    CoDisconnectObject((IUnknown *)&cf, 0);
    IStream_Release(stream);
    CloseHandle(ev_proxy_ready); CloseHandle(ev_disconnected); CloseHandle(ev_done);
}

int main(void)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    check(hr == S_OK, "CoInitializeEx %#lx", hr);
    run("CoDisconnectObject (reference)", 2);
    run("marshaler for the object", 0);
    run("marshaler without an object", 1);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
