/* Probe for patches/sg/5030: the call_as proxies and stubs of
 * IClassFactory::LockServer, IBindCtx::Set/GetBindOptions,
 * IRunnableObject::IsRunning and IMoniker::BindToObject forward the call to
 * the object across apartments (they returned E_NOTIMPL / FALSE). Objects live
 * in an STA thread; the main thread (MTA) calls them through ole32's proxies.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG locks;
static BOOL running = TRUE;
static IUnknown *bound_object;

/* a class factory counting LockServer */
static HRESULT WINAPI cf_QueryInterface(IClassFactory *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory)) { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **obj) { return E_NOTIMPL; }
static HRESULT WINAPI cf_LockServer(IClassFactory *iface, BOOL lock) { InterlockedAdd(&locks, lock ? 1 : -1); return S_OK; }
static IClassFactoryVtbl cf_vtbl = {cf_QueryInterface, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer};
static IClassFactory cf = {&cf_vtbl};

/* a runnable object */
static HRESULT WINAPI ro_QueryInterface(IRunnableObject *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRunnableObject)) { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ro_AddRef(IRunnableObject *iface) { return 2; }
static ULONG WINAPI ro_Release(IRunnableObject *iface) { return 1; }
static HRESULT WINAPI ro_GetRunningClass(IRunnableObject *iface, CLSID *clsid) { return E_NOTIMPL; }
static HRESULT WINAPI ro_Run(IRunnableObject *iface, IBindCtx *pbc) { return E_NOTIMPL; }
static BOOL WINAPI ro_IsRunning(IRunnableObject *iface) { return running; }
static HRESULT WINAPI ro_LockRunning(IRunnableObject *iface, BOOL lock, BOOL last) { return E_NOTIMPL; }
static HRESULT WINAPI ro_SetContainedObject(IRunnableObject *iface, BOOL contained) { return E_NOTIMPL; }
static IRunnableObjectVtbl ro_vtbl = {ro_QueryInterface, ro_AddRef, ro_Release, ro_GetRunningClass, ro_Run, ro_IsRunning,
                                      ro_LockRunning, ro_SetContainedObject};
static IRunnableObject ro = {&ro_vtbl};

/* a moniker whose BindToObject gives the class factory above */
static HRESULT WINAPI mk_QueryInterface(IMoniker *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMoniker) || IsEqualIID(riid, &IID_IPersist)
            || IsEqualIID(riid, &IID_IPersistStream)) { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI mk_AddRef(IMoniker *iface) { return 2; }
static ULONG WINAPI mk_Release(IMoniker *iface) { return 1; }
static HRESULT WINAPI mk_GetClassID(IMoniker *iface, CLSID *clsid) { return E_NOTIMPL; }
static HRESULT WINAPI mk_IsDirty(IMoniker *iface) { return S_FALSE; }
static HRESULT WINAPI mk_Load(IMoniker *iface, IStream *stm) { return E_NOTIMPL; }
static HRESULT WINAPI mk_Save(IMoniker *iface, IStream *stm, BOOL clear) { return E_NOTIMPL; }
static HRESULT WINAPI mk_GetSizeMax(IMoniker *iface, ULARGE_INTEGER *size) { return E_NOTIMPL; }
static HRESULT WINAPI mk_BindToObject(IMoniker *iface, IBindCtx *pbc, IMoniker *left, REFIID riid, void **obj)
{
    return IUnknown_QueryInterface(bound_object, riid, obj);
}
static HRESULT WINAPI mk_BindToStorage(IMoniker *iface, IBindCtx *pbc, IMoniker *left, REFIID riid, void **obj) { return E_NOTIMPL; }
static HRESULT WINAPI mk_Reduce(IMoniker *iface, IBindCtx *pbc, DWORD how, IMoniker **left, IMoniker **reduced) { return E_NOTIMPL; }
static HRESULT WINAPI mk_ComposeWith(IMoniker *iface, IMoniker *right, BOOL only, IMoniker **composite) { return E_NOTIMPL; }
static HRESULT WINAPI mk_Enum(IMoniker *iface, BOOL forward, IEnumMoniker **e) { return E_NOTIMPL; }
static HRESULT WINAPI mk_IsEqual(IMoniker *iface, IMoniker *other) { return E_NOTIMPL; }
static HRESULT WINAPI mk_Hash(IMoniker *iface, DWORD *hash) { return E_NOTIMPL; }
static HRESULT WINAPI mk_IsRunning(IMoniker *iface, IBindCtx *pbc, IMoniker *left, IMoniker *newly) { return E_NOTIMPL; }
static HRESULT WINAPI mk_GetTimeOfLastChange(IMoniker *iface, IBindCtx *pbc, IMoniker *left, FILETIME *t) { return E_NOTIMPL; }
static HRESULT WINAPI mk_Inverse(IMoniker *iface, IMoniker **mk) { return E_NOTIMPL; }
static HRESULT WINAPI mk_CommonPrefixWith(IMoniker *iface, IMoniker *other, IMoniker **prefix) { return E_NOTIMPL; }
static HRESULT WINAPI mk_RelativePathTo(IMoniker *iface, IMoniker *other, IMoniker **rel) { return E_NOTIMPL; }
static HRESULT WINAPI mk_GetDisplayName(IMoniker *iface, IBindCtx *pbc, IMoniker *left, LPOLESTR *name) { return E_NOTIMPL; }
static HRESULT WINAPI mk_ParseDisplayName(IMoniker *iface, IBindCtx *pbc, IMoniker *left, LPOLESTR name, ULONG *eaten, IMoniker **out) { return E_NOTIMPL; }
static HRESULT WINAPI mk_IsSystemMoniker(IMoniker *iface, DWORD *sys) { return E_NOTIMPL; }
static IMonikerVtbl mk_vtbl =
{
    mk_QueryInterface, mk_AddRef, mk_Release, mk_GetClassID, mk_IsDirty, mk_Load, mk_Save, mk_GetSizeMax,
    mk_BindToObject, mk_BindToStorage, mk_Reduce, mk_ComposeWith, mk_Enum, mk_IsEqual, mk_Hash, mk_IsRunning,
    mk_GetTimeOfLastChange, mk_Inverse, mk_CommonPrefixWith, mk_RelativePathTo, mk_GetDisplayName,
    mk_ParseDisplayName, mk_IsSystemMoniker,
};
static IMoniker mk = {&mk_vtbl};

static IStream *streams[4];
static HANDLE ready, quit;

static DWORD WINAPI sta_thread(void *arg)
{
    IBindCtx *bc;
    MSG msg;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    CreateBindCtx(0, &bc);
    bound_object = (IUnknown *)&cf;
    CoMarshalInterThreadInterfaceInStream(&IID_IClassFactory, (IUnknown *)&cf, &streams[0]);
    CoMarshalInterThreadInterfaceInStream(&IID_IBindCtx, (IUnknown *)bc, &streams[1]);
    CoMarshalInterThreadInterfaceInStream(&IID_IRunnableObject, (IUnknown *)&ro, &streams[2]);
    CoMarshalInterThreadInterfaceInStream(&IID_IMoniker, (IUnknown *)&mk, &streams[3]);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &quit, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    IBindCtx_Release(bc);
    CoUninitialize();
    return 0;
}

int main(void)
{
    IClassFactory *cfp = NULL, *got = NULL;
    IBindCtx *bcp = NULL;
    IRunnableObject *rop = NULL;
    IMoniker *mkp = NULL, *local;
    BIND_OPTS opts;
    BIND_OPTS2 opts2;
    HANDLE thread;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    quit = CreateEventA(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, sta_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    CoGetInterfaceAndReleaseStream(streams[0], &IID_IClassFactory, (void **)&cfp);
    CoGetInterfaceAndReleaseStream(streams[1], &IID_IBindCtx, (void **)&bcp);
    CoGetInterfaceAndReleaseStream(streams[2], &IID_IRunnableObject, (void **)&rop);
    CoGetInterfaceAndReleaseStream(streams[3], &IID_IMoniker, (void **)&mkp);
    check(cfp && bcp && rop && mkp, "proxies made (%p %p %p %p)", cfp, bcp, rop, mkp);
    if (!(cfp && bcp && rop && mkp)) { printf("RESULT: FAIL\n"); return 1; }
    check(cfp != &cf && rop != &ro && mkp != &mk, "they are proxies");

    hr = IClassFactory_LockServer(cfp, TRUE);
    check(hr == S_OK && locks == 1, "LockServer(TRUE): %#lx, %ld locks", hr, locks);
    hr = IClassFactory_LockServer(cfp, FALSE);
    check(hr == S_OK && locks == 0, "LockServer(FALSE): %#lx, %ld locks", hr, locks);

    memset(&opts, 0, sizeof(opts));
    opts.cbStruct = sizeof(opts);
    opts.grfMode = STGM_READWRITE;
    opts.dwTickCountDeadline = 1234;
    hr = IBindCtx_SetBindOptions(bcp, &opts);
    check(hr == S_OK, "SetBindOptions(BIND_OPTS): %#lx", hr);
    memset(&opts2, 0x55, sizeof(opts2));
    opts2.cbStruct = sizeof(opts2);
    hr = IBindCtx_GetBindOptions(bcp, (BIND_OPTS *)&opts2);
    check(hr == S_OK && opts2.grfMode == STGM_READWRITE && opts2.dwTickCountDeadline == 1234 && opts2.cbStruct == sizeof(opts2),
          "GetBindOptions(BIND_OPTS2): %#lx mode %#lx deadline %lu size %lu", hr, opts2.grfMode, opts2.dwTickCountDeadline,
          opts2.cbStruct);
    memset(&opts, 0x55, sizeof(opts));
    opts.cbStruct = sizeof(opts);
    hr = IBindCtx_GetBindOptions(bcp, &opts);
    check(hr == S_OK && opts.grfMode == STGM_READWRITE && opts.cbStruct == sizeof(opts),
          "GetBindOptions(BIND_OPTS): %#lx mode %#lx size %lu", hr, opts.grfMode, opts.cbStruct);

    running = TRUE;
    check(IRunnableObject_IsRunning(rop) == TRUE, "IsRunning while running");
    running = FALSE;
    check(IRunnableObject_IsRunning(rop) == FALSE, "IsRunning while not running");

    CreateBindCtx(0, (IBindCtx **)&local);
    hr = IMoniker_BindToObject(mkp, (IBindCtx *)local, NULL, &IID_IClassFactory, (void **)&got);
    check(hr == S_OK && got != NULL, "BindToObject: %#lx %p", hr, got);
    if (got)
    {
        hr = IClassFactory_LockServer(got, TRUE);
        check(hr == S_OK && locks == 1, "the bound object answers: %#lx %ld", hr, locks);
        IClassFactory_LockServer(got, FALSE);
        IClassFactory_Release(got);
    }
    IUnknown_Release((IUnknown *)local);

    IClassFactory_Release(cfp);
    IBindCtx_Release(bcp);
    IRunnableObject_Release(rop);
    IMoniker_Release(mkp);
    SetEvent(quit);
    WaitForSingleObject(thread, 10000);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
