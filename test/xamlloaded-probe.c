/* Windows.UI.Xaml element lifetime and compiled templates' connections (patches/sg/6151): Loading,
 * Loaded and Unloaded raised as an element enters and leaves an island's tree, IsLoaded; a compiled
 * ControlTemplate with an x:ConnectionId of its own is an x:Bind scope: applied to a control, the
 * component's binding connector for it (IComponentConnector2.GetBindingConnector with the control)
 * connects the template's elements, as does the component (IComponentConnector) -- how an
 * application's compiled x:Bind in templates is wired (NanaZip's status and address bars), and the
 * XamlBindingHelper statics compiled x:Bind code sets members through; framework
 * IDs newer than our table are dropped, not fatal. Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_Border,         0x797c4539, 0x45bd, 0x4633, 0xa0, 0x44, 0xbf, 0xb0, 0x2e, 0xf5, 0x17, 0x0f);
DEFINE_GUID(IID_Grid,           0xfd104460, 0x2e15, 0x4ba3, 0x8b, 0x8f, 0xfa, 0x69, 0x3a, 0x41, 0x61, 0xe9);
DEFINE_GUID(IID_TB,             0xae2d9271, 0x3b4a, 0x45fc, 0x84, 0x68, 0xf7, 0x94, 0x95, 0x48, 0xf4, 0xd5);
DEFINE_GUID(IID_PageFactory,    0xdfa149ac, 0x1849, 0x445e, 0x93, 0x7c, 0x40, 0xa9, 0x59, 0x0c, 0xc0, 0x76);
DEFINE_GUID(IID_UC,             0xa7a69ec9, 0xea35, 0x4679, 0xbf, 0x29, 0xf4, 0xf0, 0x92, 0x86, 0xd3, 0x14);
DEFINE_GUID(IID_AppStatics,     0x06499997, 0xf7b4, 0x45fe, 0xb7, 0x63, 0x75, 0x77, 0xd1, 0xd3, 0xcb, 0x4a);
DEFINE_GUID(IID_UriFactory,     0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
DEFINE_GUID(IID_GridFactory,    0xae814041, 0xc531, 0x43b4, 0xbf, 0x99, 0x12, 0xf5, 0x06, 0xf7, 0xb0, 0x1c);
DEFINE_GUID(IID_FE3,            0xc81c2720, 0x5c52, 0x4bbe, 0xa1, 0x99, 0x2b, 0x1e, 0x34, 0xf0, 0x0f, 0x70);
DEFINE_GUID(IID_FE7,            0x2263886c, 0xc069, 0x570f, 0xb9, 0xcc, 0x9e, 0x21, 0xdd, 0x02, 0x8d, 0x8e);
DEFINE_GUID(IID_Panel,          0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_TBStatics,      0x193ec042, 0xa847, 0x4552, 0xb4, 0xe8, 0xdb, 0x36, 0x65, 0x5b, 0x84, 0x0a);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_BindingHelper,  0xf65cfb71, 0xc80c, 0x4ffa, 0x86, 0xee, 0x55, 0x87, 0x54, 0xee, 0x33, 0x6d);
DEFINE_GUID(IID_Connector2,     0xdc8f368b, 0xeccc, 0x498e, 0xb1, 0x39, 0x91, 0x14, 0x22, 0x54, 0xd7, 0xae);
DEFINE_GUID(IID_Connector,      0xf6790987, 0xe6e5, 0x47f2, 0x92, 0xc6, 0xec, 0xcc, 0xe4, 0xba, 0x15, 0x9a);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *getd_fn)(obj *, double *);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}
static int close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 40 && abs(GetGValue(c) - g) < 40 && abs(GetBValue(c) - b) < 40;
}


/* an event handler counting its calls */
struct handler { void **vtbl; LONG ref; int calls; IInspectable *last_sender; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, IInspectable *sender, IInspectable *args) { h->calls++; h->last_sender = sender; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

/* a connector: records the IDs it is given */
struct conn { void **vtbl; LONG ref; int id; IInspectable *target; };
static HRESULT WINAPI c_qi(struct conn *c, REFIID iid, void **out) { *out = c; return S_OK; }
static ULONG WINAPI c_addref(struct conn *c) { return ++c->ref; }
static ULONG WINAPI c_release(struct conn *c) { return --c->ref; }
static HRESULT WINAPI c_iids(struct conn *c, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI c_name(struct conn *c, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI c_trust(struct conn *c, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static HRESULT WINAPI c_connect(struct conn *c, INT32 id, IInspectable *target) { c->id = id; c->target = target; return S_OK; }
static void *c_vtbl[] = { c_qi, c_addref, c_release, c_iids, c_name, c_trust, c_connect };
static struct conn binding = { c_vtbl, 1 };

/* an application's page: aggregates Page; IComponentConnector and IComponentConnector2 */
struct app_page { IInspectable IInspectable_iface; struct { void **vtbl; } connector, connector2; LONG ref; IInspectable *inner;
                  int id; IInspectable *target; int bind_id; IInspectable *bind_target; };
static HRESULT WINAPI outer_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct app_page *p = CONTAINING_RECORD(iface, struct app_page, IInspectable_iface);
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)) { *out = iface; InterlockedIncrement(&p->ref); return S_OK; }
    if (IsEqualGUID(iid, &IID_Connector)) { *out = &p->connector; InterlockedIncrement(&p->ref); return S_OK; }
    if (IsEqualGUID(iid, &IID_Connector2)) { *out = &p->connector2; InterlockedIncrement(&p->ref); return S_OK; }
    return IInspectable_QueryInterface(p->inner, iid, out);
}
static ULONG WINAPI outer_addref(IInspectable *iface) { return InterlockedIncrement(&CONTAINING_RECORD(iface, struct app_page, IInspectable_iface)->ref); }
static ULONG WINAPI outer_release(IInspectable *iface) { return InterlockedDecrement(&CONTAINING_RECORD(iface, struct app_page, IInspectable_iface)->ref); }
static HRESULT WINAPI outer_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI outer_name(IInspectable *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI outer_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl outer_vtbl = { outer_qi, outer_addref, outer_release, outer_iids, outer_name, outer_trust };
static struct app_page *from_conn(void *iface) { return CONTAINING_RECORD(iface, struct app_page, connector); }
static struct app_page *from_conn2(void *iface) { return CONTAINING_RECORD(iface, struct app_page, connector2); }
static HRESULT WINAPI conn_qi(void *iface, REFIID iid, void **out) { return outer_qi(&from_conn(iface)->IInspectable_iface, iid, out); }
static ULONG WINAPI conn_addref(void *iface) { return InterlockedIncrement(&from_conn(iface)->ref); }
static ULONG WINAPI conn_release(void *iface) { return InterlockedDecrement(&from_conn(iface)->ref); }
static HRESULT WINAPI conn_connect(void *iface, INT32 id, IInspectable *target) { from_conn(iface)->id = id; from_conn(iface)->target = target; return S_OK; }
static void *conn_vtbl[] = { conn_qi, conn_addref, conn_release, outer_iids, outer_name, outer_trust, conn_connect };
static HRESULT WINAPI conn2_qi(void *iface, REFIID iid, void **out) { return outer_qi(&from_conn2(iface)->IInspectable_iface, iid, out); }
static ULONG WINAPI conn2_addref(void *iface) { return InterlockedIncrement(&from_conn2(iface)->ref); }
static ULONG WINAPI conn2_release(void *iface) { return InterlockedDecrement(&from_conn2(iface)->ref); }
static HRESULT WINAPI conn2_get(void *iface, INT32 id, IInspectable *target, IInspectable **out)
{
    from_conn2(iface)->bind_id = id;
    from_conn2(iface)->bind_target = target;
    binding.ref++;
    *out = (IInspectable *)&binding;
    return S_OK;
}
static void *conn2_vtbl[] = { conn2_qi, conn2_addref, conn2_release, outer_iids, outer_name, outer_trust, conn2_get };

static obj *app_statics, *uri_factory, *grid_factory;
static HRESULT load(IInspectable *component, const WCHAR *name)
{
    WCHAR buf[128];
    obj *uri = NULL;
    swprintf(buf, 128, L"ms-appx:///%ls.xaml", name);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(uri_factory, 6))(uri_factory, hs(buf), &uri);
    return ((HRESULT (WINAPI *)(obj *, IInspectable *, obj *))SLOT(app_statics, 8))(app_statics, component, uri);
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}
static int is_loaded(obj *o) { boolean v = 2; ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(as(o, &IID_FE7), 6))(as(o, &IID_FE7), &v); return v; }

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static HWND make_island(obj *content)
{
    obj *ms, *mgr = NULL, *sf, *src = NULL, *native;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(content, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    return island;
}

int main(void)
{
    struct handler loading = { h_vtbl, 1 }, loaded = { h_vtbl, 1 }, unloaded = { h_vtbl, 1 };
    obj *rs, *grid = NULL, *b, *children = NULL;
    INT64 token;
    HWND island;
    HRESULT hr;
    HDC hdc;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(app_statics = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics))) return 1;
    if (!(uri_factory = factory(L"Windows.Foundation.Uri", &IID_UriFactory))) return 1;
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;

    /* Loading, Loaded, Unloaded */
    ((load_fn)SLOT(rs, 6))(rs, hs(L"<Grid " NS L"><Border x:Name=\"B\" Width=\"10\" Height=\"10\" Background=\"Red\"/></Grid>"), &grid);
    b = find(grid, L"B");
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(b, &IID_FE3), 6))(as(b, &IID_FE3), &loading, &token);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(b, &IID_FE), 43))(as(b, &IID_FE), &loaded, &token);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(b, &IID_FE), 45))(as(b, &IID_FE), &unloaded, &token);
    printf("before=%d,%d,%d\n", loading.calls, loaded.calls, is_loaded(b));
    make_island(grid);
    printf("loaded=%d,%d,%d sender=%d\n", loading.calls, loaded.calls, is_loaded(b),
           as(loaded.last_sender, &IID_IUnknown) == as(b, &IID_IUnknown));
    ((get_fn)SLOT(as(grid, &IID_Panel), 6))(as(grid, &IID_Panel), &children);
    ((HRESULT (WINAPI *)(obj *, UINT32))SLOT(children, 12))(children, 0);
    printf("unloaded=%d,%d\n", unloaded.calls, is_loaded(b));

    /* XamlBindingHelper: what compiled x:Bind code calls */
    {
        obj *bh = factory(L"Windows.UI.Xaml.Markup.XamlBindingHelper", &IID_BindingHelper);
        obj *tbs = factory(L"Windows.UI.Xaml.Controls.TextBlock", &IID_TBStatics), *tb = NULL, *textp = NULL, *sizep = NULL, *comp = NULL;
        HSTRING text = NULL;
        double size = 0;
        ((load_fn)SLOT(rs, 6))(rs, hs(L"<TextBlock " NS L"/>"), &tb);
        ((get_fn)SLOT(tbs, 16))(tbs, &textp);
        ((get_fn)SLOT(tbs, 6))(tbs, &sizep);
        hr = ((HRESULT (WINAPI *)(obj *, obj *, obj *, HSTRING))SLOT(bh, 12))(bh, tb, textp, hs(L"bound"));
        ((HRESULT (WINAPI *)(obj *, obj *, obj *, double))SLOT(bh, 16))(bh, tb, sizep, 31.0);
        ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(as(tb, &IID_TB), 26))(as(tb, &IID_TB), &text);
        ((getd_fn)SLOT(as(tb, &IID_TB), 6))(as(tb, &IID_TB), &size);
        printf("bind_set=%08lx,%ls,%.0f\n", hr, WindowsGetStringRawBuffer(text, NULL), size);
        ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(bh, 8))(bh, tb, (obj *)&binding);
        ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(bh, 7))(bh, tb, &comp);
        printf("template_component=%d\n", comp == (obj *)&binding);
    }

    /* a compiled template's connections */
    {
        struct app_page app = { { &outer_vtbl }, { conn_vtbl }, { conn2_vtbl }, 1 };
        obj *pf = factory(L"Windows.UI.Xaml.Controls.Page", &IID_PageFactory), *pg = NULL;
        ((create_fn)SLOT(pf, 6))(pf, &app.IInspectable_iface, &app.inner, &pg);
        hr = load(&app.IInspectable_iface, L"TemplConn");
        printf("load=%08lx\n", hr);
        island = make_island((obj *)&app.IInspectable_iface);
        printf("binding_connector=%d,%d\n", app.bind_id, as(app.bind_target, &IID_Control) != NULL);
        printf("connected=%d,%d\n", app.id, binding.id);
        hdc = GetDC(island);
        printf("template=%d\n", close_to(GetPixel(hdc, 50, 30), 0, 0, 255));
        ReleaseDC(island, hdc);
    }
    return 0;
}
