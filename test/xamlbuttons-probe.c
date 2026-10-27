/* Windows.UI.Xaml buttons and command bars (patches/sg/6140): a CommandBar of AppBarButtons
 * (Icon="Add" as a SymbolIcon, Label, Content as icon) and an AppBarSeparator, a Button with text
 * content, AutomationProperties and ToolTipService attached properties -- from markup, drawn in a
 * XAML Island in the desktop's look and read back. Interface identity per windows-rs 0.30.0 (MIT OR
 * Apache-2.0). */
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
DEFINE_GUID(IID_CommandBar,     0x98bc4280, 0x4a3d, 0x4cee, 0xbd, 0x07, 0x22, 0xce, 0x94, 0xc5, 0xaf, 0x76);
DEFINE_GUID(IID_AppBarButton,   0x4459a451, 0x69e8, 0x440c, 0x98, 0x96, 0x4b, 0xb4, 0xf5, 0xf6, 0x42, 0xd1);
DEFINE_GUID(IID_SymbolIcon,     0x7a4774c9, 0xa6a3, 0x4b30, 0x8f, 0xf1, 0x90, 0x81, 0xd7, 0x0e, 0x9a, 0x5c);
DEFINE_GUID(IID_DO,             0x5c526665, 0xf60e, 0x4912, 0xaf, 0x59, 0x5f, 0xe0, 0x68, 0x0f, 0x08, 0x9d);
DEFINE_GUID(IID_APStatics,      0xb618fd7b, 0x32d0, 0x4970, 0x9c, 0x42, 0x7c, 0x03, 0x9a, 0xc7, 0xbe, 0x78);
DEFINE_GUID(IID_TTSStatics,     0x86e649f8, 0xe245, 0x48aa, 0xa8, 0xc8, 0xd1, 0x07, 0x3e, 0xd7, 0x63, 0x19);
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
    L"<Grid " NS L" Background=\"White\">"
    L"<Grid.RowDefinitions><RowDefinition Height=\"Auto\"/><RowDefinition Height=\"*\"/></Grid.RowDefinitions>"
    L"<CommandBar x:Name=\"Bar\" HorizontalAlignment=\"Left\" DefaultLabelPosition=\"Collapsed\">"
    L"<AppBarButton x:Name=\"AddButton\" Width=\"48\" Icon=\"Add\" Label=\"Add\" AutomationProperties.Name=\"Add it\" ToolTipService.ToolTip=\"Adds\"/>"
    L"<AppBarSeparator/>"
    L"<AppBarButton Width=\"48\"><AppBarButton.Content><TextBlock Text=\"&#xE946;\" FontFamily=\"Segoe MDL2 Assets\"/></AppBarButton.Content></AppBarButton>"
    L"</CommandBar>"
    L"<Button x:Name=\"Btn\" Grid.Row=\"1\" Content=\"Press me\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"20,20,0,0\"/>"
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

static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

int main(void)
{
    obj *rs, *root = NULL, *bar, *add, *btn, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *cmds = NULL, *icon = NULL, *aps, *tts, *tip = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    HSTRING label = NULL, aname = NULL;
    UINT32 n = 0;
    INT32 symbol = 0;
    HRESULT hr;
    HDC hdc;
    int x, y, dark = 0, face = 0;
    double bw = 0, bh = 0;
    COLORREF control = GetSysColor(COLOR_BTNFACE), c;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    bar = find(root, L"Bar"); add = find(root, L"AddButton"); btn = find(root, L"Btn");
    ((get_fn)SLOT(as(bar, &IID_CommandBar), 6))(as(bar, &IID_CommandBar), &cmds);
    if (cmds) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(cmds, 7))(cmds, &n);
    printf("primary_commands=%u\n", n);
    ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(as(add, &IID_AppBarButton), 6))(as(add, &IID_AppBarButton), &label);
    ((get_fn)SLOT(as(add, &IID_AppBarButton), 8))(as(add, &IID_AppBarButton), &icon);
    if (as(icon, &IID_SymbolIcon)) ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(icon, &IID_SymbolIcon), 6))(as(icon, &IID_SymbolIcon), &symbol);
    printf("label=%ls icon=%x\n", label ? WindowsGetStringRawBuffer(label, NULL) : L"(none)", symbol);
    aps = factory(L"Windows.UI.Xaml.Automation.AutomationProperties", &IID_APStatics);
    ((HRESULT (WINAPI *)(obj *, obj *, HSTRING *))SLOT(aps, 31))(aps, as(add, &IID_DO), &aname);
    tts = factory(L"Windows.UI.Xaml.Controls.ToolTipService", &IID_TTSStatics);
    ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(tts, 13))(tts, as(add, &IID_DO), &tip);
    printf("automation_name=%ls tooltip=%d\n", aname ? WindowsGetStringRawBuffer(aname, NULL) : L"(none)", tip != NULL);

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
    ((getd_fn)SLOT(as(btn, &IID_FE), 13))(as(btn, &IID_FE), &bw);
    ((getd_fn)SLOT(as(btn, &IID_FE), 14))(as(btn, &IID_FE), &bh);
    printf("button_size=%d\n", bw > 40 && bh > 15);
    hdc = GetDC(island);
    /* the command bar's icons: drawn dark on the control colour */
    for (y = 8; y < 32; y++) for (x = 10; x < 38; x++) if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) dark++;
    printf("add_icon=%d\n", dark > 4);
    dark = 0;
    for (y = 8; y < 32; y++) for (x = 66; x < 104; x++) if (close_to(GetPixel(hdc, x, y), 0, 0, 0)) dark++;
    printf("content_icon=%d\n", dark > 2);
    c = GetPixel(hdc, 52, 20);
    printf("separator=%d\n", close_to(c, GetRValue(GetSysColor(COLOR_BTNSHADOW)), GetGValue(GetSysColor(COLOR_BTNSHADOW)), GetBValue(GetSysColor(COLOR_BTNSHADOW))));
    /* the button: its face in the desktop's control colour, its text on it */
    for (y = 40 + 20; y < 40 + 20 + (int)bh; y++) for (x = 20; x < 20 + (int)bw; x++)
    {
        c = GetPixel(hdc, x, y);
        if (close_to(c, GetRValue(control), GetGValue(control), GetBValue(control))) face++;
        else if (close_to(c, 0, 0, 0)) dark++;
    }
    printf("button_face=%d button_text=%d\n", face > bw * bh / 3, dark > 10);
    ReleaseDC(island, hdc);
    return 0;
}
