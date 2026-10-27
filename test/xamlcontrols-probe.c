/* Windows.UI.Xaml controls (patches/sg/6137): Page (as an application derives it), UserControl,
 * ContentControl -- Content, Padding/BorderThickness insets, Background drawn, text properties
 * (Foreground, FontSize) passed down to the text inside, a string content shown as text. Pixels
 * read back from a XAML Island. Interface identity per windows-rs 0.30.0 (MIT OR Apache-2.0). */
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
DEFINE_GUID(IID_FEFactory,      0xdeaee126, 0x03ca, 0x4966, 0xb5, 0x76, 0x60, 0x4c, 0xce, 0x93, 0xb5, 0xe8);
DEFINE_GUID(IID_PageFactory,    0xdfa149ac, 0x1849, 0x445e, 0x93, 0x7c, 0x40, 0xa9, 0x59, 0x0c, 0xc0, 0x76);
DEFINE_GUID(IID_UCFactory,      0x38b1ed92, 0xa28a, 0x4972, 0x93, 0xdf, 0xf4, 0xf7, 0x59, 0xb8, 0xaf, 0xd2);
DEFINE_GUID(IID_CCFactory,      0xab017b19, 0x196e, 0x4ed5, 0xa2, 0x7f, 0x55, 0xc5, 0x8f, 0xb1, 0x67, 0x3f);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_UC,             0xa7a69ec9, 0xea35, 0x4679, 0xbf, 0x29, 0xf4, 0xf0, 0x92, 0x86, 0xd3, 0x14);
DEFINE_GUID(IID_CC,             0xa26dd1dc, 0xcd44, 0x435c, 0xbe, 0x94, 0x01, 0xd6, 0x24, 0x1c, 0x23, 0x1c);
DEFINE_GUID(IID_CC2,            0x652c208c, 0x8ccf, 0x4399, 0xbd, 0x3e, 0x5a, 0x01, 0x5a, 0xa1, 0xbc, 0x03);
DEFINE_GUID(IID_Page,           0xc4b27075, 0xe641, 0x453e, 0x82, 0x4d, 0x01, 0x2f, 0xc7, 0xcf, 0x95, 0xcf);
DEFINE_GUID(IID_TB,             0xae2d9271, 0x3b4a, 0x45fc, 0x84, 0x68, 0xf7, 0x94, 0x95, 0x48, 0xf4, 0xd5);
DEFINE_GUID(IID_SCBFactory,     0xd935ce0c, 0x86f5, 0x4da6, 0x8a, 0x27, 0xb1, 0x61, 0x9e, 0xf7, 0xf9, 0x2b);
DEFINE_GUID(IID_Brush,          0x8806a321, 0x1e06, 0x422c, 0xa1, 0xcc, 0x01, 0x69, 0x65, 0x59, 0xe0, 0x21);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_Marker,         0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
struct size { float w, h; };
struct thickness { double l, t, r, b; };
struct color { BYTE a, r, g, b; };
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *act_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putd_fn)(obj *, double);
typedef HRESULT (WINAPI *puts_fn)(obj *, HSTRING);
typedef HRESULT (WINAPI *putth_fn)(obj *, struct thickness);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *measure_fn)(obj *, struct size);
typedef HRESULT (WINAPI *getsize_fn)(obj *, struct size *);

enum { CTL_SetFontSize = 7, CTL_SetForeground = 19, CTL_SetPadding = 31, CTL_SetBackground = 37 };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static obj *scbf;
static obj *brush(BYTE r, BYTE g, BYTE b)
{
    struct color c = { 255, r, g, b };
    obj *br = NULL;
    ((HRESULT (WINAPI *)(obj *, struct color, obj **))SLOT(scbf, 6))(scbf, c, &br);
    return as(br, &IID_Brush);
}
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
static struct size measure(obj *o, float w, float h)
{
    obj *ui = as(o, &IID_UIE);
    struct size avail = { w, h }, d = { -1, -1 };
    ((measure_fn)SLOT(ui, 91))(ui, avail);
    ((getsize_fn)SLOT(ui, 6))(ui, &d);
    return d;
}

/* an application's page: aggregates Page */
struct app_page { IInspectable IInspectable_iface; LONG ref; IInspectable *inner; };
static HRESULT WINAPI outer_qi(IInspectable *iface, REFIID iid, void **out)
{
    struct app_page *p = CONTAINING_RECORD(iface, struct app_page, IInspectable_iface);
    if (IsEqualGUID(iid, &IID_Marker) || IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable))
    { *out = iface; InterlockedIncrement(&p->ref); return S_OK; }
    return IInspectable_QueryInterface(p->inner, iid, out);
}
static ULONG WINAPI outer_addref(IInspectable *iface) { return InterlockedIncrement(&CONTAINING_RECORD(iface, struct app_page, IInspectable_iface)->ref); }
static ULONG WINAPI outer_release(IInspectable *iface) { return InterlockedDecrement(&CONTAINING_RECORD(iface, struct app_page, IInspectable_iface)->ref); }
static HRESULT WINAPI outer_iids(IInspectable *iface, ULONG *n, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI outer_name(IInspectable *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI outer_trust(IInspectable *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static IInspectableVtbl outer_vtbl = { outer_qi, outer_addref, outer_release, outer_iids, outer_name, outer_trust };

int main(void)
{
    obj *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *pf, *page = NULL, *ctl, *tbf, *text = NULL, *back = NULL, *marker = NULL;
    struct app_page app = { { &outer_vtbl }, 1, NULL };
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    HDC hdc;
    int light = 0, x, y;
    COLORREF c;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(scbf = factory(L"Windows.UI.Xaml.Media.SolidColorBrush", &IID_SCBFactory))) return 1;

    /* UserControl: Padding around its content */
    {
        obj *ucf = factory(L"Windows.UI.Xaml.Controls.UserControl", &IID_UCFactory), *uc = NULL, *fef, *fe = NULL;
        struct thickness ten = { 10, 10, 10, 10 };
        struct size d;
        if (!ucf) return 1;
        ((create_fn)SLOT(ucf, 6))(ucf, NULL, &inner, &uc);
        fef = factory(L"Windows.UI.Xaml.FrameworkElement", &IID_FEFactory);
        ((create_fn)SLOT(fef, 6))(fef, NULL, &inner, &fe);
        ((putd_fn)SLOT(fe, 16))(fe, 50); ((putd_fn)SLOT(fe, 18))(fe, 20);
        ((putobj_fn)SLOT(as(uc, &IID_UC), 7))(as(uc, &IID_UC), as(fe, &IID_UIE));         /* Content */
        ((putth_fn)SLOT(as(uc, &IID_Control), CTL_SetPadding))(as(uc, &IID_Control), ten);
        d = measure(uc, 300, 300);
        printf("usercontrol=%.0fx%.0f\n", d.w, d.h);
    }
    /* ContentControl: a string content is shown as text */
    {
        obj *ccf = factory(L"Windows.UI.Xaml.Controls.ContentControl", &IID_CCFactory), *cc = NULL, *pvs, *str = NULL, *root = NULL;
        struct size d;
        if (!ccf) return 1;
        ((create_fn)SLOT(ccf, 6))(ccf, NULL, &inner, &cc);
        pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(pvs, 18))(pvs, hs(L"Hello"), &str);    /* CreateString */
        ((putobj_fn)SLOT(as(cc, &IID_CC), 7))(as(cc, &IID_CC), str);
        d = measure(cc, 300, 300);
        ((get_fn)SLOT(as(cc, &IID_CC2), 6))(as(cc, &IID_CC2), &root);
        printf("string_content=%d root_is_text=%d\n", d.w > 10 && d.h > 10, as(root, &IID_TB) != NULL);
    }

    /* a Page, derived by the application, as the island's content */
    if (!(pf = factory(L"Windows.UI.Xaml.Controls.Page", &IID_PageFactory))) return 1;
    ((create_fn)SLOT(pf, 6))(pf, &app.IInspectable_iface, &app.inner, &page);
    printf("page_aggregated=%d\n", SUCCEEDED(((qi_fn)SLOT(page, 0))(page, &IID_Marker, (void **)&marker)));
    ctl = as(page, &IID_Control);
    ((putobj_fn)SLOT(ctl, CTL_SetBackground))(ctl, brush(0, 128, 0));
    ((putobj_fn)SLOT(ctl, CTL_SetForeground))(ctl, brush(255, 255, 255));
    ((putd_fn)SLOT(ctl, CTL_SetFontSize))(ctl, 40);
    tbf = factory(L"Windows.UI.Xaml.Controls.TextBlock", &IID_IActivationFactory);
    ((act_fn)SLOT(tbf, 6))(tbf, &text);
    text = as(text, &IID_TB);
    ((puts_fn)SLOT(text, 27))(text, hs(L"XXXX"));
    { obj *fe = as(text, &IID_FE); ((HRESULT (WINAPI *)(obj *, INT32))SLOT(fe, 28))(fe, 0); ((HRESULT (WINAPI *)(obj *, INT32))SLOT(fe, 30))(fe, 0); }
    ((putobj_fn)SLOT(as(page, &IID_UC), 7))(as(page, &IID_UC), as(text, &IID_UIE));
    ((get_fn)SLOT(as(page, &IID_UC), 6))(as(page, &IID_UC), &back);
    printf("page_content=%d\n", as(back, &IID_IUnknown) == as(text, &IID_IUnknown));

    if (!(ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics))) return 1;
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(page, &IID_UIE));
    pump(800);
    UpdateWindow(island);
    pump(300);
    hdc = GetDC(island);
    c = GetPixel(hdc, 295, 195);   printf("page_background=%d\n", close_to(c, 0, 128, 0));
    for (y = 0; y < 60; y += 2) for (x = 0; x < 200; x += 2)
        if (close_to(GetPixel(hdc, x, y), 255, 255, 255)) light++;
    printf("inherited_text=%d\n", light > 40);
    ReleaseDC(island, hdc);
    return 0;
}
