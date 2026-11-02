/* xamlrunformat-probe: inline text formatting (patches/sg/6232).
 *
 * A TextBlock's Inlines were drawn as plain text: a Run's Foreground, FontWeight, FontSize and
 * TextDecorations were not kept, and Bold, Italic, Underline and Hyperlink were missing. They now
 * format their part of the text (a Hyperlink underlined in the accent colour), a gradient
 * Foreground included.
 *
 * Prints name=value lines; see test/xamlrunformat-gate.sh.
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
#include <stdlib.h>

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
#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
#define TL L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" FontSize=\"20\""
static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<TextBlock x:Name=\"Plain\"" TL L"><Run Text=\"MMMM\"/></TextBlock>"
    L"<TextBlock x:Name=\"Heavy\"" TL L" Margin=\"0,30,0,0\"><Bold>MMMM</Bold></TextBlock>"
    L"<TextBlock x:Name=\"Big\"" TL L" Margin=\"0,60,0,0\"><Run FontSize=\"30\" Text=\"MMMM\"/></TextBlock>"
    L"<TextBlock" TL L" Margin=\"150,0,0,0\"><Run Foreground=\"#FFFF0000\" Text=\"MMMM\"/></TextBlock>"
    L"<TextBlock" TL L" Margin=\"150,40,0,0\"><Hyperlink>MMMM</Hyperlink></TextBlock>"
    L"<TextBlock" TL L" Margin=\"150,80,0,0\" Width=\"140\"><Run Text=\"MMMMMMMM\"><Run.Foreground><LinearGradientBrush StartPoint=\"0,0\" EndPoint=\"1,0\">"
    L"<GradientStop Offset=\"0\" Color=\"#FFFF0000\"/><GradientStop Offset=\"1\" Color=\"#FF0000FF\"/></LinearGradientBrush></Run.Foreground></Run></TextBlock>"
    L"</Grid>";

static double width_of(obj *root, const WCHAR *name)
{
    double w = -1;
    obj *e = as(find(root, name), &IID_FE);
    if (e) CALL(e, 13, HRESULT (WINAPI *)(obj *, double *))(e, &w);
    return w;
}

/* the colour of the first strongly coloured pixel in a box: r, b, k (black), - */
static char tint(HDC hdc, int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
        {
            COLORREF c = GetPixel(hdc, x, y);
            int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
            if (r > 180 && g < 80 && b < 80) return 'r';
            if (b > 150 && r < 100 && g < 160) return 'b';
            if (r < 60 && g < 60 && b < 60) return 'k';
        }
    return '-';
}

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    HDC hdc;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
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

    {
        double plain = width_of(root, L"Plain"), heavy = width_of(root, L"Heavy"), big = width_of(root, L"Big");
        printf("bold_wider=%d\n", heavy > plain + 1);
        printf("size_wider=%d\n", big > plain * 1.3);
    }
    hdc = GetDC(island);
    printf("run_red=%c\n", tint(hdc, 150, 0, 260, 30));
    printf("gradient=%c,%c\n", tint(hdc, 150, 80, 170, 110), tint(hdc, 250, 80, 290, 110));
    {
        int x, y, best = 999; COLORREF bc = 0;
        for (y = 40; y < 70; y++) for (x = 150; x < 260; x++)
        {
            COLORREF c = GetPixel(hdc, x, y);
            int v = GetRValue(c) + GetGValue(c) + GetBValue(c);
            if (v < best) { best = v; bc = c; }
        }
        {
            /* the accent: the desktop's highlight colour */
            COLORREF h = GetSysColor(COLOR_HIGHLIGHT);
            printf("hyperlink=%d\n", abs(GetRValue(bc) - GetRValue(h)) < 40 && abs(GetGValue(bc) - GetGValue(h)) < 40 &&
                   abs(GetBValue(bc) - GetBValue(h)) < 40);
        }
    }
    ReleaseDC(island, hdc);
    printf("done=1\n");
    return 0;
}
