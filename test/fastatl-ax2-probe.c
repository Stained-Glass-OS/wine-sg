/* atl100: container sites seen from a mock control, MSHTML: and licensed creation, property map persistence */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <olectl.h>
#include <stdio.h>
#include <string.h>
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)

static const GUID clsid_mock = {0x7e5f0001, 0x1111, 0x4111, {0x81, 0x11, 1, 2, 3, 4, 5, 6}};
static const GUID clsid_nopersist = {0x7e5f0002, 0x1111, 0x4111, {0x81, 0x11, 1, 2, 3, 4, 5, 6}};
static const GUID clsid_licensed = {0x7e5f0003, 0x1111, 0x4111, {0x81, 0x11, 1, 2, 3, 4, 5, 6}};
static const GUID clsid_htmldoc = {0x25336920, 0x03f9, 0x11cf, {0x8f, 0xd0, 0x00, 0xaa, 0x00, 0x68, 0x6f, 0x13}};

/* ---------------------------------------------------------------- the mock control */

struct mock
{
    IOleObject IOleObject_iface;
    IPersistStreamInit IPersistStreamInit_iface;
    IOleInPlaceObject IOleInPlaceObject_iface;
    IParseDisplayName IParseDisplayName_iface;
    LONG ref;
    BOOL persist, parser;
    IOleClientSite *site;
};

static struct mock *last_mock;
static int psi_load, psi_initnew, psi_save, psi_save_clear;
static char psi_data[512];
static RECT obj_pos, obj_clip;
static int rects_calls;
static int parse_calls;

static inline struct mock *impl_from_IOleObject(IOleObject *iface) { return CONTAINING_RECORD(iface, struct mock, IOleObject_iface); }
static inline struct mock *impl_from_psi(IPersistStreamInit *iface) { return CONTAINING_RECORD(iface, struct mock, IPersistStreamInit_iface); }
static inline struct mock *impl_from_ipo(IOleInPlaceObject *iface) { return CONTAINING_RECORD(iface, struct mock, IOleInPlaceObject_iface); }
static inline struct mock *impl_from_pdn(IParseDisplayName *iface) { return CONTAINING_RECORD(iface, struct mock, IParseDisplayName_iface); }

static HRESULT WINAPI mock_QI(IOleObject *iface, REFIID riid, void **ppv)
{
    struct mock *This = impl_from_IOleObject(iface);
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleObject)) *ppv = &This->IOleObject_iface;
    else if (IsEqualIID(riid, &IID_IPersistStreamInit) && This->persist) *ppv = &This->IPersistStreamInit_iface;
    else if (IsEqualIID(riid, &IID_IOleInPlaceObject) || IsEqualIID(riid, &IID_IOleWindow)) *ppv = &This->IOleInPlaceObject_iface;
    else if (IsEqualIID(riid, &IID_IParseDisplayName) && This->parser) *ppv = &This->IParseDisplayName_iface;
    if (!*ppv) return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}
static ULONG WINAPI mock_AddRef(IOleObject *iface) { return InterlockedIncrement(&impl_from_IOleObject(iface)->ref); }
static ULONG WINAPI mock_Release(IOleObject *iface)
{
    struct mock *This = impl_from_IOleObject(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    if (!ref)
    {
        if (This->site) IOleClientSite_Release(This->site);
        if (last_mock == This) last_mock = NULL;
        free(This);
    }
    return ref;
}
static HRESULT WINAPI mock_SetClientSite(IOleObject *iface, IOleClientSite *site)
{
    struct mock *This = impl_from_IOleObject(iface);
    if (site) IOleClientSite_AddRef(site);
    if (This->site) IOleClientSite_Release(This->site);
    This->site = site;
    return S_OK;
}
static HRESULT WINAPI mock_GetClientSite(IOleObject *iface, IOleClientSite **site)
{
    struct mock *This = impl_from_IOleObject(iface);
    *site = This->site;
    if (*site) IOleClientSite_AddRef(*site);
    return S_OK;
}
static HRESULT WINAPI mock_SetHostNames(IOleObject *iface, LPCOLESTR a, LPCOLESTR b) { return S_OK; }
static HRESULT WINAPI mock_Close(IOleObject *iface, DWORD flags) { return S_OK; }
static HRESULT WINAPI mock_SetMoniker(IOleObject *iface, DWORD w, IMoniker *m) { return E_NOTIMPL; }
static HRESULT WINAPI mock_GetMoniker(IOleObject *iface, DWORD a, DWORD w, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI mock_InitFromData(IOleObject *iface, IDataObject *d, BOOL c, DWORD r) { return E_NOTIMPL; }
static HRESULT WINAPI mock_GetClipboardData(IOleObject *iface, DWORD r, IDataObject **d) { return E_NOTIMPL; }
static HRESULT WINAPI mock_DoVerb(IOleObject *iface, LONG verb, MSG *msg, IOleClientSite *site, LONG idx, HWND parent, const RECT *r)
{
    IOleInPlaceSite *ips;

    if (verb == OLEIVERB_INPLACEACTIVATE && site && SUCCEEDED(IOleClientSite_QueryInterface(site, &IID_IOleInPlaceSite, (void **)&ips)))
    {
        if (IOleInPlaceSite_CanInPlaceActivate(ips) == S_OK) IOleInPlaceSite_OnInPlaceActivate(ips);
        IOleInPlaceSite_Release(ips);
    }
    return S_OK;
}
static HRESULT WINAPI mock_EnumVerbs(IOleObject *iface, IEnumOLEVERB **e) { return E_NOTIMPL; }
static HRESULT WINAPI mock_Update(IOleObject *iface) { return S_OK; }
static HRESULT WINAPI mock_IsUpToDate(IOleObject *iface) { return S_OK; }
static HRESULT WINAPI mock_GetUserClassID(IOleObject *iface, CLSID *c) { return E_NOTIMPL; }
static HRESULT WINAPI mock_GetUserType(IOleObject *iface, DWORD f, LPOLESTR *t) { return E_NOTIMPL; }
static HRESULT WINAPI mock_SetExtent(IOleObject *iface, DWORD a, SIZEL *s) { return S_OK; }
static HRESULT WINAPI mock_GetExtent(IOleObject *iface, DWORD a, SIZEL *s) { return E_NOTIMPL; }
static HRESULT WINAPI mock_Advise(IOleObject *iface, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI mock_Unadvise(IOleObject *iface, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI mock_EnumAdvise(IOleObject *iface, IEnumSTATDATA **e) { return E_NOTIMPL; }
static HRESULT WINAPI mock_GetMiscStatus(IOleObject *iface, DWORD a, DWORD *s) { return E_NOTIMPL; }
static HRESULT WINAPI mock_SetColorScheme(IOleObject *iface, LOGPALETTE *p) { return E_NOTIMPL; }

static const IOleObjectVtbl mock_vtbl =
{
    mock_QI, mock_AddRef, mock_Release, mock_SetClientSite, mock_GetClientSite, mock_SetHostNames, mock_Close,
    mock_SetMoniker, mock_GetMoniker, mock_InitFromData, mock_GetClipboardData, mock_DoVerb, mock_EnumVerbs,
    mock_Update, mock_IsUpToDate, mock_GetUserClassID, mock_GetUserType, mock_SetExtent, mock_GetExtent,
    mock_Advise, mock_Unadvise, mock_EnumAdvise, mock_GetMiscStatus, mock_SetColorScheme
};

static HRESULT WINAPI psi_QI(IPersistStreamInit *iface, REFIID riid, void **ppv)
{ return mock_QI(&impl_from_psi(iface)->IOleObject_iface, riid, ppv); }
static ULONG WINAPI psi_AddRef(IPersistStreamInit *iface) { return mock_AddRef(&impl_from_psi(iface)->IOleObject_iface); }
static ULONG WINAPI psi_Release(IPersistStreamInit *iface) { return mock_Release(&impl_from_psi(iface)->IOleObject_iface); }
static HRESULT WINAPI psi_GetClassID(IPersistStreamInit *iface, CLSID *c) { *c = clsid_mock; return S_OK; }
static HRESULT WINAPI psi_IsDirty(IPersistStreamInit *iface) { return S_OK; }
static HRESULT WINAPI psi_Load(IPersistStreamInit *iface, IStream *stm)
{
    ULONG n = 0;
    psi_load++;
    memset(psi_data, 0, sizeof(psi_data));
    IStream_Read(stm, psi_data, sizeof(psi_data) - 1, &n);
    return S_OK;
}
static HRESULT WINAPI psi_Save(IPersistStreamInit *iface, IStream *stm, BOOL clear)
{
    ULONG n;
    psi_save++;
    psi_save_clear = clear;
    return IStream_Write(stm, "STATE", 5, &n);
}
static HRESULT WINAPI psi_GetSizeMax(IPersistStreamInit *iface, ULARGE_INTEGER *s) { return E_NOTIMPL; }
static HRESULT WINAPI psi_InitNew(IPersistStreamInit *iface) { psi_initnew++; return S_OK; }
static const IPersistStreamInitVtbl psi_vtbl =
{ psi_QI, psi_AddRef, psi_Release, psi_GetClassID, psi_IsDirty, psi_Load, psi_Save, psi_GetSizeMax, psi_InitNew };

static HRESULT WINAPI ipo_QI(IOleInPlaceObject *iface, REFIID riid, void **ppv)
{ return mock_QI(&impl_from_ipo(iface)->IOleObject_iface, riid, ppv); }
static ULONG WINAPI ipo_AddRef(IOleInPlaceObject *iface) { return mock_AddRef(&impl_from_ipo(iface)->IOleObject_iface); }
static ULONG WINAPI ipo_Release(IOleInPlaceObject *iface) { return mock_Release(&impl_from_ipo(iface)->IOleObject_iface); }
static HRESULT WINAPI ipo_GetWindow(IOleInPlaceObject *iface, HWND *w) { *w = NULL; return E_FAIL; }
static HRESULT WINAPI ipo_CSH(IOleInPlaceObject *iface, BOOL m) { return S_OK; }
static HRESULT WINAPI ipo_InPlaceDeactivate(IOleInPlaceObject *iface) { return S_OK; }
static HRESULT WINAPI ipo_UIDeactivate(IOleInPlaceObject *iface) { return S_OK; }
static HRESULT WINAPI ipo_SetObjectRects(IOleInPlaceObject *iface, LPCRECT pos, LPCRECT clip)
{
    rects_calls++;
    obj_pos = *pos;
    obj_clip = *clip;
    return S_OK;
}
static HRESULT WINAPI ipo_ReactivateAndUndo(IOleInPlaceObject *iface) { return S_OK; }
static const IOleInPlaceObjectVtbl ipo_vtbl =
{ ipo_QI, ipo_AddRef, ipo_Release, ipo_GetWindow, ipo_CSH, ipo_InPlaceDeactivate, ipo_UIDeactivate, ipo_SetObjectRects, ipo_ReactivateAndUndo };

static HRESULT WINAPI pdn_QI(IParseDisplayName *iface, REFIID riid, void **ppv)
{ return mock_QI(&impl_from_pdn(iface)->IOleObject_iface, riid, ppv); }
static ULONG WINAPI pdn_AddRef(IParseDisplayName *iface) { return mock_AddRef(&impl_from_pdn(iface)->IOleObject_iface); }
static ULONG WINAPI pdn_Release(IParseDisplayName *iface) { return mock_Release(&impl_from_pdn(iface)->IOleObject_iface); }
static HRESULT WINAPI pdn_Parse(IParseDisplayName *iface, IBindCtx *pbc, LPOLESTR name, ULONG *eaten, IMoniker **mk)
{
    parse_calls++;
    *eaten = lstrlenW(name);
    return CreateItemMoniker(L"!", name, mk);
}
static const IParseDisplayNameVtbl pdn_vtbl = { pdn_QI, pdn_AddRef, pdn_Release, pdn_Parse };

/* the class factory: also an IClassFactory2 for the licensed class */
struct factory
{
    IClassFactory2 IClassFactory2_iface;
    BOOL persist, parser, licensed;
};

static HRESULT create_mock(struct factory *f, REFIID riid, void **ppv)
{
    struct mock *m = calloc(1, sizeof(*m));
    HRESULT hr;

    m->IOleObject_iface.lpVtbl = &mock_vtbl;
    m->IPersistStreamInit_iface.lpVtbl = &psi_vtbl;
    m->IOleInPlaceObject_iface.lpVtbl = &ipo_vtbl;
    m->IParseDisplayName_iface.lpVtbl = &pdn_vtbl;
    m->ref = 1;
    m->persist = f->persist;
    m->parser = f->parser;
    last_mock = m;
    hr = IOleObject_QueryInterface(&m->IOleObject_iface, riid, ppv);
    IOleObject_Release(&m->IOleObject_iface);
    return hr;
}

static HRESULT WINAPI fac_QI(IClassFactory2 *iface, REFIID riid, void **ppv)
{
    struct factory *f = CONTAINING_RECORD(iface, struct factory, IClassFactory2_iface);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory) || (f->licensed && IsEqualIID(riid, &IID_IClassFactory2)))
    {
        *ppv = iface;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI fac_AddRef(IClassFactory2 *iface) { return 2; }
static ULONG WINAPI fac_Release(IClassFactory2 *iface) { return 1; }
static HRESULT WINAPI fac_CreateInstance(IClassFactory2 *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    struct factory *f = CONTAINING_RECORD(iface, struct factory, IClassFactory2_iface);
    if (f->licensed) return CLASS_E_NOTLICENSED;
    return create_mock(f, riid, ppv);
}
static HRESULT WINAPI fac_LockServer(IClassFactory2 *iface, BOOL l) { return S_OK; }
static HRESULT WINAPI fac_GetLicInfo(IClassFactory2 *iface, LICINFO *info) { return E_NOTIMPL; }
static HRESULT WINAPI fac_RequestLicKey(IClassFactory2 *iface, DWORD r, BSTR *key) { return E_NOTIMPL; }
static HRESULT WINAPI fac_CreateInstanceLic(IClassFactory2 *iface, IUnknown *outer, IUnknown *reserved, REFIID riid, BSTR key, void **ppv)
{
    struct factory *f = CONTAINING_RECORD(iface, struct factory, IClassFactory2_iface);
    if (!key || lstrcmpW(key, L"LICKEY")) return CLASS_E_NOTLICENSED;
    return create_mock(f, riid, ppv);
}
static const IClassFactory2Vtbl fac_vtbl =
{ fac_QI, fac_AddRef, fac_Release, fac_CreateInstance, fac_LockServer, fac_GetLicInfo, fac_RequestLicKey, fac_CreateInstanceLic };

static struct factory fac_mock = { { &fac_vtbl }, TRUE, FALSE, FALSE };
static struct factory fac_nopersist = { { &fac_vtbl }, FALSE, FALSE, FALSE };
static struct factory fac_licensed = { { &fac_vtbl }, TRUE, FALSE, TRUE };
static struct factory fac_html = { { &fac_vtbl }, TRUE, FALSE, FALSE };

/* ---------------------------------------------------------------- a mock active object */

static LONG active_ref;
static HRESULT WINAPI ao_QI(IOleInPlaceActiveObject *iface, REFIID riid, void **ppv) { *ppv = NULL; return E_NOINTERFACE; }
static ULONG WINAPI ao_AddRef(IOleInPlaceActiveObject *iface) { return InterlockedIncrement(&active_ref); }
static ULONG WINAPI ao_Release(IOleInPlaceActiveObject *iface) { return InterlockedDecrement(&active_ref); }
static HRESULT WINAPI ao_GetWindow(IOleInPlaceActiveObject *iface, HWND *w) { return E_FAIL; }
static HRESULT WINAPI ao_CSH(IOleInPlaceActiveObject *iface, BOOL m) { return S_OK; }
static HRESULT WINAPI ao_TA(IOleInPlaceActiveObject *iface, MSG *m) { return S_FALSE; }
static HRESULT WINAPI ao_OnFrame(IOleInPlaceActiveObject *iface, BOOL a) { return S_OK; }
static HRESULT WINAPI ao_OnDoc(IOleInPlaceActiveObject *iface, BOOL a) { return S_OK; }
static HRESULT WINAPI ao_Resize(IOleInPlaceActiveObject *iface, LPCRECT r, IOleInPlaceUIWindow *w, BOOL f) { return S_OK; }
static HRESULT WINAPI ao_Modeless(IOleInPlaceActiveObject *iface, BOOL e) { return S_OK; }
static const IOleInPlaceActiveObjectVtbl ao_vtbl =
{ ao_QI, ao_AddRef, ao_Release, ao_GetWindow, ao_CSH, ao_TA, ao_OnFrame, ao_OnDoc, ao_Resize, ao_Modeless };
static IOleInPlaceActiveObject active_object = { &ao_vtbl };

/* ---------------------------------------------------------------- the container tests */

typedef HRESULT (WINAPI *PCreateControl)(LPCOLESTR, HWND, IStream *, IUnknown **);
typedef HRESULT (WINAPI *PCreateControlLic)(LPCOLESTR, HWND, IStream *, IUnknown **, BSTR);
typedef HRESULT (WINAPI *PGetHost)(HWND, IUnknown **);

static HRESULT create_host(HMODULE atl, HWND parent, HWND *child, const WCHAR *name, IUnknown **container, BSTR lic)
{
    PCreateControl create = (PCreateControl)GetProcAddress(atl, "AtlAxCreateControl");
    PCreateControlLic create_lic = (PCreateControlLic)GetProcAddress(atl, "AtlAxCreateControlLic");

    *child = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 120, 80, parent, NULL, GetModuleHandleW(NULL), NULL);
    if (lic) return create_lic(name, *child, NULL, container, lic);
    return create(name, *child, NULL, container);
}

static void test_container(HMODULE atl)
{
    HWND top, child;
    IUnknown *container = NULL;
    IOleClientSite *site = NULL;
    IOleContainer *oc = NULL;
    IOleInPlaceSiteWindowless *ips = NULL;
    IOleInPlaceFrame *frame = NULL;
    IOleControlSite *cs = NULL;
    struct mock *mock;
    HRESULT hr;
    WCHAR name[64];

    top = CreateWindowExW(0, L"STATIC", L"top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, GetModuleHandleW(NULL), NULL);
    CHECK(top != NULL, "top window");
    StringFromGUID2(&clsid_mock, name, ARRAY_SIZE(name));
    psi_initnew = 0;
    hr = create_host(atl, top, &child, name, &container, NULL);
    CHECK(hr == S_OK && container, "create mock control %lx", hr);
    mock = last_mock;
    CHECK(mock != NULL, "mock created");
    CHECK(psi_initnew == 1, "InitNew %d", psi_initnew);
    if (!container || !mock) return;
    hr = IUnknown_QueryInterface(container, &IID_IOleClientSite, (void **)&site);
    CHECK(hr == S_OK, "site %lx", hr);
    IOleClientSite_QueryInterface(site, &IID_IOleContainer, (void **)&oc);
    IOleClientSite_QueryInterface(site, &IID_IOleInPlaceSiteWindowless, (void **)&ips);
    IOleClientSite_QueryInterface(site, &IID_IOleInPlaceFrame, (void **)&frame);
    IOleClientSite_QueryInterface(site, &IID_IOleControlSite, (void **)&cs);

    /* SaveObject: the state of the control goes to the container */
    psi_save = 0;
    hr = IOleClientSite_SaveObject(site);
    CHECK(hr == S_OK, "SaveObject %lx", hr);
    CHECK(psi_save == 1 && psi_save_clear == TRUE, "control saved: %d clear %d", psi_save, psi_save_clear);

    /* GetMoniker, RequestNewObjectLayout */
    {
        IMoniker *mk = (IMoniker *)0x1234;
        hr = IOleClientSite_GetMoniker(site, OLEGETMONIKER_FORCEASSIGN, OLEWHICHMK_OBJFULL, &mk);
        CHECK(hr == E_NOTIMPL && mk == NULL, "GetMoniker %lx %p", hr, mk);
        hr = IOleClientSite_RequestNewObjectLayout(site);
        CHECK(hr == E_NOTIMPL, "RequestNewObjectLayout %lx", hr);
    }

    /* ShowObject restores the minimized container */
    ShowWindow(top, SW_MINIMIZE);
    if (IsIconic(top))
    {
        hr = IOleClientSite_ShowObject(site);
        CHECK(hr == S_OK && !IsIconic(top), "ShowObject restores the window: %lx iconic %d", hr, IsIconic(top));
    }
    else
    {
        hr = IOleClientSite_ShowObject(site);
        CHECK(hr == S_OK, "ShowObject %lx", hr);
    }

    /* ParseDisplayName: the control has no IParseDisplayName */
    {
        IMoniker *mk = (IMoniker *)0x1234;
        ULONG eaten = 99;
        IBindCtx *bc;
        CreateBindCtx(0, &bc);
        hr = IOleContainer_ParseDisplayName(oc, bc, (LPOLESTR)L"item", &eaten, &mk);
        CHECK(hr == MK_E_NOOBJECT && mk == NULL && eaten == 0, "ParseDisplayName without parser %lx %p %lu", hr, mk, eaten);
        hr = IOleContainer_ParseDisplayName(oc, bc, NULL, &eaten, &mk);
        CHECK(hr == E_INVALIDARG, "ParseDisplayName NULL name %lx", hr);
        hr = IOleContainer_ParseDisplayName(oc, bc, (LPOLESTR)L"item", &eaten, NULL);
        CHECK(hr == E_INVALIDARG, "ParseDisplayName NULL moniker %lx", hr);
        IBindCtx_Release(bc);
    }

    /* context sensitive help */
    hr = IOleInPlaceSiteWindowless_ContextSensitiveHelp(ips, TRUE);
    CHECK(hr == S_OK, "site ContextSensitiveHelp %lx", hr);
    hr = IOleInPlaceFrame_ContextSensitiveHelp(frame, FALSE);
    CHECK(hr == S_OK, "frame ContextSensitiveHelp %lx", hr);

    /* Scroll: the object rectangle moves, the clipping rectangle stays */
    {
        RECT pos, clip, size;
        OLEINPLACEFRAMEINFO info = { sizeof(info) };
        IOleInPlaceFrame *f2 = NULL;
        IOleInPlaceUIWindow *doc = NULL;
        SIZE ext = { 10, 5 };

        GetClientRect(child, &size);
        rects_calls = 0;
        hr = IOleInPlaceSiteWindowless_Scroll(ips, ext);
        CHECK(hr == S_OK, "Scroll %lx", hr);
        CHECK(rects_calls == 1, "SetObjectRects calls %d", rects_calls);
        CHECK(obj_pos.left == size.left - 10 && obj_pos.top == size.top - 5 && obj_pos.right == size.right - 10 && obj_pos.bottom == size.bottom - 5,
              "scrolled pos %s %ld,%ld,%ld,%ld", "", obj_pos.left, obj_pos.top, obj_pos.right, obj_pos.bottom);
        CHECK(obj_clip.left == size.left && obj_clip.top == size.top && obj_clip.right == size.right && obj_clip.bottom == size.bottom,
              "clip stays %ld,%ld,%ld,%ld", obj_clip.left, obj_clip.top, obj_clip.right, obj_clip.bottom);
        hr = IOleInPlaceSiteWindowless_Scroll(ips, ext);
        CHECK(hr == S_OK && obj_pos.left == size.left - 20 && obj_pos.top == size.top - 10, "second Scroll accumulates %ld %ld", obj_pos.left, obj_pos.top);
        hr = IOleInPlaceSiteWindowless_GetWindowContext(ips, &f2, &doc, &pos, &clip, &info);
        CHECK(hr == S_OK && pos.left == size.left - 20 && pos.top == size.top - 10 && clip.left == size.left,
              "GetWindowContext sees the scroll: %ld %ld clip %ld", pos.left, pos.top, clip.left);
        if (f2) IOleInPlaceFrame_Release(f2);
    }

    /* SetActiveObject keeps and releases the active object */
    active_ref = 0;
    hr = IOleInPlaceFrame_SetActiveObject(frame, &active_object, NULL);
    CHECK(hr == S_OK && active_ref == 1, "SetActiveObject %lx ref %ld", hr, active_ref);
    hr = IOleInPlaceFrame_SetActiveObject(frame, &active_object, L"name");
    CHECK(hr == S_OK && active_ref == 1, "SetActiveObject again %lx ref %ld", hr, active_ref);
    hr = IOleInPlaceFrame_SetActiveObject(frame, NULL, NULL);
    CHECK(hr == S_OK && active_ref == 0, "SetActiveObject NULL %lx ref %ld", hr, active_ref);
    IOleInPlaceFrame_SetActiveObject(frame, &active_object, NULL);

    /* the frame menus: the container adds nothing */
    {
        HMENU menu = CreateMenu();
        OLEMENUGROUPWIDTHS widths = { { 7, 7, 7, 7, 7, 7 } };

        hr = IOleInPlaceFrame_InsertMenus(frame, menu, &widths);
        CHECK(hr == S_OK && widths.width[0] == 0 && widths.width[2] == 0 && widths.width[4] == 0 && widths.width[1] == 7,
              "InsertMenus %lx %ld %ld %ld", hr, widths.width[0], widths.width[2], widths.width[4]);
        CHECK(GetMenuItemCount(menu) == 0, "no items inserted");
        hr = IOleInPlaceFrame_InsertMenus(frame, menu, NULL);
        CHECK(hr == E_INVALIDARG, "InsertMenus NULL widths %lx", hr);
        hr = IOleInPlaceFrame_InsertMenus(frame, (HMENU)0x1234, &widths);
        CHECK(hr == E_INVALIDARG, "InsertMenus bad menu %lx", hr);
        hr = IOleInPlaceFrame_SetMenu(frame, menu, NULL, child);
        CHECK(hr == S_OK, "SetMenu %lx", hr);
        hr = IOleInPlaceFrame_SetMenu(frame, NULL, NULL, NULL);
        CHECK(hr == S_OK, "SetMenu NULL %lx", hr);
        hr = IOleInPlaceFrame_SetMenu(frame, (HMENU)0x1234, NULL, child);
        CHECK(hr == E_INVALIDARG, "SetMenu bad menu %lx", hr);
        hr = IOleInPlaceFrame_RemoveMenus(frame, menu);
        CHECK(hr == S_OK, "RemoveMenus %lx", hr);
        hr = IOleInPlaceFrame_RemoveMenus(frame, (HMENU)0x1234);
        CHECK(hr == E_INVALIDARG, "RemoveMenus bad menu %lx", hr);
        hr = IOleInPlaceFrame_RemoveMenus(frame, NULL);
        CHECK(hr == E_INVALIDARG, "RemoveMenus NULL %lx", hr);
        DestroyMenu(menu);
    }

    /* the control site */
    {
        IDispatch *disp = (IDispatch *)0x1234;
        hr = IOleControlSite_GetExtendedControl(cs, &disp);
        CHECK(hr == E_NOTIMPL && disp == NULL, "GetExtendedControl %lx %p", hr, disp);
        hr = IOleControlSite_GetExtendedControl(cs, NULL);
        CHECK(hr == E_POINTER, "GetExtendedControl NULL %lx", hr);
        hr = IOleControlSite_ShowPropertyFrame(cs);
        CHECK(hr == E_NOTIMPL, "ShowPropertyFrame %lx", hr);
    }

    /* the active object is released with the host */
    IOleControlSite_Release(cs);
    IOleInPlaceFrame_Release(frame);
    IOleInPlaceSiteWindowless_Release(ips);
    IOleContainer_Release(oc);
    IOleClientSite_Release(site);
    IUnknown_Release(container);
    DestroyWindow(child);
    CHECK(active_ref == 0, "active object released with the host: %ld", active_ref);
    DestroyWindow(top);

    /* a control with an IParseDisplayName: the container hands the names to it */
    {
        static struct factory parser_fac = { { &fac_vtbl }, TRUE, TRUE, FALSE };
        DWORD reg;
        IMoniker *mk = NULL;
        ULONG eaten = 0;
        IBindCtx *bc;
        HWND c2;
        IUnknown *cont2 = NULL;
        IOleContainer *oc2;
        IOleClientSite *s2;
        CLSID parser_clsid = clsid_mock;

        parser_clsid.Data1 = 0x7e5f0004;
        CoRegisterClassObject(&parser_clsid, (IUnknown *)&parser_fac.IClassFactory2_iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &reg);
        StringFromGUID2(&parser_clsid, name, ARRAY_SIZE(name));
        top = CreateWindowExW(0, L"STATIC", L"top", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, GetModuleHandleW(NULL), NULL);
        hr = create_host(atl, top, &c2, name, &cont2, NULL);
        CHECK(hr == S_OK && cont2, "create parser control %lx", hr);
        if (cont2)
        {
            IUnknown_QueryInterface(cont2, &IID_IOleClientSite, (void **)&s2);
            IOleClientSite_QueryInterface(s2, &IID_IOleContainer, (void **)&oc2);
            CreateBindCtx(0, &bc);
            parse_calls = 0;
            hr = IOleContainer_ParseDisplayName(oc2, bc, (LPOLESTR)L"thing", &eaten, &mk);
            CHECK(hr == S_OK && mk && eaten == 5 && parse_calls == 1, "ParseDisplayName delegated %lx %p %lu %d", hr, mk, eaten, parse_calls);
            if (mk) IMoniker_Release(mk);
            IBindCtx_Release(bc);
            IOleContainer_Release(oc2);
            IOleClientSite_Release(s2);
            IUnknown_Release(cont2);
        }
        DestroyWindow(c2);
        DestroyWindow(top);
        CoRevokeClassObject(reg);
    }

    /* a control without persistence cannot be saved */
    {
        DWORD reg;
        HWND c2;
        IUnknown *cont2 = NULL;
        IOleClientSite *s2;
        CLSID np = clsid_nopersist;

        CoRegisterClassObject(&np, (IUnknown *)&fac_nopersist.IClassFactory2_iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &reg);
        StringFromGUID2(&np, name, ARRAY_SIZE(name));
        top = CreateWindowExW(0, L"STATIC", L"top", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, GetModuleHandleW(NULL), NULL);
        hr = create_host(atl, top, &c2, name, &cont2, NULL);
        CHECK(hr == S_OK && cont2, "create control without persistence %lx", hr);
        if (cont2)
        {
            IUnknown_QueryInterface(cont2, &IID_IOleClientSite, (void **)&s2);
            hr = IOleClientSite_SaveObject(s2);
            CHECK(hr == E_FAIL, "SaveObject without persistence %lx", hr);
            IOleClientSite_Release(s2);
            IUnknown_Release(cont2);
        }
        DestroyWindow(c2);
        DestroyWindow(top);
        CoRevokeClassObject(reg);
    }
}

static void test_create(HMODULE atl)
{
    HWND top, child;
    IUnknown *container = NULL;
    DWORD reg_lic, reg_html;
    HRESULT hr;
    WCHAR name[64];
    BSTR lic;

    CoRegisterClassObject(&clsid_licensed, (IUnknown *)&fac_licensed.IClassFactory2_iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &reg_lic);
    CoRegisterClassObject(&clsid_htmldoc, (IUnknown *)&fac_html.IClassFactory2_iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &reg_html);
    top = CreateWindowExW(0, L"STATIC", L"top", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, GetModuleHandleW(NULL), NULL);

    /* licensed controls */
    StringFromGUID2(&clsid_licensed, name, ARRAY_SIZE(name));
    lic = SysAllocString(L"LICKEY");
    hr = create_host(atl, top, &child, name, &container, lic);
    CHECK(hr == S_OK && container && last_mock, "licensed control with key %lx", hr);
    if (container) IUnknown_Release(container);
    container = NULL;
    DestroyWindow(child);
    SysFreeString(lic);
    lic = SysAllocString(L"WRONG");
    hr = create_host(atl, top, &child, name, &container, lic);
    CHECK(hr == CLASS_E_NOTLICENSED && !container, "licensed control with a wrong key %lx", hr);
    DestroyWindow(child);
    SysFreeString(lic);
    hr = create_host(atl, top, &child, name, &container, NULL);
    CHECK(hr == CLASS_E_NOTLICENSED && !container, "licensed control without key %lx", hr);
    DestroyWindow(child);
    /* a class that has no license support: the key is not needed */
    StringFromGUID2(&clsid_mock, name, ARRAY_SIZE(name));
    lic = SysAllocString(L"ANY");
    hr = create_host(atl, top, &child, name, &container, lic);
    CHECK(hr == S_OK && container, "unlicensed class with a key %lx", hr);
    if (container) IUnknown_Release(container);
    container = NULL;
    DestroyWindow(child);
    SysFreeString(lic);

    /* MSHTML: the text goes to the HTML document object as a stream */
    psi_load = 0;
    hr = create_host(atl, top, &child, L"MSHTML:<HTML><BODY>caf\x00e9 text</BODY></HTML>", &container, NULL);
    CHECK(hr == S_OK && container, "MSHTML create %lx", hr);
    CHECK(psi_load == 1, "HTML stream loaded %d", psi_load);
    CHECK(!strcmp(psi_data, "<HTML><BODY>caf\xc3\xa9 text</BODY></HTML>"), "HTML stream content '%s'", psi_data);
    if (container) IUnknown_Release(container);
    container = NULL;
    DestroyWindow(child);
    psi_load = 0;
    hr = create_host(atl, top, &child, L"mshtml:<p>x</p>", &container, NULL);
    CHECK(hr == S_OK && psi_load == 1 && !strcmp(psi_data, "<p>x</p>"), "lowercase prefix %lx '%s'", hr, psi_data);
    if (container) IUnknown_Release(container);
    DestroyWindow(child);

    DestroyWindow(top);
    CoRevokeClassObject(reg_lic);
    CoRevokeClassObject(reg_html);
}

/* ---------------------------------------------------------------- the property map */

#pragma pack(push, 8)
typedef struct
{
    LPCOLESTR szDesc;
    DISPID dispid;
    const CLSID *pclsidPropPage;
    const IID *piidDispatch;
    DWORD dwOffsetData;
    DWORD dwSizeData;
    VARTYPE vt;
} MAP30;
typedef struct
{
    LPCOLESTR szDesc;
    const CLSID *pclsidPropPage;
    const IID *piidDispatch;
    void *rgclsidAllowed;
    DWORD cclsidAllowed;
    DISPID dispid;
    DWORD dwOffsetData;
    DWORD dwSizeData;
    VARTYPE vt;
} MAP100;
#pragma pack(pop)

struct obj
{
    IDispatch IDispatch_iface;
    LONG ref;
    LONG count;        /* dispid 1 */
    BSTR name;         /* dispid 2 */
    LONGLONG extent;   /* data member */
    VARIANT_BOOL flag; /* data member */
    BSTR title;        /* data member */
};

static inline struct obj *impl_from_IDispatch(IDispatch *iface) { return CONTAINING_RECORD(iface, struct obj, IDispatch_iface); }
static HRESULT WINAPI obj_QI(IDispatch *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI obj_AddRef(IDispatch *iface) { return InterlockedIncrement(&impl_from_IDispatch(iface)->ref); }
static ULONG WINAPI obj_Release(IDispatch *iface) { return InterlockedDecrement(&impl_from_IDispatch(iface)->ref); }
static HRESULT WINAPI obj_GetTypeInfoCount(IDispatch *iface, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI obj_GetTypeInfo(IDispatch *iface, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI obj_GetIDsOfNames(IDispatch *iface, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI obj_Invoke(IDispatch *iface, DISPID id, REFIID riid, LCID lcid, WORD flags, DISPPARAMS *params,
                                 VARIANT *res, EXCEPINFO *ei, UINT *err)
{
    struct obj *This = impl_from_IDispatch(iface);

    if (flags == DISPATCH_PROPERTYGET)
    {
        if (!res) return E_INVALIDARG;
        if (id == 1) { V_VT(res) = VT_I4; V_I4(res) = This->count; return S_OK; }
        if (id == 2) { V_VT(res) = VT_BSTR; V_BSTR(res) = SysAllocString(This->name ? This->name : L""); return S_OK; }
    }
    else if (flags == DISPATCH_PROPERTYPUT)
    {
        VARIANT *v;
        if (params->cArgs != 1 || params->cNamedArgs != 1 || params->rgdispidNamedArgs[0] != DISPID_PROPERTYPUT) return DISP_E_BADPARAMCOUNT;
        v = &params->rgvarg[0];
        if (id == 1) { if (V_VT(v) != VT_I4) return DISP_E_TYPEMISMATCH; This->count = V_I4(v); return S_OK; }
        if (id == 2) { if (V_VT(v) != VT_BSTR) return DISP_E_TYPEMISMATCH; SysFreeString(This->name); This->name = SysAllocString(V_BSTR(v)); return S_OK; }
    }
    return DISP_E_MEMBERNOTFOUND;
}
static const IDispatchVtbl obj_vtbl =
{ obj_QI, obj_AddRef, obj_Release, obj_GetTypeInfoCount, obj_GetTypeInfo, obj_GetIDsOfNames, obj_Invoke };

/* a property bag in memory */
struct bag
{
    IPropertyBag IPropertyBag_iface;
    WCHAR names[8][16];
    VARIANT values[8];
    int count;
    int reads;
};
static inline struct bag *impl_from_bag(IPropertyBag *iface) { return CONTAINING_RECORD(iface, struct bag, IPropertyBag_iface); }
static HRESULT WINAPI bag_QI(IPropertyBag *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPropertyBag)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI bag_AddRef(IPropertyBag *iface) { return 2; }
static ULONG WINAPI bag_Release(IPropertyBag *iface) { return 1; }
static HRESULT WINAPI bag_Read(IPropertyBag *iface, LPCOLESTR name, VARIANT *v, IErrorLog *log)
{
    struct bag *This = impl_from_bag(iface);
    int i;
    This->reads++;
    for (i = 0; i < This->count; i++)
        if (!lstrcmpW(This->names[i], name))
        {
            VARTYPE want = V_VT(v);
            if (want == VT_EMPTY || want == V_VT(&This->values[i])) return VariantCopy(v, &This->values[i]);
            return VariantChangeType(v, &This->values[i], 0, want);
        }
    return E_INVALIDARG;
}
static HRESULT WINAPI bag_Write(IPropertyBag *iface, LPCOLESTR name, VARIANT *v)
{
    struct bag *This = impl_from_bag(iface);
    int i;
    for (i = 0; i < This->count; i++)
        if (!lstrcmpW(This->names[i], name)) { VariantClear(&This->values[i]); return VariantCopy(&This->values[i], v); }
    if (This->count == 8) return E_FAIL;
    lstrcpyW(This->names[This->count], name);
    VariantInit(&This->values[This->count]);
    VariantCopy(&This->values[This->count], v);
    This->count++;
    return S_OK;
}
static const IPropertyBagVtbl bag_vtbl = { bag_QI, bag_AddRef, bag_Release, bag_Read, bag_Write };

static VARIANT *bag_find(struct bag *b, const WCHAR *name)
{
    int i;
    for (i = 0; i < b->count; i++) if (!lstrcmpW(b->names[i], name)) return &b->values[i];
    return NULL;
}

static void init_obj(struct obj *o)
{
    memset(o, 0, sizeof(*o));
    o->IDispatch_iface.lpVtbl = &obj_vtbl;
    o->ref = 1;
}

#define BUILD_MAP(MAP, NAME, DISPATCH_FIELD_INIT) \
static void NAME(MAP *m, int *n) \
{ \
    static const CLSID clsid_null = {0}, clsid_page = {1}; \
    memset(m, 0, sizeof(*m) * 8); \
    m[0].szDesc = L"Count";  m[0].dispid = 1; m[0].pclsidPropPage = &clsid_page; m[0].piidDispatch = &IID_IDispatch; m[0].vt = VT_I4; \
    m[1].szDesc = L"Name";   m[1].dispid = 2; m[1].pclsidPropPage = &clsid_page; m[1].piidDispatch = NULL; m[1].vt = VT_BSTR; \
    m[2].szDesc = L"Extent"; m[2].pclsidPropPage = &clsid_null; m[2].piidDispatch = &IID_NULL; \
        m[2].dwOffsetData = FIELD_OFFSET(struct obj, extent); m[2].dwSizeData = sizeof(LONGLONG); m[2].vt = VT_I8; \
    m[3].pclsidPropPage = &clsid_page; m[3].piidDispatch = &IID_NULL; /* PROP_PAGE */ \
    m[4].szDesc = L"Flag";   m[4].pclsidPropPage = &clsid_null; m[4].piidDispatch = &IID_NULL; \
        m[4].dwOffsetData = FIELD_OFFSET(struct obj, flag); m[4].dwSizeData = sizeof(VARIANT_BOOL); m[4].vt = VT_BOOL; \
    m[5].szDesc = L"Title";  m[5].pclsidPropPage = &clsid_null; m[5].piidDispatch = &IID_NULL; \
        m[5].dwOffsetData = FIELD_OFFSET(struct obj, title); m[5].dwSizeData = sizeof(BSTR); m[5].vt = VT_BSTR; \
    /* m[6] is the end of the map */ \
    *n = 6; \
}

BUILD_MAP(MAP30, build_map30, 0)
BUILD_MAP(MAP100, build_map100, 0)

typedef HRESULT (WINAPI *PStreamLoad)(IStream *, void *, void *, IUnknown *);
typedef HRESULT (WINAPI *PStreamSave)(IStream *, BOOL, void *, void *, IUnknown *);
typedef HRESULT (WINAPI *PBagLoad)(IPropertyBag *, IErrorLog *, void *, void *, IUnknown *);
typedef HRESULT (WINAPI *PBagSave)(IPropertyBag *, BOOL, BOOL, void *, void *, IUnknown *);

static void test_persist(const char *dllname, void *map, const char *label)
{
    HMODULE dll = LoadLibraryA(dllname);
    PStreamLoad sload;
    PStreamSave ssave;
    PBagLoad bload;
    PBagSave bsave;
    struct obj a, b;
    struct bag bag;
    IStream *stm;
    HRESULT hr;
    LARGE_INTEGER zero = {0};
    VARIANT *v;
    STATSTG stat;

    CHECK(dll != NULL, "%s: %s not loaded", label, dllname);
    if (!dll) return;
    sload = (PStreamLoad)GetProcAddress(dll, "AtlIPersistStreamInit_Load");
    ssave = (PStreamSave)GetProcAddress(dll, "AtlIPersistStreamInit_Save");
    bload = (PBagLoad)GetProcAddress(dll, "AtlIPersistPropertyBag_Load");
    bsave = (PBagSave)GetProcAddress(dll, "AtlIPersistPropertyBag_Save");
    CHECK(sload && ssave && bload && bsave, "%s: exports", label);
    if (!(sload && ssave && bload && bsave)) return;

    init_obj(&a);
    a.count = 42;
    a.name = SysAllocString(L"alpha");
    a.extent = 0x123456789ALL;
    a.flag = VARIANT_TRUE;
    a.title = SysAllocString(L"caf\x00e9");

    /* the property bag: all the described properties, not the property page entry */
    memset(&bag, 0, sizeof(bag));
    bag.IPropertyBag_iface.lpVtbl = &bag_vtbl;
    hr = bsave(&bag.IPropertyBag_iface, TRUE, FALSE, map, &a, (IUnknown *)&a.IDispatch_iface);
    CHECK(hr == S_OK, "%s: bag save %lx", label, hr);
    CHECK(bag.count == 5, "%s: %d properties written", label, bag.count);
    v = bag_find(&bag, L"Count");
    CHECK(v && V_VT(v) == VT_I4 && V_I4(v) == 42, "%s: Count in bag", label);
    v = bag_find(&bag, L"Name");
    CHECK(v && V_VT(v) == VT_BSTR && !lstrcmpW(V_BSTR(v), L"alpha"), "%s: Name in bag", label);
    v = bag_find(&bag, L"Extent");
    CHECK(v && V_VT(v) == VT_I8 && V_I8(v) == 0x123456789ALL, "%s: Extent in bag (data member)", label);
    v = bag_find(&bag, L"Flag");
    CHECK(v && V_VT(v) == VT_BOOL && V_BOOL(v) == VARIANT_TRUE, "%s: Flag in bag", label);
    v = bag_find(&bag, L"Title");
    CHECK(v && V_VT(v) == VT_BSTR && !lstrcmpW(V_BSTR(v), L"caf\x00e9"), "%s: Title in bag", label);

    init_obj(&b);
    b.count = 7;
    b.flag = VARIANT_FALSE;
    b.extent = 5;
    bag.reads = 0;
    /* a value of another type is converted; a missing property is left alone */
    VariantClear(bag_find(&bag, L"Count"));
    V_VT(bag_find(&bag, L"Count")) = VT_BSTR;
    V_BSTR(bag_find(&bag, L"Count")) = SysAllocString(L"99");
    /* drop "Flag" (index 3): Title moves down */
    lstrcpyW(bag.names[3], bag.names[4]);
    bag.values[3] = bag.values[4];
    bag.count = 4;
    hr = bload(&bag.IPropertyBag_iface, NULL, map, &b, (IUnknown *)&b.IDispatch_iface);
    CHECK(hr == S_OK, "%s: bag load %lx", label, hr);
    CHECK(bag.reads == 5, "%s: %d reads", label, bag.reads);
    CHECK(b.count == 99, "%s: Count loaded through IDispatch and converted: %ld", label, b.count);
    CHECK(b.name && !lstrcmpW(b.name, L"alpha"), "%s: Name loaded", label);
    CHECK(b.extent == 0x123456789ALL, "%s: Extent loaded into the data member", label);
    CHECK(b.flag == VARIANT_FALSE, "%s: Flag missing from the bag is kept", label);
    hr = bload(NULL, NULL, map, &b, (IUnknown *)&b.IDispatch_iface);
    CHECK(hr == E_INVALIDARG, "%s: bag load without bag %lx", label, hr);
    hr = bsave(&bag.IPropertyBag_iface, FALSE, TRUE, NULL, &a, (IUnknown *)&a.IDispatch_iface);
    CHECK(hr == E_INVALIDARG, "%s: bag save without map %lx", label, hr);

    /* the stream */
    CreateStreamOnHGlobal(NULL, TRUE, &stm);
    hr = ssave(stm, TRUE, map, &a, (IUnknown *)&a.IDispatch_iface);
    CHECK(hr == S_OK, "%s: stream save %lx", label, hr);
    IStream_Stat(stm, &stat, STATFLAG_NONAME);
    CHECK(stat.cbSize.QuadPart > 20, "%s: stream size %lu", label, (unsigned long)stat.cbSize.QuadPart);
    IStream_Seek(stm, zero, STREAM_SEEK_SET, NULL);
    init_obj(&b);
    b.flag = VARIANT_FALSE;
    hr = sload(stm, map, &b, (IUnknown *)&b.IDispatch_iface);
    CHECK(hr == S_OK, "%s: stream load %lx", label, hr);
    CHECK(b.count == 42 && b.name && !lstrcmpW(b.name, L"alpha"), "%s: dispatch properties through the stream %ld", label, b.count);
    CHECK(b.extent == 0x123456789ALL && b.flag == VARIANT_TRUE, "%s: data members through the stream", label);
    CHECK(b.title && !lstrcmpW(b.title, L"caf\x00e9"), "%s: BSTR member through the stream", label);
    /* a damaged stream */
    {
        IStream *s2;
        ULARGE_INTEGER cut;
        LARGE_INTEGER back = {0};
        char junk[16] = "junkjunkjunkjunk";
        ULONG n;

        IStream_Seek(stm, zero, STREAM_SEEK_SET, NULL);
        cut.QuadPart = stat.cbSize.QuadPart - 3;
        IStream_SetSize(stm, cut);
        IStream_Seek(stm, zero, STREAM_SEEK_SET, NULL);
        init_obj(&b);
        hr = sload(stm, map, &b, (IUnknown *)&b.IDispatch_iface);
        CHECK(FAILED(hr), "%s: truncated stream load %lx", label, hr);
        CreateStreamOnHGlobal(NULL, TRUE, &s2);
        hr = sload(s2, map, &b, (IUnknown *)&b.IDispatch_iface);
        CHECK(hr == E_FAIL, "%s: empty stream load %lx", label, hr);
        IStream_Write(s2, junk, sizeof(junk), &n);
        IStream_Seek(s2, back, STREAM_SEEK_SET, NULL);
        hr = sload(s2, map, &b, (IUnknown *)&b.IDispatch_iface);
        CHECK(hr == E_FAIL, "%s: foreign stream load %lx", label, hr);
        IStream_Release(s2);
    }
    hr = sload(NULL, map, &b, (IUnknown *)&b.IDispatch_iface);
    CHECK(hr == E_INVALIDARG, "%s: stream load without stream %lx", label, hr);
    hr = ssave(stm, TRUE, NULL, &a, (IUnknown *)&a.IDispatch_iface);
    CHECK(hr == E_INVALIDARG, "%s: stream save without map %lx", label, hr);
    IStream_Release(stm);
}

int main(void)
{
    HMODULE atl = LoadLibraryA("atl100.dll");
    DWORD main_reg;
    MAP30 m30[8];
    MAP100 m100[8];
    int n;

    CoInitialize(NULL);
    CHECK(atl != NULL, "atl100 not loaded");
    if (!atl) return 1;
    CHECK(((BOOL (WINAPI *)(void))GetProcAddress(atl, "AtlAxWinInit"))(), "AtlAxWinInit");
    CoRegisterClassObject(&clsid_mock, (IUnknown *)&fac_mock.IClassFactory2_iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &main_reg);
    test_container(atl);
    test_create(atl);
    build_map100(m100, &n);
    test_persist("atl100.dll", m100, "atl100");
    build_map30(m30, &n);
    test_persist("atl.dll", m30, "atl30");
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    fflush(stdout);
    return fails;
}
