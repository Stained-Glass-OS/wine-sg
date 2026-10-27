/* Windows.UI.Xaml drawing (patches/sg/6136): a XAML Island (DesktopWindowXamlSource) paints its
 * content -- a Grid with a red background, a Border (blue, green 4px border) and black text -- and
 * paints again when a brush changes. Pixels are read back from the island window. Interface identity
 * per windows-rs 0.30.0 (MIT OR Apache-2.0). Prints KEY=VALUE. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_GridFactory,    0xae814041, 0xc531, 0x43b4, 0xbf, 0x99, 0x12, 0xf5, 0x06, 0xf7, 0xb0, 0x1c);
DEFINE_GUID(IID_Panel,          0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_Border,         0x797c4539, 0x45bd, 0x4633, 0xa0, 0x44, 0xbf, 0xb0, 0x2e, 0xf5, 0x17, 0x0f);
DEFINE_GUID(IID_TB,             0xae2d9271, 0x3b4a, 0x45fc, 0x84, 0x68, 0xf7, 0x94, 0x95, 0x48, 0xf4, 0xd5);
DEFINE_GUID(IID_SCBFactory,     0xd935ce0c, 0x86f5, 0x4da6, 0x8a, 0x27, 0xb1, 0x61, 0x9e, 0xf7, 0xf9, 0x2b);
DEFINE_GUID(IID_SCB,            0x9d850850, 0x66f3, 0x48df, 0x9a, 0x8f, 0x82, 0x4b, 0xd5, 0xe0, 0x70, 0xaf);
DEFINE_GUID(IID_Brush,          0x8806a321, 0x1e06, 0x422c, 0xa1, 0xcc, 0x01, 0x69, 0x65, 0x59, 0xe0, 0x21);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
struct thickness { double l, t, r, b; };
struct color { BYTE a, r, g, b; };
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *act_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putd_fn)(obj *, double);
typedef HRESULT (WINAPI *puti_fn)(obj *, INT32);
typedef HRESULT (WINAPI *puts_fn)(obj *, HSTRING);
typedef HRESULT (WINAPI *putth_fn)(obj *, struct thickness);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);

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

int main(void)
{
    obj *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *grid = NULL, *border = NULL, *text = NULL, *bf, *tbf, *red;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    HDC hdc;
    int dark = 0, x, y;
    COLORREF c;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics))) return 1;
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory))) return 1;
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);

    if (!(scbf = factory(L"Windows.UI.Xaml.Media.SolidColorBrush", &IID_SCBFactory))) return 1;
    {
        obj *gf = factory(L"Windows.UI.Xaml.Controls.Grid", &IID_GridFactory), *kids = NULL;
        ((create_fn)SLOT(gf, 6))(gf, NULL, &inner, &grid);
        red = brush(255, 0, 0);
        ((putobj_fn)SLOT(as(grid, &IID_Panel), 8))(as(grid, &IID_Panel), red);                 /* Background */
        bf = factory(L"Windows.UI.Xaml.Controls.Border", &IID_IActivationFactory);
        ((act_fn)SLOT(bf, 6))(bf, &border);
        border = as(border, &IID_Border);
        {
            obj *fe = as(border, &IID_FE);
            struct thickness m = { 10, 20, 0, 0 }, four = { 4, 4, 4, 4 };
            ((putd_fn)SLOT(fe, 16))(fe, 100); ((putd_fn)SLOT(fe, 18))(fe, 50);
            ((puti_fn)SLOT(fe, 28))(fe, 0); ((puti_fn)SLOT(fe, 30))(fe, 0);
            ((putth_fn)SLOT(fe, 32))(fe, m);
            ((putobj_fn)SLOT(border, 11))(border, brush(0, 0, 255));                          /* Background */
            ((putobj_fn)SLOT(border, 7))(border, brush(0, 255, 0));                           /* BorderBrush */
            ((putth_fn)SLOT(border, 9))(border, four);                                       /* BorderThickness */
        }
        tbf = factory(L"Windows.UI.Xaml.Controls.TextBlock", &IID_IActivationFactory);
        ((act_fn)SLOT(tbf, 6))(tbf, &text);
        text = as(text, &IID_TB);
        ((puts_fn)SLOT(text, 27))(text, hs(L"XXXX"));
        ((putd_fn)SLOT(text, 7))(text, 40);
        ((putobj_fn)SLOT(text, 19))(text, brush(0, 0, 0));
        {
            obj *fe = as(text, &IID_FE);
            struct thickness m = { 150, 100, 0, 0 };
            ((puti_fn)SLOT(fe, 28))(fe, 0); ((puti_fn)SLOT(fe, 30))(fe, 0);
            ((putth_fn)SLOT(fe, 32))(fe, m);
        }
        ((get_fn)SLOT(as(grid, &IID_Panel), 6))(as(grid, &IID_Panel), &kids);
        ((putobj_fn)SLOT(kids, 13))(kids, as(border, &IID_UIE));
        ((putobj_fn)SLOT(kids, 13))(kids, as(text, &IID_UIE));
    }
    ((putobj_fn)SLOT(src, 7))(src, as(grid, &IID_UIE));                                       /* Content */
    pump(800);
    UpdateWindow(island);
    pump(300);

    hdc = GetDC(island);
    c = GetPixel(hdc, 5, 5);       printf("background=%d\n", close_to(c, 255, 0, 0));
    c = GetPixel(hdc, 60, 45);     printf("border_fill=%d\n", close_to(c, 0, 0, 255));
    c = GetPixel(hdc, 11, 21);     printf("border_edge=%d\n", close_to(c, 0, 255, 0));
    c = GetPixel(hdc, 120, 45);    printf("outside_border=%d\n", close_to(c, 255, 0, 0));
    for (y = 100; y < 160; y += 2) for (x = 150; x < 298; x += 2)
        if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) dark++;
    printf("text=%d\n", dark > 40);
    ReleaseDC(island, hdc);

    /* a brush changes: the island paints again */
    {
        struct color yellow = { 255, 255, 255, 0 };
        ((HRESULT (WINAPI *)(obj *, struct color))SLOT(as(red, &IID_SCB), 7))(as(red, &IID_SCB), yellow);
    }
    pump(500);
    hdc = GetDC(island);
    c = GetPixel(hdc, 5, 5);       printf("repainted=%d\n", close_to(c, 255, 255, 0));
    ReleaseDC(island, hdc);
    return 0;
}
