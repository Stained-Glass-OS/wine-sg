/* Windows.UI.Xaml drawing of WinUI 2's pieces (patches/sg/6177): a Path from path markup filled, a
 * ScrollBar's thumb where its Value puts it, AcrylicBrush and XamlCompositionBrushBase in their
 * FallbackColor, a BitmapIcon's picture, and a ToolTip object as
 * ToolTipService.ToolTip shown as its content (in the tooltip colours), FlyoutBase.ShowAttachedFlyout
 * showing an element's attached MenuFlyout. Interface identity per windows-rs
 * 0.30.0 (MIT). */
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
DEFINE_GUID(IID_FlyoutBaseStatics, 0xe2d795e3, 0x85c0, 0x4de2, 0xba, 0xc1, 0x52, 0x94, 0xca, 0x01, 0x1a, 0x78);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
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
        while (GetTickCount() < end && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(5);
    }
}
static int close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 40 && abs(GetGValue(c) - g) < 40 && abs(GetBValue(c) - b) < 40;
}

/* a 4x4 blue bitmap file */
static void write_bmp(const char *path)
{
    BYTE file[14 + 40 + 64] = { 'B', 'M' };
    BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)(file + 14);
    DWORD *px = (DWORD *)(file + 54), written;
    HANDLE h;
    int i;
    *(DWORD *)(file + 2) = sizeof(file);
    *(DWORD *)(file + 10) = 54;
    bih->biSize = 40; bih->biWidth = 4; bih->biHeight = 4; bih->biPlanes = 1; bih->biBitCount = 32; bih->biCompression = BI_RGB;
    for (i = 0; i < 16; i++) px[i] = 0x0000ff;
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, file, sizeof(file), &written, NULL);
    CloseHandle(h);
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Canvas " NS L" Background=\"White\" Width=\"300\" Height=\"200\">"
    L"<Path Canvas.Left=\"10\" Canvas.Top=\"10\" Data=\"M0,0 L20,0 L20,20 L0,20 Z\" Fill=\"Red\"/>"
    L"<ScrollBar Canvas.Left=\"10\" Canvas.Top=\"50\" Width=\"200\" Height=\"12\" Orientation=\"Horizontal\" Maximum=\"100\""
    L" ViewportSize=\"100\" Value=\"100\"/>"
    L"<Border Canvas.Left=\"10\" Canvas.Top=\"70\" Width=\"20\" Height=\"20\">"
    L"<Border.Background><AcrylicBrush TintColor=\"#FF0000FF\" FallbackColor=\"#FF00FF00\"/></Border.Background></Border>"
    L"<Border Canvas.Left=\"40\" Canvas.Top=\"70\" Width=\"20\" Height=\"20\">"
    L"<Border.Background><XamlCompositionBrushBase FallbackColor=\"#FFFFFF00\"/></Border.Background></Border>"
    L"<BitmapIcon Canvas.Left=\"10\" Canvas.Top=\"100\" Width=\"20\" Height=\"20\" UriSource=\"C:\\sq.bmp\" ShowAsMonochrome=\"False\"/>"
    L"<Button Canvas.Left=\"100\" Canvas.Top=\"100\" Width=\"60\" Height=\"30\" Content=\"T\" x:Name=\"Tipped\">"
    L"<ToolTipService.ToolTip><ToolTip Content=\"Hello tip\"/></ToolTipService.ToolTip></Button>"
    L"<Button Canvas.Left=\"200\" Canvas.Top=\"100\" Width=\"60\" Height=\"30\" Content=\"S\" ToolTipService.ToolTip=\"Hello tip\"/>"
    L"</Canvas>";

static HWND island;
static COLORREF pixel(HWND hwnd, int x, int y)
{
    HDC hdc = GetDC(hwnd);
    COLORREF c;
    UpdateWindow(hwnd);
    c = GetPixel(hdc, x, y);
    ReleaseDC(hwnd, hdc);
    return c;
}

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native;
    IInspectable *inner = NULL;
    HWND host, tip = NULL, hwnd = NULL;
    COLORREF thumb, track, bk = GetSysColor(COLOR_INFOBK);
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    write_bmp("C:\\sq.bmp");
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 100, 100, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((HRESULT (WINAPI *)(obj *, obj *))SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);

    printf("path=%d,%d\n", close_to(pixel(island, 20, 20), 255, 0, 0), close_to(pixel(island, 40, 20), 255, 255, 255));
    thumb = pixel(island, 180, 56); track = pixel(island, 30, 56);   /* at Value = Maximum: the thumb at the end */
#define LUM(c) (GetRValue(c) * 3 + GetGValue(c) * 6 + GetBValue(c))
    /* the thumb (the text colour, half transparent) darker than the bare track */
    printf("scrollbar=%d,%d\n", LUM(thumb) < LUM(track), !close_to(track, 255, 255, 255) || !close_to(thumb, 255, 255, 255));
    printf("acrylic=%d\n", close_to(pixel(island, 20, 80), 0, 255, 0));
    printf("compositionbrush=%d\n", close_to(pixel(island, 50, 80), 255, 255, 0));
    printf("bitmapicon=%d\n", close_to(pixel(island, 20, 110), 0, 0, 255));

    /* the ToolTip shows its content in the tooltip colours */
    SendMessageW(island, WM_MOUSEMOVE, 0, MAKELPARAM(130, 115));
    pump(1200);
    while ((hwnd = FindWindowExW(NULL, hwnd, L"SgXamlPopup", NULL))) if (IsWindowVisible(hwnd)) { tip = hwnd; break; }
    printf("tooltip=%d,%d\n", tip != NULL, tip && close_to(pixel(tip, 4, 4), GetRValue(bk), GetGValue(bk), GetBValue(bk)));
    {
        /* the same as a string tooltip of its content */
        RECT rc1 = { 0 }, rc2 = { 0 };
        HWND tip2 = NULL;
        if (tip) GetClientRect(tip, &rc1);
        SendMessageW(island, WM_MOUSEMOVE, 0, MAKELPARAM(250, 20));
        pump(300);
        SendMessageW(island, WM_MOUSEMOVE, 0, MAKELPARAM(230, 115));
        pump(1200);
        hwnd = NULL;
        while ((hwnd = FindWindowExW(NULL, hwnd, L"SgXamlPopup", NULL))) if (IsWindowVisible(hwnd) && hwnd != tip) { tip2 = hwnd; break; }
        if (!tip2 && tip && IsWindowVisible(tip)) tip2 = tip;   /* the same window again */
        if (tip2) GetClientRect(tip2, &rc2);
        printf("tipsame=%d,%d\n", tip2 != NULL, rc1.right && rc1.right == rc2.right && rc1.bottom == rc2.bottom);
    }

    /* FlyoutBase.ShowAttachedFlyout: the element's attached MenuFlyout, in a popup window */
    SendMessageW(island, WM_MOUSEMOVE, 0, MAKELPARAM(250, 20));
    pump(300);
    {
        obj *fb = factory(L"Windows.UI.Xaml.Controls.Primitives.FlyoutBase", &IID_FlyoutBaseStatics), *button = NULL, *menu = NULL;
        HWND shown = NULL;
        ((load_fn)SLOT(rs, 6))(rs, hs(L"<MenuFlyout " NS L"><MenuFlyoutItem Text=\"One\"/></MenuFlyout>"), &menu);
        ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(L"Tipped"), &button);
        if (fb && button && menu)
        {
            ((HRESULT (WINAPI *)(obj *, obj *, obj *))SLOT(fb, 9))(fb, as(button, &IID_FE), menu);
            ((HRESULT (WINAPI *)(obj *, obj *))SLOT(fb, 10))(fb, as(button, &IID_FE));
            pump(500);
            hwnd = NULL;
            while ((hwnd = FindWindowExW(NULL, hwnd, L"SgXamlPopup", NULL))) if (IsWindowVisible(hwnd) && hwnd != tip) { shown = hwnd; break; }
        }
        printf("attached=%d,%d,%d\n", button != NULL, menu != NULL, shown != NULL);
    }
    return 0;
}
