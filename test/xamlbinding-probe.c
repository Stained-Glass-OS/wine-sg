/* Windows.UI.Xaml bindings in templates (patches/sg/6190): {Binding Path, RelativeSource TemplatedParent}
 * as Binding and RelativeSource objects, a Boolean shown on a Visibility; compiled markup's
 * {TemplateBinding X} as an object; a property a program registered for its own control class (WinUI 2's
 * TabView.IsAddTabButtonVisible, TabStripFooter -- Windows Terminal's new-tab button) found by its
 * templates.  Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_ControlProtected, 0x1791587a, 0x2a7c, 0x4981, 0xa2, 0xd8, 0xab, 0x99, 0xff, 0xfc, 0x65, 0xb9);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_SCBFactory,     0xd935ce0c, 0x86f5, 0x4da6, 0x8a, 0x27, 0xb1, 0x61, 0x9e, 0xf7, 0xf9, 0x2b);
DEFINE_GUID(IID_Brush,          0x8806a321, 0x1e06, 0x422c, 0xa1, 0xcc, 0x01, 0x69, 0x65, 0x59, 0xe0, 0x21);
DEFINE_GUID(IID_Panel,         0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_AppFactory,     0x93bbe361, 0xbe5a, 0x4ee3, 0xb4, 0xa3, 0x95, 0x11, 0x8d, 0xc9, 0x7a, 0x89);
DEFINE_GUID(IID_RDFactory,      0xea3639b5, 0x31b7, 0x4271, 0x92, 0xc9, 0x7c, 0x95, 0x84, 0xa9, 0x1c, 0x22);
DEFINE_GUID(IID_RD,             0xc1ea4f24, 0xd6de, 0x4191, 0x8e, 0x3a, 0xf4, 0x86, 0x01, 0xf7, 0x48, 0x9c);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_TextBox,        0xe48f5a8b, 0x1dff, 0x4352, 0xa1, 0xf4, 0xe5, 0x16, 0x51, 0x4e, 0xc8, 0x82);
DEFINE_GUID(IID_ItemsControl,   0xf4a91dd8, 0xd979, 0x4381, 0x86, 0x52, 0xbd, 0xa0, 0x34, 0x2a, 0x76, 0x5e);
DEFINE_GUID(IID_UIE7,           0xcafc4968, 0x6369, 0x4249, 0x80, 0xf9, 0x3d, 0x65, 0x63, 0x19, 0xe8, 0x11);
DEFINE_GUID(IID_KeyArgs,        0xd4cd3dfe, 0x4079, 0x42e9, 0xa3, 0x9a, 0x30, 0x95, 0xd3, 0xf0, 0x49, 0xc6);
DEFINE_GUID(IID_ButtonBase,     0xfa002c1a, 0x494e, 0x46cf, 0x91, 0xd4, 0xe1, 0x4a, 0x8d, 0x79, 0x86, 0x74);
DEFINE_GUID(IID_TakeFocusArgs,  0xfe61e4b9, 0xa7af, 0x52b3, 0xbd, 0xb9, 0xc3, 0x30, 0x5c, 0x0b, 0x8d, 0xf2);
DEFINE_GUID(IID_NavRequest,     0xfbb93bb5, 0x1496, 0x5a80, 0xac, 0x00, 0xe7, 0x57, 0x35, 0x97, 0x55, 0xe6);
DEFINE_GUID(IID_NavRequestFactory, 0xe746ab8f, 0xb4ef, 0x5390, 0x97, 0xe5, 0xcc, 0x0a, 0x27, 0x79, 0xc5, 0x74);
DEFINE_GUID(IID_NavResult,      0x88d55a5f, 0x9603, 0x5d8f, 0x9c, 0xc7, 0xd1, 0xc4, 0x07, 0x0d, 0x98, 0x01);
DEFINE_GUID(IID_UIE5,           0x8eed9bc2, 0xa58c, 0x4453, 0xaf, 0x0f, 0xa9, 0x2e, 0xe0, 0x6d, 0x03, 0x17);
DEFINE_GUID(IID_GT,             0xa06798b7, 0xa2ec, 0x415f, 0xad, 0xe2, 0xea, 0xde, 0x93, 0x33, 0xf2, 0xc7);
DEFINE_GUID(IID_DPStatics,      0x49e5f28f, 0x8259, 0x4d5c, 0xaa, 0xe0, 0x83, 0xd5, 0x6d, 0xbb, 0x68, 0xd9);
DEFINE_GUID(IID_CCFactory,      0xab017b19, 0x196e, 0x4ed5, 0xa2, 0x7f, 0x55, 0xc5, 0x8f, 0xb1, 0x67, 0x3f);
DEFINE_GUID(IID_TBStatics,      0xaf1eab12, 0x0128, 0x4f67, 0x9c, 0x5a, 0x82, 0x32, 0x0c, 0x44, 0x5d, 0x19);
DEFINE_GUID(IID_VTHStatics,     0xe75758c4, 0xd25d, 0x4b1d, 0x97, 0x1f, 0x59, 0x6f, 0x17, 0xf1, 0x2b, 0xaa);
DEFINE_GUID(IID_BHStatics,      0xf65cfb71, 0xc80c, 0x4ffa, 0x86, 0xee, 0x55, 0x87, 0x54, 0xee, 0x33, 0x6d);
DEFINE_GUID(IID_UIEStatics,     0x58d3573b, 0xf52c, 0x45be, 0x98, 0x8b, 0xa5, 0x86, 0x95, 0x64, 0x87, 0x3c);
DEFINE_GUID(IID_PV,           0x4bd682dd, 0x7554, 0x40e9, 0x9a, 0x9b, 0x82, 0x65, 0x4e, 0xde, 0x7e, 0x62);
DEFINE_GUID(IID_RefVisibility,  0xc19bf229, 0x815a, 0x5cbd, 0xa5, 0x06, 0xe3, 0x1f, 0x9b, 0x67, 0xe3, 0x17);   /* IReference<Visibility>, windows-rs */
DEFINE_GUID(IID_DObj,           0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
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
    L"<StackPanel " NS L">"
    L"<ToggleButton x:Name=\"T\"><ToggleButton.Template><ControlTemplate TargetType=\"ToggleButton\">"
    L"<Border Width=\"10\" Height=\"10\"><Border.Visibility>"
    L"<Binding Path=\"IsChecked\"><Binding.RelativeSource><RelativeSource Mode=\"TemplatedParent\"/></Binding.RelativeSource></Binding>"
    L"</Border.Visibility></Border></ControlTemplate></ToggleButton.Template></ToggleButton>"
    L"</StackPanel>";

static const WCHAR template_xaml[] =
    L"<ControlTemplate " NS L" TargetType=\"ContentControl\"><ContentPresenter>"
    L"<ContentPresenter.Content><TemplateBinding Property=\"Extra\"/></ContentPresenter.Content>"
    L"</ContentPresenter></ControlTemplate>";

static obj *vth;
static obj *child(obj *e, int i)
{
    obj *c = NULL;
    if (e) ((HRESULT (WINAPI *)(obj *, obj *, INT32, obj **))SLOT(vth, 10))(vth, as(e, &IID_DObj), i, &c);
    return c;
}
static int count(obj *e)
{
    INT32 n = -1;
    if (e) ((HRESULT (WINAPI *)(obj *, obj *, INT32 *))SLOT(vth, 11))(vth, as(e, &IID_DObj), &n);
    return n;
}
static INT32 visibility(obj *e) { INT32 v = -1; if (e) ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(e, &IID_UIE), 21))(as(e, &IID_UIE), &v); return v; }
static IUnknown *ident(void *o) { IUnknown *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, &IID_IUnknown, (void **)&r); return r; }

/* a program's control: a ContentControl composed under its own class name */
struct prog_ctl { IInspectable IInspectable_iface; LONG ref; IInspectable *inner; };
static struct prog_ctl *from_outer(IInspectable *iface) { return CONTAINING_RECORD(iface, struct prog_ctl, IInspectable_iface); }
static HRESULT WINAPI outer_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct prog_ctl *c = from_outer(iface);
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)) { *out = iface; InterlockedIncrement(&c->ref); return S_OK; }
    return IInspectable_QueryInterface(c->inner, iid, out);
}
static ULONG WINAPI outer_addref(IInspectable *iface) { return InterlockedIncrement(&from_outer(iface)->ref); }
static ULONG WINAPI outer_release(IInspectable *iface) { return InterlockedDecrement(&from_outer(iface)->ref); }
static HRESULT WINAPI outer_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI outer_name(IInspectable *iface, HSTRING *name) { return WindowsCreateString(L"Probe.Ctl", 9, name); }
static HRESULT WINAPI outer_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl outer_vtbl = { outer_qi, outer_addref, outer_release, outer_iids, outer_name, outer_trust };

struct type_name { HSTRING name; INT32 kind; };

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *t, *border, *tbs, *prop = NULL, *pvs, *yes = NULL;
    obj *dps, *dp = NULL, *ccf, *value = NULL, *templ = NULL, *panel, *text = NULL, *presenter;
    struct prog_ctl ctl = { { &outer_vtbl }, 1 };
    struct type_name tn_obj = { NULL, 0 }, tn_owner = { NULL, 2 };
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    INT32 before;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    if (!(vth = factory(L"Windows.UI.Xaml.Media.VisualTreeHelper", &IID_VTHStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(L"T"), &t);

    /* a program's control with a property it registered, shown by a template's {TemplateBinding} object */
    dps = factory(L"Windows.UI.Xaml.DependencyProperty", &IID_DPStatics);
    tn_obj.name = hs(L"Object"); tn_owner.name = hs(L"Probe.Ctl");
    hr = dps ? ((HRESULT (WINAPI *)(obj *, HSTRING, struct type_name, struct type_name, obj *, obj **))SLOT(dps, 7))(dps, hs(L"Extra"), tn_obj, tn_owner, NULL, &dp) : E_FAIL;
    ccf = factory(L"Windows.UI.Xaml.Controls.ContentControl", &IID_CCFactory);
    if (ccf) ((create_fn)SLOT(ccf, 6))(ccf, &ctl.IInspectable_iface, &ctl.inner, &value);
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(template_xaml), &templ);
    printf("template=%08lx,%d,%d\n", hr, dp != NULL, value != NULL);
    if (value && templ) ((putobj_fn)SLOT(as(value, &IID_Control), 29))(as(value, &IID_Control), templ);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(rs, 6))(rs, hs(L"<TextBlock " NS L" Text=\"extra\"/>"), &text);
    if (value && dp) ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(as(value, &IID_DObj), 7))(as(value, &IID_DObj), dp, text);
    panel = as(root, &IID_Panel);
    if (panel && value)
    {
        obj *children = NULL;
        ((get_fn)SLOT(panel, 6))(panel, &children);
        if (children) ((HRESULT (WINAPI *)(obj *, obj *))SLOT(children, 13))(children, as(value, &IID_UIE));
    }

    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);

    /* {Binding IsChecked, RelativeSource TemplatedParent} on a Visibility: Collapsed, then Visible */
    border = child(t, 0);
    before = visibility(border);
    tbs = factory(L"Windows.UI.Xaml.Controls.Primitives.ToggleButton", &IID_TBStatics);
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    if (tbs) ((get_fn)SLOT(tbs, 6))(tbs, &prop);
    if (pvs) ((HRESULT (WINAPI *)(obj *, boolean, obj **))SLOT(pvs, 17))(pvs, TRUE, &yes);
    if (prop && yes) ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(as(t, &IID_DObj), 7))(as(t, &IID_DObj), prop, yes);
    printf("binding=%d,%d\n", before, visibility(border));

    /* compiled bindings (x:Bind): a Boolean set on, or converted to, a Visibility */
    {
        obj *bh = factory(L"Windows.UI.Xaml.Markup.XamlBindingHelper", &IID_BHStatics);
        obj *uies = factory(L"Windows.UI.Xaml.UIElement", &IID_UIEStatics), *vprop = NULL, *conv = NULL, *pv;
        struct type_name tn_vis = { hs(L"Windows.UI.Xaml.Visibility"), 0 };
        INT32 v = -1;
        if (uies) ((get_fn)SLOT(uies, 36))(uies, &vprop);
        if (bh && vprop) ((HRESULT (WINAPI *)(obj *, obj *, obj *, boolean))SLOT(bh, 13))(bh, as(border, &IID_DObj), vprop, FALSE);
        if (bh) ((HRESULT (WINAPI *)(obj *, struct type_name, obj *, obj **))SLOT(bh, 11))(bh, tn_vis, yes, &conv);
        if ((pv = as(conv, &IID_RefVisibility))) ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(pv, 6))(pv, &v);
        printf("xbind=%d,%d\n", visibility(border), v);
    }

    presenter = child(value, 0);
    printf("program_property=%d,%d\n", count(presenter), count(presenter) == 1 && ident(child(presenter, 0)) == ident(text));
    return 0;
}
