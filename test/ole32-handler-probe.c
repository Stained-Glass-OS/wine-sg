/* ole32's default handler / embedding helper: IDataObject::GetDataHere (cache, then the running object),
 * IRunnableObject::GetRunningClass (the class, E_INVALIDARG for NULL), IOleObject::Update (it runs the
 * object: REGDB_E_CLASSNOTREG for a class that is not registered, as Wine's ole2.c test records for Windows). */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID CLSID_Test = {0x5ee0e9b1, 0x5001, 0x4c01, {1, 2, 3, 4, 5, 6, 7, 8}};
static const GUID CLSID_Unregistered = {0x5ee0e9b1, 0x5001, 0x4c01, {8, 7, 6, 5, 4, 3, 2, 1}};

static int update_calls, getdatahere_calls;

/* the object the helper wraps */
typedef struct { IOleObject IOleObject_iface; IPersistStorage IPersistStorage_iface; IDataObject IDataObject_iface; LONG ref; } Obj;
static Obj *impl_o(IOleObject *i) { return CONTAINING_RECORD(i, Obj, IOleObject_iface); }
static Obj *impl_p(IPersistStorage *i) { return CONTAINING_RECORD(i, Obj, IPersistStorage_iface); }
static Obj *impl_d(IDataObject *i) { return CONTAINING_RECORD(i, Obj, IDataObject_iface); }

static HRESULT obj_qi(Obj *o, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleObject)) *out = &o->IOleObject_iface;
    else if (IsEqualIID(riid, &IID_IPersistStorage) || IsEqualIID(riid, &IID_IPersist)) *out = &o->IPersistStorage_iface;
    else if (IsEqualIID(riid, &IID_IDataObject)) *out = &o->IDataObject_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&o->ref);
    return S_OK;
}
static HRESULT WINAPI o_QI(IOleObject *i, REFIID r, void **p) { return obj_qi(impl_o(i), r, p); }
static ULONG WINAPI o_AddRef(IOleObject *i) { return InterlockedIncrement(&impl_o(i)->ref); }
static ULONG WINAPI o_Release(IOleObject *i) { Obj *o = impl_o(i); ULONG r = InterlockedDecrement(&o->ref); if (!r) HeapFree(GetProcessHeap(), 0, o); return r; }
static HRESULT WINAPI o_SetClientSite(IOleObject *i, IOleClientSite *s) { return S_OK; }
static HRESULT WINAPI o_GetClientSite(IOleObject *i, IOleClientSite **s) { return E_NOTIMPL; }
static HRESULT WINAPI o_SetHostNames(IOleObject *i, LPCOLESTR a, LPCOLESTR b) { return S_OK; }
static HRESULT WINAPI o_Close(IOleObject *i, DWORD f) { return S_OK; }
static HRESULT WINAPI o_SetMoniker(IOleObject *i, DWORD w, IMoniker *m) { return E_NOTIMPL; }
static HRESULT WINAPI o_GetMoniker(IOleObject *i, DWORD a, DWORD w, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI o_InitFromData(IOleObject *i, IDataObject *d, BOOL f, DWORD r) { return E_NOTIMPL; }
static HRESULT WINAPI o_GetClipboardData(IOleObject *i, DWORD r, IDataObject **d) { return E_NOTIMPL; }
static HRESULT WINAPI o_DoVerb(IOleObject *i, LONG v, LPMSG m, IOleClientSite *s, LONG l, HWND h, LPCRECT r) { return E_NOTIMPL; }
static HRESULT WINAPI o_EnumVerbs(IOleObject *i, IEnumOLEVERB **e) { return E_NOTIMPL; }
static HRESULT WINAPI o_Update(IOleObject *i) { update_calls++; return S_OK; }
static HRESULT WINAPI o_IsUpToDate(IOleObject *i) { return S_OK; }
static HRESULT WINAPI o_GetUserClassID(IOleObject *i, CLSID *c) { *c = CLSID_Test; return S_OK; }
static HRESULT WINAPI o_GetUserType(IOleObject *i, DWORD f, LPOLESTR *t) { return E_NOTIMPL; }
static HRESULT WINAPI o_SetExtent(IOleObject *i, DWORD a, SIZEL *s) { return E_NOTIMPL; }
static HRESULT WINAPI o_GetExtent(IOleObject *i, DWORD a, SIZEL *s) { return E_NOTIMPL; }
static HRESULT WINAPI o_Advise(IOleObject *i, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI o_Unadvise(IOleObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI o_EnumAdvise(IOleObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static HRESULT WINAPI o_GetMiscStatus(IOleObject *i, DWORD a, DWORD *s) { return E_NOTIMPL; }
static HRESULT WINAPI o_SetColorScheme(IOleObject *i, LOGPALETTE *p) { return E_NOTIMPL; }
static const IOleObjectVtbl o_vtbl =
{
    o_QI, o_AddRef, o_Release, o_SetClientSite, o_GetClientSite, o_SetHostNames, o_Close, o_SetMoniker, o_GetMoniker,
    o_InitFromData, o_GetClipboardData, o_DoVerb, o_EnumVerbs, o_Update, o_IsUpToDate, o_GetUserClassID, o_GetUserType,
    o_SetExtent, o_GetExtent, o_Advise, o_Unadvise, o_EnumAdvise, o_GetMiscStatus, o_SetColorScheme,
};

static HRESULT WINAPI p_QI(IPersistStorage *i, REFIID r, void **p) { return obj_qi(impl_p(i), r, p); }
static ULONG WINAPI p_AddRef(IPersistStorage *i) { return InterlockedIncrement(&impl_p(i)->ref); }
static ULONG WINAPI p_Release(IPersistStorage *i) { return o_Release(&impl_p(i)->IOleObject_iface); }
static HRESULT WINAPI p_GetClassID(IPersistStorage *i, CLSID *c) { *c = CLSID_Test; return S_OK; }
static HRESULT WINAPI p_IsDirty(IPersistStorage *i) { return S_FALSE; }
static HRESULT WINAPI p_InitNew(IPersistStorage *i, IStorage *s) { return S_OK; }
static HRESULT WINAPI p_Load(IPersistStorage *i, IStorage *s) { return S_OK; }
static HRESULT WINAPI p_Save(IPersistStorage *i, IStorage *s, BOOL b) { return S_OK; }
static HRESULT WINAPI p_SaveCompleted(IPersistStorage *i, IStorage *s) { return S_OK; }
static HRESULT WINAPI p_HandsOffStorage(IPersistStorage *i) { return S_OK; }
static const IPersistStorageVtbl p_vtbl = { p_QI, p_AddRef, p_Release, p_GetClassID, p_IsDirty, p_InitNew, p_Load, p_Save, p_SaveCompleted, p_HandsOffStorage };

static HRESULT WINAPI d_QI(IDataObject *i, REFIID r, void **p) { return obj_qi(impl_d(i), r, p); }
static ULONG WINAPI d_AddRef(IDataObject *i) { return InterlockedIncrement(&impl_d(i)->ref); }
static ULONG WINAPI d_Release(IDataObject *i) { return o_Release(&impl_d(i)->IOleObject_iface); }
static HRESULT WINAPI d_GetData(IDataObject *i, FORMATETC *f, STGMEDIUM *m) { return DV_E_FORMATETC; }
static HRESULT WINAPI d_GetDataHere(IDataObject *i, FORMATETC *f, STGMEDIUM *m)
{
    getdatahere_calls++;
    if (f->cfFormat != CF_TEXT || m->tymed != TYMED_HGLOBAL) return DV_E_FORMATETC;
    memcpy(GlobalLock(m->hGlobal), "HERE", 5);
    GlobalUnlock(m->hGlobal);
    return S_OK;
}
static HRESULT WINAPI d_QueryGetData(IDataObject *i, FORMATETC *f) { return S_OK; }
static HRESULT WINAPI d_GetCanonicalFormatEtc(IDataObject *i, FORMATETC *a, FORMATETC *b) { return E_NOTIMPL; }
static HRESULT WINAPI d_SetData(IDataObject *i, FORMATETC *f, STGMEDIUM *m, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI d_EnumFormatEtc(IDataObject *i, DWORD d, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI d_DAdvise(IDataObject *i, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI d_DUnadvise(IDataObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI d_EnumDAdvise(IDataObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static const IDataObjectVtbl d_vtbl =
{
    d_QI, d_AddRef, d_Release, d_GetData, d_GetDataHere, d_QueryGetData, d_GetCanonicalFormatEtc, d_SetData,
    d_EnumFormatEtc, d_DAdvise, d_DUnadvise, d_EnumDAdvise,
};

/* a class factory making it */
static HRESULT WINAPI cf_QI(IClassFactory *i, REFIID r, void **p) { *p = i; return S_OK; }
static ULONG WINAPI cf_AddRef(IClassFactory *i) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *i) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *i, IUnknown *outer, REFIID riid, void **out)
{
    Obj *o = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*o));
    HRESULT hr;
    o->IOleObject_iface.lpVtbl = &o_vtbl;
    o->IPersistStorage_iface.lpVtbl = &p_vtbl;
    o->IDataObject_iface.lpVtbl = &d_vtbl;
    o->ref = 1;
    hr = obj_qi(o, riid, out);
    o_Release(&o->IOleObject_iface);
    return hr;
}
static HRESULT WINAPI cf_LockServer(IClassFactory *i, BOOL l) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer };
static IClassFactory cf = { &cf_vtbl };

static void test_handler(IOleObject *ole, BOOL running, const GUID *clsid, const char *kind)
{
    IDataObject *data;
    IRunnableObject *runnable;
    FORMATETC fmt = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM med;
    HRESULT hr;
    GUID got;
    char text[160];
    int before;

    hr = IOleObject_QueryInterface(ole, &IID_IDataObject, (void **)&data);
    check(hr == S_OK, "QueryInterface(IDataObject)");
    hr = IOleObject_QueryInterface(ole, &IID_IRunnableObject, (void **)&runnable);
    check(hr == S_OK, "QueryInterface(IRunnableObject)");
    if (failures) return;

    /* GetDataHere: the running object answers, a handler that is not running says so */
    memset(&med, 0, sizeof(med));
    med.tymed = TYMED_HGLOBAL;
    med.hGlobal = GlobalAlloc(GMEM_MOVEABLE, 16);
    before = getdatahere_calls;
    hr = IDataObject_GetDataHere(data, &fmt, &med);
    if (running)
    {
        sprintf(text, "%s: GetDataHere reaches the running object (hr %#lx)", kind, hr);
        check(hr == S_OK && getdatahere_calls == before + 1 && !memcmp(GlobalLock(med.hGlobal), "HERE", 5), text);
        GlobalUnlock(med.hGlobal);
    }
    else
    {
        sprintf(text, "%s: GetDataHere says OLE_E_NOTRUNNING (hr %#lx)", kind, hr);
        check(hr == OLE_E_NOTRUNNING && getdatahere_calls == before, text);
    }
    GlobalFree(med.hGlobal);

    /* GetRunningClass */
    memset(&got, 0, sizeof(got));
    hr = IRunnableObject_GetRunningClass(runnable, &got);
    sprintf(text, "%s: GetRunningClass gives the class (hr %#lx)", kind, hr);
    check(hr == S_OK && IsEqualGUID(&got, clsid), text);
    hr = IRunnableObject_GetRunningClass(runnable, NULL);
    sprintf(text, "%s: GetRunningClass(NULL) is E_INVALIDARG (hr %#lx)", kind, hr);
    check(hr == E_INVALIDARG, text);

    /* LockRunning: an external connection holds the object while it is locked */
    {
        IUnknown *unk;
        ULONG base, locked, unlocked;

        hr = IOleObject_QueryInterface(ole, &IID_IUnknown, (void **)&unk);
        check(hr == S_OK, "QueryInterface(IUnknown)");
        IUnknown_AddRef(unk);
        base = IUnknown_Release(unk);
        hr = IRunnableObject_LockRunning(runnable, TRUE, FALSE);
        sprintf(text, "%s: LockRunning(TRUE) (hr %#lx)", kind, hr);
        check(hr == S_OK, text);
        IUnknown_AddRef(unk);
        locked = IUnknown_Release(unk);
        sprintf(text, "%s: the lock holds a reference (%lu -> %lu)", kind, base, locked);
        check(locked > base, text);
        hr = IRunnableObject_LockRunning(runnable, FALSE, TRUE);
        sprintf(text, "%s: LockRunning(FALSE) (hr %#lx)", kind, hr);
        check(hr == S_OK, text);
        IUnknown_AddRef(unk);
        unlocked = IUnknown_Release(unk);
        sprintf(text, "%s: unlocking lets go (%lu -> %lu)", kind, locked, unlocked);
        check(unlocked <= base, text);
        IUnknown_Release(unk);
    }
    hr = IRunnableObject_SetContainedObject(runnable, TRUE);
    sprintf(text, "%s: SetContainedObject(TRUE) (hr %#lx)", kind, hr);
    check(hr == S_OK, text);

    /* Update runs the object */
    before = update_calls;
    hr = IOleObject_Update(ole);
    if (running)
    {
        sprintf(text, "%s: Update reaches the running object (hr %#lx)", kind, hr);
        check(hr == S_OK && update_calls == before + 1, text);
    }
    else
    {
        sprintf(text, "%s: Update must run the object and fails REGDB_E_CLASSNOTREG (hr %#lx)", kind, hr);
        check(hr == REGDB_E_CLASSNOTREG && update_calls == before, text);
    }

    IRunnableObject_Release(runnable);
    IDataObject_Release(data);
}

int main(void)
{
    IOleObject *ole;
    HRESULT hr;

    OleInitialize(NULL);

    hr = OleCreateEmbeddingHelper(&CLSID_Test, NULL, EMBDHLP_INPROC_SERVER, &cf, &IID_IOleObject, (void **)&ole);
    check(hr == S_OK, "OleCreateEmbeddingHelper with a class factory");
    if (hr == S_OK)
    {
        test_handler(ole, TRUE, &CLSID_Test, "running helper");
        IOleObject_Release(ole);
    }

    hr = OleCreateDefaultHandler(&CLSID_Unregistered, NULL, &IID_IOleObject, (void **)&ole);
    check(hr == S_OK, "OleCreateDefaultHandler for a class that is not registered");
    if (hr == S_OK)
    {
        test_handler(ole, FALSE, &CLSID_Unregistered, "handler");
        IOleObject_Release(ole);
    }

    OleUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
