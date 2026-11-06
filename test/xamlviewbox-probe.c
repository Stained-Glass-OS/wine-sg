/* Windows.UI.Xaml Viewbox (patches/sg/6263): its child laid out at the size it wants and drawn scaled
 * to the Viewbox's, by Stretch (Uniform by default, Fill, None) and StretchDirection (DownOnly), in the
 * middle of the Viewbox; its properties and statics. Windows Terminal's markup has Viewboxes (WinUI 2
 * icons), which Wine had no type for. A 20x10 red Border in 100x100 Viewboxes, drawn in a XAML Island.
 * Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_AppFactory,     0x93bbe361, 0xbe5a, 0x4ee3, 0xb4, 0xa3, 0x95, 0x11, 0x8d, 0xc9, 0x7a, 0x89);
DEFINE_GUID(IID_Viewbox,        0x05252c58, 0xba9d, 0x4809, 0x9e, 0xc3, 0xfa, 0x0d, 0x16, 0x71, 0x0b, 0xa1);
DEFINE_GUID(IID_ViewboxStatics, 0x5cd1e72d, 0xe8d3, 0x4865, 0x8f, 0x08, 0xb6, 0xb2, 0xd6, 0x89, 0xad, 0xf1);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) (((obj *)(o))->vtbl[n])
static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *factory(const WCHAR *name, REFIID iid)
{
    obj *f = NULL;
    HRESULT hr = RoGetActivationFactory(hs(name), iid, (void **)&f);
    if (FAILED(hr)) printf("factory %ls=%08lx\n", name, hr);
    return f;
}
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((HRESULT (WINAPI *)(obj *, REFIID, void **))SLOT(o, 0))(o, iid, (void **)&r); return r; }
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
static int red(HDC hdc, int x, int y)
{
    COLORREF c = GetPixel(hdc, x, y);
    return GetRValue(c) > 200 && GetGValue(c) < 60 && GetBValue(c) < 60;
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
#define BOX(x, attrs) L"<Viewbox x:Name=\"v" #x L"\" Width=\"100\" Height=\"100\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" " \
    L"Margin=\"" #x L",0,0,0\" " attrs L"><Border Width=\"20\" Height=\"10\" Background=\"Red\"/></Viewbox>"

static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    BOX(0, L"")                                         /* Uniform: 100x50, from y 25 */
    BOX(110, L"Stretch=\"Fill\"")                       /* 100x100 */
    BOX(220, L"Stretch=\"None\"")                       /* 20x10, in the middle: 40..60, 45..55 */
    BOX(330, L"StretchDirection=\"DownOnly\"")          /* not made larger: as None */
    L"</Grid>";

int main(void)
{
    obj *appf, *app = NULL, *rs, *grid = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native = NULL, *vbf, *vbs, *vb = NULL, *dp = NULL;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    INT32 stretch = -1, direction = -1;
    HRESULT hr;
    HDC hdc;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(appf = factory(L"Windows.UI.Xaml.Application", &IID_AppFactory))) return 1;
    ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(appf, 6))(appf, NULL, &inner, &app);

    /* the class: activatable, its defaults, statics */
    if ((vbf = factory(L"Windows.UI.Xaml.Controls.Viewbox", &IID_IActivationFactory)))
        ((HRESULT (WINAPI *)(obj *, obj **))SLOT(vbf, 6))(vbf, &vb);
    if (vb && (vb = as(vb, &IID_Viewbox)))
    {
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(vb, 8))(vb, &stretch);
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(vb, 10))(vb, &direction);
    }
    printf("defaults=%d stretch=%d direction=%d\n", vb != NULL, stretch, direction);
    if (vb)
    {
        ((HRESULT (WINAPI *)(obj *, INT32))SLOT(vb, 9))(vb, 1);
        ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(vb, 8))(vb, &stretch);
        printf("set_stretch=%d\n", stretch);
    }
    if ((vbs = factory(L"Windows.UI.Xaml.Controls.Viewbox", &IID_ViewboxStatics)))
        ((HRESULT (WINAPI *)(obj *, obj **))SLOT(vbs, 6))(vbs, &dp);
    printf("statics=%d\n", dp != NULL);

    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;

    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((HRESULT (WINAPI *)(obj *, IInspectable *, IInspectable **, obj **))SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 460, 130, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 460, 130, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(src, 7))(src, as(grid, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    hdc = GetDC(island);
    /* Uniform: 100x50 from (0,25) */
    printf("uniform=%d%d%d%d%d\n", red(hdc, 3, 28), red(hdc, 96, 71), red(hdc, 50, 50), !red(hdc, 50, 20), !red(hdc, 50, 80));
    /* Fill: all of it */
    printf("fill=%d%d%d\n", red(hdc, 113, 3), red(hdc, 206, 96), red(hdc, 160, 50));
    /* None: 20x10 in the middle */
    printf("none=%d%d%d%d\n", red(hdc, 270, 50), !red(hdc, 255, 50), !red(hdc, 270, 40), !red(hdc, 270, 60));
    /* DownOnly: not made larger */
    printf("downonly=%d%d%d\n", red(hdc, 380, 50), !red(hdc, 365, 50), !red(hdc, 380, 30));
    ReleaseDC(island, hdc);
    return 0;
}
