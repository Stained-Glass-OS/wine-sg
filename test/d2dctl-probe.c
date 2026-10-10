/* d2d1: device settings (texture and glyph cache memory, rendering priority,
 * ClearResources, FlushDeviceContexts), the rendering controls of a device
 * context, and ID2D1Bitmap1::CopyFromRenderTarget (patches/sg/2618). */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const GUID my_iid_dxgidevice = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID my_iid_dxgisurface = {0xcafcb56c, 0x6ac3, 0x4889, {0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec}};

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
static ID2D1Device *device;

static void test_device(void)
{
    UINT64 v;

    v = ID2D1Device_GetMaximumTextureMemory(device);
    check(v == 64 * 1024 * 1024, "default texture memory %I64u", v);
    ID2D1Device_SetMaximumTextureMemory(device, 12345678);
    v = ID2D1Device_GetMaximumTextureMemory(device);
    check(v == 12345678, "texture memory is kept (%I64u)", v);
    ID2D1Device_SetMaximumTextureMemory(device, 0);
    check(ID2D1Device_GetMaximumTextureMemory(device) == 0, "zero is a value too");

    ID2D1Device_ClearResources(device, 0);
    ID2D1Device_ClearResources(device, 5000);
    check(1, "ClearResources is callable");
}

static void test_device1(void)
{
    ID2D1Device1 *d1 = NULL;
    D2D1_RENDERING_PRIORITY prio;
    HRESULT hr = ID2D1Device_QueryInterface(device, &IID_ID2D1Device1, (void **)&d1);

    check(hr == S_OK && d1, "ID2D1Device1 (%#lx)", hr);
    if (!d1) return;

    prio = ID2D1Device1_GetRenderingPriority(d1);
    check(prio == D2D1_RENDERING_PRIORITY_NORMAL, "default priority normal (%d)", prio);
    ID2D1Device1_SetRenderingPriority(d1, D2D1_RENDERING_PRIORITY_LOW);
    prio = ID2D1Device1_GetRenderingPriority(d1);
    check(prio == D2D1_RENDERING_PRIORITY_LOW, "low priority is kept (%d)", prio);
    ID2D1Device1_SetRenderingPriority(d1, (D2D1_RENDERING_PRIORITY)77);
    prio = ID2D1Device1_GetRenderingPriority(d1);
    check(prio == D2D1_RENDERING_PRIORITY_LOW, "an unknown priority is ignored (%d)", prio);
    ID2D1Device1_SetRenderingPriority(d1, D2D1_RENDERING_PRIORITY_NORMAL);
    check(ID2D1Device1_GetRenderingPriority(d1) == D2D1_RENDERING_PRIORITY_NORMAL, "and back to normal");

    ID2D1Device1_Release(d1);
}

static void test_device4(void)
{
    ID2D1Device4 *d4 = NULL;
    HRESULT hr = ID2D1Device_QueryInterface(device, &IID_ID2D1Device4, (void **)&d4);
    UINT64 v;

    if (FAILED(hr) || !d4) { printf("note: no ID2D1Device4 (%#lx)\n", hr); return; }
    v = ID2D1Device4_GetMaximumColorGlyphCacheMemory(d4);
    check(v == 4 * 1024 * 1024, "default colour glyph cache %I64u", v);
    ID2D1Device4_SetMaximumColorGlyphCacheMemory(d4, 777);
    v = ID2D1Device4_GetMaximumColorGlyphCacheMemory(d4);
    check(v == 777, "colour glyph cache is kept (%I64u)", v);
    ID2D1Device4_Release(d4);
}

static ID3D11Texture2D *make_texture(UINT bind)
{
    D3D11_TEXTURE2D_DESC desc = {0};
    ID3D11Texture2D *tex = NULL;

    desc.Width = 64; desc.Height = 64; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bind;
    ID3D11Device_CreateTexture2D(d3d, &desc, NULL, &tex);
    return tex;
}

static void test_context(void)
{
    ID2D1DeviceContext *dc = NULL;
    ID2D1Bitmap1 *src_bmp = NULL, *dst_bmp = NULL, *cpu_bmp = NULL;
    ID2D1SolidColorBrush *brush = NULL;
    ID3D11Texture2D *tex;
    IDXGISurface *surface = NULL;
    D2D1_RENDERING_CONTROLS rc, set;
    D2D1_BITMAP_PROPERTIES1 props;
    D2D1_COLOR_F red = {1, 0, 0, 1}, blue = {0, 0, 1, 1};
    D2D1_POINT_2U point = {5, 7};
    D2D1_RECT_U rect = {0, 0, 20, 20}, bad_rect = {10, 10, 5, 5}, big_rect = {0, 0, 200, 200};
    D2D1_MAPPED_RECT mapped;
    D2D1_RECT_F fill = {10.0f, 10.0f, 30.0f, 30.0f};
    HRESULT hr;
    BYTE *p;

    hr = ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);
    check(hr == S_OK && dc, "device context (%#lx)", hr);
    if (!dc) return;

    memset(&rc, 0xcc, sizeof(rc));
    ID2D1DeviceContext_GetRenderingControls(dc, &rc);
    check(rc.bufferPrecision == D2D1_BUFFER_PRECISION_UNKNOWN && rc.tileSize.width == 512 && rc.tileSize.height == 512,
          "default rendering controls (%d, %ux%u)", rc.bufferPrecision, rc.tileSize.width, rc.tileSize.height);
    set.bufferPrecision = D2D1_BUFFER_PRECISION_16BPC_FLOAT;
    set.tileSize.width = 128;
    set.tileSize.height = 256;
    ID2D1DeviceContext_SetRenderingControls(dc, &set);
    memset(&rc, 0, sizeof(rc));
    ID2D1DeviceContext_GetRenderingControls(dc, &rc);
    check(rc.bufferPrecision == D2D1_BUFFER_PRECISION_16BPC_FLOAT && rc.tileSize.width == 128 && rc.tileSize.height == 256,
          "rendering controls are kept (%d, %ux%u)", rc.bufferPrecision, rc.tileSize.width, rc.tileSize.height);
    set.bufferPrecision = (D2D1_BUFFER_PRECISION)99;
    set.tileSize.width = 1;
    ID2D1DeviceContext_SetRenderingControls(dc, &set);
    ID2D1DeviceContext_GetRenderingControls(dc, &rc);
    check(rc.bufferPrecision == D2D1_BUFFER_PRECISION_16BPC_FLOAT && rc.tileSize.width == 128, "an unknown precision changes nothing (%d)", rc.bufferPrecision);

    /* the render target: a bitmap that is drawn to */
    tex = make_texture(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11Texture2D_QueryInterface(tex, &my_iid_dxgisurface, (void **)&surface);
    memset(&props, 0, sizeof(props));
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX = props.dpiY = 96.0f;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmapFromDxgiSurface(dc, surface, &props, &src_bmp);
    check(hr == S_OK && src_bmp, "target bitmap (%#lx)", hr);
    ID2D1DeviceContext_SetTarget(dc, (ID2D1Image *)src_bmp);
    ID2D1DeviceContext_BeginDraw(dc);
    ID2D1DeviceContext_Clear(dc, &red);
    ID2D1DeviceContext_CreateSolidColorBrush(dc, &blue, NULL, &brush);
    ID2D1DeviceContext_FillRectangle(dc, &fill, (ID2D1Brush *)brush);
    hr = ID2D1DeviceContext_EndDraw(dc, NULL, NULL);
    check(hr == S_OK, "drawing = %#lx", hr);

    /* a bitmap to copy into that can be read */
    {
        D2D1_SIZE_U size = {64, 64};

        memset(&props, 0, sizeof(props));
        props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        props.dpiX = props.dpiY = 96.0f;
        props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        hr = ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &props, &cpu_bmp);
        check(hr == S_OK && cpu_bmp, "bitmap to read (%#lx)", hr);
        (void)dst_bmp;
    }

    hr = ID2D1Bitmap1_CopyFromRenderTarget(cpu_bmp, &point, (ID2D1RenderTarget *)dc, &rect);
    check(hr == S_OK, "CopyFromRenderTarget = %#lx", hr);
    hr = ID2D1Bitmap1_Map(cpu_bmp, D2D1_MAP_OPTIONS_READ, &mapped);
    check(hr == S_OK, "Map = %#lx", hr);
    if (hr == S_OK)
    {
        /* source (10,10) is blue and lands at (15,17); source (0,0) is red and lands at (5,7); outside stays unwritten */
        p = mapped.bits + 17 * mapped.pitch + 15 * 4;
        check(p[0] == 255 && p[1] == 0 && p[2] == 0 && p[3] == 255, "the blue square is where it should be (%u,%u,%u,%u)", p[0], p[1], p[2], p[3]);
        p = mapped.bits + 7 * mapped.pitch + 5 * 4;
        check(p[0] == 0 && p[1] == 0 && p[2] == 255 && p[3] == 255, "the red corner (%u,%u,%u,%u)", p[0], p[1], p[2], p[3]);
        p = mapped.bits + 6 * mapped.pitch + 4 * 4;
        check(p[3] == 0, "and nothing before it (%u)", p[3]);
        ID2D1Bitmap1_Unmap(cpu_bmp);
    }

    hr = ID2D1Bitmap1_CopyFromRenderTarget(cpu_bmp, NULL, (ID2D1RenderTarget *)dc, NULL);
    check(hr == S_OK, "CopyFromRenderTarget of all of it = %#lx", hr);
    hr = ID2D1Bitmap1_CopyFromRenderTarget(cpu_bmp, &point, (ID2D1RenderTarget *)dc, &bad_rect);
    check(hr == E_INVALIDARG, "an empty/inverted source rectangle = %#lx", hr);
    hr = ID2D1Bitmap1_CopyFromRenderTarget(cpu_bmp, &point, (ID2D1RenderTarget *)dc, &big_rect);
    check(hr == E_INVALIDARG, "a source rectangle outside the target = %#lx", hr);
    hr = ID2D1Bitmap1_CopyFromRenderTarget(cpu_bmp, &point, NULL, &rect);
    check(hr == E_INVALIDARG, "no render target = %#lx", hr);

    /* flush */
    ID2D1Device_QueryInterface(device, &IID_ID2D1Device1, (void **)&dst_bmp); /* reused as scratch below */
    if (dst_bmp) { ID2D1Device1_Release((ID2D1Device1 *)dst_bmp); dst_bmp = NULL; }

    if (brush) ID2D1SolidColorBrush_Release(brush);
    if (cpu_bmp) ID2D1Bitmap1_Release(cpu_bmp);
    ID2D1DeviceContext_SetTarget(dc, NULL);
    if (src_bmp) ID2D1Bitmap1_Release(src_bmp);
    IDXGISurface_Release(surface);
    ID3D11Texture2D_Release(tex);
    ID2D1DeviceContext_Release(dc);
}

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    IDXGIDevice *dxgi = NULL;
    D2D1_CREATION_PROPERTIES cp = {0};
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
    check(hr == S_OK && device, "D2D1CreateDevice (%#lx)", hr);
    if (FAILED(hr)) goto done;

    test_device();
    test_device1();
    test_device4();
    test_context();

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
