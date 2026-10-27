/* Windows.UI.Xaml compiled resources, styles and templates (patches/sg/6146): Application.LoadComponent
 * of compiled XAML (XBF v2, fixtures written by test/mkxbf.py) whose Grid.Resources is a dictionary
 * section -- a brush, a keyed Style for Border (inline setters, one from {StaticResource}), an implicit
 * Style for TextBlock -- with elements using them, one a missing resource, and a Button whose compiled
 * ControlTemplate (a template section, {TemplateBinding Background}) is made for it; drawn in a XAML
 * Island and read back. Broken files (a dictionary entry past its section, an unknown setter, a
 * template section past the table, a section that refers to itself) fail without harm. Interface
 * identity per windows-rs 0.30.0 (MIT). */
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

struct color { BYTE a, r, g, b; };

static obj *app_statics, *uri_factory, *grid_factory;
static HRESULT load(IInspectable *component, const WCHAR *name)
{
    WCHAR buf[128];
    obj *uri = NULL;
    swprintf(buf, 128, L"ms-appx:///%ls.xaml", name);
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
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

int main(void)
{
    obj *root, *label, *button = NULL, *frame = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *scbf, *red = NULL;
    obj *children = NULL, *child = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    struct color c = { 255, 255, 0, 0 };
    double size = -1;
    UINT32 n = 0, i;
    HRESULT hr;
    HDC hdc;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(app_statics = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics))) return 1;
    if (!(uri_factory = factory(L"Windows.Foundation.Uri", &IID_UriFactory))) return 1;
    if (!(grid_factory = factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory))) return 1;
    root = new_grid();
    hr = load((IInspectable *)root, L"TestStyles");
    printf("load=%08lx\n", hr);
    label = find(root, L"Label");
    /* the Button: the last of the Grid's children (Panel.Children, slot 6) */
    {
        obj *panel = NULL;
        panel = as(root, &IID_Panel);
        if (panel) ((get_fn)SLOT(panel, 6))(panel, &children);
        if (children) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(children, 7))(children, &n);
        for (i = 0; i < n; i++)
        {
            child = NULL;
            ((HRESULT (WINAPI *)(obj *, UINT32, obj **))SLOT(children, 6))(children, i, &child);
            if (as(child, &IID_Control)) button = child;
        }
    }
    printf("children=%u button=%d\n", n, button != NULL);

    /* broken files fail, the component unharmed */
    printf("bad_dict=%d\n", FAILED(load((IInspectable *)new_grid(), L"BadDict")));
    printf("bad_setter=%d\n", FAILED(load((IInspectable *)new_grid(), L"BadSetter")));
    printf("bad_template=%d\n", FAILED(load((IInspectable *)new_grid(), L"BadTemplate")));
    printf("loop=%d\n", FAILED(load((IInspectable *)new_grid(), L"LoopSection")));

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
    /* an implicit style applies when the element enters the tree */
    if (label) ((getd_fn)SLOT(as(label, &IID_TB), 6))(as(label, &IID_TB), &size);
    printf("implicit=%.0f\n", size);
    hdc = GetDC(island);
    printf("styled=%d,%d\n", close_to(GetPixel(hdc, 105, 55), 0, 255, 0), close_to(GetPixel(hdc, 115, 35), 255, 255, 255));
    printf("static_resource=%d\n", close_to(GetPixel(hdc, 220, 30), 0, 255, 0));
    printf("template=%d\n", close_to(GetPixel(hdc, 50, 140), 0, 0, 255));
    ReleaseDC(island, hdc);
    if (button) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(button, &IID_ControlProtected), 8))(as(button, &IID_ControlProtected), hs(L"Frame"), &frame);
    printf("template_child=%d\n", as(frame, &IID_Border) != NULL);

    /* the Button's Background changes: its compiled template follows */
    scbf = factory(L"Windows.UI.Xaml.Media.SolidColorBrush", &IID_SCBFactory);
    ((HRESULT (WINAPI *)(obj *, struct color, obj **))SLOT(scbf, 6))(scbf, c, &red);
    if (button) ((putobj_fn)SLOT(as(button, &IID_Control), 37))(as(button, &IID_Control), as(red, &IID_Brush));
    pump(300); UpdateWindow(island); pump(200);
    hdc = GetDC(island);
    printf("binding_follows=%d\n", close_to(GetPixel(hdc, 50, 140), 255, 0, 0));
    ReleaseDC(island, hdc);
    return 0;
}
