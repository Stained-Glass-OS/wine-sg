/* d2d1: Tessellate of the geometries, meshes, FillMesh, FillOpacityMask and the text of
 * ID2D1DeviceContext4 (patches/sg/2630), on a bitmap target of a Direct3D 11 device. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

static const GUID my_iid_dxgidevice = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID my_iid_dxgisurface = {0xcafcb56c, 0x6ac3, 0x4889, {0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec}};
static const GUID my_iid_dwfactory = {0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};

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
static ID2D1DeviceContext *dc;
static ID2D1Bitmap1 *target, *readable;
#define SIZE 32

/* a sink that adds up what is given to it */
struct collector
{
    ID2D1TessellationSink iface;
    double area;
    UINT triangles;
    BOOL closed;
    float min_x, min_y, max_x, max_y;
};

static HRESULT WINAPI col_QI(ID2D1TessellationSink *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI col_AddRef(ID2D1TessellationSink *iface) { return 2; }
static ULONG WINAPI col_Release(ID2D1TessellationSink *iface) { return 1; }
static void WINAPI col_AddTriangles(ID2D1TessellationSink *iface, const D2D1_TRIANGLE *t, UINT32 count)
{
    struct collector *c = (struct collector *)iface;
    UINT32 i;

    for (i = 0; i < count; ++i)
    {
        c->area += fabs((t[i].point2.x - t[i].point1.x) * (t[i].point3.y - t[i].point1.y)
                - (t[i].point3.x - t[i].point1.x) * (t[i].point2.y - t[i].point1.y)) / 2.0;
        c->min_x = min(c->min_x, min(t[i].point1.x, min(t[i].point2.x, t[i].point3.x)));
        c->min_y = min(c->min_y, min(t[i].point1.y, min(t[i].point2.y, t[i].point3.y)));
        c->max_x = max(c->max_x, max(t[i].point1.x, max(t[i].point2.x, t[i].point3.x)));
        c->max_y = max(c->max_y, max(t[i].point1.y, max(t[i].point2.y, t[i].point3.y)));
    }
    c->triangles += count;
}
static HRESULT WINAPI col_Close(ID2D1TessellationSink *iface) { ((struct collector *)iface)->closed = TRUE; return S_OK; }
static const ID2D1TessellationSinkVtbl col_vtbl = { col_QI, col_AddRef, col_Release, col_AddTriangles, col_Close };

static void collector_init(struct collector *c)
{
    memset(c, 0, sizeof(*c));
    c->iface.lpVtbl = &col_vtbl;
    c->min_x = c->min_y = 1e9f;
    c->max_x = c->max_y = -1e9f;
}

static UINT pixel(UINT x, UINT y)
{
    D2D1_MAPPED_RECT mapped;
    D2D1_POINT_2U point = {0, 0};
    UINT value = 0xdeadbeef;

    ID2D1Bitmap1_CopyFromBitmap(readable, &point, (ID2D1Bitmap *)target, NULL);
    if (SUCCEEDED(ID2D1Bitmap1_Map(readable, D2D1_MAP_OPTIONS_READ, &mapped)))
    {
        value = *(UINT *)(mapped.bits + y * mapped.pitch + x * 4);
        ID2D1Bitmap1_Unmap(readable);
    }
    return value;
}

static void test_tessellate(void)
{
    ID2D1RectangleGeometry *rect;
    ID2D1EllipseGeometry *ellipse;
    ID2D1PathGeometry1 *path;
    ID2D1GeometrySink *sink;
    D2D1_RECT_F r = {10.0f, 10.0f, 30.0f, 20.0f};
    D2D1_ELLIPSE e = {{20.0f, 20.0f}, 10.0f, 5.0f};
    D2D1_MATRIX_3X2_F scale = {{{2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f}}};
    struct collector c;
    HRESULT hr;

    ID2D1Factory1_CreateRectangleGeometry(factory, &r, &rect);
    collector_init(&c);
    hr = ID2D1RectangleGeometry_Tessellate(rect, NULL, 0.0f, &c.iface);
    check(hr == S_OK, "rectangle Tessellate = %#lx", hr);
    check(fabs(c.area - 200.0) < 0.01 && c.triangles >= 2, "rectangle: %u triangles of area %.3f", c.triangles, c.area);
    check(c.min_x == 10.0f && c.max_x == 30.0f && c.min_y == 10.0f && c.max_y == 20.0f, "inside the rectangle");
    check(!c.closed, "the sink is left open");

    collector_init(&c);
    hr = ID2D1RectangleGeometry_Tessellate(rect, &scale, 0.0f, &c.iface);
    check(hr == S_OK && fabs(c.area - 800.0) < 0.01, "scaled by 2: area %.3f", c.area);
    hr = ID2D1RectangleGeometry_Tessellate(rect, NULL, 0.0f, NULL);
    check(hr == E_INVALIDARG, "no sink = %#lx", hr);
    ID2D1RectangleGeometry_Release(rect);

    ID2D1Factory1_CreateEllipseGeometry(factory, &e, &ellipse);
    collector_init(&c);
    hr = ID2D1EllipseGeometry_Tessellate(ellipse, NULL, 0.1f, &c.iface);
    check(hr == S_OK && c.area > 150.0 && c.area < 157.2, "ellipse: area %.3f of %.3f", c.area, 3.14159265 * 50.0);
    ID2D1EllipseGeometry_Release(ellipse);

    /* an L, with a hole next to it */
    ID2D1Factory1_CreatePathGeometry(factory, &path);
    ID2D1PathGeometry1_Open(path, &sink);
    {
        D2D1_POINT_2F l[5] = {{10, 0}, {10, 10}, {5, 10}, {5, 5}, {0, 5}};
        D2D1_POINT_2F start = {0, 0};

        ID2D1GeometrySink_BeginFigure(sink, start, D2D1_FIGURE_BEGIN_FILLED);
        ID2D1GeometrySink_AddLines(sink, l, 5);
        ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
    }
    ID2D1GeometrySink_Close(sink);
    ID2D1GeometrySink_Release(sink);
    collector_init(&c);
    hr = ID2D1PathGeometry1_Tessellate(path, NULL, 0.0f, &c.iface);
    check(hr == S_OK && fabs(c.area - 75.0) < 0.01, "path: area %.3f (an L of 75)", c.area);
    ID2D1PathGeometry1_Release(path);
}

static void test_mesh(void)
{
    static const D2D1_TRIANGLE tri = {{4.0f, 4.0f}, {28.0f, 4.0f}, {4.0f, 28.0f}};
    ID2D1SolidColorBrush *brush = NULL;
    ID2D1TessellationSink *sink = NULL, *sink2 = NULL;
    ID2D1Mesh *mesh = NULL;
    D2D1_COLOR_F red = {1, 0, 0, 1}, black = {0, 0, 0, 1};
    HRESULT hr;

    hr = ID2D1DeviceContext_CreateMesh(dc, &mesh);
    check(hr == S_OK && mesh, "mesh (%#lx)", hr);
    if (!mesh) return;
    hr = ID2D1Mesh_Open(mesh, &sink);
    check(hr == S_OK && sink, "Open (%#lx)", hr);
    hr = ID2D1Mesh_Open(mesh, &sink2);
    check(hr == D2DERR_WRONG_STATE, "Open twice = %#lx", hr);
    ID2D1TessellationSink_AddTriangles(sink, &tri, 1);
    hr = ID2D1TessellationSink_Close(sink);
    check(hr == S_OK, "Close = %#lx", hr);
    hr = ID2D1TessellationSink_Close(sink);
    check(hr == D2DERR_WRONG_STATE, "Close twice = %#lx", hr);
    ID2D1TessellationSink_Release(sink);
    hr = ID2D1Mesh_Open(mesh, &sink2);
    check(hr == D2DERR_WRONG_STATE, "Open after Close = %#lx", hr);

    ID2D1DeviceContext_CreateSolidColorBrush(dc, &red, NULL, &brush);
    ID2D1DeviceContext_SetTarget(dc, (ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc, D2D1_ANTIALIAS_MODE_ALIASED);
    ID2D1DeviceContext_BeginDraw(dc);
    ID2D1DeviceContext_Clear(dc, &black);
    ID2D1DeviceContext_FillMesh(dc, mesh, (ID2D1Brush *)brush);
    hr = ID2D1DeviceContext_EndDraw(dc, NULL, NULL);
    check(hr == S_OK, "drawing = %#lx", hr);
    check((pixel(8, 8) & 0xffffff) == 0xff0000, "inside the triangle: red (%#x)", pixel(8, 8));
    check((pixel(24, 24) & 0xffffff) == 0, "beyond its hypotenuse: black (%#x)", pixel(24, 24));
    check((pixel(2, 2) & 0xffffff) == 0, "outside it: black (%#x)", pixel(2, 2));

    ID2D1SolidColorBrush_Release(brush);
    ID2D1Mesh_Release(mesh);
}

static void test_opacity_mask(void)
{
    ID2D1SolidColorBrush *brush = NULL;
    ID2D1Bitmap *mask = NULL;
    D2D1_COLOR_F green = {0, 1, 0, 1}, black = {0, 0, 0, 1};
    D2D1_BITMAP_PROPERTIES props = {{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96.0f, 96.0f};
    D2D1_SIZE_U size = {2, 2};
    D2D1_RECT_F dst = {0.0f, 0.0f, 32.0f, 32.0f};
    UINT data[4] = {0xff000000 | 0x00ff00, 0, 0xff000000 | 0x00ff00, 0}; /* left column opaque, right transparent */
    HRESULT hr;

    hr = ID2D1RenderTarget_CreateBitmap((ID2D1RenderTarget *)dc, size, data, 8, &props, &mask);
    check(hr == S_OK && mask, "mask bitmap (%#lx)", hr);
    if (!mask) return;
    ID2D1DeviceContext_CreateSolidColorBrush(dc, &green, NULL, &brush);
    ID2D1DeviceContext_SetTarget(dc, (ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc, D2D1_ANTIALIAS_MODE_ALIASED);
    ID2D1DeviceContext_BeginDraw(dc);
    ID2D1DeviceContext_Clear(dc, &black);
    ID2D1DeviceContext_FillOpacityMask(dc, mask, (ID2D1Brush *)brush, &dst, NULL);
    hr = ID2D1DeviceContext_EndDraw(dc, NULL, NULL);
    check(hr == S_OK, "drawing = %#lx", hr);
    check((pixel(4, 16) & 0xffffff) == 0x00ff00, "where the mask is opaque: green (%#x)", pixel(4, 16));
    check((pixel(28, 16) & 0xffffff) == 0, "where it is transparent: black (%#x)", pixel(28, 16));

    ID2D1SolidColorBrush_Release(brush);
    ID2D1Bitmap_Release(mask);
}

static void test_text(void)
{
    ID2D1DeviceContext4 *dc4 = NULL;
    IDWriteFactory *dwrite = NULL;
    IDWriteTextFormat *format = NULL;
    IDWriteTextLayout *layout = NULL;
    ID2D1SolidColorBrush *brush = NULL;
    D2D1_COLOR_F white = {1, 1, 1, 1}, black = {0, 0, 0, 1};
    D2D1_POINT_2F origin = {0.0f, 0.0f};
    D2D1_RECT_F rect = {0.0f, 0.0f, 32.0f, 32.0f};
    unsigned int x, y, lit = 0;
    HRESULT hr;

    if (FAILED(ID2D1DeviceContext_QueryInterface(dc, &IID_ID2D1DeviceContext4, (void **)&dc4)))
    {
        printf("note: no ID2D1DeviceContext4\n");
        return;
    }
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &my_iid_dwfactory, (IUnknown **)&dwrite);
    hr = IDWriteFactory_CreateTextFormat(dwrite, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 28.0f, L"en-us", &format);
    check(hr == S_OK, "text format (%#lx)", hr);
    IDWriteFactory_CreateTextLayout(dwrite, L"MW", 2, format, 32.0f, 32.0f, &layout);
    ID2D1DeviceContext_CreateSolidColorBrush(dc, &white, NULL, &brush);
    ID2D1DeviceContext_SetTarget(dc, (ID2D1Image *)target);
    ID2D1DeviceContext_SetAntialiasMode(dc, D2D1_ANTIALIAS_MODE_ALIASED);
    ID2D1DeviceContext_BeginDraw(dc);
    ID2D1DeviceContext_Clear(dc, &black);
    ID2D1DeviceContext4_DrawTextLayout(dc4, origin, layout, (ID2D1Brush *)brush, NULL, 0, D2D1_DRAW_TEXT_OPTIONS_NONE);
    hr = ID2D1DeviceContext_EndDraw(dc, NULL, NULL);
    check(hr == S_OK, "DrawTextLayout of the device context 4 (%#lx)", hr);
    for (y = 0; y < SIZE; ++y)
        for (x = 0; x < SIZE; ++x)
            if ((pixel(x, y) & 0xffffff) != 0) ++lit;
    check(lit > 20, "text was drawn (%u pixels)", lit);

    ID2D1DeviceContext_BeginDraw(dc);
    ID2D1DeviceContext_Clear(dc, &black);
    ID2D1DeviceContext4_DrawText(dc4, L"MW", 2, format, &rect, (ID2D1Brush *)brush, NULL, 0, D2D1_DRAW_TEXT_OPTIONS_NONE,
            DWRITE_MEASURING_MODE_NATURAL);
    hr = ID2D1DeviceContext_EndDraw(dc, NULL, NULL);
    lit = 0;
    for (y = 0; y < SIZE; ++y)
        for (x = 0; x < SIZE; ++x)
            if ((pixel(x, y) & 0xffffff) != 0) ++lit;
    check(hr == S_OK && lit > 20, "and DrawText (%u pixels, %#lx)", lit, hr);

    ID2D1SolidColorBrush_Release(brush);
    IDWriteTextLayout_Release(layout);
    IDWriteTextFormat_Release(format);
    IDWriteFactory_Release(dwrite);
    ID2D1DeviceContext4_Release(dc4);
}

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    D2D1_CREATION_PROPERTIES cp = {0};
    D2D1_BITMAP_PROPERTIES1 props = {{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96.0f, 96.0f};
    D2D1_SIZE_U size = {SIZE, SIZE};
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
    ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);

    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    hr = ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &props, &target);
    check(hr == S_OK && target, "target bitmap (%#lx)", hr);
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    hr = ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &props, &readable);
    check(hr == S_OK && readable, "readable bitmap (%#lx)", hr);

    test_tessellate();
    test_mesh();
    test_opacity_mask();
    test_text();
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
