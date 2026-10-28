/* Windows.UI.Xaml access key scopes (patches/sg/6165): an IsAccessKeyScope element's keys only after its
 * own key, AccessKeyScopeOwner, Escape back to the top, ExitDisplayModeOnAccessKeyInvoked, KeyTipPlacementMode,
 * AccessKeyManager.IsDisplayModeEnabledChanged. Interface identity per windows-rs 0.30.0 (MIT). */
#include <wctype.h>
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
DEFINE_GUID(IID_UIE4,           0x69145cd4, 0x199a, 0x4657, 0x9e, 0x57, 0xe9, 0x9e, 0x8f, 0x13, 0x67, 0x12);
DEFINE_GUID(IID_AKArgs,         0xcfe9cd97, 0xc718, 0x4091, 0xb7, 0xdd, 0xad, 0xf1, 0xc0, 0x72, 0xb1, 0xe1);
DEFINE_GUID(IID_AKMStatics,     0x4ca0efe6, 0xd9c8, 0x4ebc, 0xb4, 0xc7, 0x30, 0xd1, 0x83, 0x8a, 0x81, 0xf1);
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
#define AT(y) L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Width=\"100\" Height=\"24\" Margin=\"0," y L",0,0\""
#define B(n, k, more) L"<Button x:Name=\"" n L"\" AccessKey=\"" k L"\" Content=\"" n L"\"" more L"/>"

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<StackPanel x:Name=\"Menu\" AccessKey=\"M\" IsAccessKeyScope=\"True\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\">"
    B(L"Open", L"O", L" Width=\"100\" Height=\"24\" Margin=\"0,0,0,16\"") B(L"Save", L"S", L" Width=\"100\" Height=\"24\" Margin=\"0,0,0,16\"")
    L"</StackPanel>"
    B(L"Other", L"O", AT(L"100"))
    B(L"Help", L"H", AT(L"150") L" KeyTipPlacementMode=\"Top\"")
    B(L"Owned", L"W", AT(L"200"))
    B(L"Stay", L"T", AT(L"250") L" ExitDisplayModeOnAccessKeyInvoked=\"False\"")
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

static obj *akm;
static int display(void)
{
    boolean on = 2;
    if (akm) ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(akm, 6))(akm, &on);
    return on;
}
static void alt_tap(HWND hwnd)
{
    SendMessageW(hwnd, WM_SYSKEYDOWN, VK_MENU, 0x20000001);
    SendMessageW(hwnd, WM_SYSKEYUP, VK_MENU, 0xc0000001);
}
static void type(HWND hwnd, WCHAR c)
{
    SendMessageW(hwnd, WM_KEYDOWN, towupper(c), 1);
    SendMessageW(hwnd, WM_CHAR, c, 1);
    SendMessageW(hwnd, WM_KEYUP, towupper(c), 0xc0000001);
}
static void escape(HWND hwnd) { SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 1); SendMessageW(hwnd, WM_KEYUP, VK_ESCAPE, 0xc0000001); }
/* dark pixels in a band of rows over x 40..60 */
static int dark_rows(HWND hwnd, int y0, int y1)
{
    HDC dc;
    int x, y, n = 0;
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    pump(100);
    dc = GetDC(hwnd);
    for (y = y0; y < y1; y++) for (x = 40; x < 60; x++)
    {
        COLORREF c = GetPixel(dc, x, y);
        n += GetRValue(c) < 100 && GetGValue(c) < 100 && GetBValue(c) < 100;
    }
    ReleaseDC(hwnd, dc);
    return n;
}

int main(void)
{
    struct handler open = { h_vtbl, 1 }, save = { h_vtbl, 1 }, other = { h_vtbl, 1 }, owned = { h_vtbl, 1 }, stay = { h_vtbl, 1 }, changed = { h_vtbl, 1 };
    obj *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    INT64 token;
    HRESULT hr;
    int a, b, c;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    akm = factory(L"Windows.UI.Xaml.Input.AccessKeyManager", &IID_AKMStatics);
    if (akm) ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(akm, 7))(akm, &changed, &token);
#define CLICK(name, h) ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(find(grid, name), &IID_ButtonBase), 14))(as(find(grid, name), &IID_ButtonBase), h, &token)
    CLICK(L"Open", &open); CLICK(L"Save", &save); CLICK(L"Other", &other); CLICK(L"Owned", &owned); CLICK(L"Stay", &stay);

    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 320, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 320, SWP_NOZORDER | SWP_SHOWWINDOW);
    SetFocus(island);
    ((putobj_fn)SLOT(src, 7))(src, as(grid, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);

    /* the top: a scope's keys are not there (O is Other's), the display mode change raised */
    alt_tap(island);
    a = display();
    type(island, 'o');
    printf("top=%d,%d,%d,%d,%d\n", a, other.calls, open.calls, display(), changed.calls);
    /* into the scope: its own keys */
    alt_tap(island);
    type(island, 'm');
    a = display();
    type(island, 's');
    printf("scope=%d,%d,%d\n", a, save.calls, display());
    /* AccessKeyScopeOwner: an element outside put in the scope */
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(as(find(grid, L"Owned"), &IID_UIE4), 13))(as(find(grid, L"Owned"), &IID_UIE4), find(grid, L"Menu"));
    alt_tap(island);
    type(island, 'w');
    a = owned.calls;
    type(island, 'm');
    type(island, 'w');
    printf("owner=%d,%d\n", a, owned.calls);
    /* Escape: out of the scope to the top, again: out of display mode */
    alt_tap(island);
    type(island, 'm');
    escape(island);
    a = display();
    type(island, 'o');
    printf("escape=%d,%d,%d\n", a, other.calls, open.calls);
    /* ExitDisplayModeOnAccessKeyInvoked False: display mode goes on */
    alt_tap(island);
    type(island, 't');
    a = display();
    escape(island);
    printf("stay=%d,%d,%d\n", stay.calls, a, display());
    /* KeyTipPlacementMode Top: Help's tip over its top edge (150), none under its bottom (174) */
    alt_tap(island);
    a = dark_rows(island, 142, 149);
    b = dark_rows(island, 176, 182);
    c = dark_rows(island, 101, 106);   /* Other's tip (Auto: bottom) is not there: it is at 124 */
    escape(island);
    printf("placement=%d,%d,%d\n", a > 10, b == 0, c == 0);
    return 0;
}
