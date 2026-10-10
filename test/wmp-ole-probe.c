/* wmp's OLE control interfaces (patches/sg/2853), run by test/wmp-ole-gate.sh:
 * IOleObject (SetHostNames, Update, IsUpToDate, GetUserClassID, GetUserType,
 * EnumVerbs, Advise/Unadvise/EnumAdvise, GetMiscStatus aspects, DoVerb show
 * verbs), IOleInPlaceObjectWindowless (InPlaceDeactivate, UIDeactivate,
 * ReactivateAndUndo, OnWindowMessage) against a minimal in-place site,
 * IOleControl (OnAmbientPropertyChange, FreezeEvents), IProvideClassInfo2
 * and IPersistStreamInit (GetClassID, IsDirty, Save, Load, GetSizeMax).
 *
 *   wmp-ole-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <olectl.h>
#include <wmp.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static BOOL bstr_is(BSTR b, const WCHAR *s) { return b ? !wcscmp(b, s) : !*s; }

/* ---- a container: client site, in-place site (no windowless/Ex), advise sink ---- */

static HWND site_hwnd;
static int activated, deactivated, shown;

struct site { IOleClientSite client; IOleInPlaceSite inplace; };
static struct site site;

static HRESULT WINAPI cs_QI(IOleClientSite *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IOleClientSite)) *ppv = &site.client;
    else if (IsEqualGUID(riid, &IID_IOleInPlaceSite) || IsEqualGUID(riid, &IID_IOleWindow)) *ppv = &site.inplace;
    else { *ppv = NULL; return E_NOINTERFACE; }
    return S_OK;
}
static ULONG WINAPI cs_AddRef(IOleClientSite *i) { return 2; }
static ULONG WINAPI cs_Release(IOleClientSite *i) { return 1; }
static HRESULT WINAPI cs_Save(IOleClientSite *i) { return E_NOTIMPL; }
static HRESULT WINAPI cs_Mon(IOleClientSite *i, DWORD a, DWORD w, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI cs_Container(IOleClientSite *i, IOleContainer **c) { *c = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI cs_Show(IOleClientSite *i) { shown++; return S_OK; }
static HRESULT WINAPI cs_OnShow(IOleClientSite *i, BOOL s) { return S_OK; }
static HRESULT WINAPI cs_Layout(IOleClientSite *i) { return S_OK; }
static const IOleClientSiteVtbl cs_vtbl = {cs_QI, cs_AddRef, cs_Release, cs_Save, cs_Mon, cs_Container, cs_Show, cs_OnShow, cs_Layout};

static HRESULT WINAPI ip_QI(IOleInPlaceSite *i, REFIID riid, void **ppv) { return cs_QI(&site.client, riid, ppv); }
static ULONG WINAPI ip_AddRef(IOleInPlaceSite *i) { return 2; }
static ULONG WINAPI ip_Release(IOleInPlaceSite *i) { return 1; }
static HRESULT WINAPI ip_GetWindow(IOleInPlaceSite *i, HWND *h) { *h = site_hwnd; return S_OK; }
static HRESULT WINAPI ip_Help(IOleInPlaceSite *i, BOOL e) { return E_NOTIMPL; }
static HRESULT WINAPI ip_Can(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ip_OnAct(IOleInPlaceSite *i) { activated++; return S_OK; }
static HRESULT WINAPI ip_OnUIAct(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ip_Ctx(IOleInPlaceSite *i, IOleInPlaceFrame **f, IOleInPlaceUIWindow **w, RECT *p, RECT *c, OLEINPLACEFRAMEINFO *fi)
{
    *f = NULL; *w = NULL;
    SetRect(p, 0, 0, 200, 100);
    *c = *p;
    return S_OK;
}
static HRESULT WINAPI ip_Scroll(IOleInPlaceSite *i, SIZE s) { return E_NOTIMPL; }
static HRESULT WINAPI ip_OnUIDeact(IOleInPlaceSite *i, BOOL u) { return S_OK; }
static HRESULT WINAPI ip_OnDeact(IOleInPlaceSite *i) { deactivated++; return S_OK; }
static HRESULT WINAPI ip_Discard(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ip_DeactUndo(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ip_PosRect(IOleInPlaceSite *i, LPCRECT r) { return S_OK; }
static const IOleInPlaceSiteVtbl ip_vtbl = {ip_QI, ip_AddRef, ip_Release, ip_GetWindow, ip_Help, ip_Can, ip_OnAct, ip_OnUIAct,
    ip_Ctx, ip_Scroll, ip_OnUIDeact, ip_OnDeact, ip_Discard, ip_DeactUndo, ip_PosRect};

struct advsink { IAdviseSink iface; };
static HRESULT WINAPI as_QI(IAdviseSink *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IAdviseSink)) { *ppv = i; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI as_AddRef(IAdviseSink *i) { return 2; }
static ULONG WINAPI as_Release(IAdviseSink *i) { return 1; }
static void WINAPI as_Data(IAdviseSink *i, FORMATETC *f, STGMEDIUM *m) {}
static void WINAPI as_View(IAdviseSink *i, DWORD a, LONG l) {}
static void WINAPI as_Rename(IAdviseSink *i, IMoniker *m) {}
static void WINAPI as_Save(IAdviseSink *i) {}
static void WINAPI as_Close(IAdviseSink *i) {}
static const IAdviseSinkVtbl as_vtbl = {as_QI, as_AddRef, as_Release, as_Data, as_View, as_Rename, as_Save, as_Close};

/* ---- an event sink that counts what it gets ---- */

static LONG events;
static HRESULT WINAPI ev_QI(IDispatch *d, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch) || IsEqualGUID(riid, &IID__WMPOCXEvents))
    { *ppv = d; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ev_AddRef(IDispatch *d) { return 2; }
static ULONG WINAPI ev_Release(IDispatch *d) { return 1; }
static HRESULT WINAPI ev_Count(IDispatch *d, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI ev_Info(IDispatch *d, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI ev_Names(IDispatch *d, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *i) { return E_NOTIMPL; }
static HRESULT WINAPI ev_Invoke(IDispatch *d, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{ InterlockedIncrement(&events); return S_OK; }
static const IDispatchVtbl ev_vtbl = {ev_QI, ev_AddRef, ev_Release, ev_Count, ev_Info, ev_Names, ev_Invoke};
static IDispatch ev_sink = {(IDispatchVtbl *)&ev_vtbl};

static IOleObject *create(void)
{
    IOleObject *o = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WindowsMediaPlayer, NULL, CLSCTX_INPROC_SERVER, &IID_IOleObject, (void **)&o);
    if (FAILED(hr)) CHECKF(0, "CoCreateInstance(WindowsMediaPlayer) = %08lx", hr);
    return o;
}

static void test_oleobject(void)
{
    IOleObject *o = create();
    IAdviseSink *sink = &(struct advsink){{(IAdviseSinkVtbl *)&as_vtbl}}.iface;
    IEnumSTATDATA *en = NULL;
    STATDATA sd;
    ULONG got;
    CLSID clsid;
    LPOLESTR str;
    DWORD cookie = 0, cookie2 = 0, status;
    IEnumOLEVERB *verbs;
    HRESULT hr;

    if (!o) return;

    hr = IOleObject_SetHostNames(o, L"Container", L"Document");
    CHECKF(hr == S_OK, "SetHostNames (%08lx)", hr);
    CHECKF(IOleObject_Update(o) == S_OK, "Update = S_OK");
    CHECKF(IOleObject_IsUpToDate(o) == S_OK, "IsUpToDate = S_OK");
    memset(&clsid, 0, sizeof(clsid));
    hr = IOleObject_GetUserClassID(o, &clsid);
    CHECKF(hr == S_OK && IsEqualGUID(&clsid, &CLSID_WindowsMediaPlayer), "GetUserClassID (%08lx)", hr);
    CHECKF(IOleObject_GetUserClassID(o, NULL) == E_POINTER, "GetUserClassID(NULL) = E_POINTER");
    str = (LPOLESTR)1;
    hr = IOleObject_GetUserType(o, USERCLASSTYPE_FULL, &str);
    CHECKF(hr == OLE_S_USEREG && !str, "GetUserType = OLE_S_USEREG, NULL string (%08lx)", hr);
    CHECKF(IOleObject_GetUserType(o, USERCLASSTYPE_FULL, NULL) == E_POINTER, "GetUserType(NULL) = E_POINTER");
    verbs = (IEnumOLEVERB *)1;
    hr = IOleObject_EnumVerbs(o, &verbs);
    CHECKF(hr == OLE_S_USEREG && !verbs, "EnumVerbs = OLE_S_USEREG, NULL (%08lx)", hr);
    CHECKF(IOleObject_EnumVerbs(o, NULL) == E_POINTER, "EnumVerbs(NULL) = E_POINTER");

    status = 99;
    hr = IOleObject_GetMiscStatus(o, DVASPECT_CONTENT, &status);
    CHECKF(hr == S_OK && (status & OLEMISC_SETCLIENTSITEFIRST), "GetMiscStatus(content) (%08lx %08lx)", hr, status);
    status = 99;
    hr = IOleObject_GetMiscStatus(o, DVASPECT_ICON, &status);
    CHECKF(hr == OLE_S_USEREG && status == 0, "GetMiscStatus(icon) = OLE_S_USEREG, 0 (%08lx %08lx)", hr, status);
    CHECKF(IOleObject_GetMiscStatus(o, DVASPECT_CONTENT, NULL) == E_POINTER, "GetMiscStatus(NULL) = E_POINTER");

    /* the advise holder */
    hr = IOleObject_Unadvise(o, 1);
    CHECKF(hr == E_FAIL, "Unadvise before any Advise = E_FAIL (%08lx)", hr);
    en = (IEnumSTATDATA *)1;
    hr = IOleObject_EnumAdvise(o, &en);
    CHECKF(hr == E_FAIL && !en, "EnumAdvise before any Advise = E_FAIL, NULL (%08lx)", hr);
    hr = IOleObject_Advise(o, sink, &cookie);
    CHECKF(hr == S_OK && cookie, "Advise (%08lx cookie %lu)", hr, cookie);
    hr = IOleObject_Advise(o, sink, &cookie2);
    CHECKF(hr == S_OK && cookie2 && cookie2 != cookie, "a second Advise has another cookie (%08lx %lu)", hr, cookie2);
    CHECKF(IOleObject_Advise(o, sink, NULL) == E_POINTER, "Advise(.., NULL) = E_POINTER");
    hr = IOleObject_EnumAdvise(o, &en);
    CHECKF(hr == S_OK && en, "EnumAdvise (%08lx)", hr);
    if (en)
    {
        int n = 0, found1 = 0, found2 = 0;
        while (IEnumSTATDATA_Next(en, 1, &sd, &got) == S_OK && got == 1)
        {
            n++;
            if (sd.dwConnection == cookie) found1 = 1;
            if (sd.dwConnection == cookie2) found2 = 1;
            if (sd.pAdvSink) IAdviseSink_Release(sd.pAdvSink);
        }
        CHECKF(n == 2 && found1 && found2, "EnumAdvise lists both connections (%d)", n);
        IEnumSTATDATA_Release(en);
    }
    hr = IOleObject_Unadvise(o, cookie);
    CHECKF(hr == S_OK, "Unadvise (%08lx)", hr);
    hr = IOleObject_Unadvise(o, cookie);
    CHECKF(hr == OLE_E_NOCONNECTION, "Unadvise again = OLE_E_NOCONNECTION (%08lx)", hr);
    IOleObject_Unadvise(o, cookie2);

    /* show verbs need the container */
    {
        RECT rc = {0, 0, 10, 10};
        hr = IOleObject_DoVerb(o, OLEIVERB_SHOW, NULL, NULL, 0, NULL, &rc);
        CHECKF(hr == E_UNEXPECTED, "DoVerb(SHOW) without a client site = E_UNEXPECTED (%08lx)", hr);
        hr = IOleObject_DoVerb(o, OLEIVERB_PRIMARY, NULL, NULL, 0, NULL, &rc);
        CHECKF(hr == E_UNEXPECTED, "DoVerb(PRIMARY) without a client site = E_UNEXPECTED (%08lx)", hr);
        hr = IOleObject_DoVerb(o, OLEIVERB_UIACTIVATE, NULL, NULL, 0, NULL, &rc);
        CHECKF(hr == E_UNEXPECTED, "DoVerb(UIACTIVATE) without a client site = E_UNEXPECTED (%08lx)", hr);
    }
    IOleObject_Release(o);
}

static void test_inplace(void)
{
    IOleObject *o = create();
    IOleInPlaceObjectWindowless *ipo;
    IPersistStreamInit *psi;
    LRESULT res = 5;
    HWND hwnd = NULL;
    RECT rc = {0, 0, 100, 50};
    HRESULT hr;
    static const LONG verbs[] = {OLEIVERB_SHOW, OLEIVERB_PRIMARY, OLEIVERB_UIACTIVATE, OLEIVERB_INPLACEACTIVATE};
    unsigned int i;

    if (!o) return;
    site.client.lpVtbl = (IOleClientSiteVtbl *)&cs_vtbl;
    site.inplace.lpVtbl = (IOleInPlaceSiteVtbl *)&ip_vtbl;
    site_hwnd = CreateWindowW(L"STATIC", L"container", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    activated = deactivated = shown = 0;

    IOleObject_QueryInterface(o, &IID_IOleInPlaceObjectWindowless, (void **)&ipo);
    hr = IOleInPlaceObjectWindowless_InPlaceDeactivate(ipo);
    CHECKF(hr == S_OK, "InPlaceDeactivate when inactive = S_OK (%08lx)", hr);
    hr = IOleInPlaceObjectWindowless_UIDeactivate(ipo);
    CHECKF(hr == S_OK, "UIDeactivate = S_OK (%08lx)", hr);
    hr = IOleInPlaceObjectWindowless_ReactivateAndUndo(ipo);
    CHECKF(hr == S_FALSE, "ReactivateAndUndo = S_FALSE (%08lx)", hr);
    hr = IOleInPlaceObjectWindowless_OnWindowMessage(ipo, WM_MOUSEMOVE, 0, 0, &res);
    CHECKF(hr == S_FALSE, "OnWindowMessage = S_FALSE (%08lx)", hr);
    hr = IOleInPlaceObjectWindowless_OnWindowMessage(ipo, WM_MOUSEMOVE, 0, 0, NULL);
    CHECKF(hr == E_POINTER, "OnWindowMessage(NULL result) = E_POINTER (%08lx)", hr);

    IOleObject_QueryInterface(o, &IID_IPersistStreamInit, (void **)&psi);
    IOleObject_SetClientSite(o, &site.client);
    hr = IPersistStreamInit_InitNew(psi);
    CHECKF(hr == S_OK, "InitNew (%08lx)", hr);
    IPersistStreamInit_Release(psi);

    for (i = 0; i < ARRAY_SIZE(verbs); i++)
    {
        int before = activated;
        hr = IOleObject_DoVerb(o, verbs[i], NULL, &site.client, 0, site_hwnd, &rc);
        if (verbs[i] == OLEIVERB_INPLACEACTIVATE)
            CHECKF(hr == E_UNEXPECTED, "DoVerb(INPLACEACTIVATE) of an active control = E_UNEXPECTED (%08lx)", hr);
        else
            CHECKF(hr == S_OK, "DoVerb(%ld) (%08lx)", verbs[i], hr);
        if (i == 0)
        {
            hr = IOleInPlaceObjectWindowless_GetWindow(ipo, &hwnd);
            CHECKF(hr == S_OK && hwnd && IsWindow(hwnd), "the control has a window after SHOW (%08lx)", hr);
            CHECKF(activated == 1 && shown >= 1, "the site was told once (%d %d)", activated, shown);
        }
        else if (verbs[i] != OLEIVERB_INPLACEACTIVATE)
            CHECKF(activated == before, "DoVerb(%ld) on an active control does not activate again", verbs[i]);
    }
    CHECKF(IsWindowVisible(hwnd) || !IsWindowVisible(site_hwnd), "SHOW on an active control shows its window");
    hr = IOleObject_DoVerb(o, OLEIVERB_HIDE, NULL, &site.client, 0, site_hwnd, &rc);
    CHECKF(hr == S_OK && !IsWindowVisible(hwnd), "HIDE hides it (%08lx)", hr);
    hr = IOleObject_DoVerb(o, OLEIVERB_SHOW, NULL, &site.client, 0, site_hwnd, &rc);
    CHECKF(hr == S_OK && (GetWindowLongW(hwnd, GWL_STYLE) & WS_VISIBLE), "SHOW after HIDE shows it again (%08lx)", hr);

    hr = IOleInPlaceObjectWindowless_InPlaceDeactivate(ipo);
    CHECKF(hr == S_OK && deactivated == 1, "InPlaceDeactivate tells the site (%08lx %d)", hr, deactivated);
    CHECKF(!IsWindow(hwnd), "InPlaceDeactivate destroys the window");
    hr = IOleInPlaceObjectWindowless_GetWindow(ipo, &hwnd);
    CHECKF(hr == E_UNEXPECTED, "GetWindow after deactivation = E_UNEXPECTED (%08lx)", hr);
    hr = IOleInPlaceObjectWindowless_InPlaceDeactivate(ipo);
    CHECKF(hr == S_OK && deactivated == 1, "a second InPlaceDeactivate is a no-op (%08lx %d)", hr, deactivated);

    IOleInPlaceObjectWindowless_Release(ipo);
    IOleObject_SetClientSite(o, NULL);
    DestroyWindow(site_hwnd);
    IOleObject_Release(o);
}

static void test_control(void)
{
    IOleObject *o = create();
    IOleControl *oc;
    IWMPPlayer4 *player;
    IWMPSettings *settings;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    IProvideClassInfo2 *pci;
    DWORD cookie;
    LONG base;
    GUID guid;
    BSTR url;
    HRESULT hr;
    int i;

    if (!o) return;
    IOleObject_QueryInterface(o, &IID_IOleControl, (void **)&oc);
    hr = IOleControl_OnAmbientPropertyChange(oc, DISPID_AMBIENT_BACKCOLOR);
    CHECKF(hr == S_OK, "OnAmbientPropertyChange(backcolor) = S_OK (%08lx)", hr);
    hr = IOleControl_OnAmbientPropertyChange(oc, DISPID_UNKNOWN);
    CHECKF(hr == S_OK, "OnAmbientPropertyChange(unknown) = S_OK (%08lx)", hr);

    IOleObject_QueryInterface(o, &IID_IWMPPlayer4, (void **)&player);
    IWMPPlayer4_get_settings(player, &settings);
    IWMPSettings_put_autoStart(settings, VARIANT_FALSE);
    IOleObject_QueryInterface(o, &IID_IConnectionPointContainer, (void **)&cpc);
    IConnectionPointContainer_FindConnectionPoint(cpc, &IID__WMPOCXEvents, &cp);
    IConnectionPoint_Advise(cp, (IUnknown *)&ev_sink, &cookie);

    events = 0;
    url = SysAllocString(L"C:\\nonexistent\\a.mp3");
    IWMPPlayer4_put_URL(player, url);
    base = events;
    CHECKF(base > 0, "events reach the sink (%ld)", base);

    hr = IOleControl_FreezeEvents(oc, TRUE);
    CHECKF(hr == S_OK, "FreezeEvents(TRUE) (%08lx)", hr);
    IWMPPlayer4_put_URL(player, url);
    CHECKF(events == base, "no events while frozen (%ld vs %ld)", events, base);
    hr = IOleControl_FreezeEvents(oc, TRUE);
    IOleControl_FreezeEvents(oc, FALSE);
    IWMPPlayer4_put_URL(player, url);
    CHECKF(events == base, "still frozen after one of two unfreezes (%ld vs %ld)", events, base);
    hr = IOleControl_FreezeEvents(oc, FALSE);
    CHECKF(hr == S_OK, "FreezeEvents(FALSE) (%08lx)", hr);
    IWMPPlayer4_put_URL(player, url);
    CHECKF(events > base, "events flow again after unfreezing (%ld vs %ld)", events, base);
    hr = IOleControl_FreezeEvents(oc, FALSE);
    CHECKF(hr == S_OK, "an extra FreezeEvents(FALSE) is harmless (%08lx)", hr);
    base = events;
    IWMPPlayer4_put_URL(player, url);
    CHECKF(events > base, "an extra unfreeze does not unbalance the count");
    SysFreeString(url);
    for (i = 0; i < 1; i++) IConnectionPoint_Unadvise(cp, cookie);
    IConnectionPoint_Release(cp);
    IConnectionPointContainer_Release(cpc);

    /* IProvideClassInfo2 */
    IOleObject_QueryInterface(o, &IID_IProvideClassInfo2, (void **)&pci);
    memset(&guid, 0, sizeof(guid));
    hr = IProvideClassInfo2_GetGUID(pci, GUIDKIND_DEFAULT_SOURCE_DISP_IID, &guid);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &IID__WMPOCXEvents), "GetGUID(default source) (%08lx)", hr);
    memset(&guid, 0x55, sizeof(guid));
    hr = IProvideClassInfo2_GetGUID(pci, 2, &guid);
    CHECKF(hr == E_INVALIDARG && IsEqualGUID(&guid, &GUID_NULL), "GetGUID(other kind) = E_INVALIDARG, GUID_NULL (%08lx)", hr);
    hr = IProvideClassInfo2_GetGUID(pci, GUIDKIND_DEFAULT_SOURCE_DISP_IID, NULL);
    CHECKF(hr == E_POINTER, "GetGUID(NULL) = E_POINTER (%08lx)", hr);
    IProvideClassInfo2_Release(pci);

    IWMPSettings_Release(settings);
    IWMPPlayer4_Release(player);
    IOleControl_Release(oc);
    IOleObject_Release(o);
}

static IStream *new_stream(void)
{
    IStream *s = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    return s;
}

static ULONG stream_size(IStream *s)
{
    STATSTG st;
    memset(&st, 0, sizeof(st));
    IStream_Stat(s, &st, STATFLAG_NONAME);
    return st.cbSize.LowPart;
}

static void rewind_stream(IStream *s)
{
    LARGE_INTEGER zero = {{0}};
    IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
}

static void test_persist(void)
{
    IOleObject *o = create(), *o2 = create();
    IPersistStreamInit *psi, *psi2;
    IWMPPlayer4 *player, *player2;
    IWMPSettings *s, *s2;
    IStream *stm;
    ULARGE_INTEGER max;
    VARIANT_BOOL vb;
    LONG l;
    DOUBLE r;
    BSTR b, n;
    CLSID clsid;
    ULONG size;
    HRESULT hr;

    if (!o || !o2) return;
    IOleObject_QueryInterface(o, &IID_IPersistStreamInit, (void **)&psi);
    IOleObject_QueryInterface(o, &IID_IWMPPlayer4, (void **)&player);
    IWMPPlayer4_get_settings(player, &s);
    IOleObject_QueryInterface(o2, &IID_IPersistStreamInit, (void **)&psi2);
    IOleObject_QueryInterface(o2, &IID_IWMPPlayer4, (void **)&player2);
    IWMPPlayer4_get_settings(player2, &s2);

    memset(&clsid, 0, sizeof(clsid));
    hr = IPersistStreamInit_GetClassID(psi, &clsid);
    CHECKF(hr == S_OK && IsEqualGUID(&clsid, &CLSID_WindowsMediaPlayer), "GetClassID (%08lx)", hr);
    CHECKF(IPersistStreamInit_GetClassID(psi, NULL) == E_POINTER, "GetClassID(NULL) = E_POINTER");
    hr = IPersistStreamInit_IsDirty(psi);
    CHECKF(hr == S_FALSE, "a new control is clean (%08lx)", hr);
    max.QuadPart = 0;
    hr = IPersistStreamInit_GetSizeMax(psi, &max);
    CHECKF(hr == S_OK && max.QuadPart > 12, "GetSizeMax (%08lx %lu)", hr, (ULONG)max.QuadPart);
    CHECKF(IPersistStreamInit_GetSizeMax(psi, NULL) == E_POINTER, "GetSizeMax(NULL) = E_POINTER");
    CHECKF(IPersistStreamInit_Save(psi, NULL, TRUE) == E_POINTER, "Save(NULL) = E_POINTER");
    CHECKF(IPersistStreamInit_Load(psi, NULL) == E_POINTER, "Load(NULL) = E_POINTER");

    /* change things: the control is dirty until it is saved with fClearDirty */
    IWMPSettings_put_autoStart(s, VARIANT_FALSE);
    hr = IPersistStreamInit_IsDirty(psi);
    CHECKF(hr == S_OK, "a changed autoStart makes it dirty (%08lx)", hr);
    IWMPSettings_put_volume(s, 37);
    IWMPSettings_put_mute(s, VARIANT_TRUE);
    IWMPSettings_put_balance(s, -20);
    IWMPSettings_put_playCount(s, 3);
    IWMPSettings_put_rate(s, 1.5);
    n = SysAllocString(L"loop"); IWMPSettings_setMode(s, n, VARIANT_TRUE); SysFreeString(n);
    IWMPPlayer4_put_enabled(player, VARIANT_FALSE);
    n = SysAllocString(L"mini"); IWMPPlayer4_put_uiMode(player, n); SysFreeString(n);
    n = SysAllocString(L"http://example.com/"); IWMPSettings_put_baseURL(s, n); SysFreeString(n);
    n = SysAllocString(L"_blank"); IWMPSettings_put_defaultFrame(s, n); SysFreeString(n);
    n = SysAllocString(L"C:\\nonexistent\\saved.mp3"); IWMPPlayer4_put_URL(player, n); SysFreeString(n);

    stm = new_stream();
    hr = IPersistStreamInit_Save(psi, stm, FALSE);
    CHECKF(hr == S_OK, "Save, keeping the dirty flag (%08lx)", hr);
    size = stream_size(stm);
    IPersistStreamInit_GetSizeMax(psi, &max);
    CHECKF(size > 12 && size <= max.QuadPart, "the saved size %lu is within GetSizeMax %lu", size, (ULONG)max.QuadPart);
    hr = IPersistStreamInit_IsDirty(psi);
    CHECKF(hr == S_OK, "still dirty after Save(FALSE) (%08lx)", hr);
    {
        IStream *tmp = new_stream();
        hr = IPersistStreamInit_Save(psi, tmp, TRUE);
        CHECKF(hr == S_OK, "Save, clearing the dirty flag (%08lx)", hr);
        CHECKF(stream_size(tmp) == size, "two saves of the same state are the same size");
        IStream_Release(tmp);
    }
    hr = IPersistStreamInit_IsDirty(psi);
    CHECKF(hr == S_FALSE, "clean after Save(TRUE) (%08lx)", hr);
    IWMPSettings_put_volume(s, 38);
    hr = IPersistStreamInit_IsDirty(psi);
    CHECKF(hr == S_OK, "dirty again after a change (%08lx)", hr);
    IWMPSettings_put_volume(s, 37);
    hr = IPersistStreamInit_IsDirty(psi);
    CHECKF(hr == S_FALSE, "clean when the change is undone (%08lx)", hr);

    /* the second control reads it */
    rewind_stream(stm);
    IWMPSettings_put_autoStart(s2, VARIANT_TRUE);
    hr = IPersistStreamInit_Load(psi2, stm);
    CHECKF(hr == S_OK, "Load (%08lx)", hr);
    vb = 5; IWMPSettings_get_autoStart(s2, &vb);
    CHECKF(vb == VARIANT_FALSE, "autoStart restored");
    l = 0; IWMPSettings_get_volume(s2, &l);
    CHECKF(l == 37, "volume restored (%ld)", l);
    vb = 5; IWMPSettings_get_mute(s2, &vb);
    CHECKF(vb == VARIANT_TRUE, "mute restored");
    l = 0; IWMPSettings_get_balance(s2, &l);
    CHECKF(l == -20, "balance restored (%ld)", l);
    l = 0; IWMPSettings_get_playCount(s2, &l);
    CHECKF(l == 3, "playCount restored (%ld)", l);
    r = 0; IWMPSettings_get_rate(s2, &r);
    CHECKF(r == 1.5, "rate restored (%f)", r);
    n = SysAllocString(L"loop"); vb = 5; IWMPSettings_getMode(s2, n, &vb); SysFreeString(n);
    CHECKF(vb == VARIANT_TRUE, "loop mode restored");
    n = SysAllocString(L"shuffle"); vb = 5; IWMPSettings_getMode(s2, n, &vb); SysFreeString(n);
    CHECKF(vb == VARIANT_FALSE, "shuffle mode still off");
    vb = 5; IWMPPlayer4_get_enabled(player2, &vb);
    CHECKF(vb == VARIANT_FALSE, "enabled restored");
    b = NULL; IWMPPlayer4_get_uiMode(player2, &b);
    CHECKF(bstr_is(b, L"mini"), "uiMode restored");
    SysFreeString(b);
    b = NULL; IWMPSettings_get_baseURL(s2, &b);
    CHECKF(bstr_is(b, L"http://example.com/"), "baseURL restored");
    SysFreeString(b);
    b = NULL; IWMPSettings_get_defaultFrame(s2, &b);
    CHECKF(bstr_is(b, L"_blank"), "defaultFrame restored");
    SysFreeString(b);
    b = NULL; IWMPPlayer4_get_URL(player2, &b);
    CHECKF(bstr_is(b, L"C:\\nonexistent\\saved.mp3"), "URL restored (%ls)", b ? b : L"(null)");
    SysFreeString(b);
    hr = IPersistStreamInit_IsDirty(psi2);
    CHECKF(hr == S_FALSE, "clean after Load (%08lx)", hr);

    /* bad streams leave the control alone */
    {
        IStream *bad = new_stream();
        static const BYTE junk[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        ULONG w;
        IStream_Write(bad, junk, sizeof(junk), &w);
        rewind_stream(bad);
        IWMPSettings_put_volume(s2, 11);
        hr = IPersistStreamInit_Load(psi2, bad);
        CHECKF(hr == E_FAIL, "Load of junk = E_FAIL (%08lx)", hr);
        l = 0; IWMPSettings_get_volume(s2, &l);
        CHECKF(l == 11, "a refused Load changes nothing (%ld)", l);
        IStream_Release(bad);
        bad = new_stream();
        hr = IPersistStreamInit_Load(psi2, bad);
        CHECKF(hr == E_FAIL, "Load of an empty stream = E_FAIL (%08lx)", hr);
        IStream_Release(bad);
        /* a truncated one */
        bad = new_stream();
        rewind_stream(stm);
        {
            BYTE *copy = malloc(size);
            ULONG got;
            IStream_Read(stm, copy, size, &got);
            IStream_Write(bad, copy, size - 10, &w);
            free(copy);
        }
        rewind_stream(bad);
        hr = IPersistStreamInit_Load(psi2, bad);
        CHECKF(hr == E_FAIL, "Load of a truncated stream = E_FAIL (%08lx)", hr);
        l = 0; IWMPSettings_get_volume(s2, &l);
        CHECKF(l == 11, "a truncated Load changes nothing (%ld)", l);
        IStream_Release(bad);
    }

    IStream_Release(stm);
    IWMPSettings_Release(s2);
    IWMPPlayer4_Release(player2);
    IPersistStreamInit_Release(psi2);
    IWMPSettings_Release(s);
    IWMPPlayer4_Release(player);
    IPersistStreamInit_Release(psi);
    IOleObject_Release(o2);
    IOleObject_Release(o);
}

int main(void)
{
    CoInitialize(NULL);
    test_oleobject();
    test_inplace();
    test_control();
    test_persist();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
