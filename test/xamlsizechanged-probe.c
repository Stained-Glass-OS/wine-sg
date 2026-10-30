/* Windows.UI.Xaml SizeChanged and combo box selection follow-ups (patches/sg/6200):
 *
 *  - FrameworkElement.SizeChanged is raised after a layout pass in which an element's RenderSize
 *    changed, with its previous and new size (the first time from 0x0);
 *  - a ComboBox reads an ItemsSource that is only an IBindableVector (as C++/WinRT's typed
 *    vectors are to XAML);
 *  - a SelectedItem given before the items keeps waiting for them; a boxed string matches an item
 *    of the same text.
 *
 * Prints name=value lines; see test/xamlsizechanged-gate.sh.
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
DEFINE_GUID(IID_Selector,       0xe30eb3a5, 0xb36b, 0x42dc, 0x85, 0x27, 0xcd, 0x25, 0x13, 0x6c, 0x08, 0x3c);
DEFINE_GUID(IID_ItemsControl,   0xf4a91dd8, 0xd979, 0x4381, 0x86, 0x52, 0xbd, 0xa0, 0x34, 0x2a, 0x76, 0x5e);
DEFINE_GUID(IID_SizeArgs,       0xd5312e60, 0x5cc1, 0x42a1, 0x92, 0x0c, 0x1a, 0xf4, 0x6b, 0xe2, 0xf9, 0x86);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);
DEFINE_GUID(IID_BindableVector, 0x393de7de, 0x6fd0, 0x4c0d, 0xbb, 0x71, 0x47, 0x24, 0x4a, 0x11, 0x3e, 0x93);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);

struct wsize { float w, h; };

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
    if (root) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

/* a SizeChanged handler: calls, the last sizes */
struct handler { void *vtbl; LONG ref; int calls; struct wsize prev, now; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_ref(struct handler *h) { return 2; }
static HRESULT WINAPI h_invoke(struct handler *h, IInspectable *sender, IInspectable *args)
{
    obj *sa = as(args, &IID_SizeArgs);
    h->calls++;
    if (sa)
    {
        ((HRESULT (WINAPI *)(obj *, struct wsize *))SLOT(sa, 6))(sa, &h->prev);
        ((HRESULT (WINAPI *)(obj *, struct wsize *))SLOT(sa, 7))(sa, &h->now);
    }
    return S_OK;
}
static void *h_vtbl[] = { h_qi, h_ref, h_ref, h_invoke };

/* an IBindableVector of three boxed strings, nothing else */
static IInspectable *strings[3];
static HRESULT WINAPI bv_qi(void *self, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_BindableVector)) { *out = self; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI bv_ref(void *self) { return 2; }
static HRESULT WINAPI bv_notimpl(void *self) { return E_NOTIMPL; }
static HRESULT WINAPI bv_trust(void *self, TrustLevel *l) { *l = BaseTrust; return S_OK; }
static HRESULT WINAPI bv_getat(void *self, UINT32 i, IInspectable **out)
{
    if (i >= 3) return 0x8000000b;
    IInspectable_AddRef((*out = strings[i]));
    return S_OK;
}
static HRESULT WINAPI bv_size(void *self, UINT32 *n) { *n = 3; return S_OK; }
static void *bv_vtbl[] = { bv_qi, bv_ref, bv_ref, bv_notimpl, bv_notimpl, bv_trust, bv_getat, bv_size,
                           bv_notimpl, bv_notimpl, bv_notimpl, bv_notimpl, bv_notimpl, bv_notimpl, bv_notimpl, bv_notimpl };
static struct { void **vtbl; } bindable = { bv_vtbl };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<Border x:Name=\"B\" Width=\"100\" Height=\"50\" Background=\"Red\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"<ComboBox x:Name=\"Cb\" Width=\"120\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"0,60,0,0\"/>"
    L"</Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *b, *cb, *pvs;
    struct handler sh = { h_vtbl, 1 };
    IInspectable *inner = NULL, *wanted = NULL;
    HWND host, island = NULL;
    INT64 token = 0;
    INT32 index = -9;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    b = find(root, L"B");
    cb = find(root, L"Cb");
    ((HRESULT (WINAPI *)(obj *, void *, INT64 *))SLOT(as(b, &IID_FE), 47))(as(b, &IID_FE), &sh, &token);

    /* the combo box: its SelectedItem first (a boxed "b" of its own), then its items */
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    if (pvs)
    {
        static const WCHAR *texts[3] = { L"a", L"b", L"c" };
        int i;
        for (i = 0; i < 3; i++) ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(texts[i]), &strings[i]);
        ((HRESULT (WINAPI *)(obj *, HSTRING, IInspectable **))SLOT(pvs, 18))(pvs, hs(L"b"), &wanted);
    }
    hr = ((HRESULT (WINAPI *)(obj *, IInspectable *))SLOT(as(cb, &IID_Selector), 9))(as(cb, &IID_Selector), wanted);
    printf("selected_early=%08lx\n", hr);
    ((HRESULT (WINAPI *)(obj *, void *))SLOT(as(cb, &IID_ItemsControl), 7))(as(cb, &IID_ItemsControl), &bindable);

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

    printf("size_changed=%d %.0fx%.0f->%.0fx%.0f\n", sh.calls, sh.prev.w, sh.prev.h, sh.now.w, sh.now.h);
    ((HRESULT (WINAPI *)(obj *, double))SLOT(as(b, &IID_FE), 16))(as(b, &IID_FE), 120);
    pump(300); UpdateWindow(island); pump(200);
    printf("size_changed2=%d %.0fx%.0f->%.0fx%.0f\n", sh.calls, sh.prev.w, sh.prev.h, sh.now.w, sh.now.h);

    ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(cb, &IID_Selector), 6))(as(cb, &IID_Selector), &index);
    printf("combo_selected=%d\n", index);
    return 0;
}
