/* Windows.UI.Xaml transforms (patches/sg/6184): TranslateTransform, ScaleTransform about a centre,
 * RotateTransform, CompositeTransform and its Inverse, MatrixTransform, a TransformGroup's children and
 * Value as GeneralTransforms; an element's RenderTransform from markup, drawn moved and scaled where it
 * was laid out (Windows Terminal's tab and title bar animations).  Interface identity per windows-rs
 * 0.30.0 (MIT). */
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
DEFINE_GUID(IID_Translate,      0xc975905c, 0x3c36, 0x4229, 0x81, 0x7b, 0x17, 0x8f, 0x64, 0xc0, 0xe1, 0x13);
DEFINE_GUID(IID_Scale,          0xed67f18d, 0x936e, 0x43ab, 0x92, 0x9a, 0xe9, 0xcd, 0x0a, 0x51, 0x1e, 0x52);
DEFINE_GUID(IID_Rotate,         0x688ea9b9, 0x1e4e, 0x4596, 0x86, 0xe3, 0x42, 0x8b, 0x27, 0x33, 0x4f, 0xaf);
DEFINE_GUID(IID_Composite,      0xc8a4385b, 0xf24a, 0x4701, 0xa2, 0x65, 0xa7, 0x88, 0x46, 0xf1, 0x42, 0xb9);
DEFINE_GUID(IID_Group,          0x63418ccc, 0x8d2d, 0x4737, 0xb9, 0x51, 0x2a, 0xfc, 0xe1, 0xdd, 0xc4, 0xc4);
DEFINE_GUID(IID_MatrixT,        0xedfdd551, 0x5fed, 0x45fc, 0xae, 0x62, 0x92, 0xa4, 0xb6, 0xcf, 0x97, 0x07);
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
    L"<Border x:Name=\"Moved\"" TL L" Width=\"40\" Height=\"40\" Background=\"Red\">"
    L"<Border.RenderTransform><TranslateTransform X=\"100\"/></Border.RenderTransform></Border>"
    L"<Border" TL L" Margin=\"0,100,0,0\" Width=\"20\" Height=\"20\" Background=\"Blue\" RenderTransformOrigin=\"0,0\">"
    L"<Border.RenderTransform><TransformGroup><ScaleTransform ScaleX=\"3\" ScaleY=\"2\"/>"
    L"<TranslateTransform X=\"150\"/></TransformGroup></Border.RenderTransform></Border>"
    L"</Grid>";

typedef struct { float X, Y; } pt;
typedef struct { double m11, m12, m21, m22, dx, dy; } matrix;
static pt apply(obj *t, float x, float y)
{
    pt in = { x, y }, out = { -1, -1 };
    t = as(t, &IID_GT);
    if (t) ((HRESULT (WINAPI *)(obj *, pt, pt *))SLOT(t, 7))(t, in, &out);
    return out;
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
static void putd(obj *o, int slot, double v) { if (o) ((HRESULT (WINAPI *)(obj *, double))SLOT(o, slot))(o, v); }
static COLORREF pixel(HWND hwnd, int x, int y)
{
    HDC hdc = GetDC(hwnd);
    COLORREF c;
    UpdateWindow(hwnd);
    c = GetPixel(hdc, x, y);
    ReleaseDC(hwnd, hdc);
    return c;
}

int main(void)
{
    obj *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *t, *inv = NULL, *g, *children = NULL, *m, *moved, *rt = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    matrix mx = { 0, 1, -1, 0, 7, 8 }, val = { 0 };
    HRESULT hr;
    pt p;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);

    t = activate(L"Windows.UI.Xaml.Media.TranslateTransform", &IID_Translate);
    putd(t, 7, 10); putd(t, 9, -3);
    p = apply(t, 5, 5);
    printf("translate=%.0f,%.0f\n", p.X, p.Y);
    t = activate(L"Windows.UI.Xaml.Media.ScaleTransform", &IID_Scale);
    putd(t, 7, 10); putd(t, 9, 10); putd(t, 11, 2); putd(t, 13, 3);   /* about 10,10 */
    p = apply(t, 12, 11);
    printf("scale=%.0f,%.0f\n", p.X, p.Y);
    t = activate(L"Windows.UI.Xaml.Media.RotateTransform", &IID_Rotate);
    putd(t, 11, 90);
    p = apply(t, 5, 2);
    printf("rotate=%.0f,%.0f\n", p.X, p.Y);
    t = activate(L"Windows.UI.Xaml.Media.CompositeTransform", &IID_Composite);
    putd(t, 11, 2); putd(t, 21, 10); putd(t, 23, 1);
    p = apply(t, 5, 5);
    printf("composite=%.0f,%.0f\n", p.X, p.Y);
    if (as(t, &IID_GT)) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(as(t, &IID_GT), 6))(as(t, &IID_GT), &inv);
    p = apply(inv, 20, 6);
    printf("inverse=%.0f,%.0f\n", p.X, p.Y);
    m = activate(L"Windows.UI.Xaml.Media.MatrixTransform", &IID_MatrixT);
    if (m) ((HRESULT (WINAPI *)(obj *, matrix))SLOT(m, 7))(m, mx);
    p = apply(m, 1, 2);
    printf("matrix=%.0f,%.0f\n", p.X, p.Y);
    /* a group: first scaled by 2, then moved by 10,0 */
    g = activate(L"Windows.UI.Xaml.Media.TransformGroup", &IID_Group);
    t = activate(L"Windows.UI.Xaml.Media.ScaleTransform", &IID_Scale);
    putd(t, 11, 2); putd(t, 13, 2);
    if (g) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(g, 6))(g, &children);
    if (children) ((HRESULT (WINAPI *)(obj *, obj *))SLOT(children, 13))(children, t);
    t = activate(L"Windows.UI.Xaml.Media.TranslateTransform", &IID_Translate);
    putd(t, 7, 10);
    if (children) ((HRESULT (WINAPI *)(obj *, obj *))SLOT(children, 13))(children, t);
    p = apply(g, 5, 5);
    if (g) ((HRESULT (WINAPI *)(obj *, matrix *))SLOT(g, 8))(g, &val);
    printf("group=%.0f,%.0f value=%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n", p.X, p.Y, val.m11, val.m12, val.m21, val.m22, val.dx, val.dy);

    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(grid, &IID_FE), 51))(as(grid, &IID_FE), hs(L"Moved"), &moved);
    if (moved) ((get_fn)SLOT(as(moved, &IID_UIE), 13))(as(moved, &IID_UIE), &rt);
    printf("render_transform=%d\n", as(rt, &IID_Translate) != NULL);
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
    /* drawn moved: red at 100..140, white where it was laid out */
    printf("moved=%d,%d\n", close_to(pixel(island, 120, 20), 255, 0, 0), close_to(pixel(island, 20, 20), 255, 255, 255));
    /* scaled 3x2 then moved 150: blue over 150..210 x 100..140 */
    printf("scaled=%d,%d,%d\n", close_to(pixel(island, 205, 135), 0, 0, 255), close_to(pixel(island, 10, 110), 255, 255, 255),
           close_to(pixel(island, 215, 110), 255, 255, 255));
    return 0;
}
