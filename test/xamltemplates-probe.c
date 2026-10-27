/* Windows.UI.Xaml templates (patches/sg/6145): a ControlTemplate set by a Style -- a Border whose
 * Background and Padding follow the control's ({TemplateBinding}), a ContentPresenter showing its
 * Content -- drawn in a XAML Island; changes to the control reach its template; GetTemplateChild;
 * an application's control gets OnApplyTemplate. Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_CommandBar,     0x98bc4280, 0x4a3d, 0x4cee, 0xbd, 0x07, 0x22, 0xce, 0x94, 0xc5, 0xaf, 0x76);
DEFINE_GUID(IID_AppBarButton,   0x4459a451, 0x69e8, 0x440c, 0x98, 0x96, 0x4b, 0xb4, 0xf5, 0xf6, 0x42, 0xd1);
DEFINE_GUID(IID_SymbolIcon,     0x7a4774c9, 0xa6a3, 0x4b30, 0x8f, 0xf1, 0x90, 0x81, 0xd7, 0x0e, 0x9a, 0x5c);
DEFINE_GUID(IID_DO,             0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_APStatics,      0xb618fd7b, 0x32d0, 0x4970, 0x9c, 0x42, 0x7c, 0x03, 0x9a, 0xc7, 0xbe, 0x78);
DEFINE_GUID(IID_TTSStatics,     0x86e649f8, 0xe245, 0x48aa, 0xa8, 0xc8, 0xd1, 0x07, 0x3e, 0xd7, 0x63, 0x19);
DEFINE_GUID(IID_ControlProtected, 0x1791587a, 0x2a7c, 0x4981, 0xa2, 0xd8, 0xab, 0x99, 0xff, 0xfc, 0x65, 0xb9);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_ControlFactory, 0xb12b1d5a, 0x6419, 0x4e16, 0xb2, 0x12, 0xa4, 0x53, 0x36, 0xb7, 0x57, 0x78);
DEFINE_GUID(IID_FEOverrides,    0xda007e54, 0xb3c2, 0x4b9a, 0xaa, 0x8e, 0xd3, 0xf0, 0x71, 0x26, 0x2b, 0x97);
DEFINE_GUID(IID_SCBFactory,     0xd935ce0c, 0x86f5, 0x4da6, 0x8a, 0x27, 0xb1, 0x61, 0x9e, 0xf7, 0xf9, 0x2b);
DEFINE_GUID(IID_Brush,          0x8806a321, 0x1e06, 0x422c, 0xa1, 0xcc, 0x01, 0x69, 0x65, 0x59, 0xe0, 0x21);
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
    L"<Style x:Key=\"Framed\" TargetType=\"ContentControl\"><Setter Property=\"Template\"><Setter.Value>"
    L"<ControlTemplate TargetType=\"ContentControl\"><Border x:Name=\"Frame\" Background=\"{TemplateBinding Background}\""
    L" BorderBrush=\"Lime\" BorderThickness=\"4\" Padding=\"{TemplateBinding Padding}\"><ContentPresenter/></Border>"
    L"</ControlTemplate></Setter.Value></Setter></Style></Grid.Resources>"
    L"<ContentControl x:Name=\"CC\" Style=\"{StaticResource Framed}\" Background=\"Blue\" Padding=\"10\""
    L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" FontSize=\"30\" Foreground=\"Black\" Content=\"XX\"/>"
    L"</Grid>";

static const WCHAR custom_template[] =
    L"<ControlTemplate " NS L" TargetType=\"Control\"><Grid x:Name=\"PartRoot\" Background=\"{TemplateBinding Background}\"/></ControlTemplate>";

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

struct fsize { float w, h; };
struct color { BYTE a, r, g, b; };

/* an application's control: its OnApplyTemplate */
struct app_control { IInspectable IInspectable_iface; struct { void **vtbl; } ov; LONG ref; IInspectable *inner; int applied; };
static struct app_control *ac_from_outer(IInspectable *iface) { return CONTAINING_RECORD(iface, struct app_control, IInspectable_iface); }
static struct app_control *ac_from_ov(void *iface) { return CONTAINING_RECORD(iface, struct app_control, ov); }
static HRESULT WINAPI ac_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct app_control *a = ac_from_outer(iface);
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable)) { *out = iface; InterlockedIncrement(&a->ref); return S_OK; }
    if (IsEqualGUID(iid, &IID_FEOverrides)) { *out = &a->ov; InterlockedIncrement(&a->ref); return S_OK; }
    return IInspectable_QueryInterface(a->inner, iid, out);
}
static ULONG WINAPI ac_addref(IInspectable *iface) { return InterlockedIncrement(&ac_from_outer(iface)->ref); }
static ULONG WINAPI ac_release(IInspectable *iface) { return InterlockedDecrement(&ac_from_outer(iface)->ref); }
static HRESULT WINAPI ac_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI ac_name(IInspectable *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI ac_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl ac_vtbl = { ac_qi, ac_addref, ac_release, ac_iids, ac_name, ac_trust };
static HRESULT WINAPI ov_qi(void *iface, REFIID iid, void **out) { return ac_qi(&ac_from_ov(iface)->IInspectable_iface, iid, out); }
static ULONG WINAPI ov_addref(void *iface) { return InterlockedIncrement(&ac_from_ov(iface)->ref); }
static ULONG WINAPI ov_release(void *iface) { return InterlockedDecrement(&ac_from_ov(iface)->ref); }
static HRESULT WINAPI ov_iids(void *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI ov_name(void *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI ov_trust(void *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static HRESULT WINAPI ov_measure(void *iface, struct fsize avail, void *result)
{
    /* the base's: MeasureOverride of the inner */
    obj *base = NULL;
    HRESULT hr;
    IInspectable_QueryInterface(ac_from_ov(iface)->inner, &IID_FEOverrides, (void **)&base);
    hr = ((HRESULT (WINAPI *)(obj *, struct fsize, void *))SLOT(base, 6))(base, avail, result);
    return hr;
}
static HRESULT WINAPI ov_arrange(void *iface, struct fsize final, void *result)
{
    obj *base = NULL;
    IInspectable_QueryInterface(ac_from_ov(iface)->inner, &IID_FEOverrides, (void **)&base);
    return ((HRESULT (WINAPI *)(obj *, struct fsize, void *))SLOT(base, 7))(base, final, result);
}
static HRESULT WINAPI ov_apply(void *iface) { ac_from_ov(iface)->applied++; return S_OK; }
static void *ov_vtbl[] = { ov_qi, ov_addref, ov_release, ov_iids, ov_name, ov_trust, ov_measure, ov_arrange, ov_apply };

static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

int main(void)
{
    obj *rs, *root = NULL, *cc, *frame = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *scbf, *red = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    HRESULT hr;
    HDC hdc;
    int x, y, dark = 0;
    struct color c = { 255, 255, 0, 0 };

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    cc = find(root, L"CC");

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

    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(cc, &IID_ControlProtected), 8))(as(cc, &IID_ControlProtected), hs(L"Frame"), &frame);
    printf("template_child=%d\n", as(frame, &IID_Border) != NULL);
    hdc = GetDC(island);
    printf("frame=%d\n", close_to(GetPixel(hdc, 1, 1), 0, 255, 0));
    printf("template_binding=%d\n", close_to(GetPixel(hdc, 7, 7), 0, 0, 255));
    for (y = 14; y < 60; y++) for (x = 14; x < 80; x++) if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) dark++;
    printf("presenter=%d\n", dark > 20);
    ReleaseDC(island, hdc);

    /* the control's Background changes: its template follows */
    scbf = factory(L"Windows.UI.Xaml.Media.SolidColorBrush", &IID_SCBFactory);
    ((HRESULT (WINAPI *)(obj *, struct color, obj **))SLOT(scbf, 6))(scbf, c, &red);
    ((putobj_fn)SLOT(as(cc, &IID_Control), 37))(as(cc, &IID_Control), as(red, &IID_Brush));
    pump(300); UpdateWindow(island); pump(200);
    hdc = GetDC(island);
    printf("binding_follows=%d\n", close_to(GetPixel(hdc, 7, 7), 255, 0, 0));
    ReleaseDC(island, hdc);

    /* an application's control: OnApplyTemplate when its template is applied */
    {
        struct app_control app = { { &ac_vtbl }, { ov_vtbl }, 1 };
        obj *cf = factory(L"Windows.UI.Xaml.Controls.Control", &IID_ControlFactory), *ctl = NULL, *tmpl = NULL, *part = NULL;
        ((create_fn)SLOT(cf, 6))(cf, &app.IInspectable_iface, &app.inner, &ctl);
        ((load_fn)SLOT(rs, 6))(rs, hs(custom_template), &tmpl);
        ((putobj_fn)SLOT(as(ctl, &IID_Control), 29))(as(ctl, &IID_Control), tmpl);                 /* Template */
        {
            obj *ui = as(ctl, &IID_UIE);
            struct fsize avail = { 100, 100 };
            ((HRESULT (WINAPI *)(obj *, struct fsize))SLOT(ui, 91))(ui, avail);
        }
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(ctl, &IID_ControlProtected), 8))(as(ctl, &IID_ControlProtected), hs(L"PartRoot"), &part);
        printf("on_apply_template=%d part=%d\n", app.applied, as(part, &IID_Grid) != NULL);
    }
    return 0;
}
