/* menuext-dll -- a context menu handler for test/menuext-gate.sh (patches/sg/0779):
 * IShellExtInit + IContextMenu, as TortoiseGit's or 7-Zip's. It adds "SG Test
 * Action" (verb "sgtest", help "SG test help"); invoked, it writes the first
 * selected item's path to C:\menuext-invoked.txt. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

static const CLSID CLSID_SgTestExt = { 0x5c1e7a10, 0x3b2d, 0x4f6e, { 0x9a, 0x11, 0x2b, 0x77, 0x0d, 0xe4, 0x51, 0x02 } };
static LONG locks;

typedef struct { IShellExtInit init; IContextMenu menu; LONG ref; WCHAR path[MAX_PATH]; UINT first; } ext_t;

static ext_t *from_init(IShellExtInit *i) { return CONTAINING_RECORD(i, ext_t, init); }
static ext_t *from_menu(IContextMenu *m) { return CONTAINING_RECORD(m, ext_t, menu); }

static HRESULT qi(ext_t *e, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IShellExtInit)) *out = &e->init;
    else if (IsEqualIID(riid, &IID_IContextMenu)) *out = &e->menu;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&e->ref);
    return S_OK;
}
static ULONG release(ext_t *e) { ULONG r = InterlockedDecrement(&e->ref); if (!r) free(e); return r; }

static HRESULT WINAPI i_qi(IShellExtInit *i, REFIID r, void **o) { return qi(from_init(i), r, o); }
static ULONG WINAPI i_addref(IShellExtInit *i) { return InterlockedIncrement(&from_init(i)->ref); }
static ULONG WINAPI i_release(IShellExtInit *i) { return release(from_init(i)); }
static HRESULT WINAPI i_initialize(IShellExtInit *i, LPCITEMIDLIST folder, IDataObject *data, HKEY key)
{
    ext_t *e = from_init(i);
    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM med;
    if (!data || FAILED(IDataObject_GetData(data, &fmt, &med))) return E_FAIL;
    DragQueryFileW((HDROP)med.hGlobal, 0, e->path, MAX_PATH);
    ReleaseStgMedium(&med);
    return S_OK;
}
static IShellExtInitVtbl init_vtbl = { i_qi, i_addref, i_release, i_initialize };

static HRESULT WINAPI m_qi(IContextMenu *m, REFIID r, void **o) { return qi(from_menu(m), r, o); }
static ULONG WINAPI m_addref(IContextMenu *m) { return InterlockedIncrement(&from_menu(m)->ref); }
static ULONG WINAPI m_release(IContextMenu *m) { return release(from_menu(m)); }
static HRESULT WINAPI m_query(IContextMenu *m, HMENU menu, UINT index, UINT first, UINT last, UINT flags)
{
    if (flags & CMF_DEFAULTONLY) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    InsertMenuW(menu, index, MF_BYPOSITION | MF_STRING, first, L"SG Test Action");
    from_menu(m)->first = first;
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 1);
}
static HRESULT WINAPI m_invoke(IContextMenu *m, CMINVOKECOMMANDINFO *ici)
{
    ext_t *e = from_menu(m);
    FILE *f;
    if (HIWORD(ici->lpVerb) ? strcmp(ici->lpVerb, "sgtest") : LOWORD(ici->lpVerb) != 0) return E_INVALIDARG;
    if (!(f = _wfopen(L"C:\\menuext-invoked.txt", L"a"))) return E_FAIL;
    fwprintf(f, L"%ls\n", e->path);
    fclose(f);
    return S_OK;
}
static HRESULT WINAPI m_string(IContextMenu *m, UINT_PTR id, UINT type, UINT *res, LPSTR name, UINT max)
{
    if (id != 0) return E_INVALIDARG;
    if (type == GCS_VERBW) lstrcpynW((WCHAR *)name, L"sgtest", max);
    else if (type == GCS_VERBA) lstrcpynA(name, "sgtest", max);
    else if (type == GCS_HELPTEXTW) lstrcpynW((WCHAR *)name, L"SG test help", max);
    else return E_NOTIMPL;
    return S_OK;
}
static IContextMenuVtbl menu_vtbl = { m_qi, m_addref, m_release, m_query, m_invoke, m_string };

static HRESULT WINAPI f_qi(IClassFactory *f, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = f; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_addref(IClassFactory *f) { return 2; }
static ULONG WINAPI f_release(IClassFactory *f) { return 1; }
static HRESULT WINAPI f_create(IClassFactory *f, IUnknown *outer, REFIID r, void **o)
{
    ext_t *e;
    HRESULT hr;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!(e = calloc(1, sizeof(*e)))) return E_OUTOFMEMORY;
    e->init.lpVtbl = &init_vtbl; e->menu.lpVtbl = &menu_vtbl; e->ref = 1;
    hr = qi(e, r, o);
    release(e);
    return hr;
}
static HRESULT WINAPI f_lock(IClassFactory *f, BOOL l) { if (l) InterlockedIncrement(&locks); else InterlockedDecrement(&locks); return S_OK; }
static IClassFactoryVtbl factory_vtbl = { f_qi, f_addref, f_release, f_create, f_lock };
static IClassFactory factory = { &factory_vtbl };

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (!IsEqualCLSID(clsid, &CLSID_SgTestExt)) return CLASS_E_CLASSNOTAVAILABLE;
    return IClassFactory_QueryInterface(&factory, riid, out);
}
HRESULT WINAPI DllCanUnloadNow(void) { return locks ? S_FALSE : S_OK; }
