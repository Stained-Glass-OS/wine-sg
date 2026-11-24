/* A property sheet handler for test/propsheet-gate.sh (wine-sg 9922): an in-process COM server whose
 * IShellPropSheetExt adds two pages, "SG Page A" and "SG Page B", and remembers what IShellExtInit got. */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <initguid.h>

DEFINE_GUID(CLSID_SgTestSheet, 0x5e6a1c2b, 0x9d3f, 0x4c71, 0x8a, 0x10, 0x2f, 0x33, 0x51, 0x7c, 0x0e, 0x91);

__declspec(dllexport) int sg_init_count;
__declspec(dllexport) IDataObject *sg_init_dobj;
__declspec(dllexport) HKEY sg_init_key;
static HINSTANCE instance;

struct sheet { IShellExtInit init; IShellPropSheetExt ext; LONG ref; };

static INT_PTR CALLBACK page_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) { return msg == WM_INITDIALOG; }

static HPROPSHEETPAGE make_page(const WCHAR *title)
{
    static struct { DLGTEMPLATE t; WORD menu, cls, caption; } tmpl = { { WS_CHILD | DS_CONTROL, 0, 0, 0, 0, 200, 100 } };
    PROPSHEETPAGEW psp = { sizeof(psp) };
    psp.dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
    psp.hInstance = instance;
    psp.pResource = &tmpl.t;
    psp.pszTitle = title;
    psp.pfnDlgProc = page_proc;
    return CreatePropertySheetPageW(&psp);
}

static struct sheet *from_init(IShellExtInit *i) { return CONTAINING_RECORD(i, struct sheet, init); }
static struct sheet *from_ext(IShellPropSheetExt *e) { return CONTAINING_RECORD(e, struct sheet, ext); }

static HRESULT qi(struct sheet *s, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IShellExtInit)) *out = &s->init;
    else if (IsEqualIID(iid, &IID_IShellPropSheetExt)) *out = &s->ext;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&s->ref);
    return S_OK;
}
static ULONG release(struct sheet *s) { LONG r = InterlockedDecrement(&s->ref); if (!r) HeapFree(GetProcessHeap(), 0, s); return r; }

static HRESULT WINAPI init_qi(IShellExtInit *i, REFIID iid, void **out) { return qi(from_init(i), iid, out); }
static ULONG WINAPI init_addref(IShellExtInit *i) { return InterlockedIncrement(&from_init(i)->ref); }
static ULONG WINAPI init_release(IShellExtInit *i) { return release(from_init(i)); }
static HRESULT WINAPI init_initialize(IShellExtInit *i, PCIDLIST_ABSOLUTE pidl, IDataObject *dobj, HKEY key)
{
    sg_init_count++;
    sg_init_dobj = dobj;
    sg_init_key = key;
    return S_OK;
}
static IShellExtInitVtbl init_vtbl = { init_qi, init_addref, init_release, init_initialize };

static HRESULT WINAPI ext_qi(IShellPropSheetExt *e, REFIID iid, void **out) { return qi(from_ext(e), iid, out); }
static ULONG WINAPI ext_addref(IShellPropSheetExt *e) { return InterlockedIncrement(&from_ext(e)->ref); }
static ULONG WINAPI ext_release(IShellPropSheetExt *e) { return release(from_ext(e)); }
static HRESULT WINAPI ext_addpages(IShellPropSheetExt *e, LPFNSVADDPROPSHEETPAGE add, LPARAM lp)
{
    HPROPSHEETPAGE a = make_page(L"SG Page A"), b = make_page(L"SG Page B");
    if (a && !add(a, lp)) DestroyPropertySheetPage(a);
    if (b && !add(b, lp)) DestroyPropertySheetPage(b);
    return S_OK;
}
static HRESULT WINAPI ext_replace(IShellPropSheetExt *e, EXPPS id, LPFNSVADDPROPSHEETPAGE f, LPARAM lp) { return E_NOTIMPL; }
static IShellPropSheetExtVtbl ext_vtbl = { ext_qi, ext_addref, ext_release, ext_addpages, ext_replace };

static HRESULT WINAPI cf_qi(IClassFactory *cf, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IClassFactory)) { *out = cf; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref(IClassFactory *cf) { return 2; }
static ULONG WINAPI cf_release(IClassFactory *cf) { return 1; }
static HRESULT WINAPI cf_create(IClassFactory *cf, IUnknown *outer, REFIID iid, void **out)
{
    struct sheet *s;
    HRESULT hr;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!(s = HeapAlloc(GetProcessHeap(), 0, sizeof(*s)))) return E_OUTOFMEMORY;
    s->init.lpVtbl = &init_vtbl; s->ext.lpVtbl = &ext_vtbl; s->ref = 1;
    hr = qi(s, iid, out);
    release(s);
    return hr;
}
static HRESULT WINAPI cf_lock(IClassFactory *cf, BOOL lock) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static IClassFactory factory = { &cf_vtbl };

__declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    if (!IsEqualCLSID(clsid, &CLSID_SgTestSheet)) return CLASS_E_CLASSNOTAVAILABLE;
    return cf_qi(&factory, iid, out);
}
__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved)
{
    if (reason == DLL_PROCESS_ATTACH) instance = inst;
    return TRUE;
}
