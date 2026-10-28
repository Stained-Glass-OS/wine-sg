/* Windows.UI.Xaml classes WinUI 2 apps build on (patches/sg/6177): the value helpers (GridLength, Rect,
 * Size, Duration), Timeline statics, DispatcherTimer, PropertyChangedEventArgs, XamlCompositionBrushBase
 * and AcrylicBrush, XamlLight and UIElement.Lights, FlyoutBase (composable, attached flyouts),
 * AppBarToggleButton, MenuFlyoutItem3, MenuFlyoutSubItem, ToolTip, IconSourceElement with a FontIconSource,
 * BitmapIcon, a program's ListViewItem (SelectorItem.IsSelected), AutomationProperties statics 2,
 * SwapChainPanel's native interfaces, ScrollBar, Path with path markup, a dependency object's Dispatcher,
 * the thread's DispatcherQueue, a ResourceDictionary iterated, and Loaded/LayoutUpdated of a hidden
 * island -- what Windows Terminal and WinUI 2.8 ask for. Interface identity per windows-rs 0.30.0 (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>
#include <math.h>

DEFINE_GUID(IID_GridLengthHelperStatics, 0x9d457b9b, 0x019f, 0x4266, 0x88, 0x72, 0x21, 0x5f, 0x19, 0x8f, 0x6a, 0x9d);
DEFINE_GUID(IID_RectHelperStatics, 0x5ee163e4, 0xc17e, 0x494f, 0xb5, 0x80, 0x2f, 0x05, 0x74, 0xfc, 0x3a, 0x15);
DEFINE_GUID(IID_SizeHelperStatics, 0x6286c5b2, 0xcf78, 0x4915, 0xaa, 0x40, 0x76, 0x00, 0x4a, 0x16, 0x5f, 0x5e);
DEFINE_GUID(IID_DurationHelperStatics, 0xbc88093e, 0x3547, 0x4ec0, 0xb5, 0x19, 0xff, 0xa8, 0xf9, 0xc4, 0x83, 0x8c);
DEFINE_GUID(IID_TimelineStatics, 0xa902ed4e, 0xef10, 0x4d6f, 0x9a, 0x40, 0x93, 0xcb, 0x88, 0x95, 0xf4, 0xe5);
DEFINE_GUID(IID_DispatcherTimer, 0xd160ce46, 0xcd22, 0x4f5f, 0x8c, 0x97, 0x40, 0xe6, 0x1d, 0xa3, 0xe2, 0xdc);
DEFINE_GUID(IID_DispatcherTimerFactory, 0xe9961e6e, 0x3626, 0x403a, 0xaf, 0xe0, 0x04, 0x0d, 0x58, 0x16, 0x56, 0x32);
DEFINE_GUID(IID_PropertyChangedEventArgs, 0x4f33a9a0, 0x5cf4, 0x47a4, 0xb1, 0x6f, 0xd7, 0xfa, 0xaf, 0x17, 0x45, 0x7e);
DEFINE_GUID(IID_PropertyChangedEventArgsFactory, 0x6dcc9c03, 0xe0c7, 0x4eee, 0x8e, 0xa9, 0x37, 0xe3, 0x40, 0x6e, 0xeb, 0x1c);
DEFINE_GUID(IID_XamlCompositionBrushBase, 0x03e432d9, 0xb35c, 0x4a79, 0x81, 0x1c, 0xc5, 0x65, 0x20, 0x04, 0xda, 0x0e);
DEFINE_GUID(IID_XamlCompositionBrushBaseFactory, 0x394f0823, 0x2451, 0x4ed8, 0xbd, 0x24, 0x48, 0x81, 0x49, 0xb3, 0x42, 0x8d);
DEFINE_GUID(IID_AcrylicBrush, 0x79bbcf4e, 0xcd66, 0x4f1b, 0xa8, 0xb6, 0xcd, 0x6d, 0x29, 0x77, 0xc1, 0x8d);
DEFINE_GUID(IID_AcrylicBrushFactory, 0x81a32568, 0xf6cc, 0x4013, 0x83, 0x63, 0x92, 0x8a, 0xe2, 0x3b, 0x7a, 0x61);
DEFINE_GUID(IID_XamlLightFactory, 0x87ded768, 0x3055, 0x43b8, 0x8e, 0xf6, 0x79, 0x8d, 0xc4, 0xc2, 0x32, 0x9a);
DEFINE_GUID(IID_FlyoutBaseFactory, 0x1c3363d7, 0xfca7, 0x407e, 0x92, 0x0e, 0x70, 0xe1, 0x5e, 0x9f, 0x0b, 0xf1);
DEFINE_GUID(IID_FlyoutBaseStatics, 0xe2d795e3, 0x85c0, 0x4de2, 0xba, 0xc1, 0x52, 0x94, 0xca, 0x01, 0x1a, 0x78);
DEFINE_GUID(IID_AppBarToggleButtonFactory, 0x028aa7d4, 0x8f54, 0x45a6, 0x9f, 0x90, 0x13, 0x60, 0x56, 0x56, 0xd7, 0x93);
DEFINE_GUID(IID_ToggleButton, 0x589877fb, 0x0fc7, 0x4036, 0x9d, 0x8b, 0x12, 0x7d, 0xfa, 0x75, 0xc1, 0x6d);
DEFINE_GUID(IID_CommandBarElement, 0x67918e1b, 0xf2d5, 0x44d1, 0x8b, 0x84, 0x92, 0xb8, 0x7f, 0x80, 0xa3, 0x50);
DEFINE_GUID(IID_MenuFlyoutItem3, 0x9727b2e3, 0xb2b6, 0x49dc, 0x8c, 0x22, 0xe0, 0x13, 0x8b, 0x94, 0x15, 0x59);
DEFINE_GUID(IID_MenuFlyoutSubItem, 0xe5386550, 0xfe2c, 0x43e2, 0xa7, 0x3f, 0x62, 0x16, 0x8f, 0x61, 0x99, 0x73);
DEFINE_GUID(IID_ToolTip, 0x7de5d75f, 0x4415, 0x429f, 0xb5, 0x23, 0x69, 0x9b, 0xdb, 0x8c, 0x30, 0xdb);
DEFINE_GUID(IID_ListViewItemFactory, 0xf431073d, 0x9cac, 0x42a2, 0x82, 0xdf, 0x0f, 0x44, 0x90, 0xbc, 0x4e, 0x2e);
DEFINE_GUID(IID_SelectorItem, 0x541c8d6c, 0x0283, 0x4581, 0xb9, 0x45, 0x2a, 0x64, 0xc2, 0x8a, 0x06, 0x46);
DEFINE_GUID(IID_AutomationPropertiesStatics2, 0x3976547f, 0x7089, 0x4801, 0x8f, 0x1d, 0xaa, 0xb7, 0x80, 0x90, 0xd1, 0xa0);
DEFINE_GUID(IID_ScrollBar, 0xf57ae6ca, 0xd1a6, 0x4b90, 0xa4, 0xe9, 0x54, 0xdf, 0x1b, 0xa8, 0xd2, 0xec);
DEFINE_GUID(IID_Path, 0x78883609, 0x3d57, 0x4f3c, 0xb8, 0xa5, 0x6c, 0xab, 0xca, 0xc9, 0x71, 0x1f);
DEFINE_GUID(IID_Geometry, 0xfa123889, 0x0acd, 0x417b, 0xb6, 0x2d, 0x5c, 0xa1, 0xbf, 0x4d, 0xfc, 0x0e);
DEFINE_GUID(IID_DependencyObject, 0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_UIElement5, 0x8eed9bc2, 0xa58c, 0x4453, 0xaf, 0x0f, 0xa9, 0x2e, 0xe0, 0x6d, 0x03, 0x17);
DEFINE_GUID(IID_VisualTreeHelperStatics, 0xe75758c4, 0xd25d, 0x4b1d, 0x97, 0x1f, 0x59, 0x6f, 0x17, 0xf1, 0x2b, 0xaa);
DEFINE_GUID(IID_FE, 0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_UIE, 0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_ReaderStatics, 0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory, 0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative, 0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_SwapChainPanelNative, 0xf92f19d2, 0x3ade, 0x45a6, 0xa2, 0x0c, 0xf6, 0xf1, 0xea, 0x90, 0x55, 0x4b);
DEFINE_GUID(IID_SwapChainPanelNative2, 0xd5a2f60c, 0x37b2, 0x44a2, 0x93, 0x7b, 0x8d, 0x8e, 0xb9, 0x72, 0x68, 0x21);
DEFINE_GUID(IID_QueueStatics, 0xa96d83d7, 0x9371, 0x4517, 0x92, 0x45, 0xd0, 0x82, 0x4a, 0xc1, 0x2c, 0x74);
DEFINE_GUID(IID_IterableKVP, 0x33ac68f0, 0x1084, 0x529a, 0x8a, 0x17, 0x4e, 0x7c, 0x8a, 0xdb, 0x7a, 0x0c);
DEFINE_GUID(IID_KVP, 0x59e7ae0c, 0xc29d, 0x5ad6, 0xbe, 0xf5, 0xde, 0xdb, 0x52, 0xa1, 0x98, 0xe1);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);
#define CALL(o, n, type) ((type)SLOT(o, n))

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
        while (GetTickCount() < end && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(5);
    }
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
static obj *reader;
static obj *load(const WCHAR *markup)
{
    obj *o = NULL;
    HRESULT hr = ((load_fn)SLOT(reader, 6))(reader, hs(markup), &o);
    if (FAILED(hr)) printf("load %08lx: %ls\n", hr, markup);
    return o;
}
static INT32 children(obj *vth, obj *elem)
{
    INT32 n = -1;
    if (vth && elem) CALL(vth, 11, HRESULT (WINAPI *)(obj *, obj *, INT32 *))(vth, as(elem, &IID_UIE), &n);
    return n;
}

struct handler { void **vtbl; LONG ref; int calls; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *a, void *b) { h->calls++; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

struct gridlength { double value; INT32 type; };
struct rect { float x, y, w, h; };
struct size { float w, h; };
struct duration { INT64 span; INT32 type; };

int main(void)
{
    struct handler tick = { h_vtbl, 1 }, loaded = { h_vtbl, 1 }, layout = { h_vtbl, 1 };
    obj *f, *ms, *mgr = NULL, *vth, *o = NULL, *o2 = NULL, *vec = NULL, *it = NULL, *kvp = NULL, *key = NULL;
    IInspectable *inner = NULL;
    struct gridlength gl = { 0 };
    struct rect r = { 0 }, a = { 0, 0, 10, 10 }, b = { 5, 5, 10, 10 };
    struct size sz = { 0 };
    struct duration d1 = { 0 }, d2 = { 0 }, d3 = { 0 }, dauto = { 0 };
    boolean flag = FALSE, flag2 = FALSE;
    INT32 cmp = 9, n = 0;
    UINT32 size = 0;
    INT64 token;
    HSTRING str = NULL;
    BYTE c[4] = { 0 };
    double dv = 0;
    HWND host, island = NULL;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(reader = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    if (ms) ((get_fn)SLOT(ms, 6))(ms, &mgr);
    vth = factory(L"Windows.UI.Xaml.Media.VisualTreeHelper", &IID_VisualTreeHelperStatics);

    /* value helpers */
    if ((f = factory(L"Windows.UI.Xaml.GridLengthHelper", &IID_GridLengthHelperStatics)))
    {
        CALL(f, 8, HRESULT (WINAPI *)(obj *, double, INT32, struct gridlength *))(f, 2.0, 2, &gl);
        CALL(f, 11, HRESULT (WINAPI *)(obj *, struct gridlength, boolean *))(f, gl, &flag);
    }
    printf("gridlength=%.0f,%d,%d\n", gl.value, gl.type, flag);
    if ((f = factory(L"Windows.UI.Xaml.RectHelper", &IID_RectHelperStatics)))
    {
        CALL(f, 17, HRESULT (WINAPI *)(obj *, struct rect, struct rect, struct rect *))(f, a, b, &r);
        CALL(f, 6, HRESULT (WINAPI *)(obj *, struct rect *))(f, &a);
        CALL(f, 10, HRESULT (WINAPI *)(obj *, struct rect, boolean *))(f, a, &flag);
    }
    printf("rect=%.0f,%.0f,%.0f,%.0f,%d\n", r.x, r.y, r.w, r.h, flag);
    if ((f = factory(L"Windows.UI.Xaml.SizeHelper", &IID_SizeHelperStatics))) CALL(f, 6, HRESULT (WINAPI *)(obj *, struct size *))(f, &sz);
    printf("size=%d\n", isinf(sz.w) && sz.w < 0);
    if ((f = factory(L"Windows.UI.Xaml.DurationHelper", &IID_DurationHelperStatics)))
    {
        CALL(f, 6, HRESULT (WINAPI *)(obj *, struct duration *))(f, &dauto);
        CALL(f, 9, HRESULT (WINAPI *)(obj *, INT64, struct duration *))(f, 10000000, &d1);
        CALL(f, 9, HRESULT (WINAPI *)(obj *, INT64, struct duration *))(f, 20000000, &d2);
        CALL(f, 8, HRESULT (WINAPI *)(obj *, struct duration, struct duration, INT32 *))(f, dauto, d1, &cmp);
        CALL(f, 11, HRESULT (WINAPI *)(obj *, struct duration, struct duration, struct duration *))(f, d1, d2, &d3);
    }
    printf("duration=%d,%d,%lld\n", cmp, d3.type, (long long)d3.span);

    /* Timeline statics, DispatcherTimer, PropertyChangedEventArgs */
    if ((f = factory(L"Windows.UI.Xaml.Media.Animation.Timeline", &IID_TimelineStatics)))
    {
        CALL(f, 7, HRESULT (WINAPI *)(obj *, boolean))(f, FALSE);
        flag = TRUE;
        CALL(f, 6, HRESULT (WINAPI *)(obj *, boolean *))(f, &flag);
    }
    printf("timeline=%d\n", flag);
    o = NULL;
    if ((f = factory(L"Windows.UI.Xaml.DispatcherTimer", &IID_DispatcherTimerFactory))) ((create_fn)SLOT(f, 6))(f, NULL, &inner, &o);
    if (o && as(o, &IID_DispatcherTimer))
    {
        obj *t = as(o, &IID_DispatcherTimer);
        CALL(t, 7, HRESULT (WINAPI *)(obj *, INT64))(t, 200000);   /* 20 ms */
        CALL(t, 9, HRESULT (WINAPI *)(obj *, void *, INT64 *))(t, &tick, &token);
        CALL(t, 11, HRESULT (WINAPI *)(obj *))(t);
        CALL(t, 8, HRESULT (WINAPI *)(obj *, boolean *))(t, &flag);
        pump(400);
        CALL(t, 12, HRESULT (WINAPI *)(obj *))(t);
        n = tick.calls;
        CALL(t, 8, HRESULT (WINAPI *)(obj *, boolean *))(t, &flag2);
        pump(150);
    }
    printf("timer=%d,%d,%d,%d\n", flag, n >= 3, flag2, tick.calls == n);
    o = NULL;
    if ((f = factory(L"Windows.UI.Xaml.Data.PropertyChangedEventArgs", &IID_PropertyChangedEventArgsFactory)))
        CALL(f, 6, HRESULT (WINAPI *)(obj *, HSTRING, IInspectable *, IInspectable **, obj **))(f, hs(L"Title"), NULL, &inner, &o);
    if (o) CALL(o, 6, HRESULT (WINAPI *)(obj *, HSTRING *))(o, &str);
    printf("propertychanged=%ls\n", str ? WindowsGetStringRawBuffer(str, NULL) : L"-");

    /* brushes */
    o = NULL;
    if ((f = factory(L"Windows.UI.Xaml.Media.XamlCompositionBrushBase", &IID_XamlCompositionBrushBaseFactory)))
        ((create_fn)SLOT(f, 6))(f, NULL, &inner, &o);
    if (o)
    {
        BYTE red[4] = { 0xff, 0xff, 0, 0 };
        UINT32 v;
        memcpy(&v, red, 4);
        CALL(o, 7, HRESULT (WINAPI *)(obj *, UINT32))(o, v);
        CALL(o, 6, HRESULT (WINAPI *)(obj *, BYTE *))(o, c);
    }
    printf("compositionbrush=%d,%02x%02x%02x%02x\n", o != NULL, c[0], c[1], c[2], c[3]);
    o = load(L"<AcrylicBrush " NS L" TintColor=\"#FF0000FF\" FallbackColor=\"#FF00FF00\" BackgroundSource=\"HostBackdrop\"/>");
    memset(c, 0, 4);
    if (o && as(o, &IID_AcrylicBrush)) CALL(as(o, &IID_AcrylicBrush), 8, HRESULT (WINAPI *)(obj *, BYTE *))(as(o, &IID_AcrylicBrush), c);
    printf("acrylic=%d,%02x%02x%02x%02x,%d\n", o != NULL, c[0], c[1], c[2], c[3], as(o, &IID_XamlCompositionBrushBase) != NULL);

    /* XamlLight in a Grid's Lights */
    o = NULL;
    if ((f = factory(L"Windows.UI.Xaml.Media.XamlLight", &IID_XamlLightFactory))) ((create_fn)SLOT(f, 6))(f, NULL, &inner, &o);
    o2 = load(L"<Grid " NS L"/>");
    vec = NULL;
    if (o2 && as(o2, &IID_UIElement5)) ((get_fn)SLOT(as(o2, &IID_UIElement5), 6))(as(o2, &IID_UIElement5), &vec);
    size = 0;
    if (vec && o) { CALL(vec, 13, HRESULT (WINAPI *)(obj *, obj *))(vec, o); CALL(vec, 7, HRESULT (WINAPI *)(obj *, UINT32 *))(vec, &size); }
    printf("lights=%d,%u\n", o != NULL, size);

    /* FlyoutBase */
    o = NULL;
    if ((f = factory(L"Windows.UI.Xaml.Controls.Primitives.FlyoutBase", &IID_FlyoutBaseFactory))) ((create_fn)SLOT(f, 6))(f, NULL, &inner, &o);
    o2 = load(L"<Button " NS L"/>");
    vec = NULL;
    if ((f = factory(L"Windows.UI.Xaml.Controls.Primitives.FlyoutBase", &IID_FlyoutBaseStatics)) && o && o2)
    {
        CALL(f, 9, HRESULT (WINAPI *)(obj *, obj *, obj *))(f, as(o2, &IID_FE), o);
        CALL(f, 8, HRESULT (WINAPI *)(obj *, obj *, obj **))(f, as(o2, &IID_FE), &vec);
    }
    printf("flyoutbase=%d,%d\n", o != NULL, vec != NULL);

    /* AppBarToggleButton, menu items, ToolTip */
    o = load(L"<AppBarToggleButton " NS L" Label=\"Pin\"/>");
    printf("appbartoggle=%d,%d\n", as(o, &IID_ToggleButton) != NULL, as(o, &IID_CommandBarElement) != NULL);
    o = load(L"<MenuFlyoutItem " NS L" Text=\"Copy\"/>");
    str = NULL;
    if (o && as(o, &IID_MenuFlyoutItem3))
    {
        CALL(as(o, &IID_MenuFlyoutItem3), 7, HRESULT (WINAPI *)(obj *, HSTRING))(as(o, &IID_MenuFlyoutItem3), hs(L"Ctrl+C"));
        CALL(as(o, &IID_MenuFlyoutItem3), 6, HRESULT (WINAPI *)(obj *, HSTRING *))(as(o, &IID_MenuFlyoutItem3), &str);
    }
    printf("menuitem3=%ls\n", str ? WindowsGetStringRawBuffer(str, NULL) : L"-");
    o = load(L"<MenuFlyoutSubItem " NS L" Text=\"More\"><MenuFlyoutItem Text=\"X\"/><MenuFlyoutItem Text=\"Y\"/></MenuFlyoutSubItem>");
    vec = NULL; size = 0; str = NULL;
    if (o && as(o, &IID_MenuFlyoutSubItem))
    {
        ((get_fn)SLOT(as(o, &IID_MenuFlyoutSubItem), 6))(as(o, &IID_MenuFlyoutSubItem), &vec);
        CALL(as(o, &IID_MenuFlyoutSubItem), 7, HRESULT (WINAPI *)(obj *, HSTRING *))(as(o, &IID_MenuFlyoutSubItem), &str);
    }
    if (vec) CALL(vec, 7, HRESULT (WINAPI *)(obj *, UINT32 *))(vec, &size);
    printf("subitem=%ls,%u\n", str ? WindowsGetStringRawBuffer(str, NULL) : L"-", size);
    str = NULL;
    if (o && as(o, &IID_MenuFlyoutSubItem))
    {
        CALL(as(o, &IID_MenuFlyoutSubItem), 8, HRESULT (WINAPI *)(obj *, HSTRING))(as(o, &IID_MenuFlyoutSubItem), hs(L"Less"));
        CALL(as(o, &IID_MenuFlyoutSubItem), 7, HRESULT (WINAPI *)(obj *, HSTRING *))(as(o, &IID_MenuFlyoutSubItem), &str);
    }
    printf("subitemset=%ls\n", str ? WindowsGetStringRawBuffer(str, NULL) : L"-");
    o = load(L"<ToolTip " NS L" Content=\"tip\"/>");
    flag = FALSE;
    if (o && as(o, &IID_ToolTip))
    {
        CALL(as(o, &IID_ToolTip), 9, HRESULT (WINAPI *)(obj *, boolean))(as(o, &IID_ToolTip), TRUE);
        CALL(as(o, &IID_ToolTip), 8, HRESULT (WINAPI *)(obj *, boolean *))(as(o, &IID_ToolTip), &flag);
    }
    printf("tooltip=%d,%d\n", o != NULL, flag);

    /* icons */
    o = load(L"<IconSourceElement " NS L"><IconSourceElement.IconSource><FontIconSource Glyph=\"&#xE710;\" FontSize=\"16\"/>"
             L"</IconSourceElement.IconSource></IconSourceElement>");
    printf("iconsource=%d,%d\n", o != NULL, children(vth, o));
    o = load(L"<BitmapIcon " NS L" UriSource=\"ms-appx:///none.png\"/>");
    printf("bitmapicon=%d,%d\n", o != NULL, children(vth, o));

    /* a program's ListViewItem */
    o = NULL;
    if ((f = factory(L"Windows.UI.Xaml.Controls.ListViewItem", &IID_ListViewItemFactory))) ((create_fn)SLOT(f, 6))(f, NULL, &inner, &o);
    flag = FALSE;
    if (o && as(o, &IID_SelectorItem))
    {
        CALL(as(o, &IID_SelectorItem), 7, HRESULT (WINAPI *)(obj *, boolean))(as(o, &IID_SelectorItem), TRUE);
        CALL(as(o, &IID_SelectorItem), 6, HRESULT (WINAPI *)(obj *, boolean *))(as(o, &IID_SelectorItem), &flag);
    }
    printf("listviewitem=%d,%d\n", o != NULL, flag);

    /* AutomationProperties statics 2 */
    o = load(L"<Border " NS L" AutomationProperties.AccessibilityView=\"Raw\"/>");
    n = -1; cmp = -1;
    if ((f = factory(L"Windows.UI.Xaml.Automation.AutomationProperties", &IID_AutomationPropertiesStatics2)) && o)
    {
        CALL(f, 7, HRESULT (WINAPI *)(obj *, obj *, INT32 *))(f, as(o, &IID_DependencyObject), &n);
        CALL(f, 8, HRESULT (WINAPI *)(obj *, obj *, INT32))(f, as(o, &IID_DependencyObject), 2);
        CALL(f, 7, HRESULT (WINAPI *)(obj *, obj *, INT32 *))(f, as(o, &IID_DependencyObject), &cmp);
    }
    printf("automation=%d,%d\n", n, cmp);

    /* SwapChainPanel, ScrollBar, Path */
    o = load(L"<SwapChainPanel " NS L"/>");
    hr = E_FAIL;
    if (as(o, &IID_SwapChainPanelNative2)) hr = CALL(as(o, &IID_SwapChainPanelNative2), 4, HRESULT (WINAPI *)(obj *, HANDLE))(as(o, &IID_SwapChainPanelNative2), NULL);
    {
    HRESULT hr2 = E_FAIL;
    if (as(o, &IID_SwapChainPanelNative)) hr2 = CALL(as(o, &IID_SwapChainPanelNative), 3, HRESULT (WINAPI *)(obj *, IUnknown *))(as(o, &IID_SwapChainPanelNative), NULL);
    printf("swapchainpanel=%d,%d,%08lx,%08lx\n", o != NULL, as(o, &IID_SwapChainPanelNative) != NULL, hr, hr2);
    }
    o = load(L"<ScrollBar " NS L" Orientation=\"Horizontal\" ViewportSize=\"10\" Maximum=\"100\"/>");
    n = -1; dv = -1;
    if (o && as(o, &IID_ScrollBar))
    {
        CALL(as(o, &IID_ScrollBar), 6, HRESULT (WINAPI *)(obj *, INT32 *))(as(o, &IID_ScrollBar), &n);
        CALL(as(o, &IID_ScrollBar), 8, HRESULT (WINAPI *)(obj *, double *))(as(o, &IID_ScrollBar), &dv);
    }
    printf("scrollbar=%d,%.0f\n", n, dv);
    o = load(L"<Path " NS L" Data=\"M 2,3 L 12,3 l 0,10 H 2 Z\" Fill=\"Red\"/>");
    o2 = NULL;
    memset(&r, 0, sizeof(r));
    if (o && as(o, &IID_Path)) ((get_fn)SLOT(as(o, &IID_Path), 6))(as(o, &IID_Path), &o2);
    if (o2 && as(o2, &IID_Geometry)) CALL(as(o2, &IID_Geometry), 8, HRESULT (WINAPI *)(obj *, struct rect *))(as(o2, &IID_Geometry), &r);
    printf("path=%d,%.0f,%.0f,%.0f,%.0f\n", o2 != NULL, r.x, r.y, r.w, r.h);

    /* a dependency object's Dispatcher, the thread's DispatcherQueue */
    o2 = NULL;
    if (o && as(o, &IID_DependencyObject)) ((get_fn)SLOT(as(o, &IID_DependencyObject), 11))(as(o, &IID_DependencyObject), &o2);
    vec = NULL;
    if ((f = factory(L"Windows.System.DispatcherQueue", &IID_QueueStatics))) ((get_fn)SLOT(f, 6))(f, &vec);
    printf("dispatchers=%d,%d\n", o2 != NULL, vec != NULL);

    /* a ResourceDictionary's entries iterated */
    o = load(L"<ResourceDictionary " NS L"><SolidColorBrush x:Key=\"A\" Color=\"Red\"/><SolidColorBrush x:Key=\"B\" Color=\"Blue\"/></ResourceDictionary>");
    n = 0; flag = FALSE; str = NULL;
    if (as(o, &IID_IterableKVP)) ((get_fn)SLOT(as(o, &IID_IterableKVP), 6))(as(o, &IID_IterableKVP), &it);
    if (it)
    {
        CALL(it, 7, HRESULT (WINAPI *)(obj *, boolean *))(it, &flag);
        while (flag)
        {
            if (!n && SUCCEEDED(((get_fn)SLOT(it, 6))(it, &kvp)) && as(kvp, &IID_KVP))
                ((get_fn)SLOT(as(kvp, &IID_KVP), 6))(as(kvp, &IID_KVP), &key);
            n++;
            CALL(it, 8, HRESULT (WINAPI *)(obj *, boolean *))(it, &flag);
        }
    }
    printf("dictionary=%d,%d\n", n, key != NULL);

    /* Loaded and LayoutUpdated of content in an island not shown */
    {
        obj *sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory), *src = NULL, *native;
        ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
        native = as(src, &IID_SourceNative);
        host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP, 0, 0, 200, 100, NULL, NULL, NULL, NULL);   /* hidden */
        ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
        ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
        o = load(L"<Border " NS L" Width=\"50\" Height=\"20\"/>");
        CALL(as(o, &IID_FE), 43, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(o, &IID_FE), &loaded, &token);
        CALL(as(o, &IID_FE), 49, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(o, &IID_FE), &layout, &token);
        ((HRESULT (WINAPI *)(obj *, obj *))SLOT(src, 7))(src, as(o, &IID_UIE));
        pump(300);
        printf("hidden=%d,%d,%d\n", IsWindowVisible(host), loaded.calls, layout.calls >= 1);
    }
    return 0;
}
