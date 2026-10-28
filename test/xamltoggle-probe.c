/* Windows.UI.Xaml toggles and split buttons (patches/sg/6173): CheckBox (two and three states, its box
 * drawn filled when checked, Checked/Unchecked raised), RadioButton (one of its parent's, or of its
 * GroupName anywhere), ToggleButton, a SplitButton's primary part clicking and its chevron opening its
 * MenuFlyout (MenuFlyoutItem Click; a separator not an entry) -- Advanced Installer's XAML dialogs
 * (TeraCopy's installer). Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_ToggleButton,   0x589877fb, 0x0fc7, 0x4036, 0x9d, 0x8b, 0x12, 0x7d, 0xfa, 0x75, 0xc1, 0x6d);
DEFINE_GUID(IID_RefBool,        0x3c00fd60, 0x2950, 0x5939, 0xa2, 0x1a, 0x2d, 0x12, 0xc5, 0xa0, 0x1b, 0x8a);
DEFINE_GUID(IID_SplitButton,    0x58bbb066, 0xc2ea, 0x5499, 0x81, 0x50, 0x40, 0xfa, 0xa7, 0x5f, 0x6b, 0xb5);
DEFINE_GUID(IID_MenuFlyout,     0xad3210f1, 0x985b, 0x4171, 0xbb, 0xba, 0x67, 0xbc, 0xc7, 0x29, 0x64, 0x9b);
DEFINE_GUID(IID_MenuFlyoutItem, 0x160775cf, 0xb598, 0x44af, 0xa4, 0x88, 0xc3, 0xce, 0x2a, 0xf6, 0xd3, 0xf0);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);
typedef HRESULT (WINAPI *add_fn)(obj *, void *, INT64 *);

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

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<StackPanel " NS L" Background=\"White\">"
    L"<CheckBox x:Name=\"C1\" Height=\"32\" Content=\"One\"/>"
    L"<CheckBox x:Name=\"C3\" Height=\"32\" IsThreeState=\"True\" Content=\"Three\"/>"
    L"<StackPanel><RadioButton x:Name=\"R1\" Height=\"32\" IsChecked=\"True\" Content=\"A\"/>"
    L"<RadioButton x:Name=\"R2\" Height=\"32\" Content=\"B\"/></StackPanel>"
    L"<Border><RadioButton x:Name=\"R3\" Height=\"32\" GroupName=\"G\" Content=\"G1\"/></Border>"
    L"<Border><RadioButton x:Name=\"R4\" Height=\"32\" GroupName=\"G\" Content=\"G2\"/></Border>"
    L"<ToggleButton x:Name=\"T\" Height=\"30\" Width=\"60\" HorizontalAlignment=\"Left\" Content=\"T\"/>"
    L"<SplitButton x:Name=\"S\" Height=\"30\" Width=\"120\" HorizontalAlignment=\"Left\" Content=\"Go\">"
    L"<SplitButton.Flyout><MenuFlyout><MenuFlyoutItem Text=\"Alpha\"/><MenuFlyoutSeparator/>"
    L"<MenuFlyoutItem Text=\"Beta\"/></MenuFlyout></SplitButton.Flyout></SplitButton>"
    L"</StackPanel>";

static HWND island;

static void click(int x, int y)
{
    SendMessageW(island, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
    SendMessageW(island, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessageW(island, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    pump(150);
}

/* IsChecked: 1, 0, or -1 (null: indeterminate) */
static int checked(obj *root, const WCHAR *name)
{
    obj *t = as(find(root, name), &IID_ToggleButton), *ref = NULL, *b;
    boolean v = FALSE;
    if (!t) return -9;
    ((get_fn)SLOT(t, 6))(t, &ref);
    if (!ref) return -1;
    if (!(b = as(ref, &IID_RefBool))) return -8;
    ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(b, 6))(b, &v);
    return v ? 1 : 0;
}

static int white_at(int x, int y)
{
    HDC hdc = GetDC(island);
    COLORREF c;
    UpdateWindow(island);
    c = GetPixel(hdc, x, y);
    ReleaseDC(island, hdc);
    return GetRValue(c) > 230 && GetGValue(c) > 230 && GetBValue(c) > 230;
}

static HWND popup(void)
{
    HWND hwnd = NULL;
    while ((hwnd = FindWindowExW(NULL, hwnd, L"SgXamlPopup", NULL))) if (IsWindowVisible(hwnd)) return hwnd;
    return NULL;
}

int main(void)
{
    struct handler on = { h_vtbl, 1 }, off = { h_vtbl, 1 }, split = { h_vtbl, 1 }, beta = { h_vtbl, 1 }, alpha = { h_vtbl, 1 };
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *c1, *s, *flyout = NULL, *items = NULL, *item = NULL;
    IInspectable *inner = NULL;
    HWND host, menu;
    RECT rc;
    INT64 token;
    HRESULT hr;
    int a, b, c, d;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 100, 100, 300, 300, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 300, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);

    /* a check box: checked and unchecked by clicks, its events raised, its box filled when checked */
    c1 = as(find(root, L"C1"), &IID_ToggleButton);
    ((add_fn)SLOT(c1, 10))(c1, &on, &token);
    ((add_fn)SLOT(c1, 12))(c1, &off, &token);
    a = checked(root, L"C1"); b = white_at(4, 10);
    click(10, 16);
    c = checked(root, L"C1"); d = white_at(4, 10);
    printf("check=%d,%d,%d,%d\n", a, c, b, !d);
    click(10, 16);
    printf("uncheck=%d,%d,%d,%d\n", checked(root, L"C1"), on.calls, off.calls, white_at(4, 10));
    /* three states: checked, indeterminate, unchecked */
    click(10, 48); a = checked(root, L"C3");
    click(10, 48); b = checked(root, L"C3");
    click(10, 48); c = checked(root, L"C3");
    printf("three=%d,%d,%d\n", a, b, c);
    /* radio buttons: one of a parent's; a checked one stays checked when clicked */
    a = checked(root, L"R1");
    click(10, 112);
    printf("radio=%d,%d,%d\n", a, checked(root, L"R1"), checked(root, L"R2"));
    click(10, 112);
    printf("radio_again=%d\n", checked(root, L"R2"));
    /* a GroupName's across parents, other groups untouched */
    click(10, 144); a = checked(root, L"R3");
    click(10, 176);
    printf("group=%d,%d,%d,%d\n", a, checked(root, L"R3"), checked(root, L"R4"), checked(root, L"R2"));
    /* a toggle button */
    click(30, 207); a = checked(root, L"T");
    click(30, 207);
    printf("toggle=%d,%d\n", a, checked(root, L"T"));

    /* a split button: its primary part clicks, its chevron opens its menu */
    s = as(find(root, L"S"), &IID_SplitButton);
    ((add_fn)SLOT(s, 12))(s, &split, &token);
    ((get_fn)SLOT(s, 6))(s, &flyout);
    if (flyout && as(flyout, &IID_MenuFlyout)) ((get_fn)SLOT(as(flyout, &IID_MenuFlyout), 6))(as(flyout, &IID_MenuFlyout), &items);
    if (items) ((HRESULT (WINAPI *)(obj *, UINT32, obj **))SLOT(items, 6))(items, 2, &item);
    if (item && as(item, &IID_MenuFlyoutItem)) ((add_fn)SLOT(as(item, &IID_MenuFlyoutItem), 12))(as(item, &IID_MenuFlyoutItem), &beta, &token);
    item = NULL;
    if (items) ((HRESULT (WINAPI *)(obj *, UINT32, obj **))SLOT(items, 6))(items, 0, &item);
    if (item && as(item, &IID_MenuFlyoutItem)) ((add_fn)SLOT(as(item, &IID_MenuFlyoutItem), 12))(as(item, &IID_MenuFlyoutItem), &alpha, &token);
    click(30, 237);
    printf("primary=%d,%d\n", split.calls, popup() != NULL);
    click(110, 237);
    menu = popup();
    rc.bottom = 0;
    if (menu) GetClientRect(menu, &rc);
    printf("chevron=%d,%d,%d\n", split.calls, menu != NULL, rc.bottom > 30 && rc.bottom < 90);
    if (menu)
    {
        int y = 2 + (rc.bottom - 4) * 3 / 4;   /* the second of two entries */
        SendMessageW(menu, WM_MOUSEMOVE, 0, MAKELPARAM(rc.right / 2, y));
        SendMessageW(menu, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rc.right / 2, y));
        SendMessageW(menu, WM_LBUTTONUP, 0, MAKELPARAM(rc.right / 2, y));
        pump(200);
    }
    printf("item=%d,%d,%d\n", beta.calls, alpha.calls, popup() == NULL);
    return 0;
}
