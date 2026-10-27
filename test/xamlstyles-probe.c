/* Windows.UI.Xaml resources and styles (patches/sg/6144): ResourceDictionary (element and
 * application resources, keyed and implicit), Style with Setters, BasedOn, a style's values below local
 * ones, {StaticResource}, {ThemeResource} from the desktop's own theme resources. Interface identity
 * per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_Run,            0x59553c83, 0x0e14, 0x49bd, 0xb8, 0x4b, 0xc5, 0x26, 0xf3, 0x03, 0x43, 0x49);
DEFINE_GUID(IID_Inline,         0x0c92712d, 0x1bc9, 0x4931, 0x8c, 0xb1, 0x1a, 0xea, 0xdf, 0x1c, 0xc6, 0x85);
DEFINE_GUID(IID_SCB,            0x9d850850, 0x66f3, 0x48df, 0x9a, 0x8f, 0x82, 0x4b, 0xd5, 0xe0, 0x70, 0xaf);
DEFINE_GUID(IID_SCBFactory,     0xd935ce0c, 0x86f5, 0x4da6, 0x8a, 0x27, 0xb1, 0x61, 0x9e, 0xf7, 0xf9, 0x2b);
DEFINE_GUID(IID_AppFactory,     0x93bbe361, 0xbe5a, 0x4ee3, 0xb4, 0xa3, 0x95, 0x11, 0x8d, 0xc9, 0x7a, 0x89);
DEFINE_GUID(IID_Map,            0xf5f69427, 0x55ed, 0x5512, 0x84, 0x29, 0xd4, 0xf6, 0x62, 0x6d, 0xfc, 0xdd);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
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

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\"><Grid.Resources>"
    L"<SolidColorBrush x:Key=\"Accent\" Color=\"#FF0000FF\"/>"
    L"<Style x:Key=\"Big\" TargetType=\"TextBlock\"><Setter Property=\"FontSize\" Value=\"40\"/>"
    L"<Setter Property=\"Foreground\" Value=\"{StaticResource Accent}\"/></Style>"
    L"<Style TargetType=\"Border\"><Setter Property=\"Background\" Value=\"Red\"/><Setter Property=\"Width\" Value=\"50\"/>"
    L"<Setter Property=\"Height\" Value=\"50\"/></Style>"
    L"<Style x:Key=\"Based\" TargetType=\"TextBlock\" BasedOn=\"{StaticResource Big}\"><Setter Property=\"FontSize\" Value=\"20\"/></Style>"
    L"</Grid.Resources>"
    L"<TextBlock x:Name=\"A\" Style=\"{StaticResource Big}\" Text=\"XX\"/>"
    L"<TextBlock x:Name=\"B\" Style=\"{StaticResource Big}\" FontSize=\"10\" Text=\"XX\"/>"
    L"<TextBlock x:Name=\"C\" Style=\"{StaticResource Based}\" Text=\"XX\"/>"
    L"<Border x:Name=\"D\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Bottom\"/>"
    L"<TextBlock x:Name=\"E\" Foreground=\"{ThemeResource TextFillColorPrimaryBrush}\" Background=\"{StaticResource AppBrush}\" Text=\"X\"/>"
    L"<TextBlock x:Name=\"F\" Foreground=\"{StaticResource AppBrush}\" Text=\"X\"/>"
    L"</Grid>";

/* an application's page: aggregates Page, connects named elements */
struct app_page { IInspectable IInspectable_iface; struct { void **vtbl; } connector; LONG ref; IInspectable *inner; int id; obj *target; };
static HRESULT WINAPI outer_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct app_page *p = CONTAINING_RECORD(iface, struct app_page, IInspectable_iface);
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)) { *out = iface; InterlockedIncrement(&p->ref); return S_OK; }
    if (IsEqualGUID(iid, &IID_Connector)) { *out = &p->connector; InterlockedIncrement(&p->ref); return S_OK; }
    return IInspectable_QueryInterface(p->inner, iid, out);
}
static ULONG WINAPI outer_addref(IInspectable *iface) { return InterlockedIncrement(&CONTAINING_RECORD(iface, struct app_page, IInspectable_iface)->ref); }
static ULONG WINAPI outer_release(IInspectable *iface) { return InterlockedDecrement(&CONTAINING_RECORD(iface, struct app_page, IInspectable_iface)->ref); }
static HRESULT WINAPI outer_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI outer_name(IInspectable *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI outer_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl outer_vtbl = { outer_qi, outer_addref, outer_release, outer_iids, outer_name, outer_trust };
static struct app_page *from_conn(void *iface) { return CONTAINING_RECORD(iface, struct app_page, connector); }
static HRESULT WINAPI conn_qi(void *iface, REFIID iid, void **out) { return outer_qi(&from_conn(iface)->IInspectable_iface, iid, out); }
static ULONG WINAPI conn_addref(void *iface) { return InterlockedIncrement(&from_conn(iface)->ref); }
static ULONG WINAPI conn_release(void *iface) { return InterlockedDecrement(&from_conn(iface)->ref); }
static HRESULT WINAPI conn_connect(void *iface, INT32 id, obj *target) { from_conn(iface)->id = id; from_conn(iface)->target = target; return S_OK; }
static void *conn_vtbl[] = { conn_qi, conn_addref, conn_release, outer_iids, outer_name, outer_trust, conn_connect };

struct size { float w, h; };
struct rect { float x, y, w, h; };
struct color { BYTE a, r, g, b; };
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}
static double font_size(obj *tb) { double v = -1; ((HRESULT (WINAPI *)(obj *, double *))SLOT(as(tb, &IID_TB), 6))(as(tb, &IID_TB), &v); return v; }
static DWORD fg(obj *tb)
{
    obj *b = NULL; struct color c = { 0 };
    ((get_fn)SLOT(as(tb, &IID_TB), 18))(as(tb, &IID_TB), &b);
    if (as(b, &IID_SCB)) ((HRESULT (WINAPI *)(obj *, struct color *))SLOT(as(b, &IID_SCB), 6))(as(b, &IID_SCB), &c);
    return (c.a << 24) | (c.r << 16) | (c.g << 8) | c.b;
}

int main(void)
{
    obj *rs, *grid = NULL, *a, *d, *res = NULL, *found = NULL, *appf, *app = NULL, *appres = NULL, *scbf, *brush = NULL, *key;
    IInspectable *inner = NULL;
    struct color green = { 255, 0, 128, 0 };
    boolean replaced = 0;
    double w = 0, h = 0;
    HRESULT hr;
    COLORREF text = GetSysColor(COLOR_WINDOWTEXT);

    RoInitialize(RO_INIT_SINGLETHREADED);
    /* the application, with a resource of its own */
    appf = factory(L"Windows.UI.Xaml.Application", &IID_AppFactory);
    ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(appf, 6))(appf, NULL, &inner, &app);
    ((get_fn)SLOT(app, 6))(app, &appres);                                         /* Resources */
    scbf = factory(L"Windows.UI.Xaml.Media.SolidColorBrush", &IID_SCBFactory);
    ((HRESULT (WINAPI *)(obj *, struct color, obj **))SLOT(scbf, 6))(scbf, green, &brush);
    key = NULL;
    {
        obj *pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(pvs, 18))(pvs, hs(L"AppBrush"), &key);
    }
    hr = ((HRESULT (WINAPI *)(obj *, obj *, obj *, boolean *))SLOT(as(appres, &IID_Map), 10))(as(appres, &IID_Map), key, brush, &replaced);
    printf("app_resources=%d insert=%08lx\n", appres != NULL, hr);

    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    a = find(grid, L"A");
    printf("style=%.0f,%08lx\n", font_size(a), fg(a));
    printf("local_wins=%.0f\n", font_size(find(grid, L"B")));
    printf("based_on=%.0f,%08lx\n", font_size(find(grid, L"C")), fg(find(grid, L"C")));
    printf("theme_resource=%d\n", fg(find(grid, L"E")) == (0xff000000 | (GetRValue(text) << 16) | (GetGValue(text) << 8) | GetBValue(text)));
    printf("app_resource=%08lx\n", fg(find(grid, L"F")));

    /* the implicit Border style: applied when laid out */
    d = find(grid, L"D");
    {
        obj *ui = as(grid, &IID_UIE);
        struct size avail = { 300, 200 };
        struct rect final = { 0, 0, 300, 200 };
        ((HRESULT (WINAPI *)(obj *, struct size))SLOT(ui, 91))(ui, avail);
        ((HRESULT (WINAPI *)(obj *, struct rect))SLOT(ui, 92))(ui, final);
    }
    ((HRESULT (WINAPI *)(obj *, double *))SLOT(as(d, &IID_FE), 13))(as(d, &IID_FE), &w);
    ((HRESULT (WINAPI *)(obj *, double *))SLOT(as(d, &IID_FE), 14))(as(d, &IID_FE), &h);
    printf("implicit=%.0fx%.0f\n", w, h);

    /* Resources from code */
    ((get_fn)SLOT(as(grid, &IID_FE), 7))(as(grid, &IID_FE), &res);
    {
        obj *pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics), *k = NULL;
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(pvs, 18))(pvs, hs(L"Accent"), &k);
        hr = ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(as(res, &IID_Map), 6))(as(res, &IID_Map), k, &found);
    }
    printf("lookup=%08lx brush=%d\n", hr, as(found, &IID_SCB) != NULL);
    return 0;
}
