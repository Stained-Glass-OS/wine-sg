/* filedrop-handler -- a drop handler for test/filedrop-gate.sh (patches/sg/7501), as an archiver's
 * (WinRAR's RarExt): registered as HKCR\<ProgID>\shellex\DropHandler, created with
 * IPersistFile, loaded with the file dropped on, and given the drop through IDropTarget. On Drop
 * it appends "drop LOADED <- FILE|FILE..." to C:\filedrop-handler.txt. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

static const CLSID CLSID_SgDropHandler = { 0x2f0e7a51, 0x9c3b, 0x4d16, { 0xb8, 0x4a, 0x61, 0x5e, 0x70, 0x00, 0x75, 0x01 } };

typedef struct { IDropTarget dt; IPersistFile pf; LONG ref; WCHAR path[MAX_PATH]; } h_t;
static h_t *from_dt(IDropTarget *i) { return CONTAINING_RECORD(i, h_t, dt); }
static h_t *from_pf(IPersistFile *i) { return CONTAINING_RECORD(i, h_t, pf); }

static HRESULT qi(h_t *h, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) *out = &h->dt;
    else if (IsEqualIID(riid, &IID_IPersist) || IsEqualIID(riid, &IID_IPersistFile)) *out = &h->pf;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&h->ref);
    return S_OK;
}
static ULONG rel(h_t *h) { ULONG r = InterlockedDecrement(&h->ref); if (!r) free(h); return r; }

static HRESULT WINAPI d_qi(IDropTarget *i, REFIID r, void **o) { return qi(from_dt(i), r, o); }
static ULONG WINAPI d_addref(IDropTarget *i) { return InterlockedIncrement(&from_dt(i)->ref); }
static ULONG WINAPI d_release(IDropTarget *i) { return rel(from_dt(i)); }
static HRESULT WINAPI d_enter(IDropTarget *i, IDataObject *d, DWORD k, POINTL p, DWORD *e) { *e &= DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI d_over(IDropTarget *i, DWORD k, POINTL p, DWORD *e) { *e &= DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI d_leave(IDropTarget *i) { return S_OK; }
static HRESULT WINAPI d_drop(IDropTarget *i, IDataObject *d, DWORD k, POINTL p, DWORD *e)
{
    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM med;
    WCHAR f[MAX_PATH];
    UINT n, j;
    FILE *out = _wfopen(L"C:\\filedrop-handler.txt", L"a");
    if (!out) return E_FAIL;
    fwprintf(out, L"drop %ls <-", from_dt(i)->path);
    if (SUCCEEDED(IDataObject_GetData(d, &fmt, &med)))
    {
        n = DragQueryFileW(med.hGlobal, ~0u, NULL, 0);
        for (j = 0; j < n; j++) { DragQueryFileW(med.hGlobal, j, f, MAX_PATH); fwprintf(out, L" %ls", f); }
        ReleaseStgMedium(&med);
    }
    fwprintf(out, L"\n");
    fclose(out);
    *e = DROPEFFECT_COPY;
    return S_OK;
}
static IDropTargetVtbl dt_vtbl = { d_qi, d_addref, d_release, d_enter, d_over, d_leave, d_drop };

static HRESULT WINAPI p_qi(IPersistFile *i, REFIID r, void **o) { return qi(from_pf(i), r, o); }
static ULONG WINAPI p_addref(IPersistFile *i) { return InterlockedIncrement(&from_pf(i)->ref); }
static ULONG WINAPI p_release(IPersistFile *i) { return rel(from_pf(i)); }
static HRESULT WINAPI p_classid(IPersistFile *i, CLSID *c) { *c = CLSID_SgDropHandler; return S_OK; }
static HRESULT WINAPI p_dirty(IPersistFile *i) { return S_FALSE; }
static HRESULT WINAPI p_load(IPersistFile *i, LPCOLESTR name, DWORD mode) { lstrcpynW(from_pf(i)->path, name, MAX_PATH); return S_OK; }
static HRESULT WINAPI p_save(IPersistFile *i, LPCOLESTR n, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI p_saved(IPersistFile *i, LPCOLESTR n) { return E_NOTIMPL; }
static HRESULT WINAPI p_cur(IPersistFile *i, LPOLESTR *n) { return E_NOTIMPL; }
static IPersistFileVtbl pf_vtbl = { p_qi, p_addref, p_release, p_classid, p_dirty, p_load, p_save, p_saved, p_cur };

static HRESULT WINAPI f_qi(IClassFactory *f, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = f; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_addref(IClassFactory *f) { return 2; }
static ULONG WINAPI f_release(IClassFactory *f) { return 1; }
static HRESULT WINAPI f_create(IClassFactory *f, IUnknown *outer, REFIID r, void **o)
{
    h_t *h;
    HRESULT hr;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!(h = calloc(1, sizeof(*h)))) return E_OUTOFMEMORY;
    h->dt.lpVtbl = &dt_vtbl; h->pf.lpVtbl = &pf_vtbl; h->ref = 1;
    hr = qi(h, r, o);
    rel(h);
    return hr;
}
static HRESULT WINAPI f_lock(IClassFactory *f, BOOL l) { return S_OK; }
static IClassFactoryVtbl factory_vtbl = { f_qi, f_addref, f_release, f_create, f_lock };
static IClassFactory factory = { &factory_vtbl };

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (!IsEqualCLSID(clsid, &CLSID_SgDropHandler)) return CLASS_E_CLASSNOTAVAILABLE;
    return IClassFactory_QueryInterface(&factory, riid, out);
}
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
