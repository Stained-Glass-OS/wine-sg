/* Windows.UI.Xaml flyouts (patches/sg/6156): a right click on a TextBox shows its context menu (Cut,
 * Copy, Paste, Select All) in a popup window, its commands act on the box through the clipboard; a
 * CommandBarFlyout's ShowAt shows its commands by their labels below its target, a command clicked runs
 * (Click) and closes it (Closed); a click elsewhere closes it -- NanaZip's address bar context menu.
 * Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_Popup,          0x62418240, 0xe6d3, 0x4705, 0xa1, 0xdc, 0x39, 0x15, 0x64, 0x56, 0xee, 0x29);
DEFINE_GUID(IID_ButtonBase,     0xfa002c1a, 0x494e, 0x46cf, 0x91, 0xd4, 0xe1, 0x4a, 0x8d, 0x79, 0x86, 0x74);
DEFINE_GUID(IID_CBFFactory,     0xdd6441b3, 0x6df1, 0x593d, 0xb8, 0x0c, 0xda, 0xf5, 0xc1, 0xda, 0xee, 0x41);
DEFINE_GUID(IID_CBFlyout,       0xfecd2c4d, 0xf16a, 0x58cd, 0x8a, 0xca, 0xdb, 0x0f, 0x04, 0x6f, 0x25, 0xc3);
DEFINE_GUID(IID_FlyoutBase,     0x723eea0b, 0xd12e, 0x430d, 0xa9, 0xf0, 0x9b, 0xb3, 0x2b, 0xbf, 0x99, 0x13);
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
    L"<TextBox x:Name=\"Box\"" TL L" Width=\"200\" Height=\"32\" Text=\"hello\"/>"
    L"<Border x:Name=\"Target\"" TL L" Margin=\"20,80,0,0\" Width=\"60\" Height=\"30\" Background=\"Blue\"/>"
    L"</Grid>";

static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

struct handler { void **vtbl; LONG ref; int calls; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *sender, void *args) { h->calls++; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

static HWND popup(void)
{
    HWND hwnd = NULL;
    while ((hwnd = FindWindowExW(NULL, hwnd, L"SgXamlPopup", NULL))) if (IsWindowVisible(hwnd)) return hwnd;
    return NULL;
}

/* a click on the n-th of count items of a menu window */
static void click_item(HWND menu, int n, int count)
{
    RECT rc;
    int y;
    GetClientRect(menu, &rc);
    y = 2 + (rc.bottom - 4) * (2 * n + 1) / (2 * count);
    SendMessageW(menu, WM_MOUSEMOVE, 0, MAKELPARAM(rc.right / 2, y));
    SendMessageW(menu, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rc.right / 2, y));
    SendMessageW(menu, WM_LBUTTONUP, 0, MAKELPARAM(rc.right / 2, y));
    pump(200);
}

static const WCHAR *text_of(obj *tb, HSTRING *h)
{
    *h = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(tb, 6))(tb, h);
    return WindowsGetStringRawBuffer(*h, NULL);
}

int main(void)
{
    struct handler run = { h_vtbl, 1 }, closed = { h_vtbl, 1 };
    obj *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *box, *cbf, *flyout = NULL, *cmd = NULL, *prim = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL, menu;
    RECT rc;
    POINT origin = { 0, 0 };
    INT64 token;
    HSTRING s;
    HRESULT hr;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    box = as(find(grid, L"Box"), &IID_TextBox);

    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 100, 100, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(grid, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    ClientToScreen(island, &origin);

    /* the text box's context menu */
    SendMessageW(island, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(50, 15));
    SendMessageW(island, WM_RBUTTONUP, 0, MAKELPARAM(50, 15));
    pump(300);
    menu = popup();
    if (menu) GetWindowRect(menu, &rc);
    printf("context_menu=%d,%ld,%ld\n", menu != NULL, menu ? rc.left - origin.x : -1, menu ? rc.top - origin.y : -1);
    if (menu) click_item(menu, 1, 4);   /* Copy */
    printf("copied=%d\n", popup() == NULL);
    SendMessageW(island, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(50, 15));
    SendMessageW(island, WM_RBUTTONUP, 0, MAKELPARAM(50, 15));
    pump(300);
    if ((menu = popup())) click_item(menu, 2, 4);   /* Paste */
    printf("pasted=%ls\n", text_of(box, &s));

    /* a CommandBarFlyout of one command, shown below a target */
    cbf = factory(L"Windows.UI.Xaml.Controls.CommandBarFlyout", &IID_CBFFactory);
    ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(cbf, 6))(cbf, NULL, &inner, &flyout);
    ((load_fn)SLOT(rs, 6))(rs, hs(L"<AppBarButton " NS L" Label=\"Run\"/>"), &cmd);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(cmd, &IID_ButtonBase), 14))(as(cmd, &IID_ButtonBase), &run, &token);
    ((get_fn)SLOT(as(flyout, &IID_CBFlyout), 6))(as(flyout, &IID_CBFlyout), &prim);
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(prim, 13))(prim, cmd);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(flyout, &IID_FlyoutBase), 10))(as(flyout, &IID_FlyoutBase), &closed, &token);
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(as(flyout, &IID_FlyoutBase), 14))(as(flyout, &IID_FlyoutBase), find(grid, L"Target"));
    pump(300);
    menu = popup();
    if (menu) GetWindowRect(menu, &rc);
    printf("shown_below=%d,%ld,%ld\n", menu != NULL, menu ? rc.left - origin.x : -1, menu ? rc.top - origin.y : -1);
    if (menu) click_item(menu, 0, 1);
    printf("command=%d,%d,%d\n", run.calls, closed.calls, popup() == NULL);
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(as(flyout, &IID_FlyoutBase), 14))(as(flyout, &IID_FlyoutBase), find(grid, L"Target"));
    pump(200);
    SendMessageW(island, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(250, 180));
    SendMessageW(island, WM_LBUTTONUP, 0, MAKELPARAM(250, 180));
    pump(200);
    printf("dismissed=%d,%d\n", popup() == NULL, closed.calls);
    return 0;
}
