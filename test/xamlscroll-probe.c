/* xamlscroll-probe: a ScrollViewer scrolls (patches/sg/6214).
 *
 * A ScrollViewer showed its content at its own size and never moved it: React Native for Windows'
 * ScrollView (OneDrive) and WinUI pages could not show what was below. Its content is measured
 * unbounded the way it scrolls, shown at minus its offsets and clipped to it; ChangeView and the
 * mouse wheel move the view and ViewChanged tells.
 *
 * Prints name=value lines; see test/xamlscroll-gate.sh.
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
DEFINE_GUID(IID_SV,             0x64e9be00, 0x4dc1, 0x493d, 0xab, 0xe7, 0xcb, 0xd3, 0xc5, 0x77, 0x49, 0x0d);
DEFINE_GUID(IID_SV2,            0x64e9be10, 0x4dd1, 0x494d, 0xab, 0xf7, 0xcb, 0xd3, 0xc5, 0x77, 0x49, 0x1d);
DEFINE_GUID(IID_PVStatics,      0x629bdbc8, 0xd932, 0x4ff4, 0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
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
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    if (root) ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}
static double sv_double(obj *sv, int slot) { double v = -1; CALL(sv, slot, HRESULT (WINAPI *)(obj *, double *))(sv, &v); return v; }
static const char *colour(COLORREF c)
{
    if (GetRValue(c) > 200 && GetGValue(c) < 60 && GetBValue(c) < 60) return "red";
    if (GetGValue(c) > 100 && GetRValue(c) < 60 && GetBValue(c) < 60) return "green";
    if (GetBValue(c) > 200 && GetRValue(c) < 60 && GetGValue(c) < 60) return "blue";
    if (GetRValue(c) > 200 && GetGValue(c) > 200 && GetBValue(c) > 200) return "white";
    return "other";
}

/* a ViewChanged handler */
static int view_changed;
static HRESULT WINAPI h_qi(void *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_ref(void *h) { return 2; }
static HRESULT WINAPI h_invoke(void *h, void *sender, void *args) { view_changed++; return S_OK; }
static void *h_vtbl[] = { h_qi, h_ref, h_ref, h_invoke };
static struct { void **vtbl; } handler = { h_vtbl };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
/* 100 high, over five 50-high bands: red, green, blue, red, green */
static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<ScrollViewer x:Name=\"S\" Width=\"100\" Height=\"100\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\">"
    L"<StackPanel>"
    L"<Border Height=\"50\" Background=\"Red\"/><Border Height=\"50\" Background=\"Lime\"/><Border Height=\"50\" Background=\"Blue\"/>"
    L"<Border Height=\"50\" Background=\"Red\"/><Border Height=\"50\" Background=\"Lime\"/>"
    L"</StackPanel>"
    L"</ScrollViewer>"
    L"</Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *s, *sv, *sv2, *pvs;
    IInspectable *inner = NULL, *y = NULL;
    INT64 token = 0;
    boolean ok = FALSE;
    HWND host, island = NULL;
    HDC hdc;
    POINT pt = { 50, 50 };

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    s = find(root, L"S");
    sv = as(s, &IID_SV);
    sv2 = as(s, &IID_SV2);
    CALL(sv, 56, HRESULT (WINAPI *)(obj *, void *, INT64 *))(sv, &handler, &token);   /* ViewChanged */

    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(600); UpdateWindow(island); pump(200);

    printf("extent=%.0f viewport=%.0f scrollable=%.0f offset=%.0f\n", sv_double(sv, 49), sv_double(sv, 46), sv_double(sv, 47),
           sv_double(sv, 45));
    hdc = GetDC(island);
    printf("top=%s,%s clipped=%s\n", colour(GetPixel(hdc, 50, 25)), colour(GetPixel(hdc, 50, 75)), colour(GetPixel(hdc, 50, 150)));

    /* ChangeView(null, 100): the third band at the top */
    pvs = factory(L"Windows.Foundation.PropertyValue", &IID_PVStatics);
    CALL(pvs, 15, HRESULT (WINAPI *)(obj *, double, IInspectable **))(pvs, 100.0, &y);     /* CreateDouble */
    CALL(sv2, 14, HRESULT (WINAPI *)(obj *, void *, void *, void *, boolean *))(sv2, NULL, y, NULL, &ok);
    pump(300); UpdateWindow(island); pump(200);
    printf("changed=%d,%.0f,%d top=%s\n", ok, sv_double(sv, 45), view_changed, colour(GetPixel(hdc, 50, 25)));

    /* the wheel down one notch: 48 further; past the end it stops at the scrollable height */
    ClientToScreen(island, &pt);
    SendMessageW(island, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), MAKELPARAM(pt.x, pt.y));
    pump(200);
    printf("wheel=%.0f\n", sv_double(sv, 45));
    SendMessageW(island, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA * 10), MAKELPARAM(pt.x, pt.y));
    pump(200);
    printf("end=%.0f events=%d\n", sv_double(sv, 45), view_changed);
    ReleaseDC(island, hdc);
    printf("done=1\n");
    return 0;
}
