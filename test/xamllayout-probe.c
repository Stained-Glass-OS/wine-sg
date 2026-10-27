/* Windows.UI.Xaml layout (patches/sg/6133): UIElement.Measure/Arrange around a FrameworkElement's
 * MeasureOverride/ArrangeOverride -- here an application's, as when it derives its own element --
 * with Margin, Width/Height, Min/Max, alignments and Visibility; DesiredSize, RenderSize,
 * ActualWidth; InvalidateMeasure/UpdateLayout; the properties through the property system.
 * Interface identity per windows-rs 0.30.0 (MIT OR Apache-2.0). Prints KEY=VALUE. */
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
static struct size desired(obj *ui) { struct size s = { -1, -1 }; ((getsize_fn)SLOT(ui, UIE_DesiredSize))(ui, &s); return s; }
static struct size rendered(obj *ui) { struct size s = { -1, -1 }; ((getsize_fn)SLOT(ui, UIE_RenderSize))(ui, &s); return s; }
static void layout(obj *ui, float w, float h)
{
    struct size avail = { w, h };
    struct rect final = { 0, 0, w, h };
    ((measure_fn)SLOT(ui, UIE_Measure))(ui, avail);
    ((arrange_fn)SLOT(ui, UIE_Arrange))(ui, final);
}

int main(void)
{
    struct app_elem *a;
    obj *fe, *ui, *fes, *wp = NULL, *pvs, *dobj = NULL, *v = NULL;
    struct size d, r;
    double w = 0;
    int n;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(fef = factory(L"Windows.UI.Xaml.FrameworkElement", &IID_FEFactory))) return 1;
    if (!(fes = factory(L"Windows.UI.Xaml.FrameworkElement", &IID_FEStatics))) return 1;
    if (!(pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics))) return 1;

    /* plain: wants 100x50, stretched over 300x200 */
    a = new_app_elem(&fe, &ui);
    layout(ui, 300, 200);
    d = desired(ui); r = rendered(ui);
    ((getd_fn)SLOT(fe, FE_ActualWidth))(fe, &w);
    printf("plain=%.0fx%.0f avail=%.0fx%.0f final=%.0fx%.0f render=%.0fx%.0f actual=%.0f\n", d.w, d.h,
           a->measure_avail.w, a->measure_avail.h, a->arrange_final.w, a->arrange_final.h, r.w, r.h, w);

    /* a margin: around the element's own sizes */
    a = new_app_elem(&fe, &ui);
    { struct thickness m = { 10, 20, 30, 40 }; ((putth_fn)SLOT(fe, FE_SetMargin))(fe, m); }
    layout(ui, 300, 200);
    d = desired(ui); r = rendered(ui);
    printf("margin=%.0fx%.0f avail=%.0fx%.0f final=%.0fx%.0f render=%.0fx%.0f\n", d.w, d.h,
           a->measure_avail.w, a->measure_avail.h, a->arrange_final.w, a->arrange_final.h, r.w, r.h);

    /* a width, aligned left/top: not stretched */
    a = new_app_elem(&fe, &ui);
    ((putd_fn)SLOT(fe, FE_SetWidth))(fe, 80);
    ((puti_fn)SLOT(fe, FE_SetHorizontalAlignment))(fe, 0);
    ((puti_fn)SLOT(fe, FE_SetVerticalAlignment))(fe, 0);
    layout(ui, 300, 200);
    d = desired(ui); r = rendered(ui);
    printf("width=%.0fx%.0f avail=%.0fx%.0f render=%.0fx%.0f\n", d.w, d.h, a->measure_avail.w, a->measure_avail.h, r.w, r.h);

    /* min and max */
    a = new_app_elem(&fe, &ui);
    ((putd_fn)SLOT(fe, FE_SetMinWidth))(fe, 150);
    ((putd_fn)SLOT(fe, FE_SetMaxHeight))(fe, 30);
    ((puti_fn)SLOT(fe, FE_SetHorizontalAlignment))(fe, 1);
    layout(ui, 300, 200);
    d = desired(ui); r = rendered(ui);
    printf("minmax=%.0fx%.0f render=%.0fx%.0f\n", d.w, d.h, r.w, r.h);

    /* collapsed: no size, not measured */
    a = new_app_elem(&fe, &ui);
    ((puti_fn)SLOT(ui, UIE_SetVisibility))(ui, 1);
    layout(ui, 300, 200);
    d = desired(ui);
    printf("collapsed=%.0fx%.0f measures=%d\n", d.w, d.h, a->measures);

    /* measured once for the same space; again after InvalidateMeasure + UpdateLayout */
    a = new_app_elem(&fe, &ui);
    layout(ui, 300, 200);
    layout(ui, 300, 200);
    n = a->measures;
    ((void_fn)SLOT(ui, UIE_InvalidateMeasure))(ui);
    ((void_fn)SLOT(ui, UIE_UpdateLayout))(ui);
    printf("cached=%d remeasured=%d\n", n, a->measures);
    /* a property change invalidates: Width through SetValue(WidthProperty) */
    ((get_fn)SLOT(fes, 10))(fes, &wp);                                   /* WidthProperty */
    ((qi_fn)SLOT(fe, 0))(fe, &IID_DO, (void **)&dobj);
    ((getvalue_fn)SLOT(dobj, 6))(dobj, wp, &v);
    {
        obj *pv = NULL; double x = 0;
        if (v) ((qi_fn)SLOT(v, 0))(v, &IID_PV, (void **)&pv);
        if (pv) ((getd_fn)SLOT(pv, 16))(pv, &x);                         /* GetDouble */
        printf("width_default_nan=%d\n", pv && isnan(x));
    }
    {
        obj *box = NULL;
        ((HRESULT (WINAPI *)(obj *, double, obj **))SLOT(pvs, 15))(pvs, 120, &box);   /* CreateDouble */
        ((setvalue_fn)SLOT(dobj, 7))(dobj, wp, box);
        ((getd_fn)SLOT(fe, FE_Width))(fe, &w);
        n = a->measures;
        ((void_fn)SLOT(ui, UIE_UpdateLayout))(ui);
        d = desired(ui);
        printf("setvalue_width=%.0f desired=%.0f remeasured=%d\n", w, d.w, a->measures > n);
    }

    /* our own FrameworkElement (not derived): nothing to measure */
    {
        obj *plain = NULL, *pui = NULL;
        IInspectable *inner = NULL;
        ((create_fn)SLOT(fef, 6))(fef, NULL, &inner, &plain);
        ((qi_fn)SLOT(plain, 0))(plain, &IID_UIE, (void **)&pui);
        layout(pui, 300, 200);
        d = desired(pui); r = rendered(pui);
        printf("own=%.0fx%.0f render=%.0fx%.0f\n", d.w, d.h, r.w, r.h);
    }
    return 0;
}
