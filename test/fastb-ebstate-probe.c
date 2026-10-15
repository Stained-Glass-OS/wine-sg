/* shell32/shlwapi (3705): ExplorerBrowser view state (SetPropertyBag, GetViewStateStream), SetEmptyText,
 * TranslateAcceleratorSB, the connection point enumerators, OnStateChange; SHGetViewStatePropertyBag */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <ocidl.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void pump(void)
{
    MSG msg;
    int i;
    for (i = 0; i < 5; i++)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(20);
    }
}

static HRESULT bag_read_i4(IPropertyBag *bag, const WCHAR *name, LONG *val)
{
    VARIANT v;
    HRESULT hr;
    V_VT(&v) = VT_I4;
    hr = IPropertyBag_Read(bag, name, &v, NULL);
    if (SUCCEEDED(hr)) *val = V_I4(&v);
    return hr;
}

static void bag_write_i4(IPropertyBag *bag, const WCHAR *name, LONG val)
{
    VARIANT v;
    V_VT(&v) = VT_I4;
    V_I4(&v) = val;
    IPropertyBag_Write(bag, name, &v);
}

static PIDLIST_ABSOLUTE path_pidl(const WCHAR *path)
{
    PIDLIST_ABSOLUTE p = NULL;
    SHParseDisplayName(path, NULL, &p, 0, NULL);
    return p;
}

static void test_bags(const WCHAR *dir_a, const WCHAR *dir_sub)
{
    PIDLIST_ABSOLUTE pa = path_pidl(dir_a), ps = path_pidl(dir_sub);
    IPropertyBag *bag = NULL, *bag2 = NULL;
    IUnknown *unk;
    LONG val;
    VARIANT v;
    HRESULT hr;
    SAFEARRAY *arr;
    void *bytes;

    CHECK(pa && ps, "pidls");
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag);
    CHECK(hr == S_OK && bag, "get bag %#lx", hr);
    if (!bag) return;
    hr = bag_read_i4(bag, L"Nothing", &val);
    CHECK(FAILED(hr), "unknown value %#lx", hr);
    bag_write_i4(bag, L"Num", 77);
    hr = bag_read_i4(bag, L"Num", &val);
    CHECK(hr == S_OK && val == 77, "roundtrip %#lx %ld", hr, val);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"text");
    hr = IPropertyBag_Write(bag, L"Str", &v);
    VariantClear(&v);
    V_VT(&v) = VT_EMPTY;
    hr = IPropertyBag_Read(bag, L"Str", &v, NULL);
    CHECK(hr == S_OK && V_VT(&v) == VT_BSTR && !lstrcmpW(V_BSTR(&v), L"text"), "string roundtrip %#lx", hr);
    VariantClear(&v);
    /* binary */
    arr = SafeArrayCreateVector(VT_UI1, 0, 5);
    SafeArrayAccessData(arr, &bytes); memcpy(bytes, "\x01\x02\x03\x00\xff", 5); SafeArrayUnaccessData(arr);
    V_VT(&v) = VT_ARRAY | VT_UI1; V_ARRAY(&v) = arr;
    hr = IPropertyBag_Write(bag, L"Bin", &v);
    CHECK(hr == S_OK, "write binary %#lx", hr);
    SafeArrayDestroy(arr);
    V_VT(&v) = VT_EMPTY;
    hr = IPropertyBag_Read(bag, L"Bin", &v, NULL);
    CHECK(hr == S_OK && V_VT(&v) == (VT_ARRAY | VT_UI1) && SafeArrayGetDim(V_ARRAY(&v)) == 1, "read binary %#lx", hr);
    if (hr == S_OK && V_ARRAY(&v))
    {
        LONG lo = 0, hi = -1;
        SafeArrayGetLBound(V_ARRAY(&v), 1, &lo); SafeArrayGetUBound(V_ARRAY(&v), 1, &hi);
        SafeArrayAccessData(V_ARRAY(&v), &bytes);
        CHECK(hi - lo + 1 == 5 && !memcmp(bytes, "\x01\x02\x03\x00\xff", 5), "binary bytes");
        SafeArrayUnaccessData(V_ARRAY(&v));
    }
    VariantClear(&v);
    /* a second bag object on the same folder sees it; another bag name or folder does not */
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == S_OK && bag_read_i4(bag2, L"Num", &val) == S_OK && val == 77, "second object sees the value");
    IPropertyBag_Release(bag2);
    SHGetViewStatePropertyBag(pa, L"SGOther", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(bag_read_i4(bag2, L"Num", &val) != S_OK, "other bag name has no value");
    IPropertyBag_Release(bag2);
    SHGetViewStatePropertyBag(ps, L"SGBag", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(bag_read_i4(bag2, L"Num", &val) != S_OK, "other folder has no value");
    IPropertyBag_Release(bag2);

    /* defaults: the inherit bag of the parent, then all folders of the user */
    SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_INHERIT | SHGVSPB_PERUSER, &IID_IPropertyBag, (void **)&bag2);
    bag_write_i4(bag2, L"Inh", 5);
    IPropertyBag_Release(bag2);
    hr = SHGetViewStatePropertyBag(ps, L"SGBag", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == S_OK && bag_read_i4(bag2, L"Inh", &val) == S_OK && val == 5, "child folder reads the parent's inherit bag");
    IPropertyBag_Release(bag2);
    SHGetViewStatePropertyBag(ps, L"SGBag", SHGVSPB_FOLDERNODEFAULTS, &IID_IPropertyBag, (void **)&bag2);
    CHECK(bag_read_i4(bag2, L"Inh", &val) != S_OK, "no defaults with NOAUTODEFAULTS");
    IPropertyBag_Release(bag2);
    SHGetViewStatePropertyBag(NULL, L"SGBag", SHGVSPB_USERDEFAULTS, &IID_IPropertyBag, (void **)&bag2);
    bag_write_i4(bag2, L"AllF", 9);
    IPropertyBag_Release(bag2);
    SHGetViewStatePropertyBag(ps, L"SGBag", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(bag_read_i4(bag2, L"AllF", &val) == S_OK && val == 9, "falls back to all folders");
    bag_write_i4(bag2, L"AllF", 10);
    CHECK(bag_read_i4(bag2, L"AllF", &val) == S_OK && val == 10, "own value wins");
    IPropertyBag_Release(bag2);
    SHGetViewStatePropertyBag(NULL, L"SGBag", SHGVSPB_USERDEFAULTS, &IID_IPropertyBag, (void **)&bag2);
    CHECK(bag_read_i4(bag2, L"AllF", &val) == S_OK && val == 9, "the default itself is unchanged by the write to the folder");
    IPropertyBag_Release(bag2);
    hr = SHGetViewStatePropertyBag(NULL, L"SGBag", SHGVSPB_GLOBALDEFAULTS, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == S_OK, "global defaults %#lx", hr);
    if (bag2) IPropertyBag_Release(bag2);

    /* arguments */
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_FOLDER, &IID_IUnknown, (void **)&unk);
    CHECK(hr == S_OK && unk, "IUnknown %#lx", hr);
    if (unk) IUnknown_Release(unk);
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_FOLDER, &IID_IStream, (void **)&unk);
    CHECK(hr == E_NOINTERFACE, "wrong iid %#lx", hr);
    hr = SHGetViewStatePropertyBag(NULL, L"SGBag", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == E_INVALIDARG, "folder without pidl %#lx", hr);
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_USERDEFAULTS, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == E_INVALIDARG, "all folders with a pidl %#lx", hr);
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_PERFOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == E_INVALIDARG, "no user flag %#lx", hr);
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_PERUSER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == E_INVALIDARG, "no folder flag %#lx", hr);
    hr = SHGetViewStatePropertyBag(pa, L"SGBag", SHGVSPB_PERUSER | SHGVSPB_ALLUSERS | SHGVSPB_PERFOLDER, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == E_INVALIDARG, "both user flags %#lx", hr);
    hr = SHGetViewStatePropertyBag(NULL, L"SGBag", SHGVSPB_USERDEFAULTS | SHGVSPB_ROAM, &IID_IPropertyBag, (void **)&bag2);
    CHECK(hr == E_INVALIDARG, "roam with all folders %#lx", hr);
    IPropertyBag_Release(bag);
    ILFree(pa); ILFree(ps);
}

/* a sink for the connection point */
typedef struct { IExplorerBrowserEvents iface; LONG ref; } sink_t;
static HRESULT WINAPI s_QI(IExplorerBrowserEvents *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IExplorerBrowserEvents)) { *o = i; IExplorerBrowserEvents_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI s_AddRef(IExplorerBrowserEvents *i) { return InterlockedIncrement(&((sink_t *)i)->ref); }
static ULONG WINAPI s_Release(IExplorerBrowserEvents *i) { return InterlockedDecrement(&((sink_t *)i)->ref); }
static HRESULT WINAPI s_Pending(IExplorerBrowserEvents *i, PCIDLIST_ABSOLUTE p) { return S_OK; }
static PIDLIST_ABSOLUTE last_complete;
static HRESULT WINAPI s_Complete(IExplorerBrowserEvents *i, PCIDLIST_ABSOLUTE p) { ILFree(last_complete); last_complete = ILClone(p); return S_OK; }
static HRESULT WINAPI s_Failed(IExplorerBrowserEvents *i, PCIDLIST_ABSOLUTE p) { return S_OK; }
static HRESULT WINAPI s_Created(IExplorerBrowserEvents *i, IShellView *v) { return S_OK; }
static IExplorerBrowserEventsVtbl sink_vtbl = { s_QI, s_AddRef, s_Release, s_Pending, s_Created, s_Complete, s_Failed };

#define DRAW_W 300
#define DRAW_H 200
static int draw_pixels(IShellView *view)
{
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), DRAW_W, -DRAW_H, 1, 32, BI_RGB } };
    UINT *bits;
    HBITMAP bmp;
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ old;
    RECTL rc = { 0, 0, DRAW_W, DRAW_H };
    IViewObject *vo;
    int i, n = 0;

    if (FAILED(IShellView_QueryInterface(view, &IID_IViewObject, (void **)&vo))) return -1;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    old = SelectObject(dc, bmp);
    for (i = 0; i < DRAW_W * DRAW_H; i++) bits[i] = 0xffffff;
    IViewObject_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &rc, NULL, NULL, 0);
    for (i = 0; i < DRAW_W * DRAW_H; i++) if ((bits[i] & 0xffffff) != 0xffffff) n++;
    SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc);
    IViewObject_Release(vo);
    return n;
}

static IExplorerBrowser *make_eb(HWND hwnd, const FOLDERSETTINGS *fs)
{
    IExplorerBrowser *eb = NULL;
    RECT rc = { 0, 0, 300, 200 };
    HRESULT hr = CoCreateInstance(&CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&eb);
    CHECK(hr == S_OK, "create EB %#lx", hr);
    hr = IExplorerBrowser_Initialize(eb, hwnd, &rc, fs);
    CHECK(hr == S_OK, "Initialize %#lx", hr);
    return eb;
}

static UINT view_mode(IExplorerBrowser *eb)
{
    IFolderView2 *fv = NULL;
    UINT mode = 999;
    if (SUCCEEDED(IExplorerBrowser_GetCurrentView(eb, &IID_IFolderView2, (void **)&fv)))
    {
        IFolderView2_GetCurrentViewMode(fv, &mode);
        IFolderView2_Release(fv);
    }
    return mode;
}

static void test_eb(const WCHAR *dir_a, const WCHAR *dir_b, const WCHAR *dir_empty)
{
    PIDLIST_ABSOLUTE pa = path_pidl(dir_a), pb = path_pidl(dir_b), pe = path_pidl(dir_empty);
    HWND hwnd = CreateWindowW(L"STATIC", L"eb", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    FOLDERSETTINGS fs = { FVM_DETAILS, 0 };
    IExplorerBrowser *eb;
    IShellBrowser *sb;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp, *cps[3];
    IEnumConnectionPoints *ecp, *ecp2;
    IEnumConnections *ec, *ec2;
    CONNECTDATA cd[4];
    sink_t sink[2] = { { { &sink_vtbl }, 1 }, { { &sink_vtbl }, 1 } };
    DWORD cookie[2];
    IStream *st;
    IPropertyBag *bag;
    IShellView *view;
    ICommDlgBrowser3 *cdb;
    LONG val;
    ULONG n;
    HRESULT hr;
    MSG msg;
    char buf[16];
    ULONG got;
    int with_text, without_text;
    LARGE_INTEGER zero = {{0}};

    eb = make_eb(hwnd, &fs);
    IExplorerBrowser_QueryInterface(eb, &IID_IShellBrowser, (void **)&sb);

    /* property bag: the view mode of a folder is kept and returns */
    hr = IExplorerBrowser_SetPropertyBag(eb, NULL);
    CHECK(hr == E_INVALIDARG, "SetPropertyBag(NULL) %#lx", hr);
    hr = IExplorerBrowser_SetPropertyBag(eb, L"SGEBTest");
    CHECK(hr == S_OK, "SetPropertyBag %#lx", hr);
    hr = IExplorerBrowser_BrowseToIDList(eb, pa, 0);
    CHECK(hr == S_OK, "browse A %#lx", hr);
    pump();
    CHECK(view_mode(eb) == FVM_DETAILS, "A starts in details: %u", view_mode(eb));
    fs.ViewMode = FVM_LIST;
    IExplorerBrowser_SetFolderSettings(eb, &fs);
    CHECK(view_mode(eb) == FVM_LIST, "A is list now: %u", view_mode(eb));
    hr = IExplorerBrowser_BrowseToIDList(eb, pb, 0);
    CHECK(hr == S_OK, "browse B %#lx", hr);
    pump();
    SHGetViewStatePropertyBag(pa, L"SGEBTest", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag);
    CHECK(bag_read_i4(bag, L"Mode", &val) == S_OK && val == FVM_LIST, "Mode of A saved at the navigation: %ld", val);
    IPropertyBag_Release(bag);
    CHECK(view_mode(eb) == FVM_LIST, "B takes the settings in force: %u", view_mode(eb));
    fs.ViewMode = FVM_ICON;
    IExplorerBrowser_SetFolderSettings(eb, &fs);
    hr = IExplorerBrowser_BrowseToIDList(eb, pa, 0);
    pump();
    CHECK(view_mode(eb) == FVM_LIST, "back in A the saved mode returns: %u", view_mode(eb));
    SHGetViewStatePropertyBag(pb, L"SGEBTest", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag);
    CHECK(bag_read_i4(bag, L"Mode", &val) == S_OK && val == FVM_ICON, "Mode of B saved: %ld", val);
    IPropertyBag_Release(bag);

    /* the view state stream */
    hr = IShellBrowser_GetViewStateStream(sb, STGM_READ, &st);
    CHECK(hr == STG_E_FILENOTFOUND && !st, "no stream yet %#lx", hr);
    hr = IShellBrowser_GetViewStateStream(sb, STGM_WRITE, &st);
    CHECK(hr == S_OK && st, "write stream %#lx", hr);
    if (st)
    {
        hr = IStream_Write(st, "state-bytes", 11, &got);
        CHECK(hr == S_OK && got == 11, "stream write %#lx", hr);
        IStream_Release(st);
    }
    hr = IShellBrowser_GetViewStateStream(sb, STGM_READ, &st);
    CHECK(hr == S_OK && st, "read stream %#lx", hr);
    if (st)
    {
        memset(buf, 0, sizeof(buf));
        hr = IStream_Read(st, buf, 16, &got);
        CHECK(got == 11 && !memcmp(buf, "state-bytes", 11), "stream content %lu", got);
        hr = IStream_Write(st, "x", 1, &got);
        CHECK(hr == STG_E_ACCESSDENIED, "read stream refuses writes %#lx", hr);
        IStream_Release(st);
    }
    hr = IShellBrowser_GetViewStateStream(sb, STGM_READWRITE, &st);
    CHECK(hr == S_OK && st, "readwrite stream %#lx", hr);
    if (st)
    {
        memset(buf, 0, sizeof(buf));
        IStream_Read(st, buf, 16, &got);
        CHECK(got == 11, "readwrite sees the data %lu", got);
        IStream_Seek(st, zero, STREAM_SEEK_SET, NULL);
        IStream_Write(st, "NEW", 3, &got);
        IStream_Commit(st, 0);
        IStream_Release(st);
    }
    IShellBrowser_GetViewStateStream(sb, STGM_READ, &st);
    memset(buf, 0, sizeof(buf));
    got = 0;
    if (st) { IStream_Read(st, buf, 16, &got); IStream_Release(st); }
    CHECK(got == 11 && !memcmp(buf, "NEWte-bytes", 11) || got == 3, "committed overwrite: %lu %.11s", got, buf);
    hr = IShellBrowser_GetViewStateStream(sb, STGM_READ, NULL);
    CHECK(hr == E_INVALIDARG, "NULL out %#lx", hr);

    /* empty text */
    hr = IExplorerBrowser_SetEmptyText(eb, NULL);
    CHECK(hr == E_INVALIDARG, "SetEmptyText(NULL) %#lx", hr);
    IExplorerBrowser_BrowseToIDList(eb, pe, 0);
    pump();
    IExplorerBrowser_GetCurrentView(eb, &IID_IShellView, (void **)&view);
    without_text = view ? draw_pixels(view) : -1;
    if (view) IShellView_Release(view);
    hr = IExplorerBrowser_SetEmptyText(eb, L"Nothing in this folder at all");
    CHECK(hr == S_OK, "SetEmptyText %#lx", hr);
    pump();
    IExplorerBrowser_GetCurrentView(eb, &IID_IShellView, (void **)&view);
    with_text = view ? draw_pixels(view) : -1;
    if (view) IShellView_Release(view);
    CHECK(with_text > without_text + 50, "empty text drawn on the current view: %d vs %d", with_text, without_text);
    /* and the text is used for the next view too */
    IExplorerBrowser_BrowseToIDList(eb, pa, 0);
    pump();
    IExplorerBrowser_BrowseToIDList(eb, pe, 0);
    pump();
    IExplorerBrowser_GetCurrentView(eb, &IID_IShellView, (void **)&view);
    with_text = view ? draw_pixels(view) : -1;
    if (view) IShellView_Release(view);
    CHECK(with_text > without_text + 50, "empty text on the next view of the folder: %d vs %d", with_text, without_text);

    /* accelerators */
    memset(&msg, 0, sizeof(msg));
    msg.message = WM_KEYDOWN; msg.wParam = 'A';
    hr = IShellBrowser_TranslateAcceleratorSB(sb, &msg, 0);
    CHECK(hr == S_FALSE, "ordinary key %#lx", hr);
    msg.message = WM_CHAR;
    msg.wParam = VK_BACK;
    hr = IShellBrowser_TranslateAcceleratorSB(sb, &msg, 0);
    CHECK(hr == S_FALSE, "not a key down %#lx", hr);
    msg.message = WM_KEYDOWN;
    {
        IConnectionPoint *tmp_cp;
        IConnectionPointContainer *tmp_cpc;
        DWORD tmp_cookie;
        PIDLIST_ABSOLUTE up = ILClone(pe);
        sink_t nav = { { &sink_vtbl }, 1 };

        ILRemoveLastID(up);
        IExplorerBrowser_QueryInterface(eb, &IID_IConnectionPointContainer, (void **)&tmp_cpc);
        IConnectionPointContainer_FindConnectionPoint(tmp_cpc, &IID_IExplorerBrowserEvents, &tmp_cp);
        IConnectionPoint_Advise(tmp_cp, (IUnknown *)&nav.iface, &tmp_cookie);
        hr = IShellBrowser_TranslateAcceleratorSB(sb, &msg, 0);
        CHECK(hr == S_OK, "backspace %#lx", hr);
        pump();
        CHECK(last_complete && ILIsEqual(last_complete, up), "backspace navigated to the parent folder");
        /* Alt+Left goes back to where we were, Alt+Right forward again */
        msg.wParam = VK_LEFT;
        msg.lParam = 1 << 29;
        ILFree(last_complete); last_complete = NULL;
        hr = IShellBrowser_TranslateAcceleratorSB(sb, &msg, 0);
        CHECK(hr == S_OK, "alt+left %#lx", hr);
        pump();
        CHECK(last_complete && ILIsEqual(last_complete, pe), "alt+left navigated back");
        msg.wParam = VK_RIGHT;
        ILFree(last_complete); last_complete = NULL;
        hr = IShellBrowser_TranslateAcceleratorSB(sb, &msg, 0);
        CHECK(hr == S_OK, "alt+right %#lx", hr);
        pump();
        CHECK(last_complete && ILIsEqual(last_complete, up), "alt+right navigated forward");
        msg.lParam = 0;
        msg.wParam = VK_LEFT;
        hr = IShellBrowser_TranslateAcceleratorSB(sb, &msg, 0);
        CHECK(hr == S_FALSE, "left without alt %#lx", hr);
        IConnectionPoint_Unadvise(tmp_cp, tmp_cookie);
        IConnectionPoint_Release(tmp_cp);
        IConnectionPointContainer_Release(tmp_cpc);
        ILFree(up);
    }
    hr = IShellBrowser_TranslateAcceleratorSB(sb, NULL, 0);
    CHECK(hr == E_INVALIDARG, "NULL msg %#lx", hr);

    /* connection points */
    IExplorerBrowser_QueryInterface(eb, &IID_IConnectionPointContainer, (void **)&cpc);
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
    CHECK(hr == S_OK && ecp, "EnumConnectionPoints %#lx", hr);
    n = 99;
    hr = IEnumConnectionPoints_Next(ecp, 2, cps, &n);
    CHECK(hr == S_FALSE && n == 1 && cps[0], "Next(2) %#lx %lu", hr, n);
    if (n == 1)
    {
        IID iid;
        CHECK(IConnectionPoint_GetConnectionInterface(cps[0], &iid) == S_OK && IsEqualIID(&iid, &IID_IExplorerBrowserEvents), "the point's interface");
        IConnectionPoint_Release(cps[0]);
    }
    n = 99;
    hr = IEnumConnectionPoints_Next(ecp, 1, cps, &n);
    CHECK(hr == S_FALSE && n == 0, "Next at the end %#lx %lu", hr, n);
    IEnumConnectionPoints_Reset(ecp);
    hr = IEnumConnectionPoints_Skip(ecp, 1);
    CHECK(hr == S_OK, "Skip 1 %#lx", hr);
    hr = IEnumConnectionPoints_Skip(ecp, 1);
    CHECK(hr == S_FALSE, "Skip past the end %#lx", hr);
    IEnumConnectionPoints_Reset(ecp);
    hr = IEnumConnectionPoints_Clone(ecp, &ecp2);
    CHECK(hr == S_OK && ecp2, "Clone %#lx", hr);
    n = 0;
    hr = IEnumConnectionPoints_Next(ecp2, 1, cps, &n);
    CHECK(hr == S_OK && n == 1, "clone Next %#lx", hr);
    if (n == 1) IConnectionPoint_Release(cps[0]);
    IEnumConnectionPoints_Release(ecp2);
    IEnumConnectionPoints_Release(ecp);

    IConnectionPointContainer_FindConnectionPoint(cpc, &IID_IExplorerBrowserEvents, &cp);
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECK(hr == S_OK && ec, "EnumConnections empty %#lx", hr);
    n = 99;
    hr = IEnumConnections_Next(ec, 1, cd, &n);
    CHECK(hr == S_FALSE && n == 0, "no connections yet %#lx %lu", hr, n);
    IEnumConnections_Release(ec);
    IConnectionPoint_Advise(cp, (IUnknown *)&sink[0].iface, &cookie[0]);
    IConnectionPoint_Advise(cp, (IUnknown *)&sink[1].iface, &cookie[1]);
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECK(hr == S_OK, "EnumConnections %#lx", hr);
    memset(cd, 0, sizeof(cd));
    n = 0;
    hr = IEnumConnections_Next(ec, 4, cd, &n);
    CHECK(hr == S_FALSE && n == 2, "two connections %#lx %lu", hr, n);
    CHECK(cd[0].dwCookie == cookie[0] && cd[1].dwCookie == cookie[1], "cookies %lu %lu / %lu %lu", cd[0].dwCookie, cd[1].dwCookie, cookie[0], cookie[1]);
    CHECK(cd[0].pUnk == (IUnknown *)&sink[0].iface && cd[1].pUnk == (IUnknown *)&sink[1].iface, "sink objects");
    if (n == 2) { IUnknown_Release(cd[0].pUnk); IUnknown_Release(cd[1].pUnk); }
    IEnumConnections_Reset(ec);
    IConnectionPoint_Unadvise(cp, cookie[0]);        /* the enumerator is a snapshot */
    hr = IEnumConnections_Clone(ec, &ec2);
    CHECK(hr == S_OK, "Clone %#lx", hr);
    n = 0;
    IEnumConnections_Skip(ec2, 1);
    hr = IEnumConnections_Next(ec2, 1, cd, &n);
    CHECK(hr == S_OK && n == 1 && cd[0].dwCookie == cookie[1], "clone keeps the position: %lu", cd[0].dwCookie);
    if (n == 1) IUnknown_Release(cd[0].pUnk);
    IEnumConnections_Release(ec2);
    IEnumConnections_Release(ec);
    IConnectionPoint_Unadvise(cp, cookie[1]);
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    n = 7;
    IEnumConnections_Next(ec, 1, cd, &n);
    CHECK(n == 0, "all unadvised: %lu", n);
    IEnumConnections_Release(ec);
    CHECK(sink[0].ref == 1 && sink[1].ref == 1, "sink references balanced: %ld %ld", sink[0].ref, sink[1].ref);
    IConnectionPoint_Release(cp);
    IConnectionPointContainer_Release(cpc);

    /* the common dialog callbacks without a site */
    hr = IExplorerBrowser_QueryInterface(eb, &IID_ICommDlgBrowser3, (void **)&cdb);
    if (SUCCEEDED(hr))
    {
        IExplorerBrowser_GetCurrentView(eb, &IID_IShellView, (void **)&view);
        hr = ICommDlgBrowser3_OnStateChange(cdb, view, CDBOSC_SELCHANGE);
        CHECK(hr == S_OK, "OnStateChange without a site %#lx", hr);
        if (view) IShellView_Release(view);
        ICommDlgBrowser3_Release(cdb);
    }
    else
    {
        /* the interface is on the browser object itself; try through the shell browser */
        IShellBrowser_QueryInterface(sb, &IID_ICommDlgBrowser3, (void **)&cdb);
        CHECK(cdb != NULL, "ICommDlgBrowser3 %#lx", hr);
        if (cdb)
        {
            hr = ICommDlgBrowser3_OnStateChange(cdb, NULL, CDBOSC_SELCHANGE);
            CHECK(hr == S_OK, "OnStateChange without a site %#lx", hr);
            ICommDlgBrowser3_Release(cdb);
        }
    }

    /* the state is also written when the browser is destroyed */
    fs.ViewMode = FVM_TILE;
    IExplorerBrowser_SetFolderSettings(eb, &fs);
    IExplorerBrowser_Destroy(eb);
    {
        /* backspace had gone to the parent of the empty folder, which is where the browser was */
        PIDLIST_ABSOLUTE up = ILClone(pe);
        ILRemoveLastID(up);
        SHGetViewStatePropertyBag(up, L"SGEBTest", SHGVSPB_FOLDER, &IID_IPropertyBag, (void **)&bag);
        ILFree(up);
    }
    val = -1;
    CHECK(bag_read_i4(bag, L"Mode", &val) == S_OK && val == FVM_TILE, "Mode saved at Destroy (in the parent): %ld", val);
    IPropertyBag_Release(bag);
    IShellBrowser_Release(sb);
    IExplorerBrowser_Release(eb);
    DestroyWindow(hwnd);
    ILFree(pa); ILFree(pb); ILFree(pe);
}

int main(void)
{
    WCHAR base[MAX_PATH], a[MAX_PATH], sub[MAX_PATH], e[MAX_PATH], f[MAX_PATH];

    OleInitialize(NULL);
    /* a fresh start (this probe only ever runs in a scratch prefix) */
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\Bags");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\Shell\\Bags");
    GetTempPathW(MAX_PATH, base);
    lstrcatW(base, L"sgebstate");
    CreateDirectoryW(base, NULL);
    wsprintfW(a, L"%s\\a", base); CreateDirectoryW(a, NULL);
    wsprintfW(sub, L"%s\\a\\sub", base); CreateDirectoryW(sub, NULL);
    wsprintfW(e, L"%s\\empty", base); CreateDirectoryW(e, NULL);
    wsprintfW(f, L"%s\\a\\one.txt", base); CloseHandle(CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    wsprintfW(f, L"%s\\a\\two.txt", base); CloseHandle(CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    {
        WCHAR b[MAX_PATH];
        wsprintfW(b, L"%s\\b", base); CreateDirectoryW(b, NULL);
        wsprintfW(f, L"%s\\b\\x.txt", base); CloseHandle(CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
        test_bags(a, sub);
        test_eb(a, b, e);
        DeleteFileW(f); RemoveDirectoryW(b);
    }
    wsprintfW(f, L"%s\\a\\one.txt", base); DeleteFileW(f);
    wsprintfW(f, L"%s\\a\\two.txt", base); DeleteFileW(f);
    RemoveDirectoryW(sub); RemoveDirectoryW(a); RemoveDirectoryW(e); RemoveDirectoryW(base);
    printf("failures: %d\nRESULT: %s\n", failed, failed ? "FAIL" : "PASS");
    OleUninitialize();
    return failed != 0;
}
