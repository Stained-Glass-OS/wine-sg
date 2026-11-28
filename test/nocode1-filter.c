/* A tiny IFilter for test/nocode1-gate.sh (BindIFilterFromStream/Storage, patches/sg/8014): it reports, as
 * its first chunk's id, how many bytes IPersistStream::Load read (or 1000 + 1 for IPersistStorage::Load). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <ole2.h>
#include <filter.h>
#include <filterr.h>

static const CLSID CLSID_SgFilter = {0x5d6a8a1e,0x2b8e,0x4d7c,{0x9a,0x51,0x3e,0x6f,0x10,0x22,0x33,0x01}};

struct filter
{
    IFilter IFilter_iface;
    IPersistStream IPersistStream_iface;
    IPersistStorage IPersistStorage_iface;
    LONG ref;
    ULONG loaded;
};

static struct filter *impl_from_IFilter(IFilter *i) { return CONTAINING_RECORD(i, struct filter, IFilter_iface); }
static struct filter *impl_from_IPersistStream(IPersistStream *i) { return CONTAINING_RECORD(i, struct filter, IPersistStream_iface); }
static struct filter *impl_from_IPersistStorage(IPersistStorage *i) { return CONTAINING_RECORD(i, struct filter, IPersistStorage_iface); }

static HRESULT WINAPI f_QueryInterface(IFilter *iface, REFIID riid, void **out)
{
    struct filter *f = impl_from_IFilter(iface);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IFilter)) *out = &f->IFilter_iface;
    else if (IsEqualIID(riid, &IID_IPersistStream) || IsEqualIID(riid, &IID_IPersist)) *out = &f->IPersistStream_iface;
    else if (IsEqualIID(riid, &IID_IPersistStorage)) *out = &f->IPersistStorage_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&f->ref);
    return S_OK;
}
static ULONG WINAPI f_AddRef(IFilter *iface) { return InterlockedIncrement(&impl_from_IFilter(iface)->ref); }
static ULONG WINAPI f_Release(IFilter *iface)
{
    struct filter *f = impl_from_IFilter(iface);
    ULONG ref = InterlockedDecrement(&f->ref);
    if (!ref) HeapFree(GetProcessHeap(), 0, f);
    return ref;
}
static SCODE WINAPI f_Init(IFilter *iface, ULONG flags, ULONG n, const FULLPROPSPEC *a, ULONG *out) { *out = 0; return S_OK; }
static SCODE WINAPI f_GetChunk(IFilter *iface, STAT_CHUNK *stat)
{
    memset(stat, 0, sizeof(*stat));
    stat->idChunk = impl_from_IFilter(iface)->loaded;
    return S_OK;
}
static SCODE WINAPI f_GetText(IFilter *iface, ULONG *len, WCHAR *buf) { return FILTER_E_NO_TEXT; }
static SCODE WINAPI f_GetValue(IFilter *iface, PROPVARIANT **val) { return FILTER_E_NO_VALUES; }
static SCODE WINAPI f_BindRegion(IFilter *iface, FILTERREGION r, REFIID riid, void **out) { return E_NOTIMPL; }
static IFilterVtbl filter_vtbl = { f_QueryInterface, f_AddRef, f_Release, f_Init, f_GetChunk, f_GetText, f_GetValue, f_BindRegion };

static HRESULT WINAPI ps_QueryInterface(IPersistStream *i, REFIID r, void **o) { return f_QueryInterface(&impl_from_IPersistStream(i)->IFilter_iface, r, o); }
static ULONG WINAPI ps_AddRef(IPersistStream *i) { return f_AddRef(&impl_from_IPersistStream(i)->IFilter_iface); }
static ULONG WINAPI ps_Release(IPersistStream *i) { return f_Release(&impl_from_IPersistStream(i)->IFilter_iface); }
static HRESULT WINAPI ps_GetClassID(IPersistStream *i, CLSID *c) { *c = CLSID_SgFilter; return S_OK; }
static HRESULT WINAPI ps_IsDirty(IPersistStream *i) { return S_FALSE; }
static HRESULT WINAPI ps_Load(IPersistStream *i, IStream *stm)
{
    char buf[256];
    ULONG got = 0, total = 0;
    while (SUCCEEDED(IStream_Read(stm, buf, sizeof(buf), &got)) && got) total += got;
    impl_from_IPersistStream(i)->loaded = total;
    return S_OK;
}
static HRESULT WINAPI ps_Save(IPersistStream *i, IStream *s, BOOL c) { return E_NOTIMPL; }
static HRESULT WINAPI ps_GetSizeMax(IPersistStream *i, ULARGE_INTEGER *s) { return E_NOTIMPL; }
static IPersistStreamVtbl ps_vtbl = { ps_QueryInterface, ps_AddRef, ps_Release, ps_GetClassID, ps_IsDirty, ps_Load, ps_Save, ps_GetSizeMax };

static HRESULT WINAPI pg_QueryInterface(IPersistStorage *i, REFIID r, void **o) { return f_QueryInterface(&impl_from_IPersistStorage(i)->IFilter_iface, r, o); }
static ULONG WINAPI pg_AddRef(IPersistStorage *i) { return f_AddRef(&impl_from_IPersistStorage(i)->IFilter_iface); }
static ULONG WINAPI pg_Release(IPersistStorage *i) { return f_Release(&impl_from_IPersistStorage(i)->IFilter_iface); }
static HRESULT WINAPI pg_GetClassID(IPersistStorage *i, CLSID *c) { *c = CLSID_SgFilter; return S_OK; }
static HRESULT WINAPI pg_IsDirty(IPersistStorage *i) { return S_FALSE; }
static HRESULT WINAPI pg_InitNew(IPersistStorage *i, IStorage *s) { return E_NOTIMPL; }
static HRESULT WINAPI pg_Load(IPersistStorage *i, IStorage *stg)
{
    IStream *stm;
    ULONG got = 0;
    char buf[64];
    impl_from_IPersistStorage(i)->loaded = 1000;
    if (SUCCEEDED(IStorage_OpenStream(stg, L"Contents", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm)))
    {
        if (SUCCEEDED(IStream_Read(stm, buf, sizeof(buf), &got))) impl_from_IPersistStorage(i)->loaded += got;
        IStream_Release(stm);
    }
    return S_OK;
}
static HRESULT WINAPI pg_Save(IPersistStorage *i, IStorage *s, BOOL b) { return E_NOTIMPL; }
static HRESULT WINAPI pg_SaveCompleted(IPersistStorage *i, IStorage *s) { return E_NOTIMPL; }
static HRESULT WINAPI pg_HandsOffStorage(IPersistStorage *i) { return E_NOTIMPL; }
static IPersistStorageVtbl pg_vtbl = { pg_QueryInterface, pg_AddRef, pg_Release, pg_GetClassID, pg_IsDirty, pg_InitNew, pg_Load, pg_Save, pg_SaveCompleted, pg_HandsOffStorage };

static HRESULT WINAPI cf_QueryInterface(IClassFactory *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *i) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *i) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *i, IUnknown *outer, REFIID riid, void **out)
{
    struct filter *f;
    HRESULT hr;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!(f = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*f)))) return E_OUTOFMEMORY;
    f->IFilter_iface.lpVtbl = &filter_vtbl;
    f->IPersistStream_iface.lpVtbl = &ps_vtbl;
    f->IPersistStorage_iface.lpVtbl = &pg_vtbl;
    f->ref = 1;
    hr = f_QueryInterface(&f->IFilter_iface, riid, out);
    f_Release(&f->IFilter_iface);
    return hr;
}
static HRESULT WINAPI cf_LockServer(IClassFactory *i, BOOL l) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_QueryInterface, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer };
static IClassFactory cf = { &cf_vtbl };

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (!IsEqualCLSID(clsid, &CLSID_SgFilter)) { *out = NULL; return CLASS_E_CLASSNOTAVAILABLE; }
    return cf_QueryInterface(&cf, riid, out);
}
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
