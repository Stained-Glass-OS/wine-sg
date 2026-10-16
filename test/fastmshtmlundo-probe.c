#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <docobj.h>
#include <servprov.h>
#include <stdio.h>

static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
/* SID_SOleUndoManager = IID_IOleUndoManager */
static int fails, did_undo, did_redo;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { IOleUndoUnit iface; LONG ref; const WCHAR *desc; int is_redo; } Unit;
static Unit *impl(IOleUndoUnit *i) { return (Unit *)i; }
static HRESULT WINAPI U_QI(IOleUndoUnit *i, REFIID r, void **p)
{ if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IOleUndoUnit)) { *p = i; IOleUndoUnit_AddRef(i); return S_OK; } *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI U_AddRef(IOleUndoUnit *i) { return InterlockedIncrement(&impl(i)->ref); }
static ULONG WINAPI U_Release(IOleUndoUnit *i) { LONG r = InterlockedDecrement(&impl(i)->ref); if (!r) free(impl(i)); return r; }
static Unit *make_unit(const WCHAR *desc, int is_redo);
static HRESULT WINAPI U_Do(IOleUndoUnit *i, IOleUndoManager *m)
{
    Unit *u = impl(i);
    if (u->is_redo) did_redo++; else did_undo++;
    /* the inverse operation goes back to the manager */
    { Unit *inv = make_unit(u->is_redo ? L"undo-again" : L"redo-me", !u->is_redo);
      HRESULT hr = IOleUndoManager_Add(m, &inv->iface); IOleUndoUnit_Release(&inv->iface); return hr; }
}
static HRESULT WINAPI U_GetDescription(IOleUndoUnit *i, BSTR *b) { *b = SysAllocString(impl(i)->desc); return S_OK; }
static HRESULT WINAPI U_GetUnitType(IOleUndoUnit *i, CLSID *c, LONG *id) { memset(c, 0, sizeof(*c)); *id = 0; return S_OK; }
static HRESULT WINAPI U_OnNextAdd(IOleUndoUnit *i) { return S_OK; }
static const IOleUndoUnitVtbl uvt = { U_QI, U_AddRef, U_Release, U_Do, U_GetDescription, U_GetUnitType, U_OnNextAdd };
static Unit *make_unit(const WCHAR *desc, int is_redo)
{ Unit *u = calloc(1, sizeof(*u)); u->iface.lpVtbl = &uvt; u->ref = 1; u->desc = desc; u->is_redo = is_redo; return u; }

static int count(IEnumOleUndoUnits *e, BSTR *first)
{
    int n = 0; IOleUndoUnit *u; ULONG got;
    if (first) *first = NULL;
    while (IEnumOleUndoUnits_Next(e, 1, &u, &got) == S_OK)
    {
        if (!n && first) IOleUndoUnit_GetDescription(u, first);
        IOleUndoUnit_Release(u); n++;
    }
    return n;
}

int main(void)
{
    IHTMLDocument2 *doc; IServiceProvider *sp; IOleUndoManager *um = NULL; HRESULT hr;
    Unit *a, *b, *c; IEnumOleUndoUnits *e; BSTR s = NULL; DWORD st;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&doc);
    CHECK(hr == S_OK, "doc %08lx", hr);
    IHTMLDocument2_QueryInterface(doc, &IID_IServiceProvider, (void **)&sp);
    hr = IServiceProvider_QueryService(sp, &IID_IOleUndoManager, &IID_IOleUndoManager, (void **)&um);
    CHECK(hr == S_OK && um, "undo manager %08lx", hr);
    if (!um) { printf("RESULT: FAIL\n"); return 1; }

    CHECK(IOleUndoManager_GetLastUndoDescription(um, &s) == E_FAIL, "empty last undo description");
    a = make_unit(L"typing", 0); b = make_unit(L"bold", 0); c = make_unit(L"paste", 0);
    CHECK(IOleUndoManager_Add(um, &a->iface) == S_OK && IOleUndoManager_Add(um, &b->iface) == S_OK &&
          IOleUndoManager_Add(um, &c->iface) == S_OK, "Add");
    hr = IOleUndoManager_EnumUndoable(um, &e);
    CHECK(hr == S_OK && count(e, &s) == 3 && s && !wcscmp(s, L"paste"), "EnumUndoable: most recent first (%ls)", s ? s : L"(null)");
    SysFreeString(s); IEnumOleUndoUnits_Release(e);
    s = NULL; hr = IOleUndoManager_GetLastUndoDescription(um, &s);
    CHECK(hr == S_OK && s && !wcscmp(s, L"paste"), "last undo description"); SysFreeString(s);
    CHECK(IOleUndoManager_GetOpenParentState(um, &st) == S_FALSE, "no open parent");

    /* undo down to and including 'bold' */
    hr = IOleUndoManager_UndoTo(um, &b->iface);
    CHECK(hr == S_OK && did_undo == 2, "UndoTo bold: %08lx, %d undone", hr, did_undo);
    hr = IOleUndoManager_EnumUndoable(um, &e); CHECK(count(e, &s) == 1 && !wcscmp(s, L"typing"), "one unit left"); SysFreeString(s); IEnumOleUndoUnits_Release(e);
    hr = IOleUndoManager_EnumRedoable(um, &e); CHECK(count(e, &s) == 2 && !wcscmp(s, L"redo-me"), "two redo units"); SysFreeString(s); IEnumOleUndoUnits_Release(e);

    hr = IOleUndoManager_RedoTo(um, NULL);
    CHECK(hr == S_OK && did_redo == 1, "RedoTo(NULL) redoes the latest: %d", did_redo);
    hr = IOleUndoManager_EnumRedoable(um, &e); CHECK(count(e, NULL) == 1, "one redo unit left"); IEnumOleUndoUnits_Release(e);

    /* a new action ends the redo history */
    { Unit *d = make_unit(L"new", 0); IOleUndoManager_Add(um, &d->iface); IOleUndoUnit_Release(&d->iface); }
    hr = IOleUndoManager_EnumRedoable(um, &e); CHECK(count(e, NULL) == 0, "redo history cleared"); IEnumOleUndoUnits_Release(e);

    CHECK(IOleUndoManager_DiscardFrom(um, NULL) == S_OK, "DiscardFrom(NULL)");
    hr = IOleUndoManager_EnumUndoable(um, &e); CHECK(count(e, NULL) == 0, "all discarded"); IEnumOleUndoUnits_Release(e);
    CHECK(IOleUndoManager_UndoTo(um, NULL) == E_FAIL, "nothing to undo");

    IOleUndoManager_Enable(um, FALSE);
    { Unit *d = make_unit(L"ignored", 0); IOleUndoManager_Add(um, &d->iface); IOleUndoUnit_Release(&d->iface); }
    hr = IOleUndoManager_EnumUndoable(um, &e); CHECK(count(e, NULL) == 0, "disabled manager drops units"); IEnumOleUndoUnits_Release(e);

    IOleUndoUnit_Release(&a->iface); IOleUndoUnit_Release(&b->iface); IOleUndoUnit_Release(&c->iface);
    IOleUndoManager_Release(um); IServiceProvider_Release(sp);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
