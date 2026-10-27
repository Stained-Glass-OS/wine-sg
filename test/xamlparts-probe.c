/* Windows.UI.Xaml parts of NanaZip's address bar (patches/sg/6154): TextCommandBarFlyout made and
 * placed, a Popup's child in its name scope, ListView selection, a ScrollViewer showing its content
 * whatever template an implicit style gives it, a deferred element of compiled markup (section
 * reference kind 9) left unrealized. Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_TCBFFactory,    0x1b5918d8, 0xb006, 0x5ecd, 0xab, 0x72, 0x6f, 0xdb, 0x48, 0xab, 0xc1, 0xf4);
DEFINE_GUID(IID_FlyoutBase,     0x723eea0b, 0xd12e, 0x430d, 0xa9, 0xf0, 0x9b, 0xb3, 0x2b, 0xbf, 0x99, 0x13);
DEFINE_GUID(IID_CBFlyout,       0xfecd2c4d, 0xf16a, 0x58cd, 0x8a, 0xca, 0xdb, 0x0f, 0x04, 0x6f, 0x25, 0xc3);
DEFINE_GUID(IID_ItemsControl,   0xf4a91dd8, 0xd979, 0x4381, 0x86, 0x52, 0xbd, 0xa0, 0x34, 0x2a, 0x76, 0x5e);
DEFINE_GUID(IID_Selector,       0xe30eb3a5, 0xb36b, 0x42dc, 0x85, 0x27, 0xcd, 0x25, 0x13, 0x6c, 0x08, 0x3c);
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
#define TL L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\""

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\"><Grid.Resources>"
    L"<Style TargetType=\"ScrollViewer\"><Setter Property=\"Template\"><Setter.Value><ControlTemplate TargetType=\"ScrollViewer\">"
    L"<Border Background=\"Red\"/></ControlTemplate></Setter.Value></Setter></Style></Grid.Resources>"
    L"<ScrollViewer" TL L" Width=\"200\" Height=\"60\"><TextBlock Text=\"XXXX\" FontSize=\"30\"/></ScrollViewer>"
    L"<Popup x:Name=\"P\"><ListView x:Name=\"L\"/></Popup>"
    L"</Grid>";

static obj *app_statics, *uri_factory, *grid_factory;
static HRESULT load(IInspectable *component, const WCHAR *name)
{
    WCHAR buf[128];
    obj *uri = NULL;
    swprintf(buf, 128, L"ms-appx:///%ls.xaml", name);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(uri_factory, 6))(uri_factory, hs(buf), &uri);
    return ((HRESULT (WINAPI *)(obj *, IInspectable *, obj *))SLOT(app_statics, 8))(app_statics, component, uri);
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

int main(void)
{
    obj *rs, *grid = NULL, *tf, *flyout = NULL, *list, *items = NULL, *pvs, *ms, *mgr = NULL, *sf, *src = NULL, *native;
    IInspectable *inner = NULL, *a = NULL, *b = NULL, *sel = NULL;
    HWND host, island = NULL;
    INT32 placement = -1, index = -2;
    UINT32 n = 99;
    HRESULT hr;
    HDC hdc;
    int x, y, dark = 0;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(app_statics = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics))) return 1;
    if (!(uri_factory = factory(L"Windows.Foundation.Uri", &IID_UriFactory))) return 1;
    if (!(grid_factory = factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory))) return 1;

    /* TextCommandBarFlyout, as NanaZip's address bar makes it */
    tf = factory(L"Windows.UI.Xaml.Controls.TextCommandBarFlyout", &IID_TCBFFactory);
    hr = tf ? ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(tf, 6))(tf, NULL, &inner, &flyout) : E_FAIL;
    if (flyout)
    {
        obj *items_ = NULL;
        ((HRESULT (WINAPI *)(obj *, int))SLOT(as(flyout, &IID_FlyoutBase), 7))(as(flyout, &IID_FlyoutBase), 5);
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(flyout, &IID_FlyoutBase), 6))(as(flyout, &IID_FlyoutBase), &placement);
        ((get_fn)SLOT(as(flyout, &IID_CBFlyout), 6))(as(flyout, &IID_CBFlyout), &items_);
        if (items_) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(items_, 7))(items_, &n);
    }
    printf("flyout=%08lx,%d,%u\n", hr, placement, n);

    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    list = find(grid, L"L");
    printf("popup_child=%d\n", as(list, &IID_ItemsControl) != NULL);
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(L"a"), &a);
    ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(L"b"), &b);
    if (list) ((get_fn)SLOT(as(list, &IID_ItemsControl), 8))(as(list, &IID_ItemsControl), &items);
    if (items)
    {
        obj *s = as(list, &IID_Selector);
        ((HRESULT (WINAPI *)(obj *, IInspectable *))SLOT(items, 13))(items, a);
        ((HRESULT (WINAPI *)(obj *, IInspectable *))SLOT(items, 13))(items, b);
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(s, 6))(s, &index);
        printf("unselected=%d\n", index);
        ((HRESULT (WINAPI *)(obj *, INT32))SLOT(s, 7))(s, 1);
        ((HRESULT (WINAPI *)(obj *, IInspectable **))SLOT(s, 8))(s, &sel);
        hr = ((HRESULT (WINAPI *)(obj *, INT32))SLOT(s, 7))(s, 5);
        printf("selection=%d,%d\n", sel == b, FAILED(hr));
    }

    /* compiled markup with a deferred element */
    {
        IInspectable *gi = NULL;
        obj *g = NULL;
        ((create_fn)SLOT(grid_factory, 6))(grid_factory, NULL, &gi, &g);
        hr = load((IInspectable *)g, L"Deferred");
        printf("deferred=%08lx,%d,%d\n", hr, find(g, L"After") != NULL, find(g, L"Lazy") == NULL);
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
    ((putobj_fn)SLOT(src, 7))(src, as(grid, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    hdc = GetDC(island);
    for (y = 2; y < 58; y += 2) for (x = 2; x < 150; x += 2) if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) dark++;
    printf("scroll_viewer=%d,%d\n", dark > 20, !close_to(GetPixel(hdc, 190, 50), 255, 0, 0));
    ReleaseDC(island, hdc);
    return 0;
}
