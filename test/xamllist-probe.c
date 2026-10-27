/* Windows.UI.Xaml items (patches/sg/6157): a ListView shows a container per item -- the item's text,
 * or its DataTemplate made for it -- one under the other; a click selects the item and raises ItemClick
 * (ClickedItem); a compiled x:Bind data template's component gets each item and its index
 * (IDataTemplateComponent.ProcessBindings) -- NanaZip's address bar suggestions. Interface identity per
 * windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_ItemsControl,   0xf4a91dd8, 0xd979, 0x4381, 0x86, 0x52, 0xbd, 0xa0, 0x34, 0x2a, 0x76, 0x5e);
DEFINE_GUID(IID_Selector,       0xe30eb3a5, 0xb36b, 0x42dc, 0x85, 0x27, 0xcd, 0x25, 0x13, 0x6c, 0x08, 0x3c);
DEFINE_GUID(IID_ListViewBase,   0x3d0813ba, 0x6890, 0x4537, 0xbf, 0xe5, 0x79, 0x6d, 0x94, 0x58, 0xed, 0xd6);
DEFINE_GUID(IID_ItemClickArgs,  0x9c314242, 0xf29a, 0x494d, 0xa3, 0xa5, 0xd4, 0xc7, 0xb2, 0xa4, 0x68, 0x52);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_DTComponent,    0x08429dc8, 0x8ab0, 0x4747, 0xaa, 0x9a, 0xfe, 0xad, 0xfc, 0x8d, 0xa8, 0xe1);
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

static obj *as_any(void *o, REFIID iid) { obj *r = NULL; if (o) ((HRESULT (WINAPI *)(obj *, REFIID, void **))((obj *)o)->vtbl[0])(o, iid, (void **)&r); return r; }
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



#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
#define TL L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\""

/* an ItemClick handler */
struct handler { void **vtbl; LONG ref; int calls; IInspectable *clicked; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *sender, IInspectable *args)
{
    obj *a = NULL;
    h->calls++;
    if (args && SUCCEEDED(IInspectable_QueryInterface(args, &IID_ItemClickArgs, (void **)&a)))
        ((HRESULT (WINAPI *)(obj *, IInspectable **))SLOT(a, 6))(a, &h->clicked);
    return S_OK;
}
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

/* a compiled data template's x:Bind component: connected, then given each item */
struct component { void **conn_vtbl; void **dtc_vtbl; LONG ref; int processed; IInspectable *last_item; int last_index; };
static struct component *comp_from_conn(void *iface) { return CONTAINING_RECORD(iface, struct component, conn_vtbl); }
static struct component *comp_from_dtc(void *iface) { return CONTAINING_RECORD(iface, struct component, dtc_vtbl); }
static HRESULT WINAPI comp_qi(struct component *c, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_DTComponent)) *out = &c->dtc_vtbl; else *out = &c->conn_vtbl;
    c->ref++;
    return S_OK;
}
static HRESULT WINAPI cc_qi(void *iface, REFIID iid, void **out) { return comp_qi(comp_from_conn(iface), iid, out); }
static ULONG WINAPI cc_addref(void *iface) { return ++comp_from_conn(iface)->ref; }
static ULONG WINAPI cc_release(void *iface) { return --comp_from_conn(iface)->ref; }
static HRESULT WINAPI cc_iids(void *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI cc_name(void *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI cc_trust(void *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static HRESULT WINAPI cc_connect(void *iface, INT32 id, IInspectable *target) { return S_OK; }
static void *cc_vtbl[] = { cc_qi, cc_addref, cc_release, cc_iids, cc_name, cc_trust, cc_connect };
static HRESULT WINAPI dc_qi(void *iface, REFIID iid, void **out) { return comp_qi(comp_from_dtc(iface), iid, out); }
static ULONG WINAPI dc_addref(void *iface) { return ++comp_from_dtc(iface)->ref; }
static ULONG WINAPI dc_release(void *iface) { return --comp_from_dtc(iface)->ref; }
static HRESULT WINAPI dc_recycle(void *iface) { return S_OK; }
static HRESULT WINAPI dc_process(void *iface, IInspectable *item, INT32 index, INT32 phase, INT32 *next)
{
    struct component *c = comp_from_dtc(iface);
    c->processed++;
    c->last_item = item;
    c->last_index = index;
    *next = -1;
    return S_OK;
}
static void *dc_vtbl[] = { dc_qi, dc_addref, dc_release, cc_iids, cc_name, cc_trust, dc_recycle, dc_process };
static struct component comp = { cc_vtbl, dc_vtbl, 1 };
static obj *binding_helper;

/* an application's page: IComponentConnector2 gives the data template's root its component */
struct app_page { IInspectable IInspectable_iface; struct { void **vtbl; } connector, connector2; LONG ref; IInspectable *inner; };
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
static HRESULT WINAPI conn_connect(void *iface, INT32 id, IInspectable *target) { return S_OK; }
static void *conn_vtbl[] = { conn_qi, conn_addref, conn_release, outer_iids, outer_name, outer_trust, conn_connect };
static HRESULT WINAPI conn2_qi(void *iface, REFIID iid, void **out) { return outer_qi(&from_conn2(iface)->IInspectable_iface, iid, out); }
static ULONG WINAPI conn2_addref(void *iface) { return InterlockedIncrement(&from_conn2(iface)->ref); }
static ULONG WINAPI conn2_release(void *iface) { return InterlockedDecrement(&from_conn2(iface)->ref); }
static HRESULT WINAPI conn2_get(void *iface, INT32 id, IInspectable *target, IInspectable **out)
{
    if (id != 7) { *out = NULL; return S_OK; }
    /* as compiled code does: the root keeps its component */
    ((HRESULT (WINAPI *)(obj *, IInspectable *, void *))SLOT(binding_helper, 8))(binding_helper, target, &comp.conn_vtbl);
    comp.ref++;
    *out = (IInspectable *)&comp.conn_vtbl;
    return S_OK;
}
static void *conn2_vtbl[] = { conn2_qi, conn2_addref, conn2_release, outer_iids, outer_name, outer_trust, conn2_get };

static obj *app_statics, *uri_factory;
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

static HWND make_island(obj *content, int x)
{
    obj *ms, *mgr = NULL, *sf, *src = NULL, *native;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, x, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(content, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    return island;
}

static int dark_in(HDC hdc, int x0, int y0, int x1, int y1)
{
    int x, y, n = 0;
    for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) n++;
    return n;
}

static void append(obj *list, IInspectable *item)
{
    obj *items = NULL;
    ((get_fn)SLOT(as(list, &IID_ItemsControl), 8))(as(list, &IID_ItemsControl), &items);
    ((HRESULT (WINAPI *)(obj *, IInspectable *))SLOT(items, 13))(items, item);
}

int main(void)
{
    static const WCHAR page_xaml[] =
        L"<Grid " NS L" Background=\"White\">"
        L"<ListView x:Name=\"Text\"" TL L" Width=\"140\" IsItemClickEnabled=\"True\"/>"
        L"<ListView x:Name=\"Templ\"" TL L" Margin=\"150,0,0,0\" Width=\"140\"><ListView.ItemTemplate><DataTemplate>"
        L"<Border Height=\"20\" Background=\"Blue\"/></DataTemplate></ListView.ItemTemplate></ListView>"
        L"</Grid>";
    struct handler click = { h_vtbl, 1 };
    obj *rs, *grid = NULL, *pvs, *text, *templ;
    IInspectable *one = NULL, *two = NULL;
    HWND island;
    INT64 token;
    INT32 index = -2;
    HRESULT hr;
    HDC hdc;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(app_statics = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics))) return 1;
    if (!(uri_factory = factory(L"Windows.Foundation.Uri", &IID_UriFactory))) return 1;
    binding_helper = factory(L"Windows.UI.Xaml.Markup.XamlBindingHelper", &IID_BindingHelper);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(L"XXXX"), &one);
    ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(L"YYYY"), &two);
    text = find(grid, L"Text");
    templ = find(grid, L"Templ");
    append(text, one); append(text, two);
    append(templ, one); append(templ, two);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(text, &IID_ListViewBase), 23))(as(text, &IID_ListViewBase), &click, &token);

    island = make_island(grid, 0);
    hdc = GetDC(island);
    printf("text_items=%d,%d\n", dark_in(hdc, 8, 2, 120, 30) > 15, dark_in(hdc, 8, 32, 120, 62) > 15);
    printf("templated_items=%d,%d,%d\n", close_to(GetPixel(hdc, 200, 16), 0, 0, 255), close_to(GetPixel(hdc, 200, 48), 0, 0, 255),
           close_to(GetPixel(hdc, 200, 70), 255, 255, 255));
    ReleaseDC(island, hdc);

    /* a click on the second item */
    SendMessageW(island, WM_MOUSEMOVE, 0, MAKELPARAM(50, 45));
    SendMessageW(island, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(50, 45));
    SendMessageW(island, WM_LBUTTONUP, 0, MAKELPARAM(50, 45));
    pump(200);
    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(text, &IID_Selector), 6))(as(text, &IID_Selector), &index);
    printf("clicked=%d,%d,%d\n", index, click.calls, click.clicked == one ? 1 : click.clicked == two ? 2 : 0);

    /* a compiled data template with x:Bind: its component gets each item */
    {
        struct app_page app = { { &outer_vtbl }, { conn_vtbl }, { conn2_vtbl }, 1 };
        obj *pf = factory(L"Windows.UI.Xaml.Controls.Page", &IID_PageFactory), *pg = NULL, *list;
        ((create_fn)SLOT(pf, 6))(pf, &app.IInspectable_iface, &app.inner, &pg);
        hr = load(&app.IInspectable_iface, L"DataList");
        list = find(pg, L"List");
        append(list, one); append(list, two);
        make_island((obj *)&app.IInspectable_iface, 0);
        printf("x_bind=%08lx,%d,%d,%d\n", hr, comp.processed, comp.last_index, comp.last_item == two);
    }
    return 0;
}
