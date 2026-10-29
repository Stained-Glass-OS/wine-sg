/* Windows.UI.Xaml templated ScrollBar (patches/sg/6187): with a template of the program's (Windows
 * Terminal's) a ScrollBar sizes its parts -- the thumb as long as the viewport is of the whole, the
 * large decrease before it as the value puts it -- hides its other orientation's root, steps Value from
 * its small and large RepeatButtons, moves it with its Thumb's drag, and goes to its states (Expanded
 * with the mouse indicator).  Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_RangeBase,      0xfa002c1a, 0x494e, 0x46cf, 0x91, 0xd4, 0xe1, 0x4a, 0x8d, 0x79, 0x86, 0x75);
DEFINE_GUID(IID_ScrollBarI,     0xf57ae6ca, 0xd1a6, 0x4b90, 0xa4, 0xe9, 0x54, 0xdf, 0x1b, 0xa8, 0xd2, 0xec);
DEFINE_GUID(IID_DependencyObject, 0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_VTHStatics,     0xe75758c4, 0xd25d, 0x4b1d, 0x97, 0x1f, 0x59, 0x6f, 0x17, 0xf1, 0x2b, 0xaa);
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
    L"<Canvas " NS L" Background=\"White\">"
    L"<ScrollBar x:Name=\"S\" Canvas.Left=\"10\" Canvas.Top=\"10\" Width=\"20\" Height=\"200\" Minimum=\"0\" Maximum=\"100\""
    L" ViewportSize=\"100\" SmallChange=\"1\" LargeChange=\"10\" Orientation=\"Vertical\">"
    L"<ScrollBar.Template><ControlTemplate TargetType=\"ScrollBar\"><Grid>"
    L"<VisualStateManager.VisualStateGroups><VisualStateGroup x:Name=\"ConsciousStates\">"
    L"<VisualState x:Name=\"Collapsed\"/>"
    L"<VisualState x:Name=\"Expanded\"><VisualState.Setters><Setter Target=\"Ind.Opacity\" Value=\"1\"/></VisualState.Setters></VisualState>"
    L"</VisualStateGroup></VisualStateManager.VisualStateGroups>"
    L"<Rectangle x:Name=\"Ind\" Opacity=\"0\"/>"
    L"<Grid x:Name=\"HorizontalRoot\"/>"
    L"<Grid x:Name=\"VerticalRoot\"><Grid.RowDefinitions><RowDefinition Height=\"Auto\"/><RowDefinition Height=\"Auto\"/>"
    L"<RowDefinition Height=\"Auto\"/><RowDefinition Height=\"*\"/><RowDefinition Height=\"Auto\"/></Grid.RowDefinitions>"
    L"<RepeatButton x:Name=\"VerticalSmallDecrease\" Grid.Row=\"0\" Height=\"20\" VerticalAlignment=\"Stretch\" HorizontalAlignment=\"Stretch\"/>"
    L"<RepeatButton x:Name=\"VerticalLargeDecrease\" Grid.Row=\"1\" Height=\"0\" HorizontalAlignment=\"Stretch\"/>"
    L"<Thumb x:Name=\"VerticalThumb\" Grid.Row=\"2\" MinHeight=\"10\" HorizontalAlignment=\"Stretch\"/>"
    L"<RepeatButton x:Name=\"VerticalLargeIncrease\" Grid.Row=\"3\" VerticalAlignment=\"Stretch\" HorizontalAlignment=\"Stretch\"/>"
    L"<RepeatButton x:Name=\"VerticalSmallIncrease\" Grid.Row=\"4\" Height=\"20\" HorizontalAlignment=\"Stretch\"/>"
    L"</Grid></Grid></ControlTemplate></ScrollBar.Template></ScrollBar>"
    L"</Canvas>";

static obj *vth;
/* a named element under ELEM in the visual tree */
static obj *named(obj *elem, const WCHAR *name)
{
    INT32 i, n = 0;
    HSTRING h = NULL;
    obj *fe = as(elem, &IID_FE), *child, *found;

    if (fe && SUCCEEDED(((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(fe, 33))(fe, &h)) && h &&
        !lstrcmpW(WindowsGetStringRawBuffer(h, NULL), name)) return elem;
    ((HRESULT (WINAPI *)(obj *, obj *, INT32 *))SLOT(vth, 11))(vth, as(elem, &IID_DependencyObject), &n);
    for (i = 0; i < n; i++)
    {
        child = NULL;
        ((HRESULT (WINAPI *)(obj *, obj *, INT32, obj **))SLOT(vth, 10))(vth, as(elem, &IID_DependencyObject), i, &child);
        if (child && (found = named(child, name))) return found;
    }
    return NULL;
}
static double actual_height(obj *e) { double v = -1; if (e) ((HRESULT (WINAPI *)(obj *, double *))SLOT(as(e, &IID_FE), 14))(as(e, &IID_FE), &v); return v; }
static double opacity(obj *e) { double v = -1; if (e) ((HRESULT (WINAPI *)(obj *, double *))SLOT(as(e, &IID_UIE), 9))(as(e, &IID_UIE), &v); return v; }
static INT32 visibility(obj *e) { INT32 v = -1; if (e) ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(e, &IID_UIE), 21))(as(e, &IID_UIE), &v); return v; }
static HWND island;
static void mouse(UINT msg, int x, int y) { SendMessageW(island, msg, msg == WM_LBUTTONDOWN ? MK_LBUTTON : 0, MAKELPARAM(x, y)); pump(30); }
static obj *rb;
static double value(void) { double v = -1; ((HRESULT (WINAPI *)(obj *, double *))SLOT(rb, 14))(rb, &v); return v; }
static void set_value(double v) { ((HRESULT (WINAPI *)(obj *, double))SLOT(rb, 15))(rb, v); }

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *sb;
    IInspectable *inner = NULL;
    HWND host;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    if (!(vth = factory(L"Windows.UI.Xaml.Media.VisualTreeHelper", &IID_VTHStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(L"S"), &sb);
    rb = as(sb, &IID_RangeBase);
    if (!rb) return 1;

    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 250, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 250, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);

    /* the track: 200 - 20 - 20 = 160; the viewport half of the whole: a thumb of 80 */
    printf("parts=%.0f,%.0f,%d\n", actual_height(named(sb, L"VerticalThumb")), actual_height(named(sb, L"VerticalLargeDecrease")),
           visibility(named(sb, L"HorizontalRoot")));
    set_value(50);
    pump(300);
    printf("half=%.0f\n", actual_height(named(sb, L"VerticalLargeDecrease")));

    /* the small increase (the bottom 20): one SmallChange; the large increase (below the thumb): one LargeChange */
    mouse(WM_LBUTTONDOWN, 20, 200); mouse(WM_LBUTTONUP, 20, 200);
    printf("small=%.0f\n", value());
    pump(200);
    mouse(WM_LBUTTONDOWN, 20, 185); mouse(WM_LBUTTONUP, 20, 185);
    printf("large=%.0f\n", value());
    /* the thumb dragged 16 down: 16 * 100 / (160 - 80) = 20 more */
    set_value(50); pump(300);
    mouse(WM_LBUTTONDOWN, 20, 110); mouse(WM_MOUSEMOVE, 20, 126); mouse(WM_LBUTTONUP, 20, 126);
    printf("drag=%.0f\n", value());

    /* its states: Expanded with the mouse indicator */
    printf("collapsed=%.0f\n", opacity(named(sb, L"Ind")));
    ((HRESULT (WINAPI *)(obj *, INT32))SLOT(as(sb, &IID_ScrollBarI), 11))(as(sb, &IID_ScrollBarI), 2);
    pump(300);
    printf("expanded=%.0f\n", opacity(named(sb, L"Ind")));
    return 0;
}
