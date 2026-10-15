/* dhtmled batch 2 probe: OLE interfaces of the control, run under Wine. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <olectl.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do{ checks++; if(c) printf("PASS %s\n",#c); else {printf("FAIL line %d %s: ",__LINE__,#c); printf(__VA_ARGS__); printf("\n"); fails++;} }while(0)

static const CLSID CLSID_Edit={0x2d360200,0xfff5,0x11d1,{0x8d,0x03,0x00,0xa0,0xc9,0x59,0xbc,0x0a}};
static const IID DIID_Events={0x588d5040,0xcf28,0x11d1,{0x8c,0xd3,0x00,0xa0,0xc9,0x59,0xbc,0x0a}};

static void set_html(IUnknown *ctl, const WCHAR *html)
{
    IDispatch *d; DISPID id, put = DISPID_PROPERTYPUT; OLECHAR *n = (OLECHAR *)L"DocumentHTML";
    VARIANT a; DISPPARAMS dp = {&a, &put, 1, 1};
    IUnknown_QueryInterface(ctl, &IID_IDispatch, (void **)&d);
    IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    V_VT(&a) = VT_BSTR; V_BSTR(&a) = SysAllocString(html);
    IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    SysFreeString(V_BSTR(&a));
    IDispatch_Release(d);
}

static WCHAR *get_html(IUnknown *ctl)
{
    static WCHAR buf[512];
    IDispatch *d; DISPID id; OLECHAR *n = (OLECHAR *)L"DocumentHTML";
    VARIANT r; DISPPARAMS dp = {NULL, NULL, 0, 0};
    IUnknown_QueryInterface(ctl, &IID_IDispatch, (void **)&d);
    IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    VariantInit(&r);
    IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp, &r, NULL, NULL);
    lstrcpynW(buf, V_BSTR(&r) ? V_BSTR(&r) : L"", 512);
    VariantClear(&r);
    IDispatch_Release(d);
    return buf;
}

static IUnknown *create(void)
{
    IUnknown *u = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_Edit, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u);
    if (FAILED(hr)) { printf("FAIL create %08lx\n", hr); exit(1); }
    return u;
}

/* ---- mock client site */
static int shown, shown_calls;
static HRESULT WINAPI cs_qi(IOleClientSite *f, REFIID r, void **o) { if (IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IOleClientSite)) { *o=f; return S_OK; } *o=NULL; return E_NOINTERFACE; }
static ULONG WINAPI cs_ar(IOleClientSite *f) { return 2; }
static ULONG WINAPI cs_rl(IOleClientSite *f) { return 1; }
static HRESULT WINAPI cs_sd(IOleClientSite *f) { return E_NOTIMPL; }
static HRESULT WINAPI cs_gm(IOleClientSite *f, DWORD a, DWORD b, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI cs_gc(IOleClientSite *f, IOleContainer **c) { return E_NOTIMPL; }
static HRESULT WINAPI cs_sw(IOleClientSite *f, BOOL s) { shown = s; shown_calls++; return S_OK; }
static HRESULT WINAPI cs_rn(IOleClientSite *f) { return S_OK; }
static IOleClientSiteVtbl cs_vtbl = { cs_qi, cs_ar, cs_rl, cs_sd, cs_gm, cs_gc, cs_rn, cs_sw, cs_rn };
static IOleClientSite site = { &cs_vtbl };

/* ---- mock advise sink / event sink */
static int data_changes, view_changes;
static HRESULT WINAPI as_qi(IAdviseSink *f, REFIID r, void **o) { if (IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IAdviseSink)) { *o=f; return S_OK; } *o=NULL; return E_NOINTERFACE; }
static ULONG WINAPI as_ar(IAdviseSink *f) { return 2; }
static ULONG WINAPI as_rl(IAdviseSink *f) { return 1; }
static void WINAPI as_dc(IAdviseSink *f, FORMATETC *fe, STGMEDIUM *m) { data_changes++; }
static void WINAPI as_vc(IAdviseSink *f, DWORD a, LONG i) { view_changes++; }
static void WINAPI as_r(IAdviseSink *f, IMoniker *m) { }
static void WINAPI as_s(IAdviseSink *f) { }
static void WINAPI as_c(IAdviseSink *f) { }
static IAdviseSinkVtbl as_vtbl = { as_qi, as_ar, as_rl, as_dc, as_vc, as_r, as_s, as_c };
static IAdviseSink adv_sink = { &as_vtbl };

static HRESULT WINAPI ev_qi(IDispatch *f, REFIID r, void **o) { if (IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IDispatch)||IsEqualIID(r,&DIID_Events)) { *o=f; return S_OK; } *o=NULL; return E_NOINTERFACE; }
static ULONG WINAPI ev_ar(IDispatch *f) { return 2; }
static ULONG WINAPI ev_rl(IDispatch *f) { return 1; }
static HRESULT WINAPI ev_c(IDispatch *f, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI ev_t(IDispatch *f, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI ev_n(IDispatch *f, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI ev_i(IDispatch *f, DISPID d, REFIID r, LCID l, WORD fl, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a) { return S_OK; }
static IDispatchVtbl ev_vtbl = { ev_qi, ev_ar, ev_rl, ev_c, ev_t, ev_n, ev_i };
static IDispatch ev_a = { &ev_vtbl }, ev_b = { &ev_vtbl };

static void test_class_info(void)
{
    IUnknown *u = create();
    IProvideClassInfo2 *pci;
    ITypeInfo *ti;
    TYPEATTR *attr;
    GUID g;
    HRESULT hr;

    hr = IUnknown_QueryInterface(u, &IID_IProvideClassInfo2, (void **)&pci);
    CHECK(hr == S_OK, "qi %08lx", hr);
    hr = IProvideClassInfo2_GetGUID(pci, GUIDKIND_DEFAULT_SOURCE_DISP_IID, &g);
    CHECK(hr == S_OK && IsEqualGUID(&g, &DIID_Events), "guid %08lx", hr);
    hr = IProvideClassInfo2_GetGUID(pci, 7, &g);
    CHECK(hr == E_INVALIDARG, "bad kind %08lx", hr);
    hr = IProvideClassInfo2_GetGUID(pci, GUIDKIND_DEFAULT_SOURCE_DISP_IID, NULL);
    CHECK(hr == E_POINTER, "null %08lx", hr);
    hr = IProvideClassInfo2_GetClassInfo(pci, &ti);
    CHECK(hr == S_OK, "classinfo %08lx", hr);
    if (hr == S_OK)
    {
        ITypeInfo_GetTypeAttr(ti, &attr);
        CHECK(attr->typekind == TKIND_COCLASS && IsEqualGUID(&attr->guid, &CLSID_Edit), "kind %d", attr->typekind);
        ITypeInfo_ReleaseTypeAttr(ti, attr);
        ITypeInfo_Release(ti);
    }
    IProvideClassInfo2_Release(pci);
    IUnknown_Release(u);
}

static void test_storage(void)
{
    IUnknown *a = create(), *b = create();
    IPersistStorage *pa, *pb;
    IStorage *stg;
    HRESULT hr;

    IUnknown_QueryInterface(a, &IID_IPersistStorage, (void **)&pa);
    IUnknown_QueryInterface(b, &IID_IPersistStorage, (void **)&pb);
    hr = StgCreateDocfile(NULL, STGM_CREATE | STGM_READWRITE | STGM_DIRECT | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg);
    CHECK(hr == S_OK, "docfile %08lx", hr);

    CHECK(IPersistStorage_InitNew(pa, NULL) == E_POINTER, "initnew null");
    CHECK(IPersistStorage_Load(pa, NULL) == E_POINTER, "load null");
    CHECK(IPersistStorage_Save(pa, NULL, TRUE) == E_POINTER, "save null");
    hr = IPersistStorage_InitNew(pa, stg);
    CHECK(hr == S_OK, "initnew %08lx", hr);
    CHECK(!wcsicmp(get_html(a), L"<HTML></HTML>"), "new doc '%ls'", get_html(a));
    set_html(a, L"<html><body>stored text</body></html>");
    CHECK(IPersistStorage_IsDirty(pa) == S_OK, "dirty before save");
    hr = IPersistStorage_Save(pa, stg, FALSE);
    CHECK(hr == S_OK, "save %08lx", hr);
    CHECK(IPersistStorage_IsDirty(pa) == S_FALSE, "clean after save");
    CHECK(IPersistStorage_SaveCompleted(pa, NULL) == S_OK, "save completed");
    hr = IPersistStorage_Load(pb, stg);
    CHECK(hr == S_OK, "load %08lx", hr);
    CHECK(!lstrcmpW(get_html(b), L"<html><body>stored text</body></html>"), "loaded '%ls'", get_html(b));
    CHECK(IPersistStorage_IsDirty(pb) == S_FALSE, "clean after load");

    IStorage_Release(stg);
    hr = StgCreateDocfile(NULL, STGM_CREATE | STGM_READWRITE | STGM_DIRECT | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg);
    hr = IPersistStorage_Load(pb, stg);
    CHECK(FAILED(hr), "load from an empty storage %08lx", hr);
    IStorage_Release(stg);
    IPersistStorage_Release(pa);
    IPersistStorage_Release(pb);
    IUnknown_Release(a);
    IUnknown_Release(b);
}

static void test_ole_object(void)
{
    IUnknown *u = create(), *v;
    IOleObject *oo;
    IDataObject *snap, *other;
    FORMATETC fe = { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM sm;
    LOGPALETTE pal = { 0x300, 0 };
    HRESULT hr;

    IUnknown_QueryInterface(u, &IID_IOleObject, (void **)&oo);

    hr = IOleObject_SetColorScheme(oo, &pal);
    CHECK(hr == S_OK, "palette %08lx", hr);
    CHECK(IOleObject_SetColorScheme(oo, NULL) == E_INVALIDARG, "no palette");
    pal.palVersion = 5;
    CHECK(IOleObject_SetColorScheme(oo, &pal) == E_INVALIDARG, "bad palette version");

    /* verbs */
    hr = IOleObject_DoVerb(oo, OLEIVERB_INPLACEACTIVATE, NULL, NULL, 0, NULL, NULL);
    CHECK(hr == E_UNEXPECTED, "verb without a site %08lx", hr);
    IOleObject_SetClientSite(oo, &site);
    shown = -1; shown_calls = 0;
    hr = IOleObject_DoVerb(oo, OLEIVERB_INPLACEACTIVATE, NULL, &site, 0, NULL, NULL);
    CHECK(hr == S_OK && shown == 1, "inplace activate %08lx shown %d", hr, shown);
    hr = IOleObject_DoVerb(oo, OLEIVERB_HIDE, NULL, &site, 0, NULL, NULL);
    CHECK(hr == S_OK && shown == 0, "hide %08lx shown %d", hr, shown);
    hr = IOleObject_DoVerb(oo, OLEIVERB_UIACTIVATE, NULL, &site, 0, NULL, NULL);
    CHECK(hr == S_OK && shown == 1, "ui activate %08lx", hr);
    hr = IOleObject_DoVerb(oo, OLEIVERB_DISCARDUNDOSTATE, NULL, &site, 0, NULL, NULL);
    CHECK(hr == S_OK, "discard undo %08lx", hr);
    hr = IOleObject_DoVerb(oo, OLEIVERB_OPEN, NULL, &site, 0, NULL, NULL);
    CHECK(hr == OLEOBJ_S_INVALIDVERB, "open %08lx", hr);

    /* clipboard data is a snapshot */
    set_html(u, L"<p>one</p>");
    hr = IOleObject_GetClipboardData(oo, 0, &snap);
    CHECK(hr == S_OK && snap, "snapshot %08lx", hr);
    set_html(u, L"<p>two</p>");
    hr = IDataObject_GetData(snap, &fe, &sm);
    CHECK(hr == S_OK && sm.tymed == TYMED_HGLOBAL && !lstrcmpW(GlobalLock(sm.hGlobal), L"<p>one</p>"), "snapshot data %08lx", hr);
    if (hr == S_OK) { GlobalUnlock(sm.hGlobal); ReleaseStgMedium(&sm); }

    /* InitFromData */
    CHECK(IOleObject_InitFromData(oo, NULL, FALSE, 0) == S_OK, "able to take data");
    hr = IOleObject_InitFromData(oo, snap, FALSE, 0);
    CHECK(hr == S_OK && !lstrcmpW(get_html(u), L"<p>one</p>"), "initfromdata %08lx '%ls'", hr, get_html(u));
    CHECK(IUnknown_QueryInterface(u, &IID_IPersistStreamInit, (void **)&other) == S_OK, "psi");
    IPersistStreamInit_Release((IPersistStreamInit *)other);

    /* a data object with only ANSI text */
    v = create();
    set_html(v, L"<b>ansi</b>");
    IUnknown_QueryInterface(v, &IID_IDataObject, (void **)&other);
    hr = IOleObject_InitFromData(oo, other, TRUE, 0);
    CHECK(hr == S_OK && !lstrcmpW(get_html(u), L"<b>ansi</b>"), "from the control's data object '%ls'", get_html(u));
    IDataObject_Release(other);
    IUnknown_Release(v);

    IDataObject_Release(snap);
    IOleObject_SetClientSite(oo, NULL);
    IOleObject_Release(oo);
    IUnknown_Release(u);
}

static void test_data_object(void)
{
    IUnknown *u = create();
    IDataObject *d;
    FORMATETC fe = { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }, out;
    STGMEDIUM sm;
    IEnumFORMATETC *en;
    UINT cf_html = RegisterClipboardFormatW(L"HTML Format");
    DWORD conn;
    HRESULT hr;
    char *p;
    ULONG n;
    FORMATETC list[8];

    set_html(u, L"<i>data</i>");
    IUnknown_QueryInterface(u, &IID_IDataObject, (void **)&d);

    CHECK(IDataObject_QueryGetData(d, &fe) == S_OK, "unicode");
    fe.cfFormat = CF_TEXT;
    CHECK(IDataObject_QueryGetData(d, &fe) == S_OK, "ansi");
    fe.cfFormat = cf_html;
    CHECK(IDataObject_QueryGetData(d, &fe) == S_OK, "html");
    fe.cfFormat = CF_BITMAP;
    CHECK(IDataObject_QueryGetData(d, &fe) == DV_E_FORMATETC, "bitmap");
    fe.cfFormat = CF_UNICODETEXT; fe.tymed = TYMED_ISTREAM;
    CHECK(IDataObject_QueryGetData(d, &fe) == DV_E_TYMED, "stream");
    fe.tymed = TYMED_HGLOBAL; fe.dwAspect = DVASPECT_ICON;
    CHECK(IDataObject_QueryGetData(d, &fe) == DV_E_DVASPECT, "icon");
    fe.dwAspect = DVASPECT_CONTENT;

    fe.cfFormat = CF_TEXT;
    hr = IDataObject_GetData(d, &fe, &sm);
    CHECK(hr == S_OK && !strcmp(GlobalLock(sm.hGlobal), "<i>data</i>"), "ansi data %08lx", hr);
    if (hr == S_OK) { GlobalUnlock(sm.hGlobal); ReleaseStgMedium(&sm); }
    fe.cfFormat = cf_html;
    hr = IDataObject_GetData(d, &fe, &sm);
    p = hr == S_OK ? GlobalLock(sm.hGlobal) : NULL;
    CHECK(p && !strncmp(p, "Version:0.9", 11) && strstr(p, "<i>data</i>"), "html format data %08lx", hr);
    if (hr == S_OK) { GlobalUnlock(sm.hGlobal); ReleaseStgMedium(&sm); }

    /* GetDataHere */
    fe.cfFormat = CF_UNICODETEXT;
    sm.tymed = TYMED_HGLOBAL; sm.pUnkForRelease = NULL;
    sm.hGlobal = GlobalAlloc(GMEM_MOVEABLE, 4);
    hr = IDataObject_GetDataHere(d, &fe, &sm);
    CHECK(hr == STG_E_MEDIUMFULL, "small medium %08lx", hr);
    GlobalFree(sm.hGlobal);
    sm.hGlobal = GlobalAlloc(GMEM_MOVEABLE, 100);
    hr = IDataObject_GetDataHere(d, &fe, &sm);
    CHECK(hr == S_OK && !lstrcmpW(GlobalLock(sm.hGlobal), L"<i>data</i>"), "data here %08lx", hr);
    GlobalUnlock(sm.hGlobal);
    GlobalFree(sm.hGlobal);

    hr = IDataObject_GetCanonicalFormatEtc(d, &fe, &out);
    CHECK(hr == DATA_S_SAMEFORMATETC, "canonical %08lx", hr);
    hr = IDataObject_SetData(d, &fe, &sm, FALSE);
    CHECK(hr == E_NOTIMPL, "set data %08lx", hr);
    hr = IDataObject_EnumFormatEtc(d, DATADIR_SET, &en);
    CHECK(hr == E_NOTIMPL, "enum set %08lx", hr);
    hr = IDataObject_EnumFormatEtc(d, DATADIR_GET, &en);
    CHECK(hr == S_OK, "enum get %08lx", hr);
    if (hr == S_OK)
    {
        hr = IEnumFORMATETC_Next(en, 8, list, &n);
        CHECK(hr == S_FALSE && n == 3 && list[0].cfFormat == CF_UNICODETEXT && list[2].cfFormat == cf_html, "enum count %lu", n);
        IEnumFORMATETC_Release(en);
    }

    /* advise */
    CHECK(IDataObject_EnumDAdvise(d, (IEnumSTATDATA **)&en) == OLE_E_ADVISENOTSUPPORTED, "no advise yet");
    CHECK(IDataObject_DUnadvise(d, 5) == OLE_E_NOCONNECTION, "unadvise nothing");
    data_changes = 0;
    hr = IDataObject_DAdvise(d, &fe, 0, &adv_sink, &conn);
    CHECK(hr == S_OK, "dadvise %08lx", hr);
    set_html(u, L"<i>changed</i>");
    CHECK(data_changes == 1, "data change notification %d", data_changes);
    CHECK(IDataObject_DUnadvise(d, conn) == S_OK, "unadvise");
    set_html(u, L"<i>again</i>");
    CHECK(data_changes == 1, "no notification after unadvise %d", data_changes);

    IDataObject_Release(d);
    IUnknown_Release(u);
}

static void test_view_object(void)
{
    IUnknown *u = create();
    IViewObjectEx *vo;
    IOleObject *oo;
    IOleInPlaceObjectWindowless *ip;
    SIZEL ext, ext2;
    RECTL rc;
    DWORD key, hit;
    LOGPALETTE *pal = (LOGPALETTE *)1;
    HRESULT hr;
    HDC dc;
    HBITMAP bmp, old;
    RECTL bounds = { 0, 0, 80, 40 };
    RECT pos = { 5, 6, 45, 36 }, clip = { 0, 0, 100, 100 }, rect;
    POINT pt = { 10, 10 };
    SIZEL natural = { 7, 7 };
    COLORREF px;

    IUnknown_QueryInterface(u, &IID_IViewObjectEx, (void **)&vo);
    IUnknown_QueryInterface(u, &IID_IOleObject, (void **)&oo);
    IUnknown_QueryInterface(u, &IID_IOleInPlaceObjectWindowless, (void **)&ip);

    hr = IViewObjectEx_GetExtent(vo, DVASPECT_CONTENT, -1, NULL, &ext);
    IOleObject_GetExtent(oo, DVASPECT_CONTENT, &ext2);
    CHECK(hr == S_OK && ext.cx == ext2.cx && ext.cy == ext2.cy && ext.cx > 0, "extent %08lx %ldx%ld", hr, ext.cx, ext.cy);
    CHECK(IViewObjectEx_GetExtent(vo, DVASPECT_ICON, -1, NULL, &ext) == DV_E_DVASPECT, "icon extent");
    hr = IViewObjectEx_GetRect(vo, DVASPECT_CONTENT, &rc);
    CHECK(hr == S_OK && rc.left == 0 && rc.top == 0 && rc.right == ext2.cx && rc.bottom == ext2.cy, "rect %08lx", hr);
    CHECK(IViewObjectEx_GetRect(vo, DVASPECT_ICON, &rc) == DV_E_DVASPECT, "icon rect");
    hr = IViewObjectEx_GetColorSet(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal);
    CHECK(hr == S_FALSE && !pal, "color set %08lx", hr);

    hr = IViewObjectEx_Freeze(vo, DVASPECT_CONTENT, -1, NULL, &key);
    CHECK(hr == S_OK && key == 1, "freeze %08lx %lu", hr, key);
    hr = IViewObjectEx_Freeze(vo, DVASPECT_CONTENT, -1, NULL, &key);
    CHECK(hr == VIEW_S_ALREADY_FROZEN && key == 1, "freeze again %08lx", hr);
    CHECK(IViewObjectEx_Freeze(vo, DVASPECT_ICON, -1, NULL, &key) == DV_E_DVASPECT, "freeze icon");
    CHECK(IViewObjectEx_Freeze(vo, DVASPECT_CONTENT, 3, NULL, &key) == DV_E_LINDEX, "freeze index");
    IViewObjectEx_SetAdvise(vo, DVASPECT_CONTENT, 0, &adv_sink);
    view_changes = 0;
    CHECK(IViewObjectEx_Unfreeze(vo, 7) == OLE_E_NOCONNECTION, "unfreeze bad key");
    CHECK(IViewObjectEx_Unfreeze(vo, 1) == S_OK, "unfreeze");
    CHECK(view_changes == 1, "view change after unfreeze %d", view_changes);
    CHECK(IViewObjectEx_Unfreeze(vo, 1) == OLE_E_NOCONNECTION, "unfreeze twice");
    IViewObjectEx_SetAdvise(vo, 0, 0, NULL);

    SetRect(&rect, 0, 0, 100, 100);
    hr = IViewObjectEx_QueryHitPoint(vo, DVASPECT_CONTENT, &rect, pt, 0, &hit);
    CHECK(hr == S_OK && hit == HITRESULT_HIT, "hit point %08lx %lu", hr, hit);
    pt.x = 300;
    hr = IViewObjectEx_QueryHitPoint(vo, DVASPECT_CONTENT, &rect, pt, 0, &hit);
    CHECK(hr == S_OK && hit == HITRESULT_OUTSIDE, "miss point %08lx %lu", hr, hit);
    CHECK(IViewObjectEx_QueryHitPoint(vo, DVASPECT_ICON, &rect, pt, 0, &hit) == E_FAIL, "hit point icon");
    SetRect(&rect, 10, 10, 20, 20);
    hr = IViewObjectEx_QueryHitRect(vo, DVASPECT_CONTENT, NULL, &rect, 0, &hit);
    CHECK(hr == S_OK && hit == HITRESULT_HIT, "hit rect %08lx", hr);
    SetRect(&rect, ext2.cx + 10, 0, ext2.cx + 20, 10);
    hr = IViewObjectEx_QueryHitRect(vo, DVASPECT_CONTENT, NULL, &rect, 0, &hit);
    CHECK(hr == S_OK && hit == HITRESULT_OUTSIDE, "miss rect %08lx", hr);

    natural.cx = natural.cy = 7;
    hr = IViewObjectEx_GetNaturalExtent(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &natural);
    CHECK(hr == E_FAIL && natural.cx == -1 && natural.cy == -1, "natural extent %08lx", hr);

    /* drawing */
    dc = CreateCompatibleDC(NULL);
    bmp = CreateCompatibleBitmap(GetDC(NULL), 100, 100);
    old = SelectObject(dc, bmp);
    SetPixel(dc, 70, 30, RGB(1, 2, 3));
    set_html(u, L"");
    hr = IViewObjectEx_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &bounds, NULL, NULL, 0);
    px = GetPixel(dc, 70, 30);
    CHECK(hr == S_OK && px == GetSysColor(COLOR_WINDOW), "draw %08lx pixel %06lx", hr, px);
    CHECK(IViewObjectEx_Draw(vo, DVASPECT_ICON, -1, NULL, NULL, NULL, dc, &bounds, NULL, NULL, 0) == DV_E_DVASPECT, "draw icon");
    CHECK(IViewObjectEx_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, &bounds, NULL, NULL, 0) == E_INVALIDARG, "draw without dc");
    CHECK(IViewObjectEx_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, NULL, NULL, NULL, 0) == E_INVALIDARG, "draw without bounds when inactive");

    /* in-place object */
    {
        HWND wnd = (HWND)5;
        hr = IOleInPlaceObjectWindowless_GetWindow(ip, &wnd);
        CHECK(hr == E_FAIL && !wnd, "windowless %08lx", hr);
    }
    CHECK(IOleInPlaceObjectWindowless_SetObjectRects(ip, NULL, &clip) == E_INVALIDARG, "rects null");
    CHECK(IOleInPlaceObjectWindowless_SetObjectRects(ip, &pos, &clip) == S_OK, "rects");
    IOleObject_SetClientSite(oo, &site);
    IOleObject_DoVerb(oo, OLEIVERB_INPLACEACTIVATE, NULL, &site, 0, NULL, NULL);
    SetPixel(dc, 20, 20, RGB(1, 2, 3));
    SetPixel(dc, 70, 30, RGB(1, 2, 3));
    hr = IViewObjectEx_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, NULL, NULL, NULL, 0);
    CHECK(hr == S_OK && GetPixel(dc, 20, 20) == GetSysColor(COLOR_WINDOW) && GetPixel(dc, 70, 30) == RGB(1, 2, 3),
          "windowless draw into the position rectangle %08lx", hr);
    CHECK(IOleInPlaceObjectWindowless_ContextSensitiveHelp(ip, TRUE) == S_OK, "help");
    CHECK(IOleInPlaceObjectWindowless_ReactivateAndUndo(ip) == INPLACE_E_NOTUNDOABLE, "undo");
    {
        LRESULT lr = 0;
        CHECK(IOleInPlaceObjectWindowless_OnWindowMessage(ip, WM_MOUSEMOVE, 0, 0, &lr) == S_FALSE, "message");
    }
    shown = -1;
    CHECK(IOleInPlaceObjectWindowless_UIDeactivate(ip) == S_OK, "ui deactivate");
    CHECK(IOleInPlaceObjectWindowless_InPlaceDeactivate(ip) == S_OK && shown == 0, "inplace deactivate");
    CHECK(IViewObjectEx_Draw(vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, NULL, NULL, NULL, 0) == E_INVALIDARG, "inactive draw without bounds");

    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    IOleObject_SetClientSite(oo, NULL);
    IOleInPlaceObjectWindowless_Release(ip);
    IOleObject_Release(oo);
    IViewObjectEx_Release(vo);
    IUnknown_Release(u);
}

static void test_active_object(void)
{
    IUnknown *u = create();
    IOleInPlaceActiveObject *ao;
    MSG msg = { 0 };

    IUnknown_QueryInterface(u, &IID_IOleInPlaceActiveObject, (void **)&ao);
    CHECK(IOleInPlaceActiveObject_TranslateAccelerator(ao, &msg) == S_FALSE, "accelerator");
    CHECK(IOleInPlaceActiveObject_OnFrameWindowActivate(ao, TRUE) == S_OK, "frame activate");
    CHECK(IOleInPlaceActiveObject_OnDocWindowActivate(ao, FALSE) == S_OK, "doc activate");
    CHECK(IOleInPlaceActiveObject_ResizeBorder(ao, NULL, NULL, TRUE) == S_OK, "resize border");
    CHECK(IOleInPlaceActiveObject_EnableModeless(ao, TRUE) == S_OK, "modeless");
    IOleInPlaceActiveObject_Release(ao);
    IUnknown_Release(u);
}

static void test_connections(void)
{
    IUnknown *u = create();
    IConnectionPointContainer *cpc;
    IEnumConnectionPoints *ecp, *clone;
    IConnectionPoint *pts[2], *cp;
    IEnumConnections *ec;
    CONNECTDATA cd[4];
    DWORD c1, c2;
    ULONG n;
    IID iid;
    HRESULT hr;

    IUnknown_QueryInterface(u, &IID_IConnectionPointContainer, (void **)&cpc);
    CHECK(IConnectionPointContainer_EnumConnectionPoints(cpc, NULL) == E_POINTER, "null enum");
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
    CHECK(hr == S_OK, "enum points %08lx", hr);
    hr = IEnumConnectionPoints_Next(ecp, 2, pts, &n);
    CHECK(hr == S_FALSE && n == 1, "next %08lx %lu", hr, n);
    IConnectionPoint_GetConnectionInterface(pts[0], &iid);
    CHECK(IsEqualGUID(&iid, &DIID_Events), "event interface");
    IConnectionPoint_Release(pts[0]);
    hr = IEnumConnectionPoints_Next(ecp, 1, pts, &n);
    CHECK(hr == S_FALSE && n == 0, "exhausted %08lx", hr);
    CHECK(IEnumConnectionPoints_Reset(ecp) == S_OK, "reset");
    CHECK(IEnumConnectionPoints_Skip(ecp, 1) == S_OK, "skip");
    CHECK(IEnumConnectionPoints_Skip(ecp, 1) == S_FALSE, "skip past the end");
    hr = IEnumConnectionPoints_Clone(ecp, &clone);
    CHECK(hr == S_OK, "clone");
    hr = IEnumConnectionPoints_Next(clone, 1, pts, &n);
    CHECK(hr == S_FALSE && n == 0, "clone keeps the position");
    IEnumConnectionPoints_Reset(clone);
    hr = IEnumConnectionPoints_Next(clone, 1, pts, &n);
    CHECK(hr == S_OK && n == 1, "clone after reset");
    IConnectionPoint_Release(pts[0]);
    IEnumConnectionPoints_Release(clone);
    IEnumConnectionPoints_Release(ecp);

    IConnectionPointContainer_FindConnectionPoint(cpc, &DIID_Events, &cp);
    CHECK(IConnectionPoint_EnumConnections(cp, NULL) == E_POINTER, "null connections");
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECK(hr == S_OK, "enum empty %08lx", hr);
    hr = IEnumConnections_Next(ec, 4, cd, &n);
    CHECK(hr == S_FALSE && n == 0, "no connections %08lx %lu", hr, n);
    IEnumConnections_Release(ec);

    IConnectionPoint_Advise(cp, (IUnknown *)&ev_a, &c1);
    IConnectionPoint_Advise(cp, (IUnknown *)&ev_b, &c2);
    IConnectionPoint_EnumConnections(cp, &ec);
    hr = IEnumConnections_Next(ec, 4, cd, &n);
    CHECK(hr == S_FALSE && n == 2, "two connections %08lx %lu", hr, n);
    CHECK(n == 2 && cd[0].dwCookie == c1 && cd[1].dwCookie == c2 && cd[0].pUnk && cd[1].pUnk, "cookies");
    if (n == 2) { IUnknown_Release(cd[0].pUnk); IUnknown_Release(cd[1].pUnk); }
    IEnumConnections_Release(ec);
    IConnectionPoint_Unadvise(cp, c1);
    IConnectionPoint_EnumConnections(cp, &ec);
    hr = IEnumConnections_Next(ec, 4, cd, &n);
    CHECK(n == 1 && cd[0].dwCookie == c2, "one connection left %lu", n);
    if (n == 1) IUnknown_Release(cd[0].pUnk);
    IEnumConnections_Reset(ec);
    CHECK(IEnumConnections_Skip(ec, 1) == S_OK, "skip connection");
    IEnumConnections_Release(ec);
    IConnectionPoint_Unadvise(cp, c2);

    IConnectionPoint_Release(cp);
    IConnectionPointContainer_Release(cpc);
    IUnknown_Release(u);
}

int main(void)
{
    CoInitialize(NULL);
    test_class_info();
    test_storage();
    test_ole_object();
    test_data_object();
    test_view_object();
    test_active_object();
    test_connections();
    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
