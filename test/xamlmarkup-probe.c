/* Windows.UI.Xaml markup (patches/sg/6138): XAML text through XamlReader.Load and
 * Application.LoadComponent -- elements, attributes converted by type (numbers, Thickness,
 * GridLength, colours, enums), property elements, attached properties, content properties, x:Name,
 * x:ConnectionId handed to the component; malformed markup fails cleanly. Drawn in a XAML Island
 * and read back. Interface identity per windows-rs 0.30.0 (MIT OR Apache-2.0). */
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
    L"<Grid " NS L" Background=\"Red\">"
    L"<Grid.RowDefinitions><RowDefinition Height=\"40\"/><RowDefinition Height=\"*\"/></Grid.RowDefinitions>"
    L"<Grid.ColumnDefinitions><ColumnDefinition Width=\"Auto\"/><ColumnDefinition Width=\"*\"/></Grid.ColumnDefinitions>"
    L"<Border x:Name=\"Box\" Grid.Row=\"1\" Grid.Column=\"1\" Background=\"#FF0000FF\" Margin=\"10,20,0,0\" Width=\"100\""
    L" Height=\"50\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" BorderBrush=\"Lime\" BorderThickness=\"4\"/>"
    L"<TextBlock Text=\"XXXX\" FontSize=\"40\" Foreground=\"Black\" Grid.Row=\"1\" VerticalAlignment=\"Top\"/>"
    L"<Unknown Thing=\"1\"><Grid/></Unknown>"
    L"<StackPanel Orientation=\"Horizontal\"><TextBlock>Hi</TextBlock></StackPanel>"
    L"</Grid>";

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

int main(void)
{
    obj *rs, *root = NULL, *box = NULL, *bad = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *cols = NULL, *c0 = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    double c0w = -1;
    HRESULT hr;
    HDC hdc;
    int x, y, dark = 0;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(L"Box"), &box);   /* FindName */
    printf("findname=%d\n", as(box, &IID_Border) != NULL);

    /* malformed markup fails, and nothing crashes */
    bad = NULL; hr = ((load_fn)SLOT(rs, 6))(rs, hs(L"<Grid " NS L"><Border>"), &bad);
    printf("unclosed=%d\n", FAILED(hr) && !bad);
    bad = NULL; hr = ((load_fn)SLOT(rs, 6))(rs, hs(L"<Grid " NS L" Width=\"abc\"/>"), &bad);
    printf("bad_value=%d\n", FAILED(hr) && !bad);
    bad = NULL; hr = ((load_fn)SLOT(rs, 6))(rs, hs(L"not markup"), &bad);
    printf("not_xml=%d\n", FAILED(hr) && !bad);
    {
        static WCHAR deep[12000];
        int i;
        wcscpy(deep, L"<Grid " NS L">");
        for (i = 0; i < 400; i++) wcscat(deep, L"<Grid>");
        for (i = 0; i < 400; i++) wcscat(deep, L"</Grid>");
        wcscat(deep, L"</Grid>");
        bad = NULL; hr = ((load_fn)SLOT(rs, 6))(rs, hs(deep), &bad);
        printf("too_deep=%d\n", FAILED(hr) && !bad);
    }

    /* drawn */
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
    ((get_fn)SLOT(as(root, &IID_Grid), 7))(as(root, &IID_Grid), &cols);
    ((HRESULT (WINAPI *)(obj *, UINT32, obj **))SLOT(cols, 6))(cols, 0, &c0);
    ((getd_fn)SLOT(c0, 12))(c0, &c0w);
    hdc = GetDC(island);
    printf("background=%d\n", close_to(GetPixel(hdc, 295, 195), 255, 0, 0));
    printf("auto_column=%d\n", c0w > 40 && c0w < 250);
    x = (int)c0w + 10;
    printf("border=%d,%d\n", close_to(GetPixel(hdc, x + 50, 40 + 20 + 25), 0, 0, 255), close_to(GetPixel(hdc, x + 1, 40 + 21), 0, 255, 0));
    for (y = 40; y < 100; y += 2) for (x = 0; x < (int)c0w; x += 2) if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) dark++;
    printf("text=%d\n", dark > 30);
    ReleaseDC(island, hdc);

    /* LoadComponent: a page's markup next to the program, x:ConnectionId to the page */
    {
        static const char page[] = "<Page xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Background=\"Green\">"
            "<TextBlock x:ConnectionId=\"2\" x:Name=\"Label\" Text=\"Hi\"/></Page>";
        struct app_page app = { { &outer_vtbl }, { conn_vtbl }, 1 };
        obj *pf = factory(L"Windows.UI.Xaml.Controls.Page", &IID_PageFactory), *pg = NULL, *as_ = NULL, *uf = NULL, *uri = NULL, *content = NULL;
        HANDLE file = CreateFileW(L"TestPage.xaml", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD written;
        WriteFile(file, page, sizeof(page) - 1, &written, NULL);
        CloseHandle(file);
        ((create_fn)SLOT(pf, 6))(pf, &app.IInspectable_iface, &app.inner, &pg);
        as_ = factory(L"Windows.UI.Xaml.Application", &IID_AppStatics);
        uf = factory(L"Windows.Foundation.Uri", &IID_UriFactory);
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(uf, 6))(uf, hs(L"ms-appx:///TestPage.xaml"), &uri);
        hr = ((HRESULT (WINAPI *)(obj *, IInspectable *, obj *))SLOT(as_, 8))(as_, &app.IInspectable_iface, uri);
        ((get_fn)SLOT(as(pg, &IID_UC), 6))(as(pg, &IID_UC), &content);
        printf("loadcomponent=%08lx content=%d connected=%d,%d\n", hr, as(content, &IID_TB) != NULL, app.id,
               as(app.target, &IID_IUnknown) == as(content, &IID_IUnknown));
    }
    return 0;
}
