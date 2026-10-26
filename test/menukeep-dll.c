/* menukeep-dll -- a context menu handler for test/menukeep-gate.sh (patches/sg/7500), written the
 * way WinRAR's RarExt behaves: no IPersist, registered both for its ProgID and for *, and it keeps
 * the IDataObject that IShellExtInit::Initialize gave it WITHOUT a reference of its own, reading
 * the items from it again when its command runs. It adds "SG Keep Action"; invoked, it appends
 * "invoked N: PATH|PATH..." to C:\menukeep-invoked.txt (N = files in CF_HDROP at that time); in a
 * folder's background menu (no items) it appends "background FOLDER". */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

static const CLSID CLSID_SgKeepExt = { 0x7d5a0c31, 0x52e4, 0x4b8f, { 0x8e, 0x21, 0x5a, 0x4c, 0x70, 0x00, 0x75, 0x00 } };
static LONG locks;

typedef struct { IShellExtInit init; IContextMenu menu; LONG ref; IDataObject *data; WCHAR folder[MAX_PATH]; } ext_t;

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
    /* a folder's background menu gives the folder and no items (test/menuverbs-gate.sh) */
    if (!data) return folder && SHGetPathFromIDListW(folder, from_init(i)->folder) ? S_OK : E_FAIL;
    from_init(i)->data = data;   /* no AddRef, as RarExt */
    return S_OK;
}
static IShellExtInitVtbl init_vtbl = { i_qi, i_addref, i_release, i_initialize };

static HRESULT WINAPI m_qi(IContextMenu *m, REFIID r, void **o) { return qi(from_menu(m), r, o); }
static ULONG WINAPI m_addref(IContextMenu *m) { return InterlockedIncrement(&from_menu(m)->ref); }
static ULONG WINAPI m_release(IContextMenu *m) { return release(from_menu(m)); }
static HRESULT WINAPI m_query(IContextMenu *m, HMENU menu, UINT index, UINT first, UINT last, UINT flags)
{
    if (flags & CMF_DEFAULTONLY) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    InsertMenuW(menu, index, MF_BYPOSITION | MF_STRING, first, L"SG Keep Action");
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 1);
}
static HRESULT WINAPI m_invoke(IContextMenu *m, CMINVOKECOMMANDINFO *ici)
{
    ext_t *e = from_menu(m);
    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM med;
    WCHAR path[MAX_PATH];
    UINT n = 0, i;
    FILE *f;
    if (HIWORD(ici->lpVerb) || LOWORD(ici->lpVerb) != 0) return E_INVALIDARG;
    if (!(f = _wfopen(L"C:\\menukeep-invoked.txt", L"a"))) return E_FAIL;
    if (!e->data)
    {
        fwprintf(f, L"background %ls\n", e->folder);
        fclose(f);
        return S_OK;
    }
    if (SUCCEEDED(IDataObject_GetData(e->data, &fmt, &med)))
    {
        n = DragQueryFileW((HDROP)med.hGlobal, ~0u, NULL, 0);
        fwprintf(f, L"invoked %u:", n);
        for (i = 0; i < n; i++)
        {
            path[0] = 0;
            DragQueryFileW((HDROP)med.hGlobal, i, path, MAX_PATH);
            fwprintf(f, L" %ls", path);
        }
        ReleaseStgMedium(&med);
    }
    else fwprintf(f, L"invoked no-data");
    fwprintf(f, L"\n");
    fclose(f);
    return n ? S_OK : E_FAIL;
}
static HRESULT WINAPI m_string(IContextMenu *m, UINT_PTR id, UINT type, UINT *res, LPSTR name, UINT max)
{
    return E_NOTIMPL;
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
    if (!IsEqualCLSID(clsid, &CLSID_SgKeepExt)) return CLASS_E_CLASSNOTAVAILABLE;
    return IClassFactory_QueryInterface(&factory, riid, out);
}
HRESULT WINAPI DllCanUnloadNow(void) { return locks ? S_FALSE : S_OK; }
