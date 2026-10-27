/* Windows.UI.Xaml text box and its hosts (patches/sg/6152): TextBox showing its Text in the desktop's
 * own field and in its template's ContentElement (a ScrollViewer), TextChanged, MaxLength;
 * RelativePanel's panel alignments; a Popup taking no room and not shown; ListView's Items --
 * NanaZip's address bar. Interface identity per windows-rs 0.30.0 (MIT). */
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
    L"<TextBox x:Name=\"Plain\"" TL L" Width=\"200\" Height=\"32\" FontSize=\"20\" Text=\"Hello\"/>"
    L"<TextBox x:Name=\"Templ\"" TL L" Margin=\"0,50,0,0\" Width=\"200\" Height=\"40\" FontSize=\"24\" Text=\"XXXX\">"
    L"<TextBox.Template><ControlTemplate TargetType=\"TextBox\"><Border Background=\"Yellow\">"
    L"<ScrollViewer x:Name=\"ContentElement\" Margin=\"10,5,0,0\"/></Border></ControlTemplate></TextBox.Template></TextBox>"
    L"<RelativePanel" TL L" Margin=\"0,100,0,0\" Width=\"200\" Height=\"40\">"
    L"<Border RelativePanel.AlignLeftWithPanel=\"True\" RelativePanel.AlignRightWithPanel=\"True\" Height=\"10\" Background=\"Blue\"/>"
    L"<Border RelativePanel.AlignRightWithPanel=\"True\" RelativePanel.AlignBottomWithPanel=\"True\" Width=\"20\" Height=\"20\" Background=\"Red\"/>"
    L"</RelativePanel>"
    L"<Popup" TL L" Margin=\"0,150,0,0\"><Border Width=\"50\" Height=\"30\" Background=\"Lime\"/></Popup>"
    L"<ListView x:Name=\"List\"" TL L" Margin=\"250,0,0,0\"/>"
    L"</Grid>";

static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

/* a TextChanged handler */
struct handler { void **vtbl; LONG ref; int calls; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *sender, void *args) { h->calls++; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

static const WCHAR *text_of(obj *tb, HSTRING *h)
{
    *h = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(tb, 6))(tb, h);
    return WindowsGetStringRawBuffer(*h, NULL);
}

static int dark_in(HDC hdc, int x0, int y0, int x1, int y1)
{
    int x, y, n = 0;
    for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) n++;
    return n;
}

int main(void)
{
    struct handler h = { h_vtbl, 1 };
    obj *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *plain, *items = NULL;
    IInspectable *inner = NULL, *one = NULL;
    HWND host, island = NULL;
    HSTRING s;
    INT64 token;
    UINT32 n = 0;
    HRESULT hr;
    HDC hdc;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    plain = as(find(grid, L"Plain"), &IID_TextBox);
    printf("text=%ls\n", plain ? text_of(plain, &s) : L"?");
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(plain, 30))(plain, &h, &token);
    ((HRESULT (WINAPI *)(obj *, HSTRING))SLOT(plain, 7))(plain, hs(L"World"));
    printf("changed=%d,%ls\n", h.calls, text_of(plain, &s));
    {
        obj *t2 = as(find(grid, L"Templ"), &IID_TextBox);
        ((HRESULT (WINAPI *)(obj *, INT32))SLOT(t2, 15))(t2, 3);
        ((HRESULT (WINAPI *)(obj *, HSTRING))SLOT(t2, 7))(t2, hs(L"XXXXXX"));
        printf("max_length=%ls\n", text_of(t2, &s));
    }
    ((get_fn)SLOT(as(find(grid, L"List"), &IID_ItemsControl), 8))(as(find(grid, L"List"), &IID_ItemsControl), &items);
    {
        obj *pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
        ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(L"item"), &one);
    }
    if (items)
    {
        ((HRESULT (WINAPI *)(obj *, IInspectable *))SLOT(items, 13))(items, one);
        ((HRESULT (WINAPI *)(obj *, IInspectable *))SLOT(items, 13))(items, one);
        ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(items, 7))(items, &n);
    }
    printf("items=%u\n", n);

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
    printf("plain_field=%d,%d\n", dark_in(hdc, 2, 2, 120, 30) > 20, !close_to(GetPixel(hdc, 0, 15), 255, 255, 255));
    printf("template_field=%d,%d\n", close_to(GetPixel(hdc, 195, 85), 255, 255, 0), dark_in(hdc, 10, 55, 120, 88) > 20);
    printf("relative=%d,%d,%d\n", close_to(GetPixel(hdc, 195, 105), 0, 0, 255), close_to(GetPixel(hdc, 190, 135), 255, 0, 0),
           close_to(GetPixel(hdc, 100, 135), 255, 255, 255));
    printf("popup_hidden=%d\n", close_to(GetPixel(hdc, 20, 160), 255, 255, 255));
    ReleaseDC(island, hdc);
    return 0;
}
