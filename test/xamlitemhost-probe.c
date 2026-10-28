/* Windows.UI.Xaml for list items made by compiled templates (patches/sg/6168): weak references to XAML
 * objects (IWeakReferenceSource, as C++/WinRT x:Bind code takes them), DataTemplate.ExtensionInstance,
 * conditional XAML blocks inside compiled objects (nodes 0x26/0x27), a ListViewItemPresenter in a control
 * template presenting the control's content -- NanaZip's address bar suggestions. Interface identity per
 * windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_DTStatics2,     0x8af77d73, 0xaa01, 0x471e, 0xbe, 0xdd, 0x8b, 0xad, 0x86, 0x21, 0x9b, 0x77);
DEFINE_GUID(IID_WRS,            0x00000038, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
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
    L"<Grid " NS L" Background=\"White\">"
    L"<Grid" TL L" Width=\"60\" Height=\"40\"><Grid.Resources><ResourceDictionary Source=\"ms-appx:///CondDict.xbf\"/></Grid.Resources>"
    L"<Border Background=\"{StaticResource CondBrush}\"/></Grid>"
    L"<ContentControl" TL L" Margin=\"0,60,0,0\"><ContentControl.Content><TextBlock Text=\"XXXXXX\" FontSize=\"30\"/></ContentControl.Content>"
    L"<ContentControl.Template><ControlTemplate TargetType=\"ContentControl\"><ListViewItemPresenter/></ControlTemplate></ContentControl.Template>"
    L"</ContentControl>"
    L"</Grid>";


int main(void)
{
    obj *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *g, *dts, *ext = NULL, *border = NULL;
    IInspectable *inner = NULL, *resolved = NULL;
    obj *wrs = NULL, *wr = NULL;
    HWND host, island = NULL;
    HRESULT hr;
    HDC hdc;
    int x, y, dark = 0, alive, gone;

    RoInitialize(RO_INIT_SINGLETHREADED);
    /* weak references: resolve while it lives, nothing after */
    g = NULL;
    ((create_fn)SLOT(factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory), 6))(factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory), NULL, &inner, &g);
    hr = ((HRESULT (WINAPI *)(obj *, REFIID, void **))SLOT(g, 0))(g, &IID_WRS, (void **)&wrs);
    /* IWeakReferenceSource.GetWeakReference and IWeakReference.Resolve: slot 3 of each */
    if (wrs) { ((HRESULT (WINAPI *)(obj *, obj **))SLOT(wrs, 3))(wrs, &wr); ((ULONG (WINAPI *)(obj *))SLOT(wrs, 2))(wrs); }
    if (wr) ((HRESULT (WINAPI *)(obj *, REFIID, IInspectable **))SLOT(wr, 3))(wr, &IID_IInspectable, &resolved);
    alive = resolved != NULL;
    if (resolved) IInspectable_Release(resolved);
    resolved = NULL;
    ((ULONG (WINAPI *)(obj *))SLOT(g, 2))(g);
    if (wr) ((HRESULT (WINAPI *)(obj *, REFIID, IInspectable **))SLOT(wr, 3))(wr, &IID_IInspectable, &resolved);
    gone = resolved == NULL;
    printf("weak=%08lx,%d,%d\n", hr, alive, gone);

    /* DataTemplate.SetExtensionInstance / GetExtensionInstance */
    dts = factory(L"Windows.UI.Xaml.DataTemplate", &IID_DTStatics2);
    g = NULL;
    ((create_fn)SLOT(factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory), 6))(factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory), NULL, &inner, &g);
    ((create_fn)SLOT(factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory), 6))(factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory), NULL, &inner, &border);
    if (dts) hr = ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(dts, 8))(dts, g, border);
    if (dts) ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(dts, 7))(dts, g, &ext);
    printf("extension=%d,%08lx,%d\n", dts != NULL, hr, ext && as(ext, &IID_IInspectable) == as(border, &IID_IInspectable));

    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
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
    printf("conditional=%d\n", close_to(GetPixel(hdc, 30, 20), 0, 255, 0));
    for (y = 60; y < 110; y++) for (x = 0; x < 200; x++) { COLORREF c = GetPixel(hdc, x, y); dark += GetRValue(c) < 80 && GetGValue(c) < 80; }
    printf("presenter=%d\n", dark > 50);
    ReleaseDC(island, hdc);
    return 0;
}
