/* Windows.UI.Xaml SwapChainPanel presenting (patches/sg/6178): a Direct3D 11 composition swap chain made
 * for a DirectComposition surface handle (IDXGIFactoryMedia::CreateSwapChainForCompositionSurfaceHandle,
 * as Windows Terminal's renderer makes it) given to a SwapChainPanel by ISwapChainPanelNative2::
 * SetSwapChainHandle, and one made by CreateSwapChainForComposition given by ISwapChainPanelNative::
 * SetSwapChain: what is presented shows in the panel's place in a hosted island, and again after the
 * island paints over it. Interface identity per windows-rs 0.30.0 (MIT); DXGI/D3D11 from mingw-w64. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <dxgi1_3.h>
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
DEFINE_GUID(IID_ReaderStatics,  0x9891c6bd, 0x534f, 0x4955, 0xb8, 0x5a, 0x8a, 0x8d, 0xc0, 0xdc, 0xa6, 0x02);
DEFINE_GUID(IID_PanelNative,    0xf92f19d2, 0x3ade, 0x45a6, 0xa2, 0x0c, 0xf6, 0xf1, 0xea, 0x90, 0x55, 0x4b);
DEFINE_GUID(IID_PanelNative2,   0xd5a2f60c, 0x37b2, 0x44a2, 0x93, 0x7b, 0x8d, 0x8e, 0xb9, 0x72, 0x68, 0x21);

HRESULT WINAPI DCompositionCreateSurfaceHandle(DWORD access, SECURITY_ATTRIBUTES *sa, HANDLE *handle);

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
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}
static BOOL close_to(COLORREF c, int r, int g, int b)
{
    return abs(GetRValue(c) - r) < 24 && abs(GetGValue(c) - g) < 24 && abs(GetBValue(c) - b) < 24;
}

static HWND island;
static COLORREF pixel(int x, int y)
{
    HDC hdc = GetDC(island);
    COLORREF c = GetPixel(hdc, x, y);
    ReleaseDC(island, hdc);
    return c;
}

#define NS L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""

static const WCHAR page_xaml[] =
    L"<Canvas " NS L" Background=\"White\" Width=\"300\" Height=\"200\">"
    L"<SwapChainPanel x:Name=\"ByHandle\" Canvas.Left=\"20\" Canvas.Top=\"20\" Width=\"64\" Height=\"64\"/>"
    L"<SwapChainPanel x:Name=\"ByChain\" Canvas.Left=\"150\" Canvas.Top=\"20\" Width=\"64\" Height=\"64\"/>"
    L"</Canvas>";

static ID3D11Device *device;
static ID3D11DeviceContext *context;

/* a frame of one colour presented */
static void present(IDXGISwapChain1 *chain, float r, float g, float b)
{
    ID3D11Texture2D *buffer = NULL;
    ID3D11RenderTargetView *rtv = NULL;
    float color[4] = { r, g, b, 1.0f };

    IDXGISwapChain1_GetBuffer(chain, 0, &IID_ID3D11Texture2D, (void **)&buffer);
    if (!buffer) return;
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)buffer, NULL, &rtv);
    if (rtv)
    {
        ID3D11DeviceContext_ClearRenderTargetView(context, rtv, color);
        ID3D11RenderTargetView_Release(rtv);
    }
    ID3D11Texture2D_Release(buffer);
    IDXGISwapChain1_Present(chain, 0, 0);
}

int main(void)
{
    obj *rs, *root = NULL, *ms, *mgr = NULL, *sf, *src = NULL, *native, *by_handle = NULL, *by_chain = NULL, *n;
    DXGI_SWAP_CHAIN_DESC1 desc = { 0 };
    IDXGISwapChain1 *chain1 = NULL, *chain2 = NULL, *none = NULL;
    IDXGIFactoryMedia *media = NULL;
    IDXGIFactory2 *dxgi = NULL;
    IInspectable *inner = NULL;
    HANDLE surface = NULL;
    HWND host;
    HRESULT hr, hr_none = E_FAIL, hr_set = E_FAIL, hr_set2 = E_FAIL;

    setvbuf(stdout, NULL, _IONBF, 0);
    RoInitialize(RO_INIT_SINGLETHREADED);

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                           D3D11_SDK_VERSION, &device, NULL, &context);
    if (FAILED(hr)) { printf("device=%08lx\n", hr); return 1; }
    CreateDXGIFactory2(0, &IID_IDXGIFactory2, (void **)&dxgi);
    if (dxgi) IDXGIFactory2_QueryInterface(dxgi, &IID_IDXGIFactoryMedia, (void **)&media);
    desc.Width = 64; desc.Height = 64; desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    DCompositionCreateSurfaceHandle(0x0003 /* COMPOSITIONOBJECT_ALL_ACCESS */, NULL, &surface);
    hr = E_NOINTERFACE;
    if (media)
    {
        hr = IDXGIFactoryMedia_CreateSwapChainForCompositionSurfaceHandle(media, (IUnknown *)device, surface, &desc, NULL, &chain1);
        hr_none = IDXGIFactoryMedia_CreateSwapChainForCompositionSurfaceHandle(media, (IUnknown *)device, NULL, &desc, NULL, &none);
    }
    printf("media=%d,%d,%08lx,%d\n", media != NULL, surface != NULL, hr, hr_none == E_INVALIDARG && !none);
    if (dxgi) IDXGIFactory2_CreateSwapChainForComposition(dxgi, (IUnknown *)device, &desc, NULL, &chain2);

    if (!(rs = factory(L"Windows.UI.Xaml.Markup.XamlReader", &IID_ReaderStatics))) return 1;
    hr = ((load_fn)SLOT(rs, 6))(rs, hs(page_xaml), &root);
    printf("load=%08lx\n", hr);
    if (!root) return 1;
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(L"ByHandle"), &by_handle);
    ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(as(root, &IID_FE), 51))(as(root, &IID_FE), hs(L"ByChain"), &by_chain);
    if ((n = as(by_handle, &IID_PanelNative2)) && surface)
        hr_set = ((HRESULT (WINAPI *)(obj *, HANDLE))SLOT(n, 4))(n, surface);
    if ((n = as(by_chain, &IID_PanelNative)) && chain2)
        hr_set2 = ((HRESULT (WINAPI *)(obj *, IDXGISwapChain1 *))SLOT(n, 3))(n, chain2);
    printf("set=%08lx,%08lx\n", hr_set, hr_set2);

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

    /* frames presented after the panels were laid out and shown */
    if (chain1) present(chain1, 1, 0, 0);
    if (chain2) present(chain2, 0, 0, 1);
    pump(300);
    printf("byhandle=%d,%d\n", close_to(pixel(50, 50), 255, 0, 0), close_to(pixel(110, 50), 255, 255, 255));
    printf("bychain=%d,%d\n", close_to(pixel(180, 50), 0, 0, 255), close_to(pixel(250, 50), 255, 255, 255));

    /* the island paints its white background over them: they are shown again after its paint */
    InvalidateRect(island, NULL, FALSE);
    UpdateWindow(island);
    printf("repaint=%d,%d\n", close_to(pixel(50, 50), 255, 0, 0), close_to(pixel(180, 50), 0, 0, 255));

    /* a new frame shows at once */
    if (chain1) present(chain1, 0, 1, 0);
    printf("next=%d\n", close_to(pixel(50, 50), 0, 255, 0));
    return 0;
}
