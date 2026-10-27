/* Windows.UI.Xaml property system (patches/sg/6131): DependencyProperty.Register/RegisterAttached,
 * PropertyMetadata (default value, default factory, change callback), DependencyObject values
 * (GetValue, SetValue, ClearValue, ReadLocalValue, UnsetValue) and change callbacks, as applications
 * and our own XAML classes use them. Interface identity per upstream Wine's windows.ui.xaml.idl.
 * Prints KEY=VALUE. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_DPStatics,   0x49e5f28f, 0x8259, 0x4d5c, 0xaa, 0xe0, 0x83, 0xd5, 0x6d, 0xbb, 0x68, 0xd9);
DEFINE_GUID(IID_MDStatics,   0x3b01077a, 0x6e06, 0x45e9, 0x8b, 0x5c, 0xaf, 0x24, 0x34, 0x58, 0xc0, 0x62);
DEFINE_GUID(IID_DOFactory,   0x9a03af92, 0x7d8a, 0x4937, 0x88, 0x4f, 0xec, 0xf3, 0x4f, 0xe0, 0x2a, 0xcb);
DEFINE_GUID(IID_DO2,         0x29fed85d, 0x3d22, 0x43a1, 0xad, 0xd0, 0x17, 0x02, 0x7c, 0x08, 0xb2, 0x12);
DEFINE_GUID(IID_PVStatics,   0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_PV,          0x4bd682dd, 0x7554, 0x40e9, 0x9a, 0x9b, 0x82, 0x65, 0x4e, 0xde, 0x7e, 0x62);
DEFINE_GUID(IID_PropChanged, 0x5a9f8a25, 0xd142, 0x44a4, 0x82, 0x31, 0xfd, 0x67, 0x67, 0x24, 0xf2, 0x9b);
DEFINE_GUID(IID_DPChanged,   0x45883d16, 0x27bf, 0x4bc1, 0xac, 0x26, 0x94, 0xc1, 0x60, 0x1f, 0x3a, 0x49);
DEFINE_GUID(IID_MakeDefault, 0xd6ecb12c, 0x15b5, 0x4ec8, 0xb9, 0x5c, 0xcd, 0xd2, 0x08, 0xf0, 0x81, 0x53);
DEFINE_GUID(IID_Marker,      0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
struct type_name { HSTRING name; INT32 kind; };
typedef HRESULT (WINAPI *register_fn)(obj *, HSTRING, struct type_name, struct type_name, obj *, obj **);
typedef HRESULT (WINAPI *getvalue_fn)(obj *, obj *, obj **);
typedef HRESULT (WINAPI *setvalue_fn)(obj *, obj *, obj *);
typedef HRESULT (WINAPI *clear_fn)(obj *, obj *);
typedef HRESULT (WINAPI *md1_fn)(obj *, void *, obj **);
typedef HRESULT (WINAPI *md2_fn)(obj *, void *, void *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *regcb_fn)(obj *, obj *, void *, INT64 *);
typedef HRESULT (WINAPI *unregcb_fn)(obj *, obj *, INT64);
typedef HRESULT (WINAPI *box_fn)(obj *, INT32, obj **);
typedef HRESULT (WINAPI *unbox_fn)(obj *, INT32 *);
typedef HRESULT (WINAPI *getmd_fn)(obj *, struct type_name, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}

static obj *pv_statics;
static obj *box(INT32 v) { obj *o = NULL; ((box_fn)SLOT(pv_statics, 10))(pv_statics, v, &o); return o; }   /* CreateInt32 */
/* a boxed Int32's value, -1000 for null, -2000 for something else */
static int unbox(obj *o)
{
    obj *pv = NULL;
    INT32 v = -2000;
    if (!o) return -1000;
    if (FAILED(((qi_fn)SLOT(o, 0))(o, &IID_PV, (void **)&pv))) return -2000;
    ((unbox_fn)SLOT(pv, 11))(pv, &v);                                                                           /* GetInt32 */
    return v;
}
static IUnknown *identity(void *o)
{
    IUnknown *unk = NULL;
    if (o) ((qi_fn)SLOT((obj *)o, 0))(o, &IID_IUnknown, (void **)&unk);
    return unk;
}

/* delegates */
struct delegate { void **vtbl; LONG ref; const GUID *iid; int calls; obj *last_sender, *last_dp; int old_v, new_v; };
static HRESULT WINAPI d_qi(struct delegate *d, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, d->iid) || IsEqualGUID(iid, &IID_IAgileObject))
    { *out = d; InterlockedIncrement(&d->ref); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI d_addref(struct delegate *d) { return InterlockedIncrement(&d->ref); }
static ULONG WINAPI d_release(struct delegate *d) { return InterlockedDecrement(&d->ref); }
/* PropertyChangedCallback(obj, args) */
static HRESULT WINAPI prop_changed(struct delegate *d, obj *sender, obj *args)
{
    obj *v = NULL;
    d->calls++;
    d->last_sender = sender;
    ((get_fn)SLOT(args, 6))(args, &d->last_dp);   /* Property */
    ((get_fn)SLOT(args, 7))(args, &v);            /* OldValue */
    d->old_v = unbox(v);
    v = NULL;
    ((get_fn)SLOT(args, 8))(args, &v);            /* NewValue */
    d->new_v = unbox(v);
    return S_OK;
}
/* DependencyPropertyChangedCallback(sender, dp) */
static HRESULT WINAPI dp_changed(struct delegate *d, obj *sender, obj *dp) { d->calls++; d->last_sender = sender; d->last_dp = dp; return S_OK; }
/* CreateDefaultValueCallback(result) */
static HRESULT WINAPI make_default(struct delegate *d, obj **result) { d->calls++; *result = box(100 + d->calls); return S_OK; }
static void *prop_changed_vtbl[] = { d_qi, d_addref, d_release, prop_changed };
static void *dp_changed_vtbl[] = { d_qi, d_addref, d_release, dp_changed };
static void *make_default_vtbl[] = { d_qi, d_addref, d_release, make_default };

/* an outer object, as an application's own DependencyObject class is */
struct outer { IInspectable IInspectable_iface; LONG ref; IInspectable *inner; };
static HRESULT WINAPI outer_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct outer *o = CONTAINING_RECORD(iface, struct outer, IInspectable_iface);
    if (IsEqualGUID(iid, &IID_Marker) || IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable))
    { *out = iface; InterlockedIncrement(&o->ref); return S_OK; }
    return IInspectable_QueryInterface(o->inner, iid, out);
}
static ULONG WINAPI outer_addref(IInspectable *iface) { return InterlockedIncrement(&CONTAINING_RECORD(iface, struct outer, IInspectable_iface)->ref); }
static ULONG WINAPI outer_release(IInspectable *iface) { return InterlockedDecrement(&CONTAINING_RECORD(iface, struct outer, IInspectable_iface)->ref); }
static HRESULT WINAPI outer_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI outer_name(IInspectable *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI outer_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl outer_vtbl = { outer_qi, outer_addref, outer_release, outer_iids, outer_name, outer_trust };

static obj *get(obj *o, obj *dp) { obj *v = (obj *)1; ((getvalue_fn)SLOT(o, 6))(o, dp, &v); return v; }
static obj *local(obj *o, obj *dp) { obj *v = (obj *)1; ((getvalue_fn)SLOT(o, 9))(o, dp, &v); return v; }
static HRESULT set(obj *o, obj *dp, obj *v) { return ((setvalue_fn)SLOT(o, 7))(o, dp, v); }

int main(void)
{
    struct delegate pc = { prop_changed_vtbl, 1, &IID_PropChanged }, dc = { dp_changed_vtbl, 1, &IID_DPChanged };
    struct delegate mk = { make_default_vtbl, 1, &IID_MakeDefault };
    struct type_name tint = { NULL, 0 }, towner = { NULL, 2 };
    obj *dps, *mds, *dof, *unset = NULL, *unset2 = NULL, *md = NULL, *md2 = NULL, *dp = NULL, *dp2 = NULL, *dp3 = NULL;
    obj *o = NULL, *o2 = NULL, *o3 = NULL, *do2 = NULL, *md_back = NULL, *agg = NULL, *marker = NULL;
    IInspectable *inner = NULL;
    struct outer outer = { { &outer_vtbl }, 1, NULL };
    INT64 token = 0;
    HRESULT hr;
    int calls;

    RoInitialize(RO_INIT_SINGLETHREADED);
    tint.name = hs(L"Int32");
    towner.name = hs(L"Probe.Owner");
    if (!(pv_statics = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics))) return 1;
    if (!(dps = factory(L"Windows.UI.Xaml.DependencyProperty", &IID_DPStatics))) return 1;
    if (!(mds = factory(L"Windows.UI.Xaml.PropertyMetadata", &IID_MDStatics))) return 1;
    if (!(dof = factory(L"Windows.UI.Xaml.DependencyObject", &IID_DOFactory))) return 1;

    ((get_fn)SLOT(dps, 6))(dps, &unset);                                            /* UnsetValue */
    ((get_fn)SLOT(dps, 6))(dps, &unset2);
    printf("unset=%d\n", unset && identity(unset) == identity(unset2));

    hr = ((md2_fn)SLOT(mds, 7))(mds, box(42), &pc, &md);                           /* CreateWithDefaultValueAndCallback */
    printf("metadata=%08lx\n", hr);
    hr = ((register_fn)SLOT(dps, 7))(dps, hs(L"Answer"), tint, towner, md, &dp);  /* Register */
    printf("register=%08lx\n", hr);
    if (!dp) return 1;
    hr = ((getmd_fn)SLOT(dp, 6))(dp, towner, &md_back);                            /* GetMetadata */
    printf("getmetadata=%d\n", SUCCEEDED(hr) && identity(md_back) == identity(md));

    hr = ((create_fn)SLOT(dof, 6))(dof, NULL, &inner, &o);                          /* CreateInstance */
    printf("object=%08lx\n", hr);
    if (!o) return 1;
    printf("default=%d\n", unbox(get(o, dp)));
    printf("local_unset=%d\n", identity(local(o, dp)) == identity(unset));

    hr = set(o, dp, box(7));
    printf("set=%08lx value=%d local=%d\n", hr, unbox(get(o, dp)), unbox(local(o, dp)));
    printf("changed=%d old=%d new=%d dp=%d sender=%d\n", pc.calls, pc.old_v, pc.new_v,
           identity(pc.last_dp) == identity(dp), identity(pc.last_sender) == identity(o));
    calls = pc.calls;
    set(o, dp, box(7));
    printf("same_value_quiet=%d\n", pc.calls == calls);

    ((qi_fn)SLOT(o, 0))(o, &IID_DO2, (void **)&do2);
    hr = ((regcb_fn)SLOT(do2, 6))(do2, dp, &dc, &token);                            /* RegisterPropertyChangedCallback */
    set(o, dp, box(8));
    printf("registered=%08lx calls=%d dp=%d sender=%d\n", hr, dc.calls, identity(dc.last_dp) == identity(dp),
           identity(dc.last_sender) == identity(o));
    ((unregcb_fn)SLOT(do2, 7))(do2, dp, token);                                     /* Unregister */
    set(o, dp, box(9));
    printf("unregistered=%d\n", dc.calls == 1);

    calls = pc.calls;
    ((clear_fn)SLOT(o, 8))(o, dp);                                                  /* ClearValue */
    printf("cleared=%d unset=%d changed=%d old=%d new=%d\n", unbox(get(o, dp)),
           identity(local(o, dp)) == identity(unset), pc.calls - calls, pc.old_v, pc.new_v);
    set(o, dp, box(5));
    set(o, dp, unset);
    printf("set_unset_clears=%d\n", unbox(get(o, dp)) == 42 && identity(local(o, dp)) == identity(unset));
    set(o, dp, NULL);
    printf("null_is_a_value=%d\n", get(o, dp) == NULL && local(o, dp) == NULL);

    /* a default from a factory: made once per object */
    ((md1_fn)SLOT(mds, 8))(mds, &mk, &md2);                                        /* CreateWithFactory */
    ((register_fn)SLOT(dps, 7))(dps, hs(L"Made"), tint, towner, md2, &dp2);
    ((create_fn)SLOT(dof, 6))(dof, NULL, &inner, &o2);
    {
        int a = unbox(get(o2, dp2)), b = unbox(get(o2, dp2));
        int c = unbox(get(o, dp2));
        printf("made_default=%d,%d,%d calls=%d\n", a, b, c, mk.calls);
    }

    /* an attached property, set on an object of another type */
    ((register_fn)SLOT(dps, 8))(dps, hs(L"Row"), tint, towner, NULL, &dp3);        /* RegisterAttached */
    printf("attached_default_null=%d\n", dp3 && get(o2, dp3) == NULL);
    set(o2, dp3, box(3));
    printf("attached=%d other=%d\n", unbox(get(o2, dp3)), unbox(get(o, dp3)));

    /* derived by an application (aggregated) */
    hr = ((create_fn)SLOT(dof, 6))(dof, &outer.IInspectable_iface, &inner, &agg);
    outer.inner = inner;
    hr = ((qi_fn)SLOT(agg, 0))(agg, &IID_Marker, (void **)&marker);
    printf("aggregated=%d\n", hr == S_OK && marker == (obj *)&outer.IInspectable_iface);
    set(agg, dp, box(11));
    printf("aggregated_value=%d sender_outer=%d\n", unbox(get(agg, dp)),
           identity(pc.last_sender) == (IUnknown *)&outer.IInspectable_iface);
    (void)o3;
    return 0;
}
