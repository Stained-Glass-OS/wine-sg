/* Windows.UI.Xaml ComboBox, ToggleSwitch, InputScope, CompositionTarget.Rendering and
 * LayoutInformation (patches/sg/6198).
 *
 *  - a ComboBox from markup shows nothing selected; a click drops its list down in a popup window,
 *    a click on an item there selects it (SelectionChanged, SelectionBoxItem) and closes the list;
 *    Up on the focused box selects the item above;
 *  - a ToggleSwitch flips on a click, raises Toggled and draws its track filled when on;
 *  - InputScopeName keeps its NameValue, InputScope its Names;
 *  - CompositionTarget.Rendering is raised every frame with a rendering time;
 *  - LayoutInformation.GetLayoutSlot gives where the parent placed an element.
 *
 * Prints name=value lines; see test/xamlcombo-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
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
DEFINE_GUID(IID_Selector,       0xe30eb3a5, 0xb36b, 0x42dc, 0x85, 0x27, 0xcd, 0x25, 0x13, 0x6c, 0x08, 0x3c);
DEFINE_GUID(IID_ComboBox,       0xb9a8d05c, 0xac97, 0x47f1, 0xa5, 0xf4, 0x3f, 0x9f, 0x4d, 0x4b, 0x11, 0x6c);
DEFINE_GUID(IID_ToggleSwitch,   0x331d8f00, 0xc5f9, 0x46a5, 0xb6, 0xc8, 0xed, 0xe5, 0x39, 0x30, 0x45, 0x67);
DEFINE_GUID(IID_ScopeName,      0xfd3e6997, 0x08fb, 0x4cba, 0xa0, 0x21, 0x79, 0x2d, 0x75, 0x89, 0xfd, 0x5a);
DEFINE_GUID(IID_ScopeNameFactory, 0x4a40bb52, 0x4bd7, 0x4e54, 0x86, 0x17, 0x1c, 0xda, 0x8a, 0x1e, 0xda, 0x7f);
DEFINE_GUID(IID_Scope,          0x5c0f85f3, 0xf9d8, 0x4220, 0xb6, 0x66, 0x04, 0x5d, 0x07, 0x4d, 0x9b, 0xfa);
DEFINE_GUID(IID_CTStatics,      0x2b1af03d, 0x1ed2, 0x4b59, 0xbd, 0x00, 0x75, 0x94, 0xee, 0x92, 0x83, 0x2b);
DEFINE_GUID(IID_RenderingArgs,  0x5bf7d30d, 0x9748, 0x4aed, 0x83, 0x80, 0xd7, 0x89, 0x0e, 0xb7, 0x76, 0xa0);
DEFINE_GUID(IID_LayoutStatics,  0xcf06cf99, 0x58e9, 0x4682, 0x83, 0x26, 0x50, 0xca, 0xab, 0x65, 0xed, 0x7c);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);

struct wrect { float x, y, w, h; };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static IUnknown *ident(void *o) { IUnknown *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, &IID_IUnknown, (void **)&r); return r; }
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
    if (root) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}
static void click(HWND hwnd, int x, int y)
{
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    pump(150);
}

struct handler { void *vtbl; LONG ref; int calls; INT64 time; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; InterlockedIncrement(&h->ref); return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return InterlockedIncrement(&h->ref); }
static ULONG WINAPI h_release(struct handler *h) { return InterlockedDecrement(&h->ref); }
static HRESULT WINAPI h_invoke(struct handler *h, IInspectable *sender, IInspectable *args)
{
    obj *ra = as(args, &IID_RenderingArgs);
    h->calls++;
    if (ra) ((HRESULT (WINAPI *)(obj *, INT64 *))SLOT(ra, 6))(ra, &h->time);
    return S_OK;
}
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<ComboBox x:Name=\"Cb\" Width=\"150\" Height=\"32\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" PlaceholderText=\"Pick\">"
    L"<ComboBoxItem Content=\"One\"/><ComboBoxItem Content=\"Two\"/></ComboBox>"
    L"<ToggleSwitch x:Name=\"Ts\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"0,60,0,0\"/>"
    L"</Grid>";

static INT32 selected(obj *cb) { INT32 i = -9; ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(cb, &IID_Selector), 6))(as(cb, &IID_Selector), &i); return i; }
static boolean dropped(obj *cb) { boolean b = 2; ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(as(cb, &IID_ComboBox), 6))(as(cb, &IID_ComboBox), &b); return b; }

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *cb, *ts, *box_item = NULL, *item1 = NULL;
    struct handler sel = { h_vtbl, 1 }, tog = { h_vtbl, 1 }, frames = { h_vtbl, 1 };
    IInspectable *inner = NULL;
    HWND host, island = NULL, popup;
    INT64 token = 0;
    HRESULT hr;
    HDC hdc;
    COLORREF off, on;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    cb = find(root, L"Cb");
    ts = find(root, L"Ts");
    printf("combo=%d toggle=%d\n", as(cb, &IID_ComboBox) != NULL, as(ts, &IID_ToggleSwitch) != NULL);
    if (!as(cb, &IID_ComboBox) || !as(ts, &IID_ToggleSwitch)) return 1;
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(cb, &IID_Selector), 16))(as(cb, &IID_Selector), &sel, &token);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(ts, &IID_ToggleSwitch), 21))(as(ts, &IID_ToggleSwitch), &tog, &token);

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

    /* the combo box: nothing selected, then its list down, an item chosen */
    printf("initial=%d,%d\n", selected(cb), dropped(cb));
    click(island, 75, 16);
    popup = FindWindowW(L"SgXamlPopup", NULL);
    printf("dropped=%d popup=%d\n", dropped(cb), popup && IsWindowVisible(popup));
    if (popup)
    {
        RECT rc;
        GetClientRect(popup, &rc);
        click(popup, 20, rc.bottom * 3 / 4);   /* the second of two items */
    }
    ((get_fn)SLOT(as(cb, &IID_ComboBox), 12))(as(cb, &IID_ComboBox), &box_item);
    printf("chosen=%d,%d changed=%d\n", selected(cb), dropped(cb), sel.calls);
    (void)item1;
    printf("box_item=%d\n", box_item != NULL);
    /* Up on the focused box: the item above */
    SendMessageW(island, WM_KEYDOWN, VK_UP, 1);
    SendMessageW(island, WM_KEYUP, VK_UP, 0xc0000001);
    pump(100);
    printf("key_up=%d\n", selected(cb));

    /* the toggle switch */
    hdc = GetDC(island);
    off = GetPixel(hdc, 20, 76);
    click(island, 20, 76);
    pump(200);
    on = GetPixel(hdc, 20, 76);
    ReleaseDC(island, hdc);
    {
        boolean is_on = 2;
        ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(as(ts, &IID_ToggleSwitch), 6))(as(ts, &IID_ToggleSwitch), &is_on);
        printf("toggle=%d toggled=%d drawn=%d\n", is_on, tog.calls, off != on);
    }

    /* InputScope and InputScopeName */
    {
        obj *nf = factory(L"Windows.UI.Xaml.Input.InputScopeName", &IID_ScopeNameFactory), *name = NULL, *names = NULL;
        IInspectable *scope = NULL;
        INT32 value = -1;
        UINT32 n = 0;
        if (nf) ((HRESULT (WINAPI *)(obj *, INT32, obj **))SLOT(nf, 6))(nf, 29, &name);
        if (name) ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(name, &IID_ScopeName), 6))(as(name, &IID_ScopeName), &value);
        RoActivateInstance(hs(L"Windows.UI.Xaml.Input.InputScope"), &scope);
        if (as(scope, &IID_Scope)) ((get_fn)SLOT(as(scope, &IID_Scope), 6))(as(scope, &IID_Scope), &names);
        if (names && name)
        {
            ((HRESULT (WINAPI *)(obj *, obj *))SLOT(names, 13))(names, name);
            ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(names, 7))(names, &n);
        }
        printf("input_scope=%d,%u\n", value, n);
    }

    /* CompositionTarget.Rendering */
    {
        obj *ct = factory(L"Windows.UI.Xaml.Media.CompositionTarget", &IID_CTStatics);
        if (ct) ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(ct, 6))(ct, &frames, &token);
        pump(500);
        if (ct) ((HRESULT (WINAPI *)(obj *, INT64))SLOT(ct, 7))(ct, token);
        printf("rendering=%d time=%d\n", frames.calls >= 5, frames.time > 0);
    }

    /* LayoutInformation */
    {
        obj *li = factory(L"Windows.UI.Xaml.Controls.Primitives.LayoutInformation", &IID_LayoutStatics);
        struct wrect slot = { -1, -1, -1, -1 };
        if (li) ((HRESULT (WINAPI *)(obj *, obj *, struct wrect *))SLOT(li, 7))(li, as(ts, &IID_FE), &slot);
        printf("layout_slot=%.0f,%.0f,%.0f,%.0f\n", slot.x, slot.y, slot.w, slot.h);
    }
    (void)ident;
    return 0;
}
