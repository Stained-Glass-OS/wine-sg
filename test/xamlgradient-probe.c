/* xamlgradient-probe: LinearGradientBrush, GradientStop (patches/sg/6229).
 *
 * LinearGradientBrush, GradientBrush and GradientStop were missing: WinUI 2's control styles draw
 * their borders with gradients (ControlElevationBorderBrush), which then fell back to the
 * desktop's grey (Windows Terminal's buttons and boxes had grey frames). A gradient is drawn in
 * the mean of its colours along it for now.
 *
 * Prints name=value lines; see test/xamlgradient-gate.sh.
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
DEFINE_GUID(IID_PanelX,         0xa50a4bbd, 0x8361, 0x469c, 0x90, 0xda, 0xe9, 0xa4, 0x0c, 0x74, 0x74, 0xdf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);

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
DEFINE_GUID(IID_GradientBrush, 0x2166e69f, 0x935a, 0x4191, 0x8e, 0x3c, 0x1c, 0x8d, 0xfd, 0xfc, 0xdc, 0x78);

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
#define TL L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\""
static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<Border" TL L" Width=\"100\" Height=\"50\" BorderThickness=\"6\"><Border.BorderBrush>"
    L"<LinearGradientBrush StartPoint=\"0,0\" EndPoint=\"0,1\"><GradientStop Offset=\"0\" Color=\"#FFFF0000\"/>"
    L"<GradientStop Offset=\"1\" Color=\"#FF0000FF\"/></LinearGradientBrush></Border.BorderBrush></Border>"
    L"<Border" TL L" Width=\"100\" Height=\"50\" Margin=\"0,60,0,0\"><Border.Background>"
    L"<LinearGradientBrush MappingMode=\"Absolute\" EndPoint=\"0,3\"><LinearGradientBrush.GradientStops>"
    L"<GradientStop Offset=\"0\" Color=\"#FFFFFFFF\"/><GradientStop Offset=\"1.0\" Color=\"#FF000000\"/>"
    L"</LinearGradientBrush.GradientStops></LinearGradientBrush></Border.Background></Border>"
    L"</Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native;
    IInspectable *inner = NULL, *brush = NULL;
    HWND host, island = NULL;
    COLORREF c;
    HDC hdc;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    printf("activate=%08lx", RoActivateInstance(hs(L"Windows.UI.Xaml.Media.LinearGradientBrush"), &brush));
    printf(",%d\n", as(brush, &IID_GradientBrush) != NULL);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("loaded=%d\n", root != NULL);

    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(600); UpdateWindow(island); pump(200);

    hdc = GetDC(island);
    /* the border: between red and blue (their mean), not grey */
    c = GetPixel(hdc, 2, 25);
    printf("border=%d,%d,%d\n", GetRValue(c) > 90 && GetRValue(c) < 170, GetGValue(c) < 40, GetBValue(c) > 90 && GetBValue(c) < 170);
    /* white to black: their mean, mid grey */
    c = GetPixel(hdc, 50, 85);
    printf("background=%d\n", GetRValue(c) > 100 && GetRValue(c) < 160 && GetRValue(c) == GetBValue(c));
    ReleaseDC(island, hdc);
    printf("done=1\n");
    return 0;
}
