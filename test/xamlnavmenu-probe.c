/* Windows.UI.Xaml pieces WinUI 2's NavigationView needs for its menu (patches/sg/6197): an element's
 * EffectiveViewportChanged inside a ScrollViewer, which is an IScrollAnchorProvider; FindName in a
 * ContentControl's content not shown yet; a compiled {TemplateBinding} on an attached property
 * (AutomationProperties.Name, not Name); x:Uid in a library's markup taking the library's strings;
 * FontIcon statics 2 and 3; PathIcon; ThemeShadow and UIElement.Shadow; BindingOperations.SetBinding
 * on a template part; routed event identifiers and AddHandler.
 *
 * Prints name=value lines; see test/xamlnavmenu-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
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
DEFINE_GUID(IID_FE7,            0x2263886c, 0xc069, 0x570f, 0xb9, 0xcc, 0x9e, 0x21, 0xdd, 0x02, 0x8d, 0x8e);
DEFINE_GUID(IID_UIE10,          0xd531c629, 0xad2c, 0x5f6b, 0xad, 0xcf, 0xfb, 0x87, 0x28, 0x7d, 0x18, 0xd7);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_TB,             0xae2d9271, 0x3b4a, 0x45fc, 0x84, 0x68, 0xf7, 0x94, 0x95, 0x48, 0xf4, 0xd5);
DEFINE_GUID(IID_AppStatics,     0x06499997, 0xf7b4, 0x45fe, 0xb7, 0x63, 0x75, 0x77, 0xd1, 0xd3, 0xcb, 0x4a);
DEFINE_GUID(IID_UriFactory,     0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
DEFINE_GUID(IID_GridFactory,    0xae814041, 0xc531, 0x43b4, 0xbf, 0x99, 0x12, 0xf5, 0x06, 0xf7, 0xb0, 0x1c);
DEFINE_GUID(IID_VTHStatics,     0xe75758c4, 0xd25d, 0x4b1d, 0x97, 0x1f, 0x59, 0x6f, 0x17, 0xf1, 0x2b, 0xaa);
DEFINE_GUID(IID_DObj,           0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_APStatics,      0xb618fd7b, 0x32d0, 0x4970, 0x9c, 0x42, 0x7c, 0x03, 0x9a, 0xc7, 0xbe, 0x78);
DEFINE_GUID(IID_ViewportArgs,   0x55ee2e81, 0x1c18, 0x59ed, 0xbd, 0x3d, 0xc4, 0xca, 0x8f, 0xa7, 0xd1, 0x90);
DEFINE_GUID(IID_AnchorProvider, 0x6ae99a0f, 0x478c, 0x5440, 0xb2, 0xc6, 0xea, 0xd4, 0x37, 0x8e, 0xf8, 0xe4);
DEFINE_GUID(IID_FontIconStatics3, 0xa4528b17, 0xb084, 0x4bd5, 0xaa, 0xb4, 0xfe, 0xae, 0x9d, 0x3b, 0x5d, 0x26);
DEFINE_GUID(IID_FontIconStatics2, 0xe6080672, 0x08f8, 0x43d3, 0x87, 0x90, 0x46, 0x04, 0x29, 0x0c, 0xbe, 0xbd);
DEFINE_GUID(IID_FontIcon3,      0x6155d919, 0x1c6c, 0x4950, 0x87, 0x94, 0x23, 0x1e, 0xf0, 0x91, 0x4d, 0x94);
DEFINE_GUID(IID_PathIcon,       0x214c62db, 0xc366, 0x4a0e, 0xb9, 0xad, 0xdc, 0xf1, 0x68, 0xd7, 0xec, 0x0c);
DEFINE_GUID(IID_ThemeShadow,    0x3eccad09, 0x7985, 0x5f39, 0x8b, 0x62, 0x6c, 0x10, 0x69, 0x6d, 0xca, 0x6f);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_FEStatics,      0x48383032, 0xfbeb, 0x4f8a, 0xae, 0xd2, 0xee, 0x21, 0xfb, 0x27, 0xa5, 0x7b);
DEFINE_GUID(IID_UIEStatics,     0x58d3573b, 0xf52c, 0x45be, 0x98, 0x8b, 0xa5, 0x86, 0x95, 0x64, 0x87, 0x3c);
DEFINE_GUID(IID_DataBinding,    0x3f7a0c6b, 0xd00f, 0x4730, 0x8c, 0x1d, 0x48, 0xe1, 0x6c, 0x46, 0xf9, 0xca);
DEFINE_GUID(IID_RelativeSource, 0x2397ce84, 0x2822, 0x483a, 0xb4, 0x99, 0xd0, 0xf0, 0x31, 0xe0, 0x6c, 0x6b);
DEFINE_GUID(IID_PathFactory,    0x4e4cdf99, 0x9826, 0x4e56, 0x84, 0x7c, 0xca, 0x05, 0x5f, 0x16, 0x29, 0x05);
DEFINE_GUID(IID_BindingOps,     0xe155ef73, 0x95a0, 0x4aab, 0x8c, 0x7d, 0x2a, 0x47, 0xda, 0x07, 0x3c, 0x79);
DEFINE_GUID(IID_Vector_UIE,     0xb4c1e3ac, 0x8768, 0x5b9d, 0xa6, 0x61, 0xf6, 0x33, 0x30, 0xb8, 0x50, 0x7b);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);

struct wrect { float x, y, w, h; };
struct wsize { float w, h; };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
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
static int close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 40 && abs(GetGValue(c) - g) < 40 && abs(GetBValue(c) - b) < 40;
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    if (root) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}
static const WCHAR *str(HSTRING s) { return s ? WindowsGetStringRawBuffer(s, NULL) : L"(null)"; }

/* an event handler (two arguments): counts calls, keeps the last viewport */
struct handler { void *vtbl; LONG ref; int calls; struct wrect viewport; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; InterlockedIncrement(&h->ref); return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return InterlockedIncrement(&h->ref); }
static ULONG WINAPI h_release(struct handler *h) { return InterlockedDecrement(&h->ref); }
static HRESULT WINAPI h_invoke(struct handler *h, IInspectable *sender, IInspectable *args)
{
    obj *va = as(args, &IID_ViewportArgs);
    h->calls++;
    if (va) ((HRESULT (WINAPI *)(obj *, struct wrect *))SLOT(va, 6))(va, &h->viewport);
    return S_OK;
}
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<TextBox x:Name=\"Tx\" Width=\"100\" Height=\"30\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"<PathIcon x:Name=\"Pi\" Data=\"M0,0 L20,0 L20,10 L0,10 Z\" Foreground=\"Lime\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"120,0,0,0\"/>"
    L"<ContentControl x:Name=\"Cc\" Width=\"77\" Height=\"20\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"160,0,0,0\">"
    L"<ContentControl.Template><ControlTemplate TargetType=\"ContentControl\"><Border Background=\"Blue\"/></ControlTemplate></ContentControl.Template>"
    L"</ContentControl>"
    L"<ContentControl x:Name=\"Pc\" Width=\"10\" Height=\"10\" HorizontalAlignment=\"Right\" VerticalAlignment=\"Bottom\"><ContentControl.Template><ControlTemplate TargetType=\"ContentControl\"><Border/></ControlTemplate></ContentControl.Template><Border x:Name=\"Inner2\"/></ContentControl>"
    L"<ScrollViewer x:Name=\"Sv\" Width=\"200\" Height=\"100\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"0,50,0,0\">"
    L"<StackPanel><Border x:Name=\"Vb\" Height=\"300\" Background=\"Red\"/></StackPanel></ScrollViewer>"
    L"</Grid>";

static obj *app_statics, *uri_factory, *grid_factory;
static HRESULT load(IInspectable *component, const WCHAR *name)
{
    WCHAR buf[128];
    obj *uri = NULL;
    swprintf(buf, 128, L"ms-appx:///%ls", name);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(uri_factory, 6))(uri_factory, hs(buf), &uri);
    return ((HRESULT (WINAPI *)(obj *, IInspectable *, obj *))SLOT(app_statics, 8))(app_statics, component, uri);
}
static obj *new_grid(void)
{
    IInspectable *inner = NULL;
    obj *g = NULL;
    ((create_fn)SLOT(grid_factory, 6))(grid_factory, NULL, &inner, &g);
    return g;
}

static obj *vth;
static obj *child(obj *e, int i)
{
    obj *c = NULL;
    if (e) ((HRESULT (WINAPI *)(obj *, obj *, INT32, obj **))SLOT(vth, 10))(vth, as(e, &IID_DObj), i, &c);
    return c;
}

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *vb, *fe7, *sv, *pending = NULL, *tx, *pi, *cc;
    obj *aps, *pvs, *grid2, *btn, *frame, *lib, *ltext;
    struct handler vh = { h_vtbl, 1 }, kh = { h_vtbl, 1 };
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    INT64 token = 0;
    HSTRING s = NULL;
    HRESULT hr;
    HDC hdc;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    if (!(vth = factory(L"Windows.UI.Xaml.Media.VisualTreeHelper", &IID_VTHStatics))) return 1;
    app_statics = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics);
    uri_factory = factory(L"Windows.Foundation.Uri", &IID_UriFactory);
    grid_factory = factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory);
    aps = factory(L"Windows.UI.Xaml.Automation.AutomationProperties", &IID_APStatics);
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;

    /* a ContentControl's content is in its name scope before it is shown */
    ((load_fn)SLOT(rs, 6))(rs, hs(L"<ContentControl " NS L"><ContentControl.Template><ControlTemplate TargetType=\"ContentControl\"><ContentPresenter/></ControlTemplate></ContentControl.Template><Border x:Name=\"Inner\"/></ContentControl>"), &pending);
    printf("pending_name=%d\n", find(pending, L"Inner") != NULL);

    /* ScrollViewer is an IScrollAnchorProvider */
    sv = find(root, L"Sv");
    printf("anchor_provider=%d\n", as(sv, &IID_AnchorProvider) != NULL);

    /* EffectiveViewportChanged */
    vb = find(root, L"Vb");
    fe7 = as(vb, &IID_FE7);
    if (fe7) ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(fe7, 7))(fe7, &vh, &token);

    /* AddHandler(KeyDownEvent) on the TextBox */
    tx = find(root, L"Tx");
    {
        obj *uies = factory(L"Windows.UI.Xaml.UIElement", &IID_UIEStatics), *kd = NULL, *kd2 = NULL, *pr = NULL;
        if (uies)
        {
            ((get_fn)SLOT(uies, 6))(uies, &kd);
            ((get_fn)SLOT(uies, 6))(uies, &kd2);
            ((get_fn)SLOT(uies, 9))(uies, &pr);
        }
        hr = kd && tx ? ((HRESULT (WINAPI *)(obj *, obj *, void *, boolean))SLOT(as(tx, &IID_UIE), 96))(as(tx, &IID_UIE), kd, &kh, FALSE) : E_FAIL;
        printf("routed_events=%d,%d,%d add_handler=%08lx\n", kd != NULL, ident(kd) == ident(kd2), pr != NULL && ident(pr) != ident(kd), hr);
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

    printf("pending_shown=%d\n", find(root, L"Inner2") != NULL);
    printf("viewport=%d %.0fx%.0f\n", vh.calls > 0, vh.viewport.w, vh.viewport.h);

    /* a key on the focused TextBox reaches the handler AddHandler added */
    SendMessageW(island, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(50, 15));
    SendMessageW(island, WM_LBUTTONUP, 0, MAKELPARAM(50, 15));
    pump(100);
    SendMessageW(island, WM_KEYDOWN, 'A', 1);
    SendMessageW(island, WM_KEYUP, 'A', 0xc0000001);
    pump(100);
    printf("key_handler=%d\n", kh.calls > 0);

    /* PathIcon: its geometry, where it is, in its Foreground */
    pi = find(root, L"Pi");
    {
        struct wsize d = { -1, -1 };
        if (pi) ((HRESULT (WINAPI *)(obj *, struct wsize *))SLOT(as(pi, &IID_UIE), 6))(as(pi, &IID_UIE), &d);
        hdc = GetDC(island);
        printf("path_icon=%d size=%.0fx%.0f drawn=%d\n", as(pi, &IID_PathIcon) != NULL, d.w, d.h,
               close_to(GetPixel(hdc, 130, 5), 0, 255, 0));
        ReleaseDC(island, hdc);
    }

    /* FontIcon statics 2 and 3: their properties are the instance's */
    {
        obj *fs3 = factory(L"Windows.UI.Xaml.Controls.FontIcon", &IID_FontIconStatics3);
        obj *fs2 = factory(L"Windows.UI.Xaml.Controls.FontIcon", &IID_FontIconStatics2);
        obj *mprop = NULL, *tprop = NULL, *fi = NULL, *yes = NULL;
        boolean mirrored = 2;
        if (fs3) ((get_fn)SLOT(fs3, 6))(fs3, &mprop);
        if (fs2) ((get_fn)SLOT(fs2, 6))(fs2, &tprop);
        ((load_fn)SLOT(rs, 6))(rs, hs(L"<FontIcon " NS L" Glyph=\"&#xE700;\"/>"), &fi);
        if (pvs) ((HRESULT (WINAPI *)(obj *, boolean, obj **))SLOT(pvs, 17))(pvs, TRUE, &yes);
        if (fi && mprop && yes) ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(as(fi, &IID_DObj), 7))(as(fi, &IID_DObj), mprop, yes);
        if (as(fi, &IID_FontIcon3)) ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(as(fi, &IID_FontIcon3), 6))(as(fi, &IID_FontIcon3), &mirrored);
        printf("font_icon_statics=%d,%d mirrored=%d\n", mprop != NULL, tprop != NULL, mirrored);
    }

    /* ThemeShadow: its Receivers; a UIElement's Shadow */
    {
        IInspectable *shadow = NULL;
        obj *receivers = NULL, *got = NULL, *ts;
        UINT32 n = 0;
        hr = RoActivateInstance(hs(L"Windows.UI.Xaml.Media.ThemeShadow"), &shadow);
        ts = as(shadow, &IID_ThemeShadow);
        if (ts) ((get_fn)SLOT(ts, 6))(ts, &receivers);
        if (receivers)
        {
            ((HRESULT (WINAPI *)(obj *, obj *))SLOT(receivers, 13))(receivers, as(vb, &IID_UIE));
            ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(receivers, 7))(receivers, &n);
        }
        if (shadow && as(sv, &IID_UIE10))
        {
            ((putobj_fn)SLOT(as(sv, &IID_UIE10), 12))(as(sv, &IID_UIE10), (obj *)shadow);
            ((get_fn)SLOT(as(sv, &IID_UIE10), 11))(as(sv, &IID_UIE10), &got);
        }
        printf("theme_shadow=%08lx receivers=%u shadow=%d\n", hr, n, got && ident(got) == ident(shadow));
    }

    /* BindingOperations.SetBinding on a template part: {Binding Width, RelativeSource TemplatedParent} */
    cc = find(root, L"Cc");
    {
        obj *ops = factory(L"Windows.UI.Xaml.Data.BindingOperations", &IID_BindingOps);
        obj *fes = factory(L"Windows.UI.Xaml.FrameworkElement", &IID_FEStatics), *wprop = NULL, *path = NULL;
        obj *pf = factory(L"Windows.UI.Xaml.PropertyPath", &IID_PathFactory), *border = child(cc, 0);
        IInspectable *binding = NULL, *rsrc = NULL;
        double width = -1;
        RoActivateInstance(hs(L"Windows.UI.Xaml.Data.Binding"), &binding);
        RoActivateInstance(hs(L"Windows.UI.Xaml.Data.RelativeSource"), &rsrc);
        if (fes) ((get_fn)SLOT(fes, 10))(fes, &wprop);
        if (pf) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(pf, 6))(pf, hs(L"Width"), &path);
        if (rsrc) ((HRESULT (WINAPI *)(obj *, INT32))SLOT(as(rsrc, &IID_RelativeSource), 7))(as(rsrc, &IID_RelativeSource), 1);
        if (binding)
        {
            ((putobj_fn)SLOT(as(binding, &IID_DataBinding), 7))(as(binding, &IID_DataBinding), path);
            ((putobj_fn)SLOT(as(binding, &IID_DataBinding), 13))(as(binding, &IID_DataBinding), (obj *)rsrc);
        }
        hr = ops && border ? ((HRESULT (WINAPI *)(obj *, obj *, obj *, IInspectable *))SLOT(ops, 6))(ops, as(border, &IID_DObj), wprop, binding) : E_FAIL;
        if (border) ((HRESULT (WINAPI *)(obj *, double *))SLOT(as(border, &IID_FE), 15))(as(border, &IID_FE), &width);
        printf("set_binding=%08lx width=%.0f\n", hr, width);
    }

    /* a compiled {TemplateBinding} on AutomationProperties.Name sets that, not the part's Name */
    grid2 = new_grid();
    hr = load((IInspectable *)grid2, L"TemplOwner.xbf");
    btn = find(grid2, L"TheButton");
    {
        obj *panel_children = NULL;
        ((putobj_fn)SLOT(src, 7))(src, as(grid2, &IID_UIE));
        pump(500);
        frame = child(btn, 0);
        s = NULL;
        if (frame) ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(as(frame, &IID_FE), 33))(as(frame, &IID_FE), &s);
        printf("templ_owner=%08lx name=%ls", hr, str(s));
        s = NULL;
        if (frame && aps) ((HRESULT (WINAPI *)(obj *, obj *, HSTRING *))SLOT(aps, 31))(aps, as(frame, &IID_DObj), &s);
        printf(" automation=%ls\n", str(s));
        (void)panel_children;
    }

    /* x:Uid in a library's markup (ms-appx:///Lib/...): the library's strings */
    lib = new_grid();
    hr = load((IInspectable *)lib, L"Lib/LibPage.xaml");
    ltext = find(lib, L"T");
    s = NULL;
    if (ltext) ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(as(ltext, &IID_TB), 26))(as(ltext, &IID_TB), &s);
    printf("library_uid=%08lx %ls\n", hr, str(s));
    return 0;
}
