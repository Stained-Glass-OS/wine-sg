/* xamlopacity-probe: a UIElement's Opacity is drawn (patches/sg/6216).
 *
 * Opacity was kept but never drawn: what was meant hidden showed (Windows Terminal's settings
 * menu had every item's selection marker lit) and what was meant faded was solid. At 0 an element
 * and its content are not drawn; below 1 they are drawn through a layer at that opacity, nested
 * opacities multiplying; setting it draws again.
 *
 * Prints name=value lines; see test/xamlopacity-gate.sh.
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
/* red over white at an opacity: how red each pixel is, 0 (white) to 100 (red) */
static int redness(COLORREF c)
{
    if (GetRValue(c) < 200) return -1;   /* not red over white */
    return (255 - (GetGValue(c) + GetBValue(c)) / 2) * 100 / 255;
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
/* 40-high red bands over white: hidden, half, a half in a half, full (to be hidden), and full */
static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<StackPanel Width=\"100\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\">"
    L"<Border Height=\"40\" Background=\"Red\" Opacity=\"0\"/>"
    L"<Border Height=\"40\" Background=\"Red\" Opacity=\"0.5\"/>"
    L"<Grid Height=\"40\" Opacity=\"0.5\"><Border Background=\"Red\" Opacity=\"0.5\"/></Grid>"
    L"<Border x:Name=\"D\" Height=\"40\" Background=\"Red\"/>"
    L"<Border Height=\"40\" Background=\"Red\" Opacity=\"1\"/>"
    L"</StackPanel>"
    L"</Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *d;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    double value = -1;
    HDC hdc;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    d = as(find(root, L"D"), &IID_UIE);

    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 240, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 240, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(600); UpdateWindow(island); pump(200);

    hdc = GetDC(island);
    printf("hidden=%d half=%d quarter=%d full=%d,%d\n", redness(GetPixel(hdc, 50, 20)), redness(GetPixel(hdc, 50, 60)),
           redness(GetPixel(hdc, 50, 100)), redness(GetPixel(hdc, 50, 140)), redness(GetPixel(hdc, 50, 180)));

    /* set: it is drawn again at what it is set to */
    CALL(d, 10, HRESULT (WINAPI *)(obj *, double))(d, 0.0);    /* put_Opacity */
    CALL(d, 9, HRESULT (WINAPI *)(obj *, double *))(d, &value);
    pump(300); UpdateWindow(island); pump(200);
    printf("set=%.1f now=%d\n", value, redness(GetPixel(hdc, 50, 140)));
    ReleaseDC(island, hdc);
    printf("done=1\n");
    return 0;
}
