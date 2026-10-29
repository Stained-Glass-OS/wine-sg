/* Windows.UI.Xaml measure counts (patches/sg/6184): a Grid measures each child once in a layout pass
 * (Auto rows and columns included, as on Windows) and an element keeps the results of the last few
 * constraints it was measured with while nothing in it changed, so nested panels measure their content
 * a bounded number of times -- an element whose MeasureOverride depends on the space it is given then
 * settles (MUX TabView in Windows Terminal flip-flopped forever).  Interface identity per windows-rs
 * 0.30.0 (MIT OR Apache-2.0). Prints KEY=VALUE. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(IID_FEFactory,   0xdeaee126, 0x03ca, 0x4966, 0xb5, 0x76, 0x60, 0x4c, 0xce, 0x93, 0xb5, 0xe8);
DEFINE_GUID(IID_FEStatics,   0x48383032, 0xfbeb, 0x4f8a, 0xae, 0xd2, 0xee, 0x21, 0xfb, 0x27, 0xa5, 0x7b);
DEFINE_GUID(IID_FE,          0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_UIE,         0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FEOverrides, 0xda007e54, 0xb3c2, 0x4b9a, 0xaa, 0x8e, 0xd3, 0xf0, 0x71, 0x26, 0x2b, 0x97);
DEFINE_GUID(IID_DO,          0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_PVStatics,   0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_ReaderStatics, 0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_Panel,       0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_PV,          0x4bd682dd, 0x7554, 0x40e9, 0x9a, 0x9b, 0x82, 0x65, 0x4e, 0xde, 0x7e, 0x62);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
struct size { float w, h; };
struct rect { float x, y, w, h; };
struct thickness { double l, t, r, b; };
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *measure_fn)(obj *, struct size);
typedef HRESULT (WINAPI *arrange_fn)(obj *, struct rect);
typedef HRESULT (WINAPI *getsize_fn)(obj *, struct size *);
typedef HRESULT (WINAPI *void_fn)(obj *);
typedef HRESULT (WINAPI *putd_fn)(obj *, double);
typedef HRESULT (WINAPI *getd_fn)(obj *, double *);
typedef HRESULT (WINAPI *puti_fn)(obj *, INT32);
typedef HRESULT (WINAPI *putth_fn)(obj *, struct thickness);
typedef HRESULT (WINAPI *getvalue_fn)(obj *, obj *, obj **);
typedef HRESULT (WINAPI *setvalue_fn)(obj *, obj *, obj *);

/* IUIElement slots */
enum { UIE_DesiredSize = 6, UIE_SetVisibility = 22, UIE_RenderSize = 23, UIE_Measure = 91, UIE_Arrange = 92,
       UIE_InvalidateMeasure = 99, UIE_UpdateLayout = 101 };
/* IFrameworkElement slots */
enum { FE_ActualWidth = 13, FE_Width = 15, FE_SetWidth = 16, FE_SetMinWidth = 20, FE_SetMaxHeight = 26,
       FE_SetHorizontalAlignment = 28, FE_SetVerticalAlignment = 30, FE_SetMargin = 32 };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}

/* an application's element: its own MeasureOverride (it wants 100x50) and ArrangeOverride */
struct app_elem
{
    IInspectable IInspectable_iface;
    struct { void **vtbl; } overrides;
    LONG ref;
    IInspectable *inner;
    int measures, arranges;
    struct size measure_avail, arrange_final;
};
static struct app_elem *from_outer(IInspectable *iface) { return CONTAINING_RECORD(iface, struct app_elem, IInspectable_iface); }
static struct app_elem *from_ov(void *iface) { return CONTAINING_RECORD(iface, struct app_elem, overrides); }
static HRESULT WINAPI outer_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct app_elem *a = from_outer(iface);
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)) { *out = iface; InterlockedIncrement(&a->ref); return S_OK; }
    if (IsEqualGUID(iid, &IID_FEOverrides)) { *out = &a->overrides; InterlockedIncrement(&a->ref); return S_OK; }
    return IInspectable_QueryInterface(a->inner, iid, out);
}
static ULONG WINAPI outer_addref(IInspectable *iface) { return InterlockedIncrement(&from_outer(iface)->ref); }
static ULONG WINAPI outer_release(IInspectable *iface) { return InterlockedDecrement(&from_outer(iface)->ref); }
static HRESULT WINAPI outer_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI outer_name(IInspectable *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI outer_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl outer_vtbl = { outer_qi, outer_addref, outer_release, outer_iids, outer_name, outer_trust };
static HRESULT WINAPI ov_qi(void *iface, REFIID iid, void **out) { return outer_qi(&from_ov(iface)->IInspectable_iface, iid, out); }
static ULONG WINAPI ov_addref(void *iface) { return InterlockedIncrement(&from_ov(iface)->ref); }
static ULONG WINAPI ov_release(void *iface) { return InterlockedDecrement(&from_ov(iface)->ref); }
static HRESULT WINAPI ov_iids(void *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI ov_name(void *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI ov_trust(void *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static HRESULT WINAPI ov_measure(void *iface, struct size avail, struct size *result)
{
    struct app_elem *a = from_ov(iface);
    a->measures++;
    a->measure_avail = avail;
    result->w = 100; result->h = 50;
    return S_OK;
}
static HRESULT WINAPI ov_arrange(void *iface, struct size final, struct size *result)
{
    struct app_elem *a = from_ov(iface);
    a->arranges++;
    a->arrange_final = final;
    *result = final;
    return S_OK;
}
static HRESULT WINAPI ov_apply(void *iface) { return S_OK; }
static void *ov_vtbl[] = { ov_qi, ov_addref, ov_release, ov_iids, ov_name, ov_trust, ov_measure, ov_arrange, ov_apply };

static obj *fef;
static struct app_elem *new_app_elem(obj **fe, obj **ui)
{
    struct app_elem *a = calloc(1, sizeof(*a));
    obj *value = NULL;
    a->IInspectable_iface.lpVtbl = &outer_vtbl;
    a->overrides.vtbl = ov_vtbl;
    a->ref = 1;
    ((create_fn)SLOT(fef, 6))(fef, &a->IInspectable_iface, &a->inner, &value);
    ((qi_fn)SLOT(value, 0))(value, &IID_FE, (void **)fe);
    ((qi_fn)SLOT(value, 0))(value, &IID_UIE, (void **)ui);
    return a;
}
static void measure(obj *ui, float w, float h)
{
    struct size avail = { w, h };
    ((measure_fn)SLOT(ui, UIE_Measure))(ui, avail);
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
#define AUTO L"<Grid.RowDefinitions><RowDefinition Height=\"Auto\"/></Grid.RowDefinitions><Grid.ColumnDefinitions><ColumnDefinition Width=\"Auto\"/></Grid.ColumnDefinitions>"

static obj *reader;
static obj *load(const WCHAR *xaml)
{
    HSTRING h;
    obj *r = NULL;
    WindowsCreateString(xaml, lstrlenW(xaml), &h);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(reader, 6))(reader, h, &r);
    return r;
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *fe = NULL, *r = NULL;
    HSTRING h;
    WindowsCreateString(name, lstrlenW(name), &h);
    ((qi_fn)SLOT(root, 0))(root, &IID_FE, (void **)&fe);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(fe, 51))(fe, h, &r);
    return r;
}
static void add_child(obj *panel, obj *ui)
{
    obj *p = NULL, *children = NULL;
    ((qi_fn)SLOT(panel, 0))(panel, &IID_Panel, (void **)&p);
    if (p) ((get_fn)SLOT(p, 6))(p, &children);
    if (children) ((HRESULT (WINAPI *)(obj *, obj *))SLOT(children, 13))(children, ui);
}

int main(void)
{
    struct app_elem *a;
    obj *fe, *ui, *grid, *gui = NULL;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(fef = factory(L"Windows.UI.Xaml.FrameworkElement", &IID_FEFactory))) return 1;
    if (!(reader = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;

    /* the last few constraints kept: 100x100, 50x50, 100x100 measures it twice */
    a = new_app_elem(&fe, &ui);
    measure(ui, 100, 100); measure(ui, 50, 50); measure(ui, 100, 100); measure(ui, 50, 50);
    printf("kept=%d\n", a->measures);
    ((void_fn)SLOT(ui, UIE_InvalidateMeasure))(ui);
    measure(ui, 100, 100);
    printf("invalidated=%d\n", a->measures);

    /* in an Auto row and column of a Grid: once */
    grid = load(L"<Grid " NS L">" AUTO L"</Grid>");
    a = new_app_elem(&fe, &ui);
    add_child(grid, ui);
    ((qi_fn)SLOT(grid, 0))(grid, &IID_UIE, (void **)&gui);
    measure(gui, 300, 300);
    printf("auto_cell=%d\n", a->measures);

    /* five Grids deep, Auto and star: a bounded number of times */
    grid = load(L"<Grid " NS L">" AUTO L"<Grid>" AUTO L"<Grid><Grid>" AUTO L"<Grid x:Name=\"In\"/></Grid></Grid></Grid></Grid>");
    a = new_app_elem(&fe, &ui);
    add_child(find(grid, L"In"), ui);
    ((qi_fn)SLOT(grid, 0))(grid, &IID_UIE, (void **)&gui);
    measure(gui, 300, 300);
    printf("nested=%d\n", a->measures);
    return 0;
}
