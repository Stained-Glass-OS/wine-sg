/* CoGetCallerTID, message filter retries, CoRegisterSurrogate (patch 3760) */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- the object in the server apartment: a class factory that tells who called ---- */
static volatile LONG seen_tid_hr;
static volatile DWORD seen_tid;
static volatile LONG lock_calls;

static HRESULT WINAPI cf_QI(IClassFactory *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
/* the method that ran: it records who called, and answers E_FAIL (nothing is created) */
#define RAN E_FAIL
static HRESULT WINAPI cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    DWORD tid = 0xdeadbeef;
    HRESULT hr = CoGetCallerTID(&tid);
    seen_tid = tid;
    seen_tid_hr = hr;
    InterlockedIncrement(&lock_calls);
    *ppv = NULL;
    return RAN;
}
static HRESULT WINAPI cf_LockServer(IClassFactory *iface, BOOL lock) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer };
static IClassFactory factory = { &cf_vtbl };

/* ---- message filters ---- */
struct filter
{
    IMessageFilter iface;
    volatile LONG reject_left;       /* HandleInComingCall: reject this many calls first */
    DWORD reject_type;               /* SERVERCALL_REJECTED / SERVERCALL_RETRYLATER */
    volatile LONG incoming;
    volatile LONG retry_calls;       /* RetryRejectedCall */
    DWORD retry_answer;              /* what it answers */
    DWORD retry_types[8], retry_ticks[8], retry_tasks[8];
};
static struct filter *impl_filter(IMessageFilter *i) { return (struct filter *)i; }
static HRESULT WINAPI mf_QI(IMessageFilter *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IMessageFilter)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI mf_AddRef(IMessageFilter *iface) { return 2; }
static ULONG WINAPI mf_Release(IMessageFilter *iface) { return 1; }
static DWORD WINAPI mf_Handle(IMessageFilter *iface, DWORD calltype, HTASK caller, DWORD tick, LPINTERFACEINFO info)
{
    struct filter *flt = impl_filter(iface);
    InterlockedIncrement(&flt->incoming);
    if (flt->reject_left > 0) { InterlockedDecrement(&flt->reject_left); return flt->reject_type; }
    return SERVERCALL_ISHANDLED;
}
static DWORD WINAPI mf_Retry(IMessageFilter *iface, HTASK callee, DWORD tick, DWORD type)
{
    struct filter *flt = impl_filter(iface);
    LONG n = InterlockedIncrement(&flt->retry_calls) - 1;
    if (n < 8) { flt->retry_types[n] = type; flt->retry_ticks[n] = tick; flt->retry_tasks[n] = HandleToUlong(callee); }
    return flt->retry_answer;
}
static DWORD WINAPI mf_Pending(IMessageFilter *iface, HTASK callee, DWORD tick, DWORD type) { return PENDINGMSG_WAITNOPROCESS; }
static const IMessageFilterVtbl mf_vtbl = { mf_QI, mf_AddRef, mf_Release, mf_Handle, mf_Retry, mf_Pending };

/* ---- the server thread ---- */
struct server
{
    BOOL mta;
    struct filter *filter;
    IStream *stream;
    HANDLE ready, quit;
    DWORD tid;
};

static DWORD WINAPI server_thread(void *arg)
{
    struct server *s = arg;
    MSG msg;
    HRESULT hr;

    s->tid = GetCurrentThreadId();
    CoInitializeEx(NULL, s->mta ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED);
    if (s->filter && !s->mta) CoRegisterMessageFilter(&s->filter->iface, NULL);
    hr = CreateStreamOnHGlobal(NULL, TRUE, &s->stream);
    if (SUCCEEDED(hr)) hr = CoMarshalInterface(s->stream, &IID_IClassFactory, (IUnknown *)&factory, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    CHECK(SUCCEEDED(hr), "marshal: %lx", hr);
    SetEvent(s->ready);
    if (s->mta) WaitForSingleObject(s->quit, INFINITE);
    else
        for (;;)
        {
            DWORD r = MsgWaitForMultipleObjects(1, &s->quit, FALSE, INFINITE, QS_ALLINPUT);
            if (r == WAIT_OBJECT_0) break;
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
    if (s->filter && !s->mta) CoRegisterMessageFilter(NULL, NULL);
    CoUninitialize();
    return 0;
}

static HANDLE start_server(struct server *s, BOOL mta, struct filter *flt)
{
    HANDLE th;
    memset(s, 0, sizeof(*s));
    s->mta = mta; s->filter = flt;
    s->ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    s->quit = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, server_thread, s, 0, NULL);
    WaitForSingleObject(s->ready, 10000);
    return th;
}

static void stop_server(struct server *s, HANDLE th)
{
    SetEvent(s->quit);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CloseHandle(s->ready); CloseHandle(s->quit);
}

static IClassFactory *unmarshal(struct server *s)
{
    IClassFactory *cf = NULL;
    LARGE_INTEGER zero = { 0 };
    IStream_Seek(s->stream, zero, STREAM_SEEK_SET, NULL);
    CoUnmarshalInterface(s->stream, &IID_IClassFactory, (void **)&cf);
    return cf;
}

static void init_filter(struct filter *flt, LONG rejects, DWORD type, DWORD retry_answer)
{
    memset(flt, 0, sizeof(*flt));
    flt->iface.lpVtbl = &mf_vtbl;
    flt->reject_left = rejects;
    flt->reject_type = type;
    flt->retry_answer = retry_answer;
}

/* ---- ISurrogate mock ---- */
static LONG sur_refs;
static HRESULT WINAPI sur_QI(ISurrogate *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ISurrogate)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sur_AddRef(ISurrogate *iface) { return InterlockedIncrement(&sur_refs); }
static ULONG WINAPI sur_Release(ISurrogate *iface) { return InterlockedDecrement(&sur_refs); }
static HRESULT WINAPI sur_Load(ISurrogate *iface, REFCLSID clsid) { return S_OK; }
static HRESULT WINAPI sur_Free(ISurrogate *iface) { return S_OK; }
static const ISurrogateVtbl sur_vtbl = { sur_QI, sur_AddRef, sur_Release, sur_Load, sur_Free };
struct sur { ISurrogate iface; LONG refs; };

static LONG refs1, refs2;
static ULONG WINAPI s1_AddRef(ISurrogate *iface) { return InterlockedIncrement(&refs1); }
static ULONG WINAPI s1_Release(ISurrogate *iface) { return InterlockedDecrement(&refs1); }
static ULONG WINAPI s2_AddRef(ISurrogate *iface) { return InterlockedIncrement(&refs2); }
static ULONG WINAPI s2_Release(ISurrogate *iface) { return InterlockedDecrement(&refs2); }
static const ISurrogateVtbl s1_vtbl = { sur_QI, s1_AddRef, s1_Release, sur_Load, sur_Free };
static const ISurrogateVtbl s2_vtbl = { sur_QI, s2_AddRef, s2_Release, sur_Load, sur_Free };

static void surrogate_tests(void)
{
    ISurrogate s1 = { &s1_vtbl }, s2 = { &s2_vtbl };
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    SetLastError(0xdeadbeef);
    hr = CoRegisterSurrogate(NULL);
    CHECK(hr == E_INVALIDARG, "NULL surrogate: %lx", hr);
    hr = CoRegisterSurrogate(&s1);
    CHECK(hr == S_OK, "register: %lx", hr);
    CHECK(refs1 == 1, "COM holds a reference: %ld", refs1);
    hr = CoRegisterSurrogate(&s1);
    CHECK(hr == S_OK && refs1 == 1, "the same again: %lx refs %ld", hr, refs1);
    hr = CoRegisterSurrogate(&s2);
    CHECK(hr == S_OK, "another: %lx", hr);
    CHECK(refs1 == 0 && refs2 == 1, "the first is released, the second held: %ld %ld", refs1, refs2);
    CoUninitialize();
}

static const CLSID CLSID_Fastb = { 0x6d2a3f10, 0x1111, 0x4222, { 0x93, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xab } };

static int child_main(void)
{
    IClassFactory *cf = NULL;
    HRESULT hr;
    void *unk;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoGetClassObject(&CLSID_Fastb, CLSCTX_LOCAL_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    if (FAILED(hr)) { printf("CHILD getclass %lx\n", hr); return 2; }
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, &unk);
    IClassFactory_Release(cf);
    printf("CHILD call %lx\n", hr);
    CoUninitialize();
    return hr == E_FAIL ? 0 : 3;
}

static void remote_test(void)
{
    DWORD reg = 0;
    WCHAR path[MAX_PATH], cmd[MAX_PATH + 16];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HRESULT hr = CoRegisterClassObject(&CLSID_Fastb, (IUnknown *)&factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &reg);

    CHECK(hr == S_OK, "CoRegisterClassObject: %lx", hr);
    if (hr != S_OK) return;
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wsprintfW(cmd, L"\"%s\" child", path);
    seen_tid_hr = 0x1234; seen_tid = 0x5678;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        DWORD code = 99;
        MSG msg;
        for (;;)
        {
            DWORD r = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 30000, QS_ALLINPUT);
            if (r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT) break;
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        CHECK(code == 0, "the child's call ran: exit %lu", code);
        CHECK(seen_tid_hr == S_FALSE && seen_tid == 0, "a caller in another process: %lx tid %lx", seen_tid_hr, seen_tid);
    }
    else CHECK(0, "CreateProcess: %lu", GetLastError());
    CoRevokeClassObject(reg);
}

int main(int argc, char **argv)
{
    struct server srv;
    struct filter sf, cfl;
    HANDLE th;
    if (argc >= 2 && !strcmp(argv[1], "child")) return child_main();
    IClassFactory *cf;
    HRESULT hr;
    DWORD tid, start;
    void *unk;

    /* outside a call: the caller is this apartment */
    tid = 0xdeadbeef;
    hr = CoGetCallerTID(&tid);
    CHECK(hr == CO_E_NOTINITIALIZED, "no apartment: %lx", hr);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoGetCallerTID(&tid);
    CHECK(hr == S_OK && tid == GetCurrentThreadId(), "STA, no call: %lx %lx", hr, tid);
    CHECK(CoGetCallerTID(NULL) == E_INVALIDARG, "NULL out pointer");

    /* an STA calls an STA: the callee sees the caller's thread */
    init_filter(&sf, 0, 0, 0);
    th = start_server(&srv, FALSE, &sf);
    cf = unmarshal(&srv);
    CHECK(cf != NULL, "unmarshal");
    lock_calls = 0;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN, "call: %lx", hr);
    CHECK(seen_tid_hr == S_OK && seen_tid == GetCurrentThreadId(), "STA caller: %lx tid %lx / %lx", seen_tid_hr, seen_tid, GetCurrentThreadId());
    tid = 0;
    CoGetCallerTID(&tid);
    CHECK(tid == GetCurrentThreadId(), "after the call: back to this thread");

    /* retry: the callee's filter rejects twice with RETRYLATER; the caller has no filter */
    sf.reject_left = 1; sf.reject_type = SERVERCALL_RETRYLATER;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RPC_E_SERVERCALL_RETRYLATER, "no caller filter, retry later: %lx", hr);
    sf.reject_left = 1; sf.reject_type = SERVERCALL_REJECTED;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RPC_E_CALL_REJECTED, "no caller filter, rejected: %lx", hr);

    /* the caller's filter says retry at once */
    init_filter(&cfl, 0, 0, 0);
    hr = CoRegisterMessageFilter(&cfl.iface, NULL);
    CHECK(hr == S_OK, "register the caller's filter");
    sf.reject_left = 2; sf.reject_type = SERVERCALL_RETRYLATER;
    lock_calls = 0;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN, "retried at once: %lx", hr);
    CHECK(cfl.retry_calls == 2, "RetryRejectedCall twice: %ld", cfl.retry_calls);
    CHECK(cfl.retry_types[0] == SERVERCALL_RETRYLATER && cfl.retry_types[1] == SERVERCALL_RETRYLATER, "types %lu %lu", cfl.retry_types[0], cfl.retry_types[1]);
    CHECK(cfl.retry_tasks[0] == srv.tid, "the callee thread: %lx / %lx", cfl.retry_tasks[0], srv.tid);
    CHECK(cfl.retry_ticks[1] >= cfl.retry_ticks[0], "ticks grow: %lu %lu", cfl.retry_ticks[0], cfl.retry_ticks[1]);
    CHECK(lock_calls == 1 && sf.incoming >= 3, "the method ran once after three tries: %ld calls, %ld incoming", lock_calls, sf.incoming);

    /* a rejected call is offered too, with its type */
    cfl.retry_calls = 0;
    sf.reject_left = 1; sf.reject_type = SERVERCALL_REJECTED;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN && cfl.retry_calls == 1 && cfl.retry_types[0] == SERVERCALL_REJECTED, "rejected: %lx, %ld calls, type %lu", hr, cfl.retry_calls, cfl.retry_types[0]);

    /* -1 cancels the call: RPC_E_CALL_REJECTED */
    cfl.retry_calls = 0; cfl.retry_answer = 0xffffffff;
    sf.reject_left = 5; sf.reject_type = SERVERCALL_RETRYLATER;
    lock_calls = 0;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RPC_E_CALL_REJECTED, "cancel: %lx", hr);
    CHECK(cfl.retry_calls == 1 && lock_calls == 0, "asked once, the method did not run: %ld / %ld", cfl.retry_calls, lock_calls);

    /* 100 and more: wait that many milliseconds, then retry */
    sf.reject_left = 0;
    cfl.retry_calls = 0; cfl.retry_answer = 300;
    sf.reject_left = 1; sf.reject_type = SERVERCALL_RETRYLATER;
    start = GetTickCount();
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN, "after a wait: %lx", hr);
    CHECK(GetTickCount() - start >= 280, "it waited: %lu ms", GetTickCount() - start);
    /* less than 100: at once */
    cfl.retry_calls = 0; cfl.retry_answer = 99;
    sf.reject_left = 1; sf.reject_type = SERVERCALL_RETRYLATER;
    start = GetTickCount();
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN && GetTickCount() - start < 90, "99 is at once: %lx %lu ms", hr, GetTickCount() - start);

    /* the tick count is the time since the call was first made */
    cfl.retry_calls = 0; cfl.retry_answer = 300;
    sf.reject_left = 2; sf.reject_type = SERVERCALL_RETRYLATER;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN && cfl.retry_calls == 2, "two waits: %lx %ld", hr, cfl.retry_calls);
    CHECK(cfl.retry_ticks[0] < 150 && cfl.retry_ticks[1] >= 280 && cfl.retry_ticks[1] < 2000, "ticks since the first try: %lu then %lu", cfl.retry_ticks[0], cfl.retry_ticks[1]);

    /* the filter keeps on answering 0: the loop is the filter's to end */
    cfl.retry_calls = 0; cfl.retry_answer = 0;
    sf.reject_left = 6; sf.reject_type = SERVERCALL_RETRYLATER;
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN && cfl.retry_calls == 6, "six rejections: %lx, %ld asks", hr, cfl.retry_calls);

    CoRegisterMessageFilter(NULL, NULL);
    IClassFactory_Release(cf);
    stop_server(&srv, th);

    /* an STA calls an MTA object: the callee sees the caller's thread; the caller is an MTA thread: 0 */
    th = start_server(&srv, TRUE, NULL);
    cf = unmarshal(&srv);
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN && seen_tid_hr == S_OK && seen_tid == GetCurrentThreadId(), "STA to MTA: %lx tid %lx", seen_tid_hr, seen_tid);
    IClassFactory_Release(cf);
    stop_server(&srv, th);

    /* a caller in another process */
    remote_test();
    CoUninitialize();

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoGetCallerTID(&tid);
    CHECK(hr == S_OK && tid == 0, "MTA, no call: %lx %lx", hr, tid);
    th = start_server(&srv, FALSE, NULL);
    cf = unmarshal(&srv);
    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&unk);
    CHECK(hr == RAN && seen_tid_hr == S_OK && seen_tid == 0, "MTA to STA: the caller is the MTA: %lx tid %lx", seen_tid_hr, seen_tid);
    IClassFactory_Release(cf);
    stop_server(&srv, th);
    CoUninitialize();

    surrogate_tests();

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
