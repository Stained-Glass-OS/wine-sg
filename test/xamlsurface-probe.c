/* xamlsurface-probe: Windows.UI.Xaml.Media.Imaging.SurfaceImageSource (patches/sg/6212).
 *
 * The class did not exist: OneDrive's sign-in page draws its pictures with RNSVG into one and ended
 * with an unhandled exception. A program draws into it with its own device, through
 * ISurfaceImageSourceNativeWithD2D (a Direct2D device context, or a DXGI surface of a Direct2D
 * device) or ISurfaceImageSourceNative (a Direct3D device's surface); an Image shows what it drew.
 *
 * Prints name=value lines; see test/xamlsurface-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <stdio.h>

DEFINE_GUID(IID_ManagerStatics, 0x28258a12, 0x7d82, 0x505b, 0xb2, 0x10, 0x71, 0x2b, 0x04, 0xa5, 0x88, 0x82);
DEFINE_GUID(IID_SourceFactory,  0x5cd61dc0, 0x2561, 0x56e1, 0x8e, 0x75, 0x6e, 0x44, 0x17, 0x38, 0x05, 0xe3);
DEFINE_GUID(IID_SourceNative,   0x3cbcf1bf, 0x2f76, 0x4e9c, 0x96, 0xab, 0xe8, 0x4b, 0x37, 0x97, 0x25, 0x54);
DEFINE_GUID(IID_UIE,            0x676d0be9, 0xb65c, 0x41c6, 0xba, 0x40, 0x58, 0xcf, 0x87, 0xf2, 0x01, 0xc1);
DEFINE_GUID(IID_FE,             0xa391d09b, 0x4a99, 0x4b7c, 0x9d, 0x8d, 0x6f, 0xa5, 0xd0, 0x1f, 0x6f, 0xbf);
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_Image,          0x495b7402, 0x9af3, 0x4e50, 0xaa, 0x90, 0x03, 0x38, 0x8f, 0x30, 0x86, 0xd2);
DEFINE_GUID(IID_SISFactory,     0x3ab2212a, 0xef65, 0x4a5f, 0xbf, 0xac, 0x73, 0x99, 0x3e, 0x8c, 0x12, 0xc9);
DEFINE_GUID(IID_SISNative,      0xf2e9edc1, 0xd307, 0x4525, 0x98, 0x86, 0x0f, 0xaf, 0xaa, 0x44, 0x16, 0x3c);
DEFINE_GUID(IID_SISNativeD2D,   0x54298223, 0x41e1, 0x4a41, 0x9c, 0x08, 0x02, 0xe8, 0x25, 0x68, 0x64, 0xa1);
DEFINE_GUID(IID_D2DDeviceContext_p, 0xe8f7fe7a, 0x191c, 0x466d, 0xad, 0x95, 0x97, 0x56, 0x78, 0xbd, 0xa9, 0x98);
DEFINE_GUID(IID_DXGISurface_p,  0xcafcb56c, 0x6ac3, 0x4889, 0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec);
DEFINE_GUID(IID_DXGIDevice_p,   0x54ec77fa, 0x1377, 0x44e6, 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c);
DEFINE_GUID(IID_D3D11Resource_p, 0xdc8e63f3, 0xd12b, 0x4952, 0xb4, 0x7b, 0x5e, 0x45, 0x02, 0x6a, 0x86, 0x2d);

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
    if (GetRValue(c) > 200 && GetGValue(c) < 60 && GetBValue(c) < 60) return "red";
    if (GetGValue(c) > 100 && GetRValue(c) < 60 && GetBValue(c) < 60) return "green";
    if (GetBValue(c) > 200 && GetRValue(c) < 60 && GetGValue(c) < 60) return "blue";
    if (GetRValue(c) > 200 && GetGValue(c) > 200 && GetBValue(c) > 200) return "white";
    return "other";
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
static const WCHAR page_xaml[] =
    L"<Grid " NS L" Background=\"White\">"
    L"<Image x:Name=\"I\" Width=\"40\" Height=\"40\" Stretch=\"Fill\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\"/>"
    L"</Grid>";

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *image, *sisf, *sis = NULL, *d2dnative, *nat;
    IInspectable *inner = NULL;
    ID3D11Device *d3d = NULL;
    ID3D11DeviceContext *immediate = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *d2df = NULL;
    ID2D1Device *d2d = NULL;
    ID2D1DeviceContext *ctx = NULL;
    IDXGISurface *surface = NULL;
    ID2D1Bitmap1 *bitmap = NULL;
    POINT offset = { -1, -1 };
    RECT all = { 0, 0, 4, 4 }, bottom_left = { 0, 2, 2, 4 }, right = { 2, 0, 4, 4 };
    D2D1_COLOR_F red = { 1, 0, 0, 1 }, green = { 0, 1, 0, 1 };
    HWND host, island = NULL;
    HDC hdc;
    HRESULT hr, hr2, hr3;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ms = factory(L"Windows.UI.Xaml.Hosting.WindowsXamlManager", &IID_ManagerStatics);
    ((get_fn)SLOT(ms, 6))(ms, &mgr);
    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    image = find(root, L"I");

    sisf = factory(L"Windows.UI.Xaml.Media.Imaging.SurfaceImageSource", &IID_SISFactory);
    hr = sisf ? CALL(sisf, 6, HRESULT (WINAPI *)(obj *, INT32, INT32, void *, void *, obj **))(sisf, 4, 4, NULL, NULL, &sis) : E_FAIL;
    printf("made=%08lx natives=%d,%d\n", hr, as(sis, &IID_SISNative) != NULL, as(sis, &IID_SISNativeD2D) != NULL);
    if (!sis) return 1;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2,
                           D3D11_SDK_VERSION, &d3d, NULL, &immediate);
    if (FAILED(hr)) hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2,
                                           D3D11_SDK_VERSION, &d3d, NULL, &immediate);
    printf("device=%08lx\n", hr);
    if (!d3d) return 1;
    ID3D11Device_QueryInterface(d3d, &IID_DXGIDevice_p, (void **)&dxgi);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&d2df);
    ID2D1Factory1_CreateDevice(d2df, dxgi, &d2d);

    /* a Direct2D device: a device context to draw with (red everywhere) */
    d2dnative = as(sis, &IID_SISNativeD2D);
    CALL(d2dnative, 3, HRESULT (WINAPI *)(obj *, IUnknown *))(d2dnative, (IUnknown *)d2d);   /* SetDevice */
    hr = CALL(d2dnative, 4, HRESULT (WINAPI *)(obj *, const RECT *, REFIID, void **, POINT *))(d2dnative, &all,
            &IID_D2DDeviceContext_p, (void **)&ctx, &offset);
    if (ctx)
    {
        ID2D1RenderTarget_Clear((ID2D1RenderTarget *)ctx, &red);
        ID2D1RenderTarget_Release((ID2D1RenderTarget *)ctx);
    }
    hr2 = CALL(d2dnative, 5, HRESULT (WINAPI *)(obj *))(d2dnative);   /* EndDraw */
    printf("d2d_context=%08lx,%08lx offset=%ld,%ld\n", hr, hr2, offset.x, offset.y);

    /* a DXGI surface of the Direct2D device, drawn through a bitmap on it (green, bottom left) */
    hr = CALL(d2dnative, 4, HRESULT (WINAPI *)(obj *, const RECT *, REFIID, void **, POINT *))(d2dnative, &bottom_left,
            &IID_DXGISurface_p, (void **)&surface, &offset);
    if (surface)
    {
        D2D1_BITMAP_PROPERTIES1 props = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96,
                                          D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW };
        ID2D1Device_CreateDeviceContext(d2d, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &ctx);
        hr3 = ID2D1DeviceContext_CreateBitmapFromDxgiSurface(ctx, surface, &props, &bitmap);
        if (bitmap)
        {
            ID2D1DeviceContext_SetTarget(ctx, (void *)bitmap);
            ID2D1RenderTarget_BeginDraw((ID2D1RenderTarget *)ctx);
            ID2D1RenderTarget_Clear((ID2D1RenderTarget *)ctx, &green);
            ID2D1RenderTarget_EndDraw((ID2D1RenderTarget *)ctx, NULL, NULL);
        }
        printf("on_device=%08lx\n", hr3);
    }
    hr2 = CALL(d2dnative, 5, HRESULT (WINAPI *)(obj *))(d2dnative);
    printf("d2d_surface=%08lx,%08lx\n", hr, hr2);

    /* ISurfaceImageSourceNative with the Direct3D device: its surface (blue, right half) */
    nat = as(sis, &IID_SISNative);
    CALL(nat, 3, HRESULT (WINAPI *)(obj *, IUnknown *))(nat, (IUnknown *)dxgi);
    surface = NULL;
    hr = CALL(nat, 4, HRESULT (WINAPI *)(obj *, RECT, void **, POINT *))(nat, right, (void **)&surface, &offset);
    if (surface)
    {
        ID3D11Resource *resource = NULL;
        ID3D11RenderTargetView *view = NULL;
        float blue[4] = { 0, 0, 1, 1 };

        IDXGISurface_QueryInterface(surface, &IID_D3D11Resource_p, (void **)&resource);
        ID3D11Device_CreateRenderTargetView(d3d, resource, NULL, &view);
        if (view) ID3D11DeviceContext_ClearRenderTargetView(immediate, view, blue);
    }
    hr2 = CALL(nat, 5, HRESULT (WINAPI *)(obj *))(nat);
    printf("d3d_surface=%08lx,%08lx\n", hr, hr2);
    hr = CALL(nat, 5, HRESULT (WINAPI *)(obj *))(nat);
    printf("end_twice=%d\n", FAILED(hr));

    /* shown by an Image: 40x40, each surface pixel 10x10 */
    CALL(as(image, &IID_Image), 7, putobj_fn)(as(image, &IID_Image), sis);
    sf = factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", &IID_SourceFactory);
    ((create_fn)SLOT(sf, 6))(sf, NULL, &inner, &src);
    native = as(src, &IID_SourceNative);
    host = CreateWindowExW(0, L"STATIC", L"host", WS_POPUP | WS_VISIBLE, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(obj *, HWND))SLOT(native, 3))(native, host);
    ((HRESULT (WINAPI *)(obj *, HWND *))SLOT(native, 4))(native, &island);
    SetWindowPos(island, NULL, 0, 0, 200, 100, SWP_NOZORDER | SWP_SHOWWINDOW);
    ((putobj_fn)SLOT(src, 7))(src, as(root, &IID_UIE));
    pump(800); UpdateWindow(island); pump(300);
    hdc = GetDC(island);
    printf("shown=%s,%s,%s\n", colour(GetPixel(hdc, 5, 5)), colour(GetPixel(hdc, 5, 35)), colour(GetPixel(hdc, 35, 20)));
    ReleaseDC(island, hdc);
    printf("done=1\n");
    return 0;
}
