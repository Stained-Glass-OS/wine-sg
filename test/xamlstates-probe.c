/* Windows.UI.Xaml visual states and storyboards (patches/sg/6184): VisualStateManager.GoToState over a
 * UserControl's VisualStateGroups from markup -- a state's Setters and its Storyboard's end values,
 * taken back when another state is entered (an animated value above the local one) -- a composable
 * VisualStateManager, a Storyboard of the program's own with Storyboard.SetTarget: Begin, Completed,
 * Stop; a DependencyObject's Dispatcher asked from another thread (Windows Terminal's render thread),
 * ElementCompositionPreview.GetElementVisual; a KeySpline written "0,0,0,1" (6187).  Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_DO,             0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_VSMStatics,     0x01d0e9e0, 0xd713, 0x414e, 0xa7, 0x4e, 0xe6, 0x3e, 0xc7, 0xac, 0x8c, 0x3d);
DEFINE_GUID(IID_VSMFactory,     0x85e598fd, 0xa575, 0x47b6, 0x9e, 0x30, 0x38, 0x3c, 0xd0, 0x85, 0x85, 0xf2);
DEFINE_GUID(IID_VSGroup,        0xe4f9d9a4, 0xe028, 0x44de, 0x9b, 0x15, 0x49, 0x29, 0xae, 0x0a, 0x26, 0xc2);
DEFINE_GUID(IID_VState,         0x6320affc, 0xc31a, 0x4450, 0xaf, 0xde, 0xf6, 0xea, 0x7b, 0xd1, 0xf5, 0x86);
DEFINE_GUID(IID_Storyboard,     0xd45c1e6e, 0x3594, 0x460e, 0x98, 0x1a, 0x32, 0x27, 0x1b, 0xd3, 0xaa, 0x06);
DEFINE_GUID(IID_SBStatics,      0xd82f07d8, 0x73d5, 0x4379, 0xbd, 0x48, 0x7e, 0x05, 0x18, 0x4a, 0x8b, 0xad);
DEFINE_GUID(IID_Timeline,       0x0bc465dc, 0xbe4d, 0x4d0d, 0x95, 0x49, 0x22, 0x08, 0xb7, 0x15, 0xf4, 0x0d);
DEFINE_GUID(IID_DoubleAnim,     0x7e9f3d59, 0x0f07, 0x4bc9, 0x97, 0x7d, 0x03, 0x76, 0x3f, 0xf8, 0x15, 0x4f);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_RefDouble,      0x2f2d6c29, 0x5473, 0x5f3e, 0x92, 0xe7, 0x96, 0x57, 0x2b, 0xb9, 0x90, 0xe2);
DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_ECPStatics,     0x08c92b38, 0xec99, 0x4c55, 0xbc, 0x85, 0xa1, 0xc1, 0x80, 0xb2, 0x76, 0x46);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);
typedef HRESULT (WINAPI *getd_fn)(obj *, double *);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *activate(const WCHAR *name, REFIID iid)
{
    IInspectable *i = NULL;
    obj *r = NULL;
    HRESULT hr = RoActivateInstance(hs(name), &i);
    if (FAILED(hr)) { printf("activate %ls=%08lx\n", name, hr); return NULL; }
    IInspectable_QueryInterface(i, iid, (void **)&r);
    return r;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static IUnknown *ident(void *o) { IUnknown *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, &IID_IUnknown, (void **)&r); return r; }
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

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<UserControl " NS L">"
    L"<Grid x:Name=\"Root\">"
    L"<VisualStateManager.VisualStateGroups>"
    L"<VisualStateGroup x:Name=\"Common\">"
    L"<VisualState x:Name=\"Normal\"/>"
    L"<VisualState x:Name=\"Wide\"><VisualState.Setters><Setter Target=\"Box.Width\" Value=\"80\"/></VisualState.Setters></VisualState>"
    L"<VisualState x:Name=\"Tall\"><Storyboard>"
    L"<DoubleAnimation Storyboard.TargetName=\"Box\" Storyboard.TargetProperty=\"Height\" To=\"33\" Duration=\"0\"/>"
    L"</Storyboard></VisualState>"
    L"<VisualState x:Name=\"Spline\"><Storyboard>"
    L"<DoubleAnimationUsingKeyFrames Storyboard.TargetName=\"Box\" Storyboard.TargetProperty=\"Width\">"
    L"<SplineDoubleKeyFrame KeyTime=\"0:0:0.1\" Value=\"55\" KeySpline=\"0,0,0,1\"/>"
    L"</DoubleAnimationUsingKeyFrames></Storyboard></VisualState>"
    L"</VisualStateGroup>"
    L"</VisualStateManager.VisualStateGroups>"
    L"<Border x:Name=\"Box\" Width=\"20\" Height=\"10\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"</Grid></UserControl>";

static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}
static double fe_double(obj *e, int slot) { double d = -1; ((getd_fn)SLOT(as(e, &IID_FE), slot))(as(e, &IID_FE), &d); return d; }
static double opacity(obj *e) { double d = -1; ((getd_fn)SLOT(as(e, &IID_UIE), 9))(as(e, &IID_UIE), &d); return d; }

static obj *vsm;
static int go(obj *control, const WCHAR *state)
{
    boolean ok = 0;
    HRESULT hr = ((HRESULT (WINAPI *)(obj *, obj *, HSTRING, boolean, boolean *))SLOT(vsm, 10))(vsm, as(control, &IID_Control), hs(state), 0, &ok);
    return SUCCEEDED(hr) && ok;
}
static void current_state(obj *group, WCHAR *out)
{
    obj *state = NULL;
    HSTRING name = NULL;
    lstrcpyW(out, L"(none)");
    ((get_fn)SLOT(group, 9))(group, &state);
    if (state && SUCCEEDED(((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(as(state, &IID_VState), 6))(as(state, &IID_VState), &name)))
        lstrcpyW(out, WindowsGetStringRawBuffer(name, NULL));
}

/* EventHandler<IInspectable> for Storyboard.Completed */
static int completed;
static HRESULT WINAPI h_qi(void *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI h_ref(void *iface) { return 2; }
static HRESULT WINAPI h_invoke(void *iface, IInspectable *sender, IInspectable *args) { completed++; return S_OK; }
static void *h_vtbl[] = { h_qi, h_ref, h_ref, h_invoke };
static obj handler = { h_vtbl };

struct dispatcher_ask { obj *elem; IUnknown *result; };
static DWORD WINAPI ask_dispatcher(void *arg)
{
    struct dispatcher_ask *ask = arg;
    obj *d = NULL;
    RoInitialize(RO_INIT_MULTITHREADED);
    ((get_fn)SLOT(as(ask->elem, &IID_DO), 11))(as(ask->elem, &IID_DO), &d);
    ask->result = ident(d);
    return 0;
}

struct duration { INT64 span; INT32 type; };

int main(void)
{
    obj *rs, *uc = NULL, *box, *root_grid, *groups = NULL, *group = NULL, *vsmf, *made = NULL, *pvs;
    obj *sb, *anim, *sbs, *children = NULL, *to = NULL, *ecp, *visual = NULL, *d = NULL, *ms, *mgr = NULL;
    IInspectable *inner = NULL;
    struct dispatcher_ask ask;
    struct duration dur = { 2000000, 1 };   /* 0.2 s */
    UINT32 n = 0;
    INT64 token = 0;
    WCHAR name[64];
    HANDLE thread;
    HRESULT hr;
    int ok1, ok2, ok3;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    /* XAML on this thread, as an island program has it */
    if (!(ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics))) return 1;
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &uc);
    printf("load=%08lx\n", hr);
    if (!uc) return 1;
    box = find(uc, L"Box"); root_grid = find(uc, L"Root");
    if (!(vsm = factory(L"Windows.UI.Xaml.VisualStateManager", &IID_VSMStatics))) return 1;

    ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(vsm, 6))(vsm, as(root_grid, &IID_FE), &groups);
    if (groups) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(groups, 7))(groups, &n);
    if (groups && n) ((HRESULT (WINAPI *)(obj *, UINT32, obj **))SLOT(groups, 6))(groups, 0, &group);
    group = as(group, &IID_VSGroup);
    printf("groups=%u,%d\n", n, group != NULL);

    ok1 = go(uc, L"Wide");
    current_state(group, name);
    printf("wide=%d,%.0f,%.0f,%ls\n", ok1, fe_double(box, 15), fe_double(box, 17), name);
    ok2 = go(uc, L"Tall");
    current_state(group, name);
    printf("tall=%d,%.0f,%.0f,%ls\n", ok2, fe_double(box, 15), fe_double(box, 17), name);
    ok3 = go(uc, L"Normal");
    printf("normal=%d,%.0f,%.0f\n", ok3, fe_double(box, 15), fe_double(box, 17));
    printf("nostate=%d\n", go(uc, L"NoSuchState"));
    ok1 = go(uc, L"Spline");
    printf("spline=%d,%.0f\n", ok1, fe_double(box, 15));
    go(uc, L"Normal");

    vsmf = factory(L"Windows.UI.Xaml.VisualStateManager", &IID_VSMFactory);
    hr = vsmf ? ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(vsmf, 6))(vsmf, NULL, &inner, &made) : E_FAIL;
    printf("vsm_new=%08lx,%d\n", hr, made != NULL);

    /* a storyboard of the program's: Opacity 1 -> 0.25 in 0.2 s */
    sb = activate(L"Windows.UI.Xaml.Media.Animation.Storyboard", &IID_Storyboard);
    anim = activate(L"Windows.UI.Xaml.Media.Animation.DoubleAnimation", &IID_DoubleAnim);
    sbs = factory(L"Windows.UI.Xaml.Media.Animation.Storyboard", &IID_SBStatics);
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    if (!sb || !anim || !sbs || !pvs) return 1;
    ((HRESULT (WINAPI *)(obj *, double, obj **))SLOT(pvs, 15))(pvs, 0.25, &to);
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(anim, 9))(anim, as(to, &IID_RefDouble));
    ((HRESULT (WINAPI *)(obj *, struct duration))SLOT(as(anim, &IID_Timeline), 11))(as(anim, &IID_Timeline), dur);
    ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(sbs, 12))(sbs, as(anim, &IID_Timeline), as(box, &IID_DO));
    ((HRESULT (WINAPI *)(obj *, obj *, HSTRING))SLOT(sbs, 8))(sbs, as(anim, &IID_Timeline), hs(L"Opacity"));
    ((get_fn)SLOT(sb, 6))(sb, &children);
    if (children) ((HRESULT (WINAPI *)(obj *, obj *))SLOT(children, 13))(children, as(anim, &IID_Timeline));
    ((HRESULT (WINAPI *)(obj *, obj *, INT64 *))SLOT(as(sb, &IID_Timeline), 18))(as(sb, &IID_Timeline), &handler, &token);
    hr = ((HRESULT (WINAPI *)(obj *))SLOT(sb, 9))(sb);
    pump(700);
    printf("begun=%08lx,%.2f,%d\n", hr, opacity(box), completed);
    ((HRESULT (WINAPI *)(obj *))SLOT(sb, 8))(sb);
    printf("stopped=%.2f\n", opacity(box));

    /* the dispatcher of the thread it was made on, asked from another */
    ((get_fn)SLOT(as(box, &IID_DO), 11))(as(box, &IID_DO), &d);
    ask.elem = box; ask.result = NULL;
    thread = CreateThread(NULL, 0, ask_dispatcher, &ask, 0, NULL);
    WaitForSingleObject(thread, 5000);
    printf("dispatcher=%d,%d\n", d != NULL, d != NULL && ask.result == ident(d));

    ecp = factory(L"Windows.UI.Xaml.Hosting.ElementCompositionPreview", &IID_ECPStatics);
    hr = ecp ? ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(ecp, 6))(ecp, as(box, &IID_UIE), &visual) : E_FAIL;
    printf("visual=%08lx,%d\n", hr, visual != NULL);
    return 0;
}
