/* xamlbindpath-probe: {Binding A.B, RelativeSource TemplatedParent} and a ContentPresenter's
 * horizontal default (patches/sg/6220).
 *
 * A template binding whose path goes through an object the control holds
 * (Path=TemplateSettings.TopPaneVisibility, as WinUI 2's NavigationView template has it) was not
 * made: Windows Terminal's settings showed NavigationView's top bar in its left-pane mode. The path
 * is now followed, now and when any member on it changes. A ContentPresenter's text sat in its
 * middle (a Control's default, Center); a presenter's own default is Left.
 *
 * Prints name=value lines; see test/xamlbindpath-gate.sh.
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
static const char *colour(COLORREF c)
{
    int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    if (r > 200 && g < 60 && b < 60) return "red";
    if (r > 200 && g > 200 && b > 200) return "white";
    return "other";
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<ContentControl x:Name=\"C\" Width=\"100\" Height=\"40\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\">"
    L"<ContentControl.Tag><Border x:Name=\"Holder\" Visibility=\"Collapsed\"/></ContentControl.Tag>"
    L"<ContentControl.Template><ControlTemplate TargetType=\"ContentControl\">"
    L"<Grid Background=\"Red\" Visibility=\"{Binding RelativeSource={RelativeSource TemplatedParent}, Path=Tag.Visibility}\"/>"
    L"</ControlTemplate></ContentControl.Template></ContentControl>"
    L"<ContentPresenter Content=\"MMMM\" Foreground=\"Red\" FontSize=\"20\" Width=\"240\" Height=\"40\" Margin=\"0,60,0,0\""
    L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"</Grid>";

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *c, *holder, *other = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    int x, left = -1;
    HDC hdc;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    c = as(find(root, L"C"), &IID_FE);
    printf("loaded=%d\n", root != NULL && c != NULL);

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
    printf("collapsed=%s\n", colour(GetPixel(hdc, 50, 20)));

    /* the member at the end of the path changes: the bound Visibility follows */
    ((CALL(c, 9, get_fn))(c, &holder));
    if (holder) CALL(as(holder, &IID_UIE), 22, HRESULT (WINAPI *)(obj *, INT32))(as(holder, &IID_UIE), 0);
    pump(300); UpdateWindow(island); pump(200);
    printf("visible=%s\n", colour(GetPixel(hdc, 50, 20)));

    /* the first member changes (another Tag, collapsed): followed too */
    ((load_fn)SLOT(rs, 6))(rs, hs(L"<Border " NS L" Visibility=\"Collapsed\"/>"), &other);
    CALL(c, 10, putobj_fn)(c, other);
    pump(300); UpdateWindow(island); pump(200);
    printf("retagged=%s\n", colour(GetPixel(hdc, 50, 20)));

    /* the presenter's text: at its left */
    for (x = 0; x < 240 && left < 0; x++)
    {
        int y;
        for (y = 62; y < 98; y++) if (!strcmp(colour(GetPixel(hdc, x, y)), "red") || !strcmp(colour(GetPixel(hdc, x, y)), "other")) { left = x; break; }
    }
    printf("text_left=%d\n", left);
    ReleaseDC(island, hdc);
    printf("done=1\n");
    return 0;
}
