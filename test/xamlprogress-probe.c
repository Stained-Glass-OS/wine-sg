/* Windows.UI.Xaml ProgressBar and ProgressRing (patches/sg/6173): a bar's value part in the accent colour
 * over its track (0 to 100), Value kept between Minimum and Maximum, ValueChanged; an indeterminate
 * bar's segment and an active ring moving; ShowError red -- Advanced Installer's progress page
 * (TeraCopy's installer). Interface identity per windows-rs 0.30.0 (MIT). */
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
DEFINE_GUID(IID_RangeBase,      0xfa002c1a, 0x494e, 0x46cf, 0x91, 0xd4, 0xe1, 0x4a, 0x8d, 0x79, 0x86, 0x75);
DEFINE_GUID(IID_ToggleButton,   0x589877fb, 0x0fc7, 0x4036, 0x9d, 0x8b, 0x12, 0x7d, 0xfa, 0x75, 0xc1, 0x6d);
DEFINE_GUID(IID_RefBool,        0x3c00fd60, 0x2950, 0x5939, 0xa2, 0x1a, 0x2d, 0x12, 0xc5, 0xa0, 0x1b, 0x8a);
DEFINE_GUID(IID_SplitButton,    0x58bbb066, 0xc2ea, 0x5499, 0x81, 0x50, 0x40, 0xfa, 0xa7, 0x5f, 0x6b, 0xb5);
DEFINE_GUID(IID_MenuFlyout,     0xad3210f1, 0x985b, 0x4171, 0xbb, 0xba, 0x67, 0xbc, 0xc7, 0x29, 0x64, 0x9b);
DEFINE_GUID(IID_MenuFlyoutItem, 0x160775cf, 0xb598, 0x44af, 0xa4, 0x88, 0xc3, 0xce, 0x2a, 0xf6, 0xd3, 0xf0);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
typedef HRESULT (WINAPI *load_fn)(obj *, HSTRING, obj **);
typedef HRESULT (WINAPI *add_fn)(obj *, void *, INT64 *);

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
        /* (moving looks keep the queue busy: bounded by the time too) */
        while (GetTickCount() < end && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

struct handler { void **vtbl; LONG ref; int calls; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *sender, void *args) { h->calls++; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<StackPanel " NS L" Background=\"White\">"
    L"<ProgressBar x:Name=\"P\" Value=\"25\" Height=\"10\" Margin=\"0,10,0,0\"/>"
    L"<ProgressBar x:Name=\"I\" IsIndeterminate=\"True\" Height=\"10\" Margin=\"0,10,0,0\"/>"
    L"<ProgressRing x:Name=\"R\" IsActive=\"True\" Width=\"40\" Height=\"40\" HorizontalAlignment=\"Left\" Margin=\"0,10,0,0\"/>"
    L"<ProgressBar x:Name=\"E\" Value=\"50\" ShowError=\"True\" Height=\"10\"/>"
    L"</StackPanel>";

static HWND island;

/* the island's pixels, copied at once */
static DWORD *bits;
static void snapshot(void)
{
    static HDC mem;
    static HBITMAP dib;
    HDC hdc;
    if (!mem)
    {
        BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), 300, -200, 1, 32, BI_RGB } };
        mem = CreateCompatibleDC(NULL);
        dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
        SelectObject(mem, dib);
    }
    UpdateWindow(island);
    hdc = GetDC(island);
    BitBlt(mem, 0, 0, 300, 200, hdc, 0, 0, SRCCOPY);
    ReleaseDC(island, hdc);
    GdiFlush();
}
static COLORREF pixel(int x, int y)
{
    DWORD v = bits[y * 300 + x];
    return RGB((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
}
static int white(COLORREF c) { return GetRValue(c) > 230 && GetGValue(c) > 230 && GetBValue(c) > 230; }

/* the drawn columns of a row: a signature of where things are */
static unsigned int row_print(int y, int from, int to, int *count)
{
    unsigned int sig = 0;
    int x;
    *count = 0;
    for (x = from; x < to; x++) if (!white(pixel(x, y))) { sig = sig * 31 + x; (*count)++; }
    return sig;
}

static unsigned int area_print(int y0, int y1, int x1, int *count)
{
    unsigned int sig = 0;
    int y, n;
    *count = 0;
    for (y = y0; y < y1; y += 3) { sig = sig * 131 + row_print(y, 0, x1, &n); *count += n; }
    return sig;
}

static double get_value(obj *r, int slot) { double v = -1; ((HRESULT (WINAPI *)(obj *, double *))SLOT(r, slot))(r, &v); return v; }
static void put_value(obj *r, int slot, double v) { ((HRESULT (WINAPI *)(obj *, double))SLOT(r, slot))(r, v); }

int main(void)
{
    struct handler changed = { h_vtbl, 1 };
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *p;
    IInspectable *inner = NULL;
    HWND host;
    INT64 token;
    HRESULT hr;
    COLORREF low, mid, err;
    unsigned int s1, s2, r1, r2;
    int n1, n2, m1, m2;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
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
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);

    /* a quarter: its first quarter in the accent colour, the rest its track */
    p = as(find(root, L"P"), &IID_RangeBase);
    if (!p) { printf("no range base\n"); return 1; }
    snapshot();
    low = pixel(30, 15); mid = pixel(150, 15);
    printf("value=%d,%d,%d,%.0f,%.0f,%.0f\n", !white(low), !white(mid), low != mid, get_value(p, 14), get_value(p, 6), get_value(p, 8));
    ((add_fn)SLOT(p, 16))(p, &changed, &token);
    put_value(p, 15, 80);
    pump(100); snapshot();
    printf("moved=%d,%d\n", pixel(150, 15) == low, changed.calls);
    /* kept within its range */
    put_value(p, 15, 150);
    printf("coerced=%.0f,%d\n", get_value(p, 14), changed.calls);
    put_value(p, 9, 50);
    printf("max=%.0f,%.0f\n", get_value(p, 8), get_value(p, 14));

    /* moving looks: an indeterminate bar's segment, an active ring */
    snapshot();
    s1 = row_print(35, 0, 300, &n1); r1 = area_print(50, 90, 40, &m1);
    pump(400);
    snapshot();
    s2 = row_print(35, 0, 300, &n2); r2 = area_print(50, 90, 40, &m2);
    printf("indeterminate=%d,%d,%d\n", n1 > 0 && n1 <= 130, n2 > 0 && n2 <= 130, s1 != s2);
    printf("ring=%d,%d\n", m1 > 0 && m2 > 0, r1 != r2);

    /* ShowError: red */
    setvbuf(stdout, NULL, _IONBF, 0);
    err = pixel(30, 95);
    printf("error=%d\n", GetRValue(err) > 150 && GetGValue(err) < 100 && GetBValue(err) < 100);
    return 0;
}
