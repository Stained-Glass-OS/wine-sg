/* xamlpointer-probe: Windows.UI.Xaml pointer routed events (patches/sg/6213).
 *
 * No pointer event was raised: React Native for Windows takes the pointer from the root view's
 * PointerPressed/Moved/Released (AddHandler, handledEventsToo) and OneDrive's Sign in button did
 * nothing. Mouse messages to an island raise them on the element under the pointer, bubbling up
 * while not handled; GetCurrentPoint is relative to the element asked; capture holds them.
 *
 * Prints name=value lines; see test/xamlpointer-gate.sh.
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

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_UIEStatics,     0x58d3573b, 0xf52c, 0x45be, 0x98, 0x8b, 0xa5, 0x86, 0x95, 0x64, 0x87, 0x3c);
DEFINE_GUID(IID_PointerArgs,    0xda628f0a, 0x9752, 0x49e2, 0xbd, 0xe2, 0x49, 0xec, 0xca, 0xb9, 0x19, 0x4d);
DEFINE_GUID(IID_Pointer,        0x5ee8f39f, 0x747d, 0x4171, 0x90, 0xe6, 0xcd, 0x37, 0xa9, 0xdf, 0xfb, 0x11);
DEFINE_GUID(IID_TextBox,        0xe48f5a8b, 0x1dff, 0x4352, 0xa1, 0xf4, 0xe5, 0x16, 0x51, 0x4e, 0xc8, 0x82);
DEFINE_GUID(IID_TextBox3,       0x7df18c3c, 0x2826, 0x425e, 0x9a, 0x94, 0x00, 0xb7, 0xf7, 0x3f, 0x37, 0x56);
DEFINE_GUID(IID_Control,        0xa8912263, 0x2951, 0x4f58, 0xa9, 0xc5, 0x5a, 0x13, 0x4e, 0xaa, 0x7f, 0x07);
DEFINE_GUID(IID_ChangingArgs2,  0xc0723d44, 0x53a6, 0x415f, 0xa9, 0x81, 0x50, 0xdf, 0xaf, 0x1b, 0xec, 0xbe);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef ULONG (WINAPI *rel_fn)(obj *);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);

struct wpoint { float x, y; };

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }
static void release(obj *o) { if (o) ((rel_fn)SLOT(o, 2))(o); }
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

/* a PointerEventHandler: what it saw */
struct handler
{
    void **vtbl;
    LONG ref;
    const char *name;
    int calls;
    obj *relative;              /* GetCurrentPoint relative to this */
    struct wpoint at;
    boolean left;
    UINT32 kind, id, type;
    BOOL set_handled;
    char order[64];
};
static char order[256];
/* a delegate answers IUnknown and its own interface, not IInspectable */
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IInspectable)) { *out = NULL; return E_NOINTERFACE; }
    *out = h; h->ref++; return S_OK;
}
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, obj *sender, obj *args)
{
    obj *pa = as(args, &IID_PointerArgs), *point = NULL, *props = NULL, *pointer = NULL;

    h->calls++;
    strcat(order, h->name);
    if (!pa) return S_OK;
    if (CALL(pa, 10, HRESULT (WINAPI *)(obj *, obj *, obj **))(pa, h->relative, &point) == S_OK && point)   /* GetCurrentPoint */
    {
        CALL(point, 7, HRESULT (WINAPI *)(obj *, struct wpoint *))(point, &h->at);                  /* Position */
        CALL(point, 13, get_fn)(point, &props);                                                    /* Properties */
        if (props)
        {
            CALL(props, 16, HRESULT (WINAPI *)(obj *, boolean *))(props, &h->left);                  /* IsLeftButtonPressed */
            CALL(props, 27, HRESULT (WINAPI *)(obj *, UINT32 *))(props, &h->kind);                   /* PointerUpdateKind */
            release(props);
        }
        release(point);
    }
    CALL(pa, 6, get_fn)(pa, &pointer);
    if (pointer)
    {
        CALL(pointer, 6, HRESULT (WINAPI *)(obj *, UINT32 *))(pointer, &h->id);
        CALL(pointer, 7, HRESULT (WINAPI *)(obj *, UINT32 *))(pointer, &h->type);
        release(pointer);
    }
    if (h->set_handled) CALL(pa, 9, HRESULT (WINAPI *)(obj *, boolean))(pa, TRUE);
    release(pa);
    return S_OK;
}
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

/* the text box's events: their order, and TextChanging's IsContentChanging */
static char text_order[32];
static boolean content_changing;
static HRESULT WINAPI t_invoke(struct handler *h, obj *sender, obj *args)
{
    obj *a2 = as(args, &IID_ChangingArgs2);
    strcat(text_order, h->name);
    if (a2) { CALL(a2, 6, HRESULT (WINAPI *)(obj *, boolean *))(a2, &content_changing); release(a2); }
    return S_OK;
}
static void *t_vtbl[] = { h_qi, h_addref, h_release, t_invoke };

/* a delegate boxed as C++/WinRT's box_value makes it: an IInspectable (an IReference<T>) whose Value is it */
struct boxed { void **vtbl; LONG ref; struct handler *delegate; };
static HRESULT WINAPI b_qi(struct boxed *b, REFIID iid, void **out)
{
    static const GUID IID_IPropertyValue_p = { 0x4bd682dd, 0x7554, 0x40e9, { 0x9a, 0x9b, 0x82, 0x65, 0x4e, 0xde, 0x7e, 0x62 } };
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IPropertyValue_p))
    { *out = b; b->ref++; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI b_addref(struct boxed *b) { return ++b->ref; }
static ULONG WINAPI b_release(struct boxed *b) { return --b->ref; }
static HRESULT WINAPI b_getiids(struct boxed *b, ULONG *count, IID **iids) { *count = 2; *iids = NULL; return S_OK; }
static HRESULT WINAPI b_notimpl(struct boxed *b) { return E_NOTIMPL; }
static HRESULT WINAPI b_get_Value(struct boxed *b, void **value) { *value = b->delegate; b->delegate->ref++; return S_OK; }
static void *b_vtbl[] = { b_qi, b_addref, b_release, b_getiids, b_notimpl, b_notimpl, b_get_Value };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
static const WCHAR page_xaml[] =
    L"<Grid " NS L" x:Name=\"G\" Background=\"White\">"
    L"<Border x:Name=\"B\" Width=\"100\" Height=\"60\" Margin=\"20,10,0,0\" Background=\"Red\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\">"
    L"<Border x:Name=\"C\" Width=\"40\" Height=\"20\" Background=\"Blue\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"</Border>"
    L"<TextBox x:Name=\"T\" Width=\"120\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"0,100,0,0\"/>"
    L"</Grid>";

int main(void)
{
    struct handler pressed = { h_vtbl, 1, "B" }, root_pressed = { h_vtbl, 1, "G" }, inner = { h_vtbl, 1, "C" };
    struct handler moved = { h_vtbl, 1, "m" }, released = { h_vtbl, 1, "r" }, entered = { h_vtbl, 1, "e" };
    struct handler boxed_released = { h_vtbl, 1, "x" };
    struct boxed box = { b_vtbl, 1, &boxed_released };
    obj *ev_released = NULL;
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *g, *b, *c, *uis, *ev_pressed = NULL;
    IInspectable *inner_obj = NULL;
    INT64 token = 0;
    HWND host, island = NULL;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    g = find(root, L"G");
    b = find(root, L"B");
    c = find(root, L"C");
    pressed.relative = b;
    inner.relative = c;
    moved.relative = released.relative = b;

    /* the Border's own PointerPressed/Moved/Released/Entered; the root's with AddHandler(handledEventsToo) */
    CALL(as(b, &IID_UIE), 57, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(b, &IID_UIE), &pressed, &token);
    CALL(as(b, &IID_UIE), 59, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(b, &IID_UIE), &moved, &token);
    CALL(as(b, &IID_UIE), 61, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(b, &IID_UIE), &released, &token);
    CALL(as(b, &IID_UIE), 63, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(b, &IID_UIE), &entered, &token);
    uis = factory(L"Windows.UI.Xaml.UIElement", &IID_UIEStatics);
    CALL(uis, 9, get_fn)(uis, &ev_pressed);                                       /* PointerPressedEvent */
    hr = CALL(as(g, &IID_UIE), 96, HRESULT (WINAPI *)(obj *, obj *, void *, boolean))(as(g, &IID_UIE), ev_pressed, &root_pressed, TRUE);
    printf("add_handler=%08lx\n", hr);
    /* a boxed delegate: its Value is the handler called */
    CALL(uis, 11, get_fn)(uis, &ev_released);                                     /* PointerReleasedEvent */
    boxed_released.relative = b;
    hr = CALL(as(g, &IID_UIE), 96, HRESULT (WINAPI *)(obj *, obj *, void *, boolean))(as(g, &IID_UIE), ev_released, &box, TRUE);
    printf("add_boxed=%08lx\n", hr);

    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner_obj, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(600);

    /* over the red border (not the blue one): 70,50 is 50,40 in the border */
    SendMessageW(island, WM_MOUSEMOVE, 0, MAKELPARAM(70, 50));
    printf("moved=%d entered=%d at=%.0f,%.0f\n", moved.calls, entered.calls, moved.at.x, moved.at.y);
    SendMessageW(island, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(70, 50));
    printf("pressed=%d at=%.0f,%.0f left=%d kind=%u id=%u type=%u root=%d\n", pressed.calls, pressed.at.x, pressed.at.y,
           pressed.left, pressed.kind, pressed.id, pressed.type, root_pressed.calls);
    SendMessageW(island, WM_LBUTTONUP, 0, MAKELPARAM(70, 50));
    printf("released=%d kind=%u left=%d\n", released.calls, released.kind, released.left);
    printf("boxed=%d at=%.0f,%.0f\n", boxed_released.calls, boxed_released.at.x, boxed_released.at.y);

    /* over the blue border, which handles it: the red one does not see it, the root (handledEventsToo) does */
    CALL(as(c, &IID_UIE), 57, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(c, &IID_UIE), &inner, &token);
    inner.set_handled = TRUE;
    order[0] = 0;
    SendMessageW(island, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 15));
    SendMessageW(island, WM_LBUTTONUP, 0, MAKELPARAM(30, 15));
    printf("handled=%s inner_at=%.0f,%.0f\n", order, inner.at.x, inner.at.y);

    /* the text box: TextChanging before TextChanged, then SelectionChanged; Select tells too */
    {
        struct handler changing = { t_vtbl, 1, "1" }, changed = { t_vtbl, 1, "2" }, selection = { t_vtbl, 1, "3" };
        obj *t = find(root, L"T"), *tb = as(t, &IID_TextBox);
        boolean focused = FALSE;

        CALL(as(t, &IID_TextBox3), 18, HRESULT (WINAPI *)(obj *, void *, INT64 *))(as(t, &IID_TextBox3), &changing, &token);
        CALL(tb, 30, HRESULT (WINAPI *)(obj *, void *, INT64 *))(tb, &changed, &token);      /* TextChanged */
        CALL(tb, 32, HRESULT (WINAPI *)(obj *, void *, INT64 *))(tb, &selection, &token);    /* SelectionChanged */
        CALL(as(t, &IID_Control), 46, HRESULT (WINAPI *)(obj *, INT32, boolean *))(as(t, &IID_Control), 3, &focused);
        SetFocus(island);
        SendMessageW(island, WM_CHAR, 'a', 0);
        printf("typed=%s,%d,%d\n", text_order, content_changing, focused);
        text_order[0] = 0;
        CALL(tb, 36, HRESULT (WINAPI *)(obj *, INT32, INT32))(tb, 0, 1);                     /* Select */
        printf("selected=%s\n", text_order);
    }
    printf("done=1\n");
    return 0;
}
