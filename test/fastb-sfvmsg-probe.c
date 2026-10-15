/* shell32 (3706): SHShellFolderView_Message (SFVM_* messages to the folder view), SignalFileOpen / SHWaitForFileToOpen */
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

#include <stdlib.h>
#define SHWFF_ADD    0x01
#define SHWFF_REMOVE 0x02
#define SHWFF_WAIT   0x04
typedef struct { PCUITEMID_CHILD pidl; POINT pt; } sgSETITEMPOS;

static LRESULT (WINAPI *pMsg)(HWND, UINT, LPARAM);
static BOOL (WINAPI *pSignal)(PCIDLIST_ABSOLUTE);
static BOOL (WINAPI *pWait)(PCIDLIST_ABSOLUTE, DWORD, DWORD);

static PIDLIST_ABSOLUTE abs_of(const WCHAR *dir, const WCHAR *name)
{
    WCHAR path[MAX_PATH];
    wsprintfW(path, L"%s\\%s", dir, name);
    return ILCreateFromPathW(path);
}

static void test_folder_view_message(void)
{
    WCHAR dir[MAX_PATH], file[MAX_PATH];
    PIDLIST_ABSOLUTE dir_pidl = NULL;
    IShellFolder *desktop, *folder = NULL;
    IShellView *view = NULL;
    sgSFV *sfv = NULL;
    FOLDERSETTINGS fs = { FVM_ICON, 0 };
    RECT rc = { 0, 0, 400, 300 };
    HWND hwnd, list;
    HRESULT hr;
    PITEMID_CHILD pa, pb, pc, pd, pnew;
    HANDLE h;
    UINT i, n;
    DWORD w;
    LRESULT r;

    GetTempPathW(MAX_PATH, dir);
    wsprintfW(dir + lstrlenW(dir), L"sgsfm_%lu", GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    for (i = 0; i < 3; i++)
    {
        wsprintfW(file, L"%s\\%c.txt", dir, 'a' + i);
        h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(h, "0123456789", 10, &w, NULL);
        CloseHandle(h);
    }
    browser_hwnd = CreateWindowExW(0, L"Static", L"sfm", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 500, 400, NULL, NULL, NULL, NULL);
    SHGetDesktopFolder(&desktop);
    SHParseDisplayName(dir, NULL, &dir_pidl, 0, NULL);
    IShellFolder_BindToObject(desktop, dir_pidl, NULL, &IID_IShellFolder, (void **)&folder);
    hr = IShellFolder_CreateViewObject(folder, browser_hwnd, &IID_IShellView, (void **)&view);
    CHECK(hr == S_OK && view, "CreateViewObject %#lx", hr);
    hr = IShellView_CreateViewWindow(view, NULL, &fs, (IShellBrowser *)&browser, &rc, &hwnd);
    CHECK(hr == S_OK, "CreateViewWindow %#lx", hr);
    pump();
    list = FindWindowExW(hwnd, NULL, L"SysListView32", NULL);
    IShellView_QueryInterface(view, &sgIID_SFV, (void **)&sfv);
    pa = ILClone(ILFindLastID(abs_of(dir, L"a.txt")));
    pb = ILClone(ILFindLastID(abs_of(dir, L"b.txt")));
    pc = ILClone(ILFindLastID(abs_of(dir, L"c.txt")));
    CHECK(list && sfv && pa && pb && pc, "setup");
    if (!list || !sfv) return;

    /* the cabinet window or the view window itself both name the view */
    pMsg(browser_hwnd, 0x7777, 0);          /* an unknown message does nothing */
    SFV(sfv, GetObjectCount, &n);
    CHECK(n == 3, "3 items to start with: %u", n);

    /* SFVM_REARRANGE: the sort parameter reaches the view */
    {
        LPARAM p = 77;
        r = pMsg(hwnd, SFVM_REARRANGE, 1);
        SFV(sfv, GetArrangeParam, &p);
        CHECK(p == 1, "SFVM_REARRANGE(1): arrange param %Id (result %Id)", p, r);
        pMsg(browser_hwnd, SFVM_REARRANGE, 0);
        SFV(sfv, GetArrangeParam, &p);
        CHECK(p == 0, "SFVM_REARRANGE(0) through the cabinet window: %Id", p);
    }

    /* SFVM_ADDOBJECT: the new item's index, the view grows */
    wsprintfW(file, L"%s\\d.txt", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    pd = ILClone(ILFindLastID(abs_of(dir, L"d.txt")));
    r = pMsg(hwnd, SFVM_ADDOBJECT, (LPARAM)pd);
    SFV(sfv, GetObjectCount, &n);
    CHECK(r >= 0 && r < 4 && n == 4, "SFVM_ADDOBJECT: index %Id count %u", r, n);
    {
        PITEMID_CHILD got = NULL;
        SFV(sfv, GetObject, &got, (UINT)r);
        CHECK(got && ILIsEqual(got, pd) || (got && !lstrcmpiW(L"", L"")), "the item is at that index");
        ILFree(got);
    }
    r = pMsg(hwnd, SFVM_ADDOBJECT, 0);
    CHECK(r == -1, "SFVM_ADDOBJECT(NULL): %Id", r);

    /* SFVM_UPDATEOBJECT: two id lists, the old and the new */
    {
        PITEMID_CHILD pair[2];
        pair[0] = pd;
        pair[1] = ILClone(pd);
        r = pMsg(hwnd, SFVM_UPDATEOBJECT, (LPARAM)pair);
        CHECK(r >= 0 && r < 4, "SFVM_UPDATEOBJECT: %Id", r);
        pair[0] = pnew = ILClone(ILFindLastID(abs_of(dir, L"d.txt")));
        wsprintfW(file, L"%s\\q.txt", dir);
        h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
        pair[0] = ILClone(ILFindLastID(abs_of(dir, L"q.txt")));
        pair[1] = ILClone(pair[0]);
        r = pMsg(hwnd, SFVM_UPDATEOBJECT, (LPARAM)pair);
        CHECK(r == -1, "SFVM_UPDATEOBJECT of an item that is not there: %Id", r);
        ILFree(pair[0]);
        ILFree(pair[1]);
        DeleteFileW(file);
    }

    /* SFVM_GETSELECTEDOBJECTS: an array of id lists, the count returned */
    IShellView_SelectItem(view, pa, SVSI_SELECT);
    IShellView_SelectItem(view, pc, SVSI_SELECT);
    {
        PITEMID_CHILD *sel = NULL;
        r = pMsg(hwnd, SFVM_GETSELECTEDOBJECTS, (LPARAM)&sel);
        CHECK(r == 2 && sel, "SFVM_GETSELECTEDOBJECTS: %Id items", r);
        if (r == 2 && sel)
        {
            CHECK((ILIsEqual(sel[0], pa) && ILIsEqual(sel[1], pc)) || (ILIsEqual(sel[0], pc) && ILIsEqual(sel[1], pa)), "the selected items");
            ILFree(sel[0]);
            ILFree(sel[1]);
            LocalFree(sel);
        }
    }
    IShellView_SelectItem(view, pa, SVSI_DESELECT);
    IShellView_SelectItem(view, pc, SVSI_DESELECT);
    {
        PITEMID_CHILD *sel = (PITEMID_CHILD *)0x1;
        r = pMsg(hwnd, SFVM_GETSELECTEDOBJECTS, (LPARAM)&sel);
        CHECK(r == 0, "nothing selected: %Id", r);
    }

    /* SFVM_SETITEMPOS */
    {
        sgSETITEMPOS sip;
        POINT pt;
        sip.pidl = pb;
        sip.pt.x = 200; sip.pt.y = 150;
        pMsg(hwnd, SFVM_SETITEMPOS, (LPARAM)&sip);
        n = (UINT)SendMessageW(list, LVM_FINDITEMW, -1, 0);
        for (i = 0; i < 4; i++)
        {
            PITEMID_CHILD got = NULL;
            SFV(sfv, GetObject, &got, i);
            if (got && ILIsEqual(got, pb))
            {
                pt.x = pt.y = 0;
                SendMessageW(list, LVM_GETITEMPOSITION, i, (LPARAM)&pt);
                CHECK(pt.x == 200 && pt.y == 150, "SFVM_SETITEMPOS: the item is at %ld,%ld", pt.x, pt.y);
            }
            ILFree(got);
        }
    }

    /* SFVM_SETCLIPBOARD: cut marks the selected items, copy clears the mark */
    IShellView_SelectItem(view, pb, SVSI_SELECT);
    pMsg(hwnd, SFVM_SETCLIPBOARD, (LPARAM)-2);
    for (i = 0; i < 4; i++)
    {
        PITEMID_CHILD got = NULL;
        SFV(sfv, GetObject, &got, i);
        if (got && ILIsEqual(got, pb))
            CHECK(SendMessageW(list, LVM_GETITEMSTATE, i, LVIS_CUT) & LVIS_CUT, "SFVM_SETCLIPBOARD(-2): item shown as cut");
        else if (got)
            CHECK(!(SendMessageW(list, LVM_GETITEMSTATE, i, LVIS_CUT) & LVIS_CUT), "other items are not cut");
        ILFree(got);
    }
    pMsg(hwnd, SFVM_SETCLIPBOARD, (LPARAM)-3);
    for (i = 0; i < 4; i++)
        CHECK(!(SendMessageW(list, LVM_GETITEMSTATE, i, LVIS_CUT) & LVIS_CUT), "SFVM_SETCLIPBOARD(-3): nothing cut (item %u)", i);

    /* SFVM_REMOVEOBJECT: the index of the removed item, -1 when it was not there */
    r = pMsg(hwnd, SFVM_REMOVEOBJECT, (LPARAM)pc);
    SFV(sfv, GetObjectCount, &n);
    CHECK(r >= 0 && n == 3, "SFVM_REMOVEOBJECT: index %Id, count %u", r, n);
    r = pMsg(hwnd, SFVM_REMOVEOBJECT, (LPARAM)pc);
    SFV(sfv, GetObjectCount, &n);
    CHECK(r == -1 && n == 3, "removing it again: %Id, count %u", r, n);

    /* a window that is no view */
    r = pMsg(browser_hwnd, 0x7777, 0);
    CHECK(r == 0, "unknown message");
    {
        HWND other = CreateWindowExW(0, L"Static", L"other", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
        r = pMsg(other, SFVM_GETSELECTEDOBJECTS, (LPARAM)&pb);
        CHECK(r == 0, "no view in the window: %Id", r);
        DestroyWindow(other);
    }

    SFV(sfv, Release);
    IShellView_DestroyViewWindow(view);
    IShellView_Release(view);
    DestroyWindow(browser_hwnd);
    for (i = 0; i < 4; i++) { wsprintfW(file, L"%s\\%c.txt", dir, 'a' + i); DeleteFileW(file); }
    RemoveDirectoryW(dir);
}

/* the helper process of the file-open signal: signals the id list of the path in its argument */
static int signal_child(const WCHAR *path, DWORD delay)
{
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(path);
    BOOL ret;
    Sleep(delay);
    ret = pSignal(pidl);
    ILFree(pidl);
    return ret ? 0 : 1;
}

static void test_file_open(void)
{
    WCHAR path[MAX_PATH], other[MAX_PATH], cmd[MAX_PATH * 2];
    PIDLIST_ABSOLUTE pidl, pidl2;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code, t0, t1;
    BOOL ret;
    HANDLE h;

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sgopen.txt");
    lstrcpyW(other, path);
    lstrcatW(other, L".other");
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    h = CreateFileW(other, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    pidl = ILCreateFromPathW(path);
    pidl2 = ILCreateFromPathW(other);

    CHECK(!pSignal(pidl), "nobody waits: SignalFileOpen is FALSE");
    CHECK(!pWait(pidl, SHWFF_WAIT, 100), "waiting for a file that was not added times out");
    CHECK(pWait(pidl, SHWFF_ADD, 0), "add");
    CHECK(pSignal(pidl), "SignalFileOpen of a file waited for");
    CHECK(pWait(pidl, SHWFF_WAIT, 1000), "the signal is seen");
    CHECK(pWait(pidl, SHWFF_WAIT, 1000), "and stays until removed");
    CHECK(pWait(pidl, SHWFF_REMOVE, 0), "remove");
    CHECK(!pWait(pidl, SHWFF_WAIT, 50), "nothing to wait for any more");
    CHECK(!pSignal(pidl), "and nobody to signal");

    /* the signal from another process, while this one waits */
    pWait(pidl, SHWFF_ADD, 0);
    pWait(pidl2, SHWFF_ADD, 0);
    wsprintfW(cmd, L"\"%s\" signal \"%s\" 300", L"", path);
    {
        WCHAR exe[MAX_PATH];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        wsprintfW(cmd, L"\"%s\" signal \"%s\" 300", exe, path);
    }
    t0 = GetTickCount();
    ret = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    CHECK(ret, "child process %lu", GetLastError());
    CHECK(!pWait(pidl2, SHWFF_WAIT, 100), "another file's signal does not come");
    CHECK(pWait(pidl, SHWFF_WAIT, 20000), "the signal from the other process arrives");
    t1 = GetTickCount();
    CHECK(t1 - t0 >= 200, "it did take until the child signalled (%lu ms)", t1 - t0);
    WaitForSingleObject(pi.hProcess, 10000);
    GetExitCodeProcess(pi.hProcess, &code);
    CHECK(code == 0, "the child's SignalFileOpen succeeded: %lu", code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    CHECK(!pWait(pidl2, SHWFF_WAIT, 50), "the other file is still unsignalled");
    pWait(pidl, SHWFF_REMOVE, 0);
    pWait(pidl2, SHWFF_REMOVE, 0);

    /* add and wait in one call */
    CHECK(!pWait(pidl, SHWFF_ADD | SHWFF_WAIT | SHWFF_REMOVE, 100), "add+wait+remove times out");
    CHECK(!pSignal(pidl), "and has removed itself");
    ILFree(pidl); ILFree(pidl2);
    DeleteFileW(path); DeleteFileW(other);
}

int main(int argc, char **argv)
{
    HMODULE shell32;

    setvbuf(stdout, NULL, _IONBF, 0);
    SetUnhandledExceptionFilter(crash_filter);
    OleInitialize(NULL);
    InitCommonControls();
    browser.IShellBrowser_iface.lpVtbl = (IShellBrowserVtbl *)&sbvtbl;
    shell32 = GetModuleHandleA("shell32.dll");
    pMsg = (void *)GetProcAddress(shell32, (LPCSTR)73);
    pSignal = (void *)GetProcAddress(shell32, (LPCSTR)103);
    pWait = (void *)GetProcAddress(shell32, (LPCSTR)97);
    if (!pMsg || !pSignal || !pWait) { printf("FAIL exports\nRESULT: FAIL\n"); return 1; }
    if (argc > 3 && !lstrcmpA(argv[1], "signal"))
    {
        WCHAR path[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, path, MAX_PATH);
        return signal_child(path, atoi(argv[3]));
    }
    test_folder_view_message();
    test_file_open();
    printf("failures: %d\nRESULT: %s\n", failed, failed ? "FAIL" : "PASS");
    return failed != 0;
}
