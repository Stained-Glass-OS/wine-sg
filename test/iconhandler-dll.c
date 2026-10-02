/* iconhandler-dll: a file type's icon handler for iconhandler-gate.sh --
 * IPersistFile + IExtractIconW; each file's icon is C:\handler-<name>.ico,0
 * (its own name, so the shell asked about that file). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <stdio.h>

DEFINE_GUID(CLSID_Test, 0x5a1c0e11, 0x2b6d, 0x4c11, 0x9a, 0x01, 0x1c, 0x0d, 0xe5, 0x7a, 0x11, 0x0e);

struct ext;
struct ext_vtbl {
    HRESULT (WINAPI *QueryInterface)(struct ext *, REFIID, void **);
    ULONG (WINAPI *AddRef)(struct ext *);
    ULONG (WINAPI *Release)(struct ext *);
    HRESULT (WINAPI *GetIconLocation)(struct ext *, UINT, LPWSTR, UINT, int *, UINT *);
    HRESULT (WINAPI *Extract)(struct ext *, LPCWSTR, UINT, HICON *, HICON *, UINT);
};
struct ext { const struct ext_vtbl *lpVtbl; };
struct obj { IPersistFile pf; struct ext ei; LONG ref; WCHAR path[MAX_PATH]; };

static HRESULT query(struct obj *o, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPersist) || IsEqualIID(riid, &IID_IPersistFile)) *out = &o->pf;
    else if (IsEqualIID(riid, &IID_IExtractIconW)) *out = &o->ei;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&o->ref);
    return S_OK;
}
static ULONG rel(struct obj *o) { ULONG r = InterlockedDecrement(&o->ref); if (!r) HeapFree(GetProcessHeap(), 0, o); return r; }
#define PF(i) CONTAINING_RECORD(i, struct obj, pf)
#define EI(i) CONTAINING_RECORD(i, struct obj, ei)
static HRESULT WINAPI pf_qi(IPersistFile *i, REFIID r, void **o) { return query(PF(i), r, o); }
static ULONG WINAPI pf_ar(IPersistFile *i) { return InterlockedIncrement(&PF(i)->ref); }
static ULONG WINAPI pf_rl(IPersistFile *i) { return rel(PF(i)); }
static HRESULT WINAPI pf_cid(IPersistFile *i, CLSID *c) { *c = CLSID_Test; return S_OK; }
static HRESULT WINAPI pf_dirty(IPersistFile *i) { return S_FALSE; }
static HRESULT WINAPI pf_load(IPersistFile *i, LPCOLESTR n, DWORD m) { lstrcpynW(PF(i)->path, n, MAX_PATH); return S_OK; }
static HRESULT WINAPI pf_save(IPersistFile *i, LPCOLESTR n, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI pf_done(IPersistFile *i, LPCOLESTR n) { return E_NOTIMPL; }
static HRESULT WINAPI pf_cur(IPersistFile *i, LPOLESTR *n) { return E_NOTIMPL; }
static IPersistFileVtbl pf_vtbl = { pf_qi, pf_ar, pf_rl, pf_cid, pf_dirty, pf_load, pf_save, pf_done, pf_cur };
static HRESULT WINAPI ei_qi(struct ext *i, REFIID r, void **o) { return query(EI(i), r, o); }
static ULONG WINAPI ei_ar(struct ext *i) { return InterlockedIncrement(&EI(i)->ref); }
static ULONG WINAPI ei_rl(struct ext *i) { return rel(EI(i)); }
static HRESULT WINAPI ei_loc(struct ext *i, UINT f, LPWSTR file, UINT len, int *idx, UINT *flags)
{
    WCHAR name[MAX_PATH];
    lstrcpynW(name, PathFindFileNameW(EI(i)->path), MAX_PATH);
    PathRemoveExtensionW(name);
    _snwprintf(file, len, L"C:\\handler-%ls.ico", name);
    *idx = 0;
    *flags = GIL_PERINSTANCE;
    return S_OK;
}
static HRESULT WINAPI ei_ext(struct ext *i, LPCWSTR f, UINT n, HICON *l, HICON *s, UINT z) { return S_FALSE; }
static const struct ext_vtbl ei_vtbl = { ei_qi, ei_ar, ei_rl, ei_loc, ei_ext };

static HRESULT WINAPI cf_qi(IClassFactory *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_ar(IClassFactory *i) { return 2; }
static ULONG WINAPI cf_rl(IClassFactory *i) { return 1; }
static HRESULT WINAPI cf_ci(IClassFactory *i, IUnknown *outer, REFIID r, void **out)
{
    struct obj *o = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*o));
    HRESULT hr;
    if (!o) return E_OUTOFMEMORY;
    o->pf.lpVtbl = &pf_vtbl; o->ei.lpVtbl = &ei_vtbl; o->ref = 1;
    hr = query(o, r, out);
    rel(o);
    return hr;
}
static HRESULT WINAPI cf_lock(IClassFactory *i, BOOL l) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_qi, cf_ar, cf_rl, cf_ci, cf_lock };
static IClassFactory factory = { &cf_vtbl };
HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID r, void **o)
{
    if (!IsEqualCLSID(c, &CLSID_Test)) { *o = NULL; return CLASS_E_CLASSNOTAVAILABLE; }
    return IClassFactory_QueryInterface(&factory, r, o);
}
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
