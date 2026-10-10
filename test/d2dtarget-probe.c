/* d2d1: results recorded from Windows by Wine's tests (patches/sg/2619): the access
 * rules of ID2D1Bitmap1::Map, pixel formats of bitmaps made from DXGI surfaces,
 * a closed command list stops being a target, GetSurface errors, the lock a WIC
 * render target holds while drawing and GetDC without a target. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const GUID my_iid_dxgidevice = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID my_iid_dxgisurface = {0xcafcb56c, 0x6ac3, 0x4889, {0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec}};
static const GUID my_clsid_wicfactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_iid_wicfactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID my_pf_pbgra = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10}};

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static ID3D11Device *d3d;
static ID2D1Factory1 *factory;
static ID2D1Device *device;

static ID3D11Texture2D *make_texture(UINT bind, UINT cpu, D3D11_USAGE usage)
{
    D3D11_TEXTURE2D_DESC desc = {0};
    ID3D11Texture2D *tex = NULL;

    desc.Width = 16; desc.Height = 16; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = usage;
    desc.BindFlags = bind; desc.CPUAccessFlags = cpu;
    ID3D11Device_CreateTexture2D(d3d, &desc, NULL, &tex);
    return tex;
}

static void test_map(ID2D1DeviceContext *dc)
{
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE}, 96.0f, 96.0f};
    D2D1_SIZE_U size = {4, 4};
    D2D1_MAPPED_RECT mapped;
    ID2D1Bitmap1 *bitmap = NULL;
    ID3D11Texture2D *tex;
    IDXGISurface *surface = NULL;
    HRESULT hr;

    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    hr = ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &props, &bitmap);
    check(hr == S_OK, "read-only bitmap (%#lx)", hr);
    if (!bitmap) return;

    hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_NONE, &mapped);
    check(hr == E_INVALIDARG, "map no options = %#lx", hr);
    hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_READ, &mapped);
    check(hr == S_OK, "map read = %#lx", hr);
    hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_READ, &mapped);
    check(hr == D2DERR_WRONG_STATE, "map read again = %#lx", hr);
    hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_WRITE, &mapped);
    check(hr == E_INVALIDARG, "write on a read-only bitmap while mapped = %#lx", hr);
    hr = ID2D1Bitmap1_Unmap(bitmap);
    check(hr == S_OK, "unmap = %#lx", hr);
    hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_WRITE, &mapped);
    check(hr == E_INVALIDARG, "write on a read-only bitmap = %#lx", hr);
    ID2D1Bitmap1_Release(bitmap);

    /* a surface the program made readable and writable */
    tex = make_texture(0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_STAGING);
    if (!tex) { check(0, "staging texture"); return; }
    ID3D11Texture2D_QueryInterface(tex, &my_iid_dxgisurface, (void **)&surface);
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    hr = ID2D1DeviceContext_CreateBitmapFromDxgiSurface(dc, surface, &props, &bitmap);
    check(hr == S_OK && bitmap, "staging bitmap (%#lx)", hr);
    if (bitmap)
    {
        hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_WRITE, &mapped);
        check(hr == S_OK, "write on a writable bitmap = %#lx", hr);
        ID2D1Bitmap1_Unmap(bitmap);
        hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_READ | D2D1_MAP_OPTIONS_WRITE, &mapped);
        check(hr == S_OK, "read+write = %#lx", hr);
        ID2D1Bitmap1_Unmap(bitmap);
        hr = ID2D1Bitmap1_Map(bitmap, D2D1_MAP_OPTIONS_WRITE | D2D1_MAP_OPTIONS_DISCARD, &mapped);
        check(hr == E_INVALIDARG, "discard on a staging bitmap = %#lx", hr);
        if (SUCCEEDED(hr)) ID2D1Bitmap1_Unmap(bitmap);
        ID2D1Bitmap1_Release(bitmap);
    }
    IDXGISurface_Release(surface);
    ID3D11Texture2D_Release(tex);
}

static void test_surface_formats(ID2D1DeviceContext *dc)
{
    static const struct { DXGI_FORMAT format; D2D1_ALPHA_MODE alpha; HRESULT hr; } tests[] =
    {
        {DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED, S_OK},
        {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, D2DERR_UNSUPPORTED_PIXEL_FORMAT},
        {DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_IGNORE, S_OK},
        {DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN, D2DERR_UNSUPPORTED_PIXEL_FORMAT},
        {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_UNKNOWN, D2DERR_UNSUPPORTED_PIXEL_FORMAT},
        {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, S_OK},
        {DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_STRAIGHT, D2DERR_UNSUPPORTED_PIXEL_FORMAT},
    };
    ID3D11Texture2D *tex = make_texture(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_USAGE_DEFAULT);
    IDXGISurface *surface = NULL;
    unsigned int i;

    ID3D11Texture2D_QueryInterface(tex, &my_iid_dxgisurface, (void **)&surface);
    for (i = 0; i < (sizeof(tests) / sizeof(tests[0])); ++i)
    {
        D2D1_BITMAP_PROPERTIES1 props = {{tests[i].format, tests[i].alpha}, 0, 0};
        ID2D1Bitmap1 *bitmap = (void *)0xdeadbeef;
        HRESULT hr;

        props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        hr = ID2D1DeviceContext_CreateBitmapFromDxgiSurface(dc, surface, &props, &bitmap);
        check(hr == tests[i].hr, "surface format %u = %#lx (expected %#lx)", i, hr, tests[i].hr);
        if (SUCCEEDED(hr)) ID2D1Bitmap1_Release(bitmap);
    }
    IDXGISurface_Release(surface);
    ID3D11Texture2D_Release(tex);
}

static void test_command_list_close(ID2D1DeviceContext *dc)
{
    ID2D1CommandList *list = NULL;
    ID2D1Image *target;
    HRESULT hr;

    hr = ID2D1DeviceContext_CreateCommandList(dc, &list);
    check(hr == S_OK && list, "command list (%#lx)", hr);
    if (!list) return;
    ID2D1DeviceContext_SetTarget(dc, (ID2D1Image *)list);
    ID2D1DeviceContext_GetTarget(dc, &target);
    check(target == (ID2D1Image *)list, "the list is the target");
    if (target) ID2D1Image_Release(target);
    hr = ID2D1CommandList_Close(list);
    check(hr == S_OK, "close = %#lx", hr);
    target = (void *)0xdeadbeef;
    ID2D1DeviceContext_GetTarget(dc, &target);
    check(target == NULL, "a closed list is no target (%p)", target);
    if (target && target != (void *)0xdeadbeef) ID2D1Image_Release(target);
    hr = ID2D1CommandList_Close(list);
    check(hr == D2DERR_WRONG_STATE, "close twice = %#lx", hr);
    ID2D1CommandList_Release(list);
}

static void test_wic(void)
{
    D2D1_RENDER_TARGET_PROPERTIES rt_desc = {D2D1_RENDER_TARGET_TYPE_DEFAULT, {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96.0f, 96.0f};
    IWICImagingFactory *wic = NULL;
    IWICBitmap *wic_bitmap = NULL;
    IWICBitmapLock *lock = NULL;
    ID2D1RenderTarget *rt = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1Bitmap1 *bitmap = NULL;
    IDXGISurface *surface;
    WICRect all = {0, 0, 16, 16};
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&my_clsid_wicfactory, NULL, CLSCTX_INPROC_SERVER, &my_iid_wicfactory, (void **)&wic);
    check(hr == S_OK, "WIC factory (%#lx)", hr);
    if (!wic) return;
    IWICImagingFactory_CreateBitmap(wic, 16, 16, &my_pf_pbgra, WICBitmapCacheOnDemand, &wic_bitmap);
    IWICImagingFactory_Release(wic);
    hr = ID2D1Factory1_CreateWicBitmapRenderTarget(factory, wic_bitmap, &rt_desc, &rt);
    check(hr == S_OK, "WIC render target (%#lx)", hr);
    if (!rt) return;

    ID2D1RenderTarget_QueryInterface(rt, &IID_ID2D1DeviceContext, (void **)&dc);
    ID2D1DeviceContext_GetTarget(dc, (ID2D1Image **)&bitmap);
    surface = (void *)0xdeadbeef;
    hr = ID2D1Bitmap1_GetSurface(bitmap, &surface);
    check(hr == E_FAIL, "the target of a WIC render target hides its surface = %#lx", hr);
    ID2D1Bitmap1_Release(bitmap);
    ID2D1DeviceContext_Release(dc);

    hr = IWICBitmap_Lock(wic_bitmap, &all, WICBitmapLockRead, &lock);
    check(hr == S_OK, "lock before drawing = %#lx", hr);
    if (lock) IWICBitmapLock_Release(lock);
    lock = NULL;

    ID2D1RenderTarget_BeginDraw(rt);
    hr = IWICBitmap_Lock(wic_bitmap, &all, WICBitmapLockRead, &lock);
    check(hr == WINCODEC_ERR_ALREADYLOCKED, "lock while drawing = %#lx", hr);
    if (SUCCEEDED(hr)) IWICBitmapLock_Release(lock);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    check(hr == S_OK, "end draw = %#lx", hr);

    hr = IWICBitmap_Lock(wic_bitmap, &all, WICBitmapLockRead, &lock);
    check(hr == S_OK, "lock after drawing = %#lx", hr);
    if (lock) IWICBitmapLock_Release(lock);

    hr = IWICBitmap_Lock(wic_bitmap, &all, WICBitmapLockRead, &lock);
    ID2D1RenderTarget_BeginDraw(rt);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    check(hr == WINCODEC_ERR_ALREADYLOCKED, "drawing to a locked bitmap = %#lx", hr);
    if (lock) IWICBitmapLock_Release(lock);

    ID2D1RenderTarget_BeginDraw(rt);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    check(hr == S_OK, "drawing after the lock is gone = %#lx", hr);

    ID2D1RenderTarget_Release(rt);
    IWICBitmap_Release(wic_bitmap);
}

static void test_dc_target(void)
{
    D2D1_RENDER_TARGET_PROPERTIES rt_desc = {D2D1_RENDER_TARGET_TYPE_DEFAULT, {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96.0f, 96.0f};
    ID2D1GdiInteropRenderTarget *interop = NULL;
    ID2D1DCRenderTarget *rt = NULL;
    HDC hdc;
    HRESULT hr;

    hr = ID2D1Factory1_CreateDCRenderTarget(factory, &rt_desc, &rt);
    check(hr == S_OK, "DC render target (%#lx)", hr);
    if (!rt) return;
    ID2D1DCRenderTarget_QueryInterface(rt, &IID_ID2D1GdiInteropRenderTarget, (void **)&interop);
    ID2D1DCRenderTarget_BeginDraw(rt);
    hdc = (void *)0xdeadbeef;
    hr = ID2D1GdiInteropRenderTarget_GetDC(interop, D2D1_DC_INITIALIZE_MODE_COPY, &hdc);
    check(hr == D2DERR_WRONG_STATE && !hdc, "GetDC without a bound DC = %#lx (%p)", hr, hdc);
    hr = ID2D1DCRenderTarget_EndDraw(rt, NULL, NULL);
    check(hr == D2DERR_WRONG_STATE, "EndDraw without a bound DC = %#lx", hr);
    ID2D1GdiInteropRenderTarget_Release(interop);
    ID2D1DCRenderTarget_Release(rt);
}

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    D2D1_CREATION_PROPERTIES cp = {0};
    ID2D1DeviceContext *dc = NULL;
    IDXGIDevice *dxgi = NULL;
    HRESULT hr;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
            D3D11_SDK_VERSION, &d3d, NULL, NULL);
    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
                D3D11_SDK_VERSION, &d3d, NULL, NULL);
    check(SUCCEEDED(hr), "D3D11CreateDevice");
    if (FAILED(hr)) goto done;
    ID3D11Device_QueryInterface(d3d, &my_iid_dxgidevice, (void **)&dxgi);
    cp.threadingMode = D2D1_THREADING_MODE_SINGLE_THREADED;
    hr = D2D1CreateDevice(dxgi, &cp, &device);
    check(hr == S_OK, "D2D1CreateDevice (%#lx)", hr);
    if (FAILED(hr)) goto done;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    check(hr == S_OK, "factory (%#lx)", hr);
    if (FAILED(hr)) goto done;
    ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);

    test_map(dc);
    test_surface_formats(dc);
    test_command_list_close(dc);
    test_wic();
    test_dc_target();

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
