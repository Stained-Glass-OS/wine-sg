/* shell32 folder view (patch 3694): IShellFolderView, IViewObject, IFolderView2 view properties,
 * tile properties, empty text, group subset count, IsMoveInSameFolder */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <stdio.h>


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
/* IShellFolderView and its callback, declared here from their documentation (the MinGW headers lack them) */
typedef struct sgSFV sgSFV;
typedef struct sgCB sgCB;
struct sgSFVVtbl
{
    HRESULT (WINAPI *QueryInterface)(sgSFV *, REFIID, void **);
    ULONG (WINAPI *AddRef)(sgSFV *);
    ULONG (WINAPI *Release)(sgSFV *);
    HRESULT (WINAPI *Rearrange)(sgSFV *, LPARAM);
    HRESULT (WINAPI *GetArrangeParam)(sgSFV *, LPARAM *);
    HRESULT (WINAPI *ArrangeGrid)(sgSFV *);
    HRESULT (WINAPI *AutoArrange)(sgSFV *);
    HRESULT (WINAPI *GetAutoArrange)(sgSFV *);
    HRESULT (WINAPI *AddObject)(sgSFV *, PITEMID_CHILD, UINT *);
    HRESULT (WINAPI *GetObject)(sgSFV *, PITEMID_CHILD *, UINT);
    HRESULT (WINAPI *RemoveObject)(sgSFV *, PITEMID_CHILD, UINT *);
    HRESULT (WINAPI *GetObjectCount)(sgSFV *, UINT *);
    HRESULT (WINAPI *SetObjectCount)(sgSFV *, UINT, UINT);
    HRESULT (WINAPI *UpdateObject)(sgSFV *, PITEMID_CHILD, PITEMID_CHILD, UINT *);
    HRESULT (WINAPI *RefreshObject)(sgSFV *, PITEMID_CHILD, UINT *);
    HRESULT (WINAPI *SetRedraw)(sgSFV *, BOOL);
    HRESULT (WINAPI *GetSelectedCount)(sgSFV *, UINT *);
    HRESULT (WINAPI *GetSelectedObjects)(sgSFV *, PCITEMID_CHILD **, UINT *);
    HRESULT (WINAPI *IsDropOnSource)(sgSFV *, IDropTarget *);
    HRESULT (WINAPI *GetDragPoint)(sgSFV *, POINT *);
    HRESULT (WINAPI *GetDropPoint)(sgSFV *, POINT *);
    HRESULT (WINAPI *MoveIcons)(sgSFV *, IDataObject *);
    HRESULT (WINAPI *SetItemPos)(sgSFV *, PCUITEMID_CHILD, POINT *);
    HRESULT (WINAPI *IsBkDropTarget)(sgSFV *, IDropTarget *);
    HRESULT (WINAPI *SetClipboard)(sgSFV *, BOOL);
    HRESULT (WINAPI *SetPoints)(sgSFV *, IDataObject *);
    HRESULT (WINAPI *GetItemSpacing)(sgSFV *, ITEMSPACING *);
    HRESULT (WINAPI *SetCallback)(sgSFV *, sgCB *, sgCB **);
    HRESULT (WINAPI *Select)(sgSFV *, UINT);
    HRESULT (WINAPI *QuerySupport)(sgSFV *, UINT *);
    HRESULT (WINAPI *SetAutomationObject)(sgSFV *, IDispatch *);
};
struct sgSFV { const struct sgSFVVtbl *lpVtbl; };
static const GUID sgIID_SFV = { 0x37a378c0, 0xf82d, 0x11ce, { 0xae, 0x65, 0x08, 0x00, 0x2b, 0x2e, 0x12, 0x62 } };
#define SFV(p, m, ...) ((p)->lpVtbl->m((p), ##__VA_ARGS__))
struct sgCBVtbl
{
    HRESULT (WINAPI *QueryInterface)(sgCB *, REFIID, void **);
    ULONG (WINAPI *AddRef)(sgCB *);
    ULONG (WINAPI *Release)(sgCB *);
    HRESULT (WINAPI *MessageSFVCB)(sgCB *, UINT, WPARAM, LPARAM);
};
struct sgCB { const struct sgCBVtbl *lpVtbl; };
static const GUID IID_sgCB = { 0x2047e320, 0xf2a9, 0x11ce, { 0xae, 0x65, 0x08, 0x00, 0x2b, 0x2e, 0x12, 0x62 } };
#define CBV(p) ((p)->lpVtbl->AddRef(p))
#define CBV2(p) ((p)->lpVtbl->Release(p))

static int failed;
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
    HMODULE mod = NULL;
    WCHAR name[MAX_PATH] = L"?";
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, ep->ExceptionRecord->ExceptionAddress, &mod);
    if (mod) GetModuleFileNameW(mod, name, MAX_PATH);
    printf("CRASH %lx at %p (%ls + %#llx)\n", ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress, name,
           (unsigned long long)((char *)ep->ExceptionRecord->ExceptionAddress - (char *)mod));
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HWND browser_hwnd;
typedef struct { IShellBrowser IShellBrowser_iface; } browser_t;
static browser_t browser;
static HRESULT WINAPI sb_QI(IShellBrowser *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IOleWindow) || IsEqualIID(r, &IID_IShellBrowser)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sb_AddRef(IShellBrowser *i) { return 2; }
static ULONG WINAPI sb_Release(IShellBrowser *i) { return 1; }
static HRESULT WINAPI sb_GetWindow(IShellBrowser *i, HWND *h) { *h = browser_hwnd; return S_OK; }
static HRESULT WINAPI sb_Ctx(IShellBrowser *i, BOOL b) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Insert(IShellBrowser *i, HMENU m, LPOLEMENUGROUPWIDTHS w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Set(IShellBrowser *i, HMENU m, HOLEMENU h, HWND w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Remove(IShellBrowser *i, HMENU m) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Status(IShellBrowser *i, LPCOLESTR t) { return E_NOTIMPL; }
static HRESULT WINAPI sb_EnableModeless(IShellBrowser *i, BOOL b) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Translate(IShellBrowser *i, MSG *m, WORD w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Browse(IShellBrowser *i, PCUIDLIST_RELATIVE p, UINT f) { return E_NOTIMPL; }
static HRESULT WINAPI sb_GetViewState(IShellBrowser *i, DWORD m, IStream **s) { return E_FAIL; }
static HRESULT WINAPI sb_Ctrl(IShellBrowser *i, UINT id, HWND *h) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Send(IShellBrowser *i, UINT id, UINT m, WPARAM w, LPARAM l, LRESULT *r) { return E_NOTIMPL; }
static HRESULT WINAPI sb_QueryActive(IShellBrowser *i, IShellView **v) { return E_NOTIMPL; }
static HRESULT WINAPI sb_OnActive(IShellBrowser *i, IShellView *v) { return E_NOTIMPL; }
static HRESULT WINAPI sb_SetToolbar(IShellBrowser *i, LPTBBUTTONSB b, UINT n, UINT f) { return E_NOTIMPL; }
static const IShellBrowserVtbl sbvtbl = { sb_QI, sb_AddRef, sb_Release, sb_GetWindow, sb_Ctx, sb_Insert, sb_Set, sb_Remove,
    sb_Status, sb_EnableModeless, sb_Translate, sb_Browse, sb_GetViewState, sb_Ctrl, sb_Send, sb_QueryActive, sb_OnActive, sb_SetToolbar };

/* an advise sink that counts view changes, and its references */
static LONG sink_ref = 1, sink_changes;
static HRESULT WINAPI as_QI(IAdviseSink *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IAdviseSink)) { *o = i; IAdviseSink_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI as_AddRef(IAdviseSink *i) { return InterlockedIncrement(&sink_ref); }
static ULONG WINAPI as_Release(IAdviseSink *i) { return InterlockedDecrement(&sink_ref); }
static void WINAPI as_OnDataChange(IAdviseSink *i, FORMATETC *f, STGMEDIUM *m) {}
static void WINAPI as_OnViewChange(IAdviseSink *i, DWORD a, LONG l) { InterlockedIncrement(&sink_changes); }
static void WINAPI as_OnRename(IAdviseSink *i, IMoniker *m) {}
static void WINAPI as_OnSave(IAdviseSink *i) {}
static void WINAPI as_OnClose(IAdviseSink *i) {}
static const IAdviseSinkVtbl asvtbl = { as_QI, as_AddRef, as_Release, as_OnDataChange, as_OnViewChange, as_OnRename, as_OnSave, as_OnClose };
static IAdviseSink sink = { (IAdviseSinkVtbl *)&asvtbl };

/* a callback object for SetCallback */
static LONG cb_ref[2] = { 1, 1 };
static HRESULT WINAPI cb_QI(sgCB *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_sgCB)) { *o = i; CBV(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static sgCB cbs[2];
static LONG *cb_counter(sgCB *i) { return &cb_ref[i == &cbs[1]]; }
static ULONG WINAPI cb_AddRef(sgCB *i) { return InterlockedIncrement(cb_counter(i)); }
static ULONG WINAPI cb_Release(sgCB *i) { return InterlockedDecrement(cb_counter(i)); }
static HRESULT WINAPI cb_Message(sgCB *i, UINT m, WPARAM w, LPARAM l) { return E_NOTIMPL; }
static const struct sgCBVtbl cbvtbl = { cb_QI, cb_AddRef, cb_Release, cb_Message };

/* a data object that records SetData */
static FORMATETC set_fmt;
static STGMEDIUM set_med;
static HRESULT WINAPI do_QI(IDataObject *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDataObject)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI do_AddRef(IDataObject *i) { return 2; }
static ULONG WINAPI do_Release(IDataObject *i) { return 1; }
static HRESULT WINAPI do_GetData(IDataObject *i, FORMATETC *f, STGMEDIUM *m) { return DV_E_FORMATETC; }
static HRESULT WINAPI do_GetDataHere(IDataObject *i, FORMATETC *f, STGMEDIUM *m) { return E_NOTIMPL; }
static HRESULT WINAPI do_Query(IDataObject *i, FORMATETC *f) { return DV_E_FORMATETC; }
static HRESULT WINAPI do_Canon(IDataObject *i, FORMATETC *f, FORMATETC *o) { return E_NOTIMPL; }
static HRESULT WINAPI do_Set(IDataObject *i, FORMATETC *f, STGMEDIUM *m, BOOL r) { set_fmt = *f; set_med = *m; return S_OK; }
static HRESULT WINAPI do_Enum(IDataObject *i, DWORD d, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI do_DAdvise(IDataObject *i, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI do_DUnadvise(IDataObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI do_EnumDAdvise(IDataObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static const IDataObjectVtbl dovtbl = { do_QI, do_AddRef, do_Release, do_GetData, do_GetDataHere, do_Query, do_Canon, do_Set,
    do_Enum, do_DAdvise, do_DUnadvise, do_EnumDAdvise };
static IDataObject dobj = { (IDataObjectVtbl *)&dovtbl };

static BOOL CALLBACK stop_cb(ULONG_PTR c) { return FALSE; }

static void pump(void)
{
    MSG msg;
    int n;
    for (n = 0; n < 12; n++) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } Sleep(10); }
}

static PITEMID_CHILD child_of(const WCHAR *dir, const WCHAR *name)
{
    WCHAR path[MAX_PATH];
    PIDLIST_ABSOLUTE abs;
    PITEMID_CHILD c;

    wsprintfW(path, L"%s\\%s", dir, name);
    abs = ILCreateFromPathW(path);
    if (!abs) return NULL;
    c = ILClone(ILFindLastID(abs));
    ILFree(abs);
    return c;
}

#define DRAW_W 300
#define DRAW_H 200
/* draws the view into a white bitmap; returns the count of non-white pixels, *sum their sum */
static HRESULT draw_view(IViewObject *vo, int *nonwhite, unsigned *sum)
{
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), DRAW_W, -DRAW_H, 1, 32, BI_RGB } };
    UINT *bits;
    HBITMAP bmp;
    HDC dc;
    HGDIOBJ old;
    RECTL rc = { 0, 0, DRAW_W, DRAW_H };
    HRESULT hr;
    int i;

    dc = CreateCompatibleDC(NULL);
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    old = SelectObject(dc, bmp);
    for (i = 0; i < DRAW_W * DRAW_H; i++) bits[i] = 0xffffff;
    hr = IViewObject_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &rc, NULL, NULL, 0);
    *nonwhite = 0;
    *sum = 0;
    for (i = 0; i < DRAW_W * DRAW_H; i++)
        if ((bits[i] & 0xffffff) != 0xffffff) { (*nonwhite)++; *sum = *sum * 31 + bits[i] + i; }
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    return hr;
}

int main(void)
{
    WCHAR dir[MAX_PATH], file[MAX_PATH];
    PIDLIST_ABSOLUTE dir_pidl = NULL;
    IShellFolder *desktop, *folder = NULL;
    IShellFolder2 *folder2 = NULL;
    IShellView *view = NULL;
    sgSFV *sfv = NULL;
    IViewObject *vo = NULL;
    IFolderView2 *fv2 = NULL;
    FOLDERSETTINGS fs = { FVM_ICON, 0 };
    RECT rc = { 0, 0, 400, 300 };
    HWND hwnd, list;
    HRESULT hr;
    UINT n, idx, idx2, i;
    int i1, i2;
    unsigned s1, s2;
    PITEMID_CHILD pa, pb, pd, pe, pbogus;
    HANDLE h;

    setvbuf(stdout, NULL, _IONBF, 0);
    SetUnhandledExceptionFilter(crash_filter);
    OleInitialize(NULL);
    InitCommonControls();
    browser.IShellBrowser_iface.lpVtbl = (IShellBrowserVtbl *)&sbvtbl;
    cbs[0].lpVtbl = cbs[1].lpVtbl = (struct sgCBVtbl *)&cbvtbl;
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(dir + lstrlenW(dir), L"sgsfv_%lu", GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    for (i = 0; i < 6; i++) { wsprintfW(file, L"%s\\%c.txt", dir, 'a' + i); DeleteFileW(file); }
    for (i = 0; i < 5; i++)
    {
        wsprintfW(file, L"%s\\%c.txt", dir, 'a' + i);
        if (i >= 3) continue;   /* d and e are made later */
        h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(h, "0123456789", 10, (DWORD *)&n, NULL);
        CloseHandle(h);
    }
    browser_hwnd = CreateWindowExW(0, L"Static", L"sfv", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 500, 400, NULL, NULL, NULL, NULL);

    SHGetDesktopFolder(&desktop);
    SHParseDisplayName(dir, NULL, &dir_pidl, 0, NULL);
    IShellFolder_BindToObject(desktop, dir_pidl, NULL, &IID_IShellFolder, (void **)&folder);
    IShellFolder_QueryInterface(folder, &IID_IShellFolder2, (void **)&folder2);
    hr = IShellFolder_CreateViewObject(folder, browser_hwnd, &IID_IShellView, (void **)&view);
    CHECK(hr == S_OK && view, "CreateViewObject %#lx", hr);
    if (!view) return 1;
    hr = IShellView_CreateViewWindow(view, NULL, &fs, (IShellBrowser *)&browser, &rc, &hwnd);
    CHECK(hr == S_OK, "CreateViewWindow %#lx", hr);
    if (hr != S_OK) return 1;
    pump();
    list = FindWindowExW(hwnd, NULL, L"SysListView32", NULL);
    CHECK(list != NULL, "list window");
    hr = IShellView_QueryInterface(view, &sgIID_SFV, (void **)&sfv);
    CHECK(hr == S_OK, "QI IShellFolderView %#lx", hr);
    hr = IShellView_QueryInterface(view, &IID_IViewObject, (void **)&vo);
    CHECK(hr == S_OK, "QI IViewObject %#lx", hr);
    hr = IShellView_QueryInterface(view, &IID_IFolderView2, (void **)&fv2);
    CHECK(hr == S_OK, "QI IFolderView2 %#lx", hr);
    if (!sfv || !vo || !fv2 || !list) return 1;

    pa = child_of(dir, L"a.txt");
    pb = child_of(dir, L"b.txt");

    /* ---- IShellFolderView: arrangement ---- */
    printf("# IShellFolderView: arrangement\n");
    n = 0;
    hr = SFV(sfv, GetObjectCount, &n);
    CHECK(hr == S_OK && n == 3, "object count %u", n);
    {
        LPARAM p = 77;
        hr = SFV(sfv, Rearrange, 0);
        CHECK(hr == S_OK, "Rearrange(0) %#lx", hr);
        hr = SFV(sfv, GetArrangeParam, &p);
        CHECK(hr == S_OK && p == 0, "GetArrangeParam %#lx %Id", hr, p);
        hr = SFV(sfv, Rearrange, 1);
        CHECK(hr == S_OK, "Rearrange(1) %#lx", hr);
        p = 77;
        SFV(sfv, GetArrangeParam, &p);
        CHECK(p == 1, "arrange param after Rearrange(1): %Id", p);
        hr = SFV(sfv, Rearrange, 0x10000000);
        CHECK(hr == S_OK, "Rearrange(CANONICALONLY) %#lx", hr);
        p = 77;
        SFV(sfv, GetArrangeParam, &p);
        CHECK(p == 0x10000000, "param with flag: %#Ix", p);
        hr = SFV(sfv, Rearrange, 0x80000001);
        CHECK(hr == E_INVALIDARG, "ALLFIELDS with a column: %#lx", hr);
        hr = SFV(sfv, Rearrange, 999);
        CHECK(hr == E_INVALIDARG, "column out of range: %#lx", hr);
        SFV(sfv, Rearrange, 0);
    }
    hr = SFV(sfv, ArrangeGrid);
    CHECK(hr == S_OK, "ArrangeGrid %#lx", hr);
    hr = SFV(sfv, SetObjectCount, 5, 0);
    CHECK(hr == E_NOTIMPL, "SetObjectCount %#lx", hr);
    hr = SFV(sfv, MoveIcons, &dobj);
    CHECK(hr == E_NOTIMPL, "MoveIcons %#lx", hr);

    /* ---- add, update, refresh, remove ---- */
    printf("# add, update, refresh, remove\n");
    wsprintfW(file, L"%s\\d.txt", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    wsprintfW(file, L"%s\\e.txt", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    pd = child_of(dir, L"d.txt");
    pe = child_of(dir, L"e.txt");
    wsprintfW(file, L"%s\\z.txt", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    pbogus = child_of(dir, L"z.txt");
    DeleteFileW(file);
    idx = 99;
    hr = SFV(sfv, AddObject, pd, &idx);
    CHECK(hr == S_OK && idx == 3, "AddObject %#lx index %u", hr, idx);
    {
        PITEMID_CHILD got = NULL;
        hr = SFV(sfv, GetObject, &got, idx);
        CHECK(hr == S_OK && got && ILIsEqual(got, pd), "GetObject after AddObject %#lx", hr);
        ILFree(got);
    }
    SFV(sfv, GetObjectCount, &n);
    CHECK(n == 4, "count after add %u", n);
    hr = SFV(sfv, AddObject, NULL, &idx);
    CHECK(hr == E_INVALIDARG, "AddObject(NULL) %#lx", hr);
    idx = 99;
    hr = SFV(sfv, UpdateObject, pd, pe, &idx);
    CHECK(hr == S_OK && idx == 3, "UpdateObject %#lx index %u", hr, idx);
    {
        PITEMID_CHILD got = NULL;
        SFV(sfv, GetObject, &got, idx);
        CHECK(got && ILIsEqual(got, pe), "item replaced");
        ILFree(got);
    }
    hr = SFV(sfv, UpdateObject, pbogus, pe, &idx);
    CHECK(hr == E_FAIL, "UpdateObject of an absent item %#lx", hr);
    idx = 99;
    hr = SFV(sfv, RefreshObject, pe, &idx);
    CHECK(hr == S_OK && idx == 3, "RefreshObject %#lx index %u", hr, idx);
    hr = SFV(sfv, RefreshObject, pbogus, &idx);
    CHECK(hr == E_FAIL, "RefreshObject of an absent item %#lx", hr);

    /* ---- IViewObject: advise ---- */
    printf("# IViewObject: advise\n");
    {
        DWORD aspects = 9, advf = 9;
        IAdviseSink *got = (IAdviseSink *)0x1;

        hr = IViewObject_SetAdvise(vo, DVASPECT_ICON, 0, &sink);
        CHECK(hr == DV_E_DVASPECT, "SetAdvise(ICON) %#lx", hr);
        CHECK(sink_ref == 1, "sink not held after a refused advise: %ld", sink_ref);
        sink_changes = 0;
        hr = IViewObject_SetAdvise(vo, DVASPECT_CONTENT, 0, &sink);
        CHECK(hr == S_OK, "SetAdvise %#lx", hr);
        CHECK(sink_ref == 2, "sink held by the view: %ld", sink_ref);
        hr = IViewObject_GetAdvise(vo, &aspects, &advf, &got);
        CHECK(hr == S_OK && aspects == DVASPECT_CONTENT && advf == 0 && got == &sink, "GetAdvise %#lx", hr);
        if (got == &sink) IAdviseSink_Release(got);
        SFV(sfv, RefreshObject, pe, &idx);
        SFV(sfv, Rearrange, 0);
        CHECK(sink_changes > 0, "view changes reached the sink: %ld", sink_changes);
        hr = IViewObject_SetAdvise(vo, 0, 0, NULL);
        CHECK(hr == S_OK && sink_ref == 1, "advise removed %#lx ref %ld", hr, sink_ref);
        got = (IAdviseSink *)0x1;
        IViewObject_GetAdvise(vo, NULL, NULL, &got);
        CHECK(got == NULL, "no sink after removal");
        /* ADVF_PRIMEFIRST is a notice at once; ADVF_ONLYONCE ends after the first */
        sink_changes = 0;
        IViewObject_SetAdvise(vo, DVASPECT_CONTENT, ADVF_PRIMEFIRST | ADVF_ONLYONCE, &sink);
        CHECK(sink_changes == 1, "PRIMEFIRST notice: %ld", sink_changes);
        SFV(sfv, Rearrange, 0);
        SFV(sfv, Rearrange, 0);
        CHECK(sink_changes == 1 && sink_ref == 1, "ONLYONCE: %ld notices, ref %ld", sink_changes, sink_ref);
    }

    /* ---- IViewObject: colours, draw, freeze ---- */
    printf("# IViewObject: colours, draw, freeze\n");
    {
        LOGPALETTE *pal = (LOGPALETTE *)0x1;
        DWORD key = 0, key2 = 0;
        RECTL empty = { 5, 5, 5, 5 }, ok_rc = { 0, 0, 100, 100 };
        HDC dc = CreateCompatibleDC(NULL);

        hr = IViewObject_GetColorSet(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal);
        CHECK(hr == S_FALSE && pal == NULL, "GetColorSet %#lx", hr);
        hr = IViewObject_GetColorSet(vo, DVASPECT_ICON, -1, NULL, NULL, NULL, &pal);
        CHECK(hr == DV_E_DVASPECT, "GetColorSet(ICON) %#lx", hr);
        hr = IViewObject_GetColorSet(vo, DVASPECT_CONTENT, 0, NULL, NULL, NULL, &pal);
        CHECK(hr == DV_E_LINDEX, "GetColorSet(lindex 0) %#lx", hr);

        hr = IViewObject_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, NULL, NULL, NULL, 0);
        CHECK(hr == E_INVALIDARG, "Draw without bounds %#lx", hr);
        hr = IViewObject_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &empty, NULL, NULL, 0);
        CHECK(hr == OLE_E_INVALIDRECT, "Draw in an empty rect %#lx", hr);
        hr = IViewObject_Draw(vo, DVASPECT_ICON, -1, NULL, NULL, NULL, dc, &ok_rc, NULL, NULL, 0);
        CHECK(hr == DV_E_DVASPECT, "Draw(ICON) %#lx", hr);
        hr = IViewObject_Draw(vo, DVASPECT_CONTENT, 3, NULL, NULL, NULL, dc, &ok_rc, NULL, NULL, 0);
        CHECK(hr == DV_E_LINDEX, "Draw(lindex 3) %#lx", hr);
        hr = IViewObject_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &ok_rc, NULL, stop_cb, 0);
        CHECK(hr == E_ABORT, "Draw stopped by the callback %#lx", hr);
        DeleteDC(dc);

        hr = draw_view(vo, &i1, &s1);
        CHECK(hr == S_OK && i1 > 100, "Draw %#lx drew %d pixels", hr, i1);

        hr = IViewObject_Unfreeze(vo, 5);
        CHECK(hr == OLE_E_NOCONNECTION, "Unfreeze when not frozen %#lx", hr);
        hr = IViewObject_Freeze(vo, DVASPECT_ICON, -1, NULL, &key);
        CHECK(hr == DV_E_DVASPECT, "Freeze(ICON) %#lx", hr);
        hr = IViewObject_Freeze(vo, DVASPECT_CONTENT, -1, NULL, &key);
        CHECK(hr == S_OK && key != 0, "Freeze %#lx key %lu", hr, key);
        hr = IViewObject_Freeze(vo, DVASPECT_CONTENT, -1, NULL, &key2);
        CHECK(hr == VIEW_S_ALREADY_FROZEN && key2 == key, "Freeze again %#lx key %lu", hr, key2);
        sink_changes = 0;
        IViewObject_SetAdvise(vo, DVASPECT_CONTENT, 0, &sink);
        SFV(sfv, RemoveObject, pa, &idx);          /* the view changes while frozen */
        pump();
        CHECK(sink_changes == 0, "no notice while frozen: %ld", sink_changes);
        draw_view(vo, &i2, &s2);
        CHECK(i2 == i1 && s2 == s1, "frozen picture unchanged: %d/%u vs %d/%u", i2, s2, i1, s1);
        hr = IViewObject_Unfreeze(vo, key + 1);
        CHECK(hr == OLE_E_NOCONNECTION, "Unfreeze with a wrong key %#lx", hr);
        hr = IViewObject_Unfreeze(vo, key);
        CHECK(hr == S_OK, "Unfreeze %#lx", hr);
        CHECK(sink_changes > 0, "the held notice arrives at Unfreeze: %ld", sink_changes);
        draw_view(vo, &i2, &s2);
        CHECK(s2 != s1, "picture changes after Unfreeze: %d/%u", i2, s2);
        hr = IViewObject_Unfreeze(vo, key);
        CHECK(hr == OLE_E_NOCONNECTION, "Unfreeze twice %#lx", hr);
        IViewObject_SetAdvise(vo, 0, 0, NULL);
        CHECK(sink_ref == 1, "sink ref after advise end: %ld", sink_ref);
    }
    SFV(sfv, AddObject, pa, &idx);    /* a.txt is back */

    /* ---- selection, positions, spacing ---- */
    printf("# selection, positions, spacing\n");
    {
        UINT sel;
        POINT pt = { 150, 90 }, got;

        hr = SFV(sfv, Select, SFVS_SELECT_ALLITEMS);
        CHECK(hr == S_OK, "Select all %#lx", hr);
        SFV(sfv, GetObjectCount, &n);
        sel = 99;
        SFV(sfv, GetSelectedCount, &sel);
        CHECK(sel == n && n == 4, "all selected: %u of %u", sel, n);
        SFV(sfv, Select, SFVS_SELECT_INVERT);
        sel = 99;
        SFV(sfv, GetSelectedCount, &sel);
        CHECK(sel == 0, "inverted all: %u", sel);
        SFV(sfv, Select, SFVS_SELECT_INVERT);
        sel = 99;
        SFV(sfv, GetSelectedCount, &sel);
        CHECK(sel == n, "inverted none: %u", sel);
        SFV(sfv, Select, SFVS_SELECT_NONE);
        sel = 99;
        SFV(sfv, GetSelectedCount, &sel);
        CHECK(sel == 0, "none: %u", sel);
        hr = SFV(sfv, Select, 9);
        CHECK(hr == E_INVALIDARG, "Select(9) %#lx", hr);

        hr = SFV(sfv, SetItemPos, pb, &pt);
        CHECK(hr == S_OK, "SetItemPos %#lx", hr);
        hr = IFolderView2_GetItemPosition(fv2, pb, &got);
        CHECK(hr == S_OK && got.x == 150 && got.y == 90, "position %#lx (%ld,%ld)", hr, got.x, got.y);
        hr = SFV(sfv, SetItemPos, pbogus, &pt);
        CHECK(hr == E_FAIL, "SetItemPos of an absent item %#lx", hr);

        {
            ITEMSPACING sp = { 0 };
            hr = SFV(sfv, GetItemSpacing, &sp);
            CHECK(hr == S_OK && sp.cxSmall > 0 && sp.cyLarge > 0, "spacing in icon mode %#lx %d %d", hr, sp.cxSmall, sp.cyLarge);
        }

        /* the points of the selection go to the data object: the group's corner, then offsets */
        {
            POINT p1 = { 40, 30 }, p2 = { 200, 120 };
            POINT *pts;
            UINT sel2;
            SFV(sfv, SetItemPos, pa, &p1);
            SFV(sfv, SetItemPos, pb, &p2);
            { LVITEMW sl = { 0 }; sl.stateMask = LVIS_SELECTED; SendMessageW(list, LVM_SETITEMSTATE, -1, (LPARAM)&sl); }
            IFolderView2_SelectAndPositionItems(fv2, 1, (PCUITEMID_CHILD_ARRAY)&pa, NULL, SVSI_SELECT);
            IFolderView2_SelectAndPositionItems(fv2, 1, (PCUITEMID_CHILD_ARRAY)&pb, NULL, SVSI_SELECT);
            sel2 = 0;
            SFV(sfv, GetSelectedCount, &sel2);
            CHECK(sel2 == 2, "two selected: %u", sel2);
            memset(&set_fmt, 0, sizeof(set_fmt));
            memset(&set_med, 0, sizeof(set_med));
            hr = SFV(sfv, SetPoints, &dobj);
            CHECK(hr == S_OK, "SetPoints %#lx", hr);
            CHECK(set_fmt.cfFormat == RegisterClipboardFormatW(L"Shell Object Offsets") && set_med.tymed == TYMED_HGLOBAL,
                  "SetPoints format %u", set_fmt.cfFormat);
            if (set_med.hGlobal && GlobalSize(set_med.hGlobal) >= 3 * sizeof(POINT))
            {
                pts = GlobalLock(set_med.hGlobal);
                CHECK(pts[0].x == 40 && pts[0].y == 30, "corner (%ld,%ld)", pts[0].x, pts[0].y);
                CHECK((pts[1].x == 0 && pts[1].y == 0 && pts[2].x == 160 && pts[2].y == 90) ||
                      (pts[2].x == 0 && pts[2].y == 0 && pts[1].x == 160 && pts[1].y == 90),
                      "offsets (%ld,%ld) (%ld,%ld)", pts[1].x, pts[1].y, pts[2].x, pts[2].y);
                GlobalUnlock(set_med.hGlobal);
            }
            else CHECK(0, "no point array recorded");
        }
    }

    /* ---- drag and drop points, backgrounds ---- */
    printf("# drag and drop points, backgrounds\n");
    {
        POINT pt = { 1, 1 };
        IDropTarget *other = (IDropTarget *)0x10, *own = NULL;

        CHECK(SFV(sfv, GetDragPoint, &pt) == S_FALSE, "no drag point");
        CHECK(SFV(sfv, GetDropPoint, &pt) == S_FALSE, "no drop point");
        IShellView_QueryInterface(view, &IID_IDropTarget, (void **)&own);
        CHECK(SFV(sfv, IsBkDropTarget, NULL) == S_OK, "background by NULL");
        CHECK(SFV(sfv, IsBkDropTarget, own) == S_OK, "background by the view's target");
        CHECK(SFV(sfv, IsBkDropTarget, other) == S_FALSE, "a folder's target is not the background");
        CHECK(SFV(sfv, IsDropOnSource, other) == S_FALSE, "drop on another folder");
        CHECK(SFV(sfv, IsDropOnSource, NULL) == S_FALSE, "nothing started a drag or cut");
        if (own) IDropTarget_Release(own);
    }

    /* ---- callback and automation object ---- */
    printf("# callback and automation object\n");
    {
        sgCB *old = (sgCB *)0x1;

        hr = SFV(sfv, SetCallback, &cbs[0], &old);
        CHECK(hr == S_OK && old == NULL && cb_ref[0] == 2, "SetCallback #1 %#lx old %p ref %ld", hr, old, cb_ref[0]);
        hr = SFV(sfv, SetCallback, &cbs[1], &old);
        CHECK(hr == S_OK && old == &cbs[0] && cb_ref[1] == 2 && cb_ref[0] == 2, "SetCallback #2 %#lx old %p", hr, old);
        if (old) CBV2(old);
        CHECK(cb_ref[0] == 1, "old callback released: %ld", cb_ref[0]);
        hr = SFV(sfv, SetCallback, NULL, NULL);
        CHECK(hr == S_OK && cb_ref[1] == 1, "callback dropped %#lx ref %ld", hr, cb_ref[1]);
        hr = SFV(sfv, SetAutomationObject, NULL);
        CHECK(hr == S_OK, "SetAutomationObject(NULL) %#lx", hr);
    }

    /* ---- cut: the clipboard, IsMoveInSameFolder ---- */
    printf("# cut: the clipboard, IsMoveInSameFolder\n");
    {
        hr = IFolderView2_IsMoveInSameFolder(fv2);
        CHECK(hr == S_FALSE, "nothing cut yet %#lx", hr);
        hr = SFV(sfv, SetClipboard, FALSE);
        CHECK(hr == E_INVALIDARG, "SetClipboard(FALSE) %#lx", hr);
        IFolderView2_SelectAndPositionItems(fv2, 1, (PCUITEMID_CHILD_ARRAY)&pa, NULL, SVSI_SELECT | SVSI_DESELECTOTHERS);
        hr = SFV(sfv, SetClipboard, TRUE);
        CHECK(hr == S_OK, "SetClipboard(TRUE) %#lx", hr);
        pump();
        hr = IFolderView2_IsMoveInSameFolder(fv2);
        CHECK(hr == S_OK, "cut from this folder %#lx", hr);
        CHECK(SFV(sfv, IsDropOnSource, NULL) == S_OK, "the cut items' source is this view");
        OleSetClipboard(NULL);
        hr = IFolderView2_IsMoveInSameFolder(fv2);
        CHECK(hr == S_FALSE, "clipboard cleared %#lx", hr);
    }

    /* ---- IFolderView2: cached properties, shown in their column (details view) ---- */
    printf("# IFolderView2: cached properties, shown in their column (details view)\n");
    {
        PROPERTYKEY key;
        SHCOLUMNID scid;
        PROPVARIANT pv, got;
        int item;
        WCHAR text[64];

        hr = IFolderView2_SetCurrentViewMode(fv2, FVM_DETAILS);
        CHECK(hr == S_OK, "details mode %#lx", hr);
        pump();
        hr = IShellFolder2_MapColumnToSCID(folder2, 1, &scid);
        CHECK(hr == S_OK, "MapColumnToSCID(1) %#lx", hr);
        key.fmtid = scid.fmtid;
        key.pid = scid.pid;
        PropVariantInit(&pv);
        pv.vt = VT_LPWSTR;
        pv.pwszVal = (WCHAR *)L"sgviewprop";
        hr = IFolderView2_GetViewProperty(fv2, pb, &key, &got);
        CHECK(hr == E_FAIL, "property not cached yet %#lx", hr);
        hr = IFolderView2_SetViewProperty(fv2, pb, &key, &pv);
        CHECK(hr == S_OK, "SetViewProperty %#lx", hr);
        PropVariantInit(&got);
        hr = IFolderView2_GetViewProperty(fv2, pb, &key, &got);
        CHECK(hr == S_OK && got.vt == VT_LPWSTR && !lstrcmpW(got.pwszVal, L"sgviewprop"), "GetViewProperty %#lx", hr);
        PropVariantClear(&got);
        hr = IFolderView2_GetViewProperty(fv2, pa, &key, &got);
        CHECK(hr == E_FAIL, "another item has no value %#lx", hr);
        hr = IFolderView2_SetViewProperty(fv2, NULL, &key, &pv);
        CHECK(hr == E_INVALIDARG, "SetViewProperty(NULL pidl) %#lx", hr);
        pv.pwszVal = (WCHAR *)L"second";
        IFolderView2_SetViewProperty(fv2, pb, &key, &pv);      /* replaces */
        pump();
        item = -1;
        IFolderView2_GetItemPosition(fv2, pb, &(POINT){0});    /* the item exists */
        IFolderView2_ItemCount(fv2, SVGIO_ALLVIEW, &i1);
        for (i = 0; i < (UINT)i1; i++)
        {
            LVITEMW lv = { LVIF_PARAM, i };
            SendMessageW(list, LVM_GETITEMW, 0, (LPARAM)&lv);
            if (ILIsEqual((PITEMID_CHILD)lv.lParam, pb)) item = i;
        }
        text[0] = 0;
        if (item >= 0) { LVITEMW tl = { 0 }; tl.iSubItem = 1; tl.pszText = text; tl.cchTextMax = ARRAY_SIZE(text); SendMessageW(list, LVM_GETITEMTEXTW, item, (LPARAM)&tl); }
        CHECK(!lstrcmpW(text, L"second"), "the cached value is the column's text: '%ls'", text);
    }

    /* ---- tile properties: Tiles show one list, Content the extended one ---- */
    printf("# tile properties: Tiles show one list, Content the extended one\n");
    {
        LVTILEINFO ti;
        UINT cols[8];
        int item = -1;

        hr = IFolderView2_SetCurrentViewMode(fv2, FVM_TILE);
        CHECK(hr == S_OK, "tile mode %#lx", hr);
        pump();
        for (i = 0; i < 4; i++)
        {
            LVITEMW lv = { LVIF_PARAM, i };
            if (SendMessageW(list, LVM_GETITEMW, 0, (LPARAM)&lv) && ILIsEqual((PITEMID_CHILD)lv.lParam, pb)) item = i;
        }
        hr = IFolderView2_SetTileViewProperties(fv2, pb, L"prop:System.Size;System.ItemTypeText");
        CHECK(hr == S_OK, "SetTileViewProperties %#lx", hr);
        memset(&ti, 0, sizeof(ti));
        ti.cbSize = sizeof(ti); ti.iItem = item; ti.cColumns = ARRAY_SIZE(cols); ti.puColumns = cols;
        ListView_GetTileInfo(list, &ti);
        CHECK(ti.cColumns == 2, "tile shows two lines: %u", ti.cColumns);
        hr = IFolderView2_SetTileViewProperties(fv2, pb, L"prop:System.Size");
        memset(&ti, 0, sizeof(ti));
        ti.cbSize = sizeof(ti); ti.iItem = item; ti.cColumns = ARRAY_SIZE(cols); ti.puColumns = cols;
        ListView_GetTileInfo(list, &ti);
        CHECK(ti.cColumns == 1, "after the list changed: %u", ti.cColumns);
        hr = IFolderView2_SetTileViewProperties(fv2, NULL, L"prop:System.Size");
        CHECK(hr == E_INVALIDARG, "SetTileViewProperties(NULL) %#lx", hr);
        hr = IFolderView2_SetExtendedTileViewProperties(fv2, pb, L"prop:System.Size;System.ItemTypeText;System.ItemNameDisplay");
        CHECK(hr == S_OK, "SetExtendedTileViewProperties %#lx", hr);
        memset(&ti, 0, sizeof(ti));
        ti.cbSize = sizeof(ti); ti.iItem = item; ti.cColumns = ARRAY_SIZE(cols); ti.puColumns = cols;
        ListView_GetTileInfo(list, &ti);
        CHECK(ti.cColumns == 1, "the extended list does not change Tiles: %u", ti.cColumns);
        IFolderView2_SetCurrentViewMode(fv2, FVM_CONTENT);
        pump();
        memset(&ti, 0, sizeof(ti));
        ti.cbSize = sizeof(ti); ti.iItem = item; ti.cColumns = ARRAY_SIZE(cols); ti.puColumns = cols;
        ListView_GetTileInfo(list, &ti);
        CHECK(ti.cColumns == 3, "Content shows the extended list: %u", ti.cColumns);
    }

    /* ---- group subset count ---- */
    printf("# group subset count\n");
    {
        UINT rows = 77;
        hr = IFolderView2_GetGroupSubsetCount(fv2, &rows);
        CHECK(hr == S_OK && rows == 0, "subsetting off at first %#lx %u", hr, rows);
        hr = IFolderView2_SetGroupSubsetCount(fv2, 5);
        CHECK(hr == S_OK, "SetGroupSubsetCount %#lx", hr);
        IFolderView2_GetGroupSubsetCount(fv2, &rows);
        CHECK(rows == 5, "subset rows %u", rows);
        IFolderView2_SetGroupSubsetCount(fv2, 0);
        IFolderView2_GetGroupSubsetCount(fv2, &rows);
        CHECK(rows == 0, "subsetting turned off %u", rows);
    }

    /* ---- the text of an empty view ---- */
    printf("# the text of an empty view\n");
    {
        IFolderView2_SetCurrentViewMode(fv2, FVM_ICON);
        pump();
        hr = IFolderView2_SetText(fv2, FVST_EMPTYTEXT, L"Nothing to see here");
        CHECK(hr == S_OK, "SetText %#lx", hr);
        CHECK(IFolderView2_SetText(fv2, (FVTEXTTYPE)5, L"x") == E_INVALIDARG, "SetText wrong type");
        CHECK(IFolderView2_SetText(fv2, FVST_EMPTYTEXT, NULL) == E_INVALIDARG, "SetText NULL");
        SFV(sfv, RemoveObject, NULL, &idx);
        pump();
        SFV(sfv, GetObjectCount, &n);
        CHECK(n == 0, "view emptied: %u", n);
        UpdateWindow(list);
        RedrawWindow(list, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        draw_view(vo, &i1, &s1);
        CHECK(i1 > 50, "the empty text is drawn: %d pixels", i1);
    }

    IFolderView2_Release(fv2);
    IViewObject_Release(vo);
    SFV(sfv, Release);
    IShellView_DestroyViewWindow(view);
    IShellView_Release(view);
    DestroyWindow(browser_hwnd);
    for (i = 0; i < 5; i++) { wsprintfW(file, L"%s\\%c.txt", dir, 'a' + i); DeleteFileW(file); }
    RemoveDirectoryW(dir);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
