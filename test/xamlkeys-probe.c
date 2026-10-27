/* Windows.UI.Xaml keyboard (patches/sg/6153): a TextBox clicked takes the focus (GotFocus), typed
 * characters and editing keys change its text, KeyDown routes up to its parents and a handler setting
 * Handled keeps the box from using the key, Control.Focus moves the focus (LostFocus) -- NanaZip's
 * address bar takes a path. Interface identity per windows-rs 0.30.0 (MIT). */
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
    L"<TextBox x:Name=\"Box\"" TL L" Width=\"200\" Height=\"32\" Text=\"ab\"/>"
    L"<TextBox x:Name=\"Other\"" TL L" Margin=\"0,50,0,0\" Width=\"200\" Height=\"32\"/>"
    L"</Grid>";

static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

/* a handler: counts, keeps the last key, sets Handled for one key */
struct handler { void **vtbl; LONG ref; int calls, last_key, handle_key; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *sender, void *args)
{
    obj *key = as(args, &IID_KeyArgs);
    h->calls++;
    if (key)
    {
        ((HRESULT (WINAPI *)(obj *, int *))SLOT(key, 6))(key, &h->last_key);
        if (h->last_key == h->handle_key) ((HRESULT (WINAPI *)(obj *, boolean))SLOT(key, 9))(key, TRUE);
    }
    return S_OK;
}
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

static const WCHAR *text_of(obj *tb, HSTRING *h)
{
    *h = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING *))SLOT(tb, 6))(tb, h);
    return WindowsGetStringRawBuffer(*h, NULL);
}

static void key(HWND hwnd, UINT vk) { SendMessageW(hwnd, WM_KEYDOWN, vk, 1); SendMessageW(hwnd, WM_KEYUP, vk, 0xc0000001); }

int main(void)
{
    struct handler got = { h_vtbl, 1 }, lost = { h_vtbl, 1 }, grid_keys = { h_vtbl, 1 }, box_keys = { h_vtbl, 1, 0, 0, VK_HOME };
    obj *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *box, *other;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    INT64 token;
    HSTRING s;
    HRESULT hr;
    boolean focused = 0;
    int state = -1;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;
    box = find(grid, L"Box");
    other = find(grid, L"Other");
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(box, &IID_UIE), 45))(as(box, &IID_UIE), &got, &token);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(box, &IID_UIE), 47))(as(box, &IID_UIE), &lost, &token);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(grid, &IID_UIE), 43))(as(grid, &IID_UIE), &grid_keys, &token);
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(box, &IID_UIE), 43))(as(box, &IID_UIE), &box_keys, &token);

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

    /* a click on the box */
    SendMessageW(island, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(50, 15));
    SendMessageW(island, WM_LBUTTONUP, 0, MAKELPARAM(50, 15));
    ((HRESULT (WINAPI *)(obj *, int *))SLOT(as(box, &IID_Control), 42))(as(box, &IID_Control), &state);
    printf("focus=%d,%d\n", got.calls, state != 0);

    SendMessageW(island, WM_CHAR, 'c', 1);
    printf("typed=%ls\n", text_of(as(box, &IID_TextBox), &s));
    key(island, VK_LEFT);
    SendMessageW(island, WM_CHAR, 'X', 1);
    printf("caret=%ls\n", text_of(as(box, &IID_TextBox), &s));
    key(island, VK_BACK);
    printf("back=%ls\n", text_of(as(box, &IID_TextBox), &s));
    printf("routed=%d,%d\n", grid_keys.calls > 0, grid_keys.last_key);
    /* a KeyDown handler that handles Home: the box does not move its caret */
    key(island, VK_HOME);
    SendMessageW(island, WM_CHAR, 'Z', 1);
    printf("handled=%ls\n", text_of(as(box, &IID_TextBox), &s));
    key(island, VK_END);
    key(island, VK_DELETE);
    key(island, VK_LEFT);
    key(island, VK_DELETE);
    printf("delete=%ls\n", text_of(as(box, &IID_TextBox), &s));

    /* PreviewKeyDown tunnels from the root first: the Grid handles Back, the box keeps its text */
    {
        struct handler preview = { h_vtbl, 1, 0, 0, VK_BACK };
        int before = box_keys.calls;
        ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(grid, &IID_UIE7), 11))(as(grid, &IID_UIE7), &preview, &token);
        key(island, VK_BACK);
        printf("preview=%d,%d,%ls\n", preview.calls > 0, box_keys.calls - before, text_of(as(box, &IID_TextBox), &s));
    }

    /* Control.Focus moves the focus */
    hr = ((HRESULT (WINAPI *)(obj *, int, boolean *))SLOT(as(other, &IID_Control), 46))(as(other, &IID_Control), 3, &focused);
    SendMessageW(island, WM_CHAR, 'o', 1);
    printf("moved=%08lx,%d,%d,%ls\n", hr, focused, lost.calls, text_of(as(other, &IID_TextBox), &s));
    return 0;
}
