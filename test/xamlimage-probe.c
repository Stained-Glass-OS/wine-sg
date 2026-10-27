/* Windows.UI.Xaml images (patches/sg/6151): Image with a SoftwareBitmapSource
 * (SetBitmapAsync of a SoftwareBitmap, as NanaZip's address bar icon) and with a BitmapImage from
 * markup (Source="ms-appx:///blue.png", decoded with WIC), Stretch Fill and Uniform, drawn in a XAML
 * Island; a source that is not a SoftwareBitmap fails, a missing file draws nothing. Interface identity
 * per windows-rs 0.30.0 (MIT) and Wine's windows.graphics.imaging.idl. */
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
DEFINE_GUID(IID_Image,          0x495b7402, 0x9af3, 0x4e50, 0xaa, 0x90, 0x03, 0x38, 0x8f, 0x30, 0x86, 0xd2);
DEFINE_GUID(IID_SBSource,       0xd2dd9ed0, 0xd3c5, 0x4056, 0x91, 0xb5, 0xb7, 0xc1, 0xd1, 0xe8, 0x13, 0x0e);
DEFINE_GUID(IID_BitmapSource,   0x23d86411, 0x202f, 0x41b2, 0x8c, 0x5b, 0xa8, 0xa3, 0xb3, 0x33, 0x80, 0x0b);
DEFINE_GUID(IID_SBFactory,      0xc99feb69, 0x2d62, 0x4d47, 0xa6, 0xb3, 0x4f, 0xdb, 0x6a, 0x07, 0xfd, 0xf8);
DEFINE_GUID(IID_MemBuf,         0xfbc4dd2a, 0x245b, 0x11e4, 0xaf, 0x98, 0x68, 0x94, 0x23, 0x26, 0x0c, 0xf8);
DEFINE_GUID(IID_MemBytes,       0x5b0d3235, 0x4dba, 0x4d44, 0x86, 0x5e, 0x8f, 0x1d, 0x0e, 0x4f, 0xd0, 0x4d);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
typedef HRESULT (WINAPI *get_fn)(obj *, obj **);
typedef HRESULT (WINAPI *create_fn)(obj *, IInspectable *, IInspectable **, obj **);
typedef HRESULT (WINAPI *putobj_fn)(obj *, obj *);
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
static int close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 40 && abs(GetGValue(c) - g) < 40 && abs(GetBValue(c) - b) < 40;
}
static obj *find(obj *root, const WCHAR *name)
{
    obj *r = NULL;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(name), &r);
    return r;
}

/* a Completed handler */
struct handler { void **vtbl; LONG ref; int calls, status; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *op, int status) { h->calls++; h->status = status; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
#define TL L" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\""

int main(void)
{
    static const WCHAR page_xaml[] =
        L"<Grid " NS L" Background=\"White\">"
        L"<Image x:Name=\"Soft\"" TL L" Width=\"40\" Height=\"40\" Stretch=\"Fill\"/>"
        L"<Image x:Name=\"Png\"" TL L" Margin=\"100,0,0,0\" Width=\"40\" Height=\"40\" Source=\"ms-appx:///blue.png\"/>"
        L"<Image" TL L" Margin=\"200,0,0,0\" Width=\"40\" Height=\"40\" Source=\"ms-appx:///missing.png\"/>"
        L"</Grid>";
    struct handler h = { h_vtbl, 1 };
    obj *rs, *grid = NULL, *sbf, *sb = NULL, *src = NULL, *action = NULL, *png, *pngsrc = NULL, *ms, *mgr = NULL, *sf, *xs = NULL, *native;
    IInspectable *inner = NULL;
    HWND host, island = NULL;
    INT32 width = 0;
    HRESULT hr;
    HDC hdc;

    RoInitialize(RO_INIT_SINGLETHREADED);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(rs, 6))(rs, hs(page_xaml), &grid);
    printf("load=%08lx\n", hr);
    if (!grid) return 1;

    /* a red SoftwareBitmap through a SoftwareBitmapSource */
    sbf = factory(L"Windows.Graphics.Imaging.SoftwareBitmap", &IID_SBFactory);
    ((HRESULT (WINAPI *)(obj *, int, INT32, INT32, obj **))SLOT(sbf, 6))(sbf, 87, 2, 2, &sb);
    {
        obj *bb = NULL, *ref = NULL, *bytes;
        BYTE *p = NULL;
        UINT32 cap = 0, i;
        ((HRESULT (WINAPI *)(obj *, int, obj **))SLOT(sb, 15))(sb, 1, &bb);
        ((HRESULT (WINAPI *)(obj *, obj **))SLOT(as(bb, &IID_MemBuf), 6))(as(bb, &IID_MemBuf), &ref);
        bytes = as(ref, &IID_MemBytes);
        ((HRESULT (WINAPI *)(obj *, BYTE **, UINT32 *))SLOT(bytes, 3))(bytes, &p, &cap);
        for (i = 0; i < cap; i += 4) { p[i] = 0; p[i + 1] = 0; p[i + 2] = 255; p[i + 3] = 255; }
    }
    src = NULL;
    {
        IInspectable *si = NULL;
        RoActivateInstance(hs(L"Windows.UI.Xaml.Media.Imaging.SoftwareBitmapSource"), &si);
        src = as(si, &IID_SBSource);
    }
    hr = ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(src, 6))(src, sb, &action);
    if (action) ((HRESULT (WINAPI *)(obj *, void *))SLOT(action, 6))(action, &h);
    printf("set_bitmap=%08lx completed=%d,%d\n", hr, h.calls, h.status);
    ((putobj_fn)SLOT(as(find(grid, L"Soft"), &IID_Image), 7))(as(find(grid, L"Soft"), &IID_Image), src);
    {
        obj *bad = NULL;
        hr = ((HRESULT (WINAPI *)(obj *, obj *, obj **))SLOT(src, 6))(src, grid, &bad);
        printf("not_a_bitmap=%d\n", FAILED(hr));
    }

    png = find(grid, L"Png");
    ((get_fn)SLOT(as(png, &IID_Image), 6))(as(png, &IID_Image), &pngsrc);
    if (pngsrc) ((HRESULT (WINAPI *)(obj *, INT32 *))SLOT(as(pngsrc, &IID_BitmapSource), 6))(as(pngsrc, &IID_BitmapSource), &width);
    printf("png_width=%d\n", width);

    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &xs);
    native = as(xs, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 300, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(xs, 7))(xs, as(grid, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    hdc = GetDC(island);
    printf("software_image=%d,%d\n", close_to(GetPixel(hdc, 20, 20), 255, 0, 0), close_to(GetPixel(hdc, 38, 38), 255, 0, 0));
    printf("png_uniform=%d,%d\n", close_to(GetPixel(hdc, 120, 20), 0, 0, 255), close_to(GetPixel(hdc, 120, 4), 255, 255, 255));
    printf("missing=%d\n", close_to(GetPixel(hdc, 220, 20), 255, 255, 255));
    ReleaseDC(island, hdc);
    return 0;
}
