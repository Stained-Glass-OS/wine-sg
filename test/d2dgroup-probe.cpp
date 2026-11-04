/* Direct2D geometry groups filled and outlined (patch 6243): a group of a device context's
 * FillGeometry or DrawGeometry is drawn as its members' figures under the group's fill mode. */
#include <windows.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ID2D1Factory1 *factory;
static ID2D1DeviceContext *ctx;
static ID2D1Bitmap1 *target;

static DWORD pixel(UINT x, UINT y)
{
    D2D1_BITMAP_PROPERTIES1 p = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96,
                                  D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, NULL };
    ID2D1Bitmap1 *cpu = NULL;
    D2D1_MAPPED_RECT m;
    DWORD v = 0xdeadbeef;
    if (FAILED(ctx->CreateBitmap(D2D1::SizeU(100, 100), NULL, 0, &p, &cpu))) return v;
    cpu->CopyFromBitmap(NULL, target, NULL);
    if (SUCCEEDED(cpu->Map(D2D1_MAP_OPTIONS_READ, &m))) { v = ((DWORD *)(m.bits + y * m.pitch))[x]; cpu->Unmap(); }
    cpu->Release();
    return v;
}

static ID2D1Geometry *rect(float l, float t, float r, float b)
{
    ID2D1RectangleGeometry *g = NULL;
    factory->CreateRectangleGeometry(D2D1::RectF(l, t, r, b), &g);
    return g;
}

static ID2D1Geometry *path_square(float l, float t, float size)
{
    ID2D1PathGeometry *g = NULL;
    ID2D1GeometrySink *sink;
    factory->CreatePathGeometry(&g);
    g->Open(&sink);
    sink->BeginFigure(D2D1::Point2F(l, t), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(l + size, t));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(l + size, t), D2D1::Point2F(l + size, t + size),
                                        D2D1::Point2F(l + size, t + size)));
    sink->AddLine(D2D1::Point2F(l, t + size));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    sink->Release();
    return g;
}

static void draw(ID2D1Geometry *g, BOOL outline, ID2D1Brush *brush)
{
    ctx->BeginDraw();
    ctx->Clear(D2D1::ColorF(0, 0.0f));
    if (outline) ctx->DrawGeometry(g, brush, 4.0f);
    else ctx->FillGeometry(g, brush);
    ctx->EndDraw();
}

int main(void)
{
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi;
    ID2D1Device *device;
    ID2D1SolidColorBrush *red;
    ID2D1GeometryGroup *group;
    ID2D1Geometry *members[2], *single[1];
    D2D1_BITMAP_PROPERTIES1 p = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96, 96,
                                  D2D1_BITMAP_OPTIONS_TARGET, NULL };

    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                      D3D11_SDK_VERSION, &d3d, NULL, NULL);
    if (!d3d) D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                D3D11_SDK_VERSION, &d3d, NULL, NULL);
    if (!d3d) { printf("SKIP no d3d11\nRESULT: PASS\n"); return 0; }
    d3d->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), NULL, (void **)&factory);
    factory->CreateDevice(dxgi, &device);
    device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &ctx);
    ctx->CreateBitmap(D2D1::SizeU(100, 100), NULL, 0, &p, &target);
    ctx->SetTarget(target);
    ctx->CreateSolidColorBrush(D2D1::ColorF(1, 0, 0, 1), &red);

    /* one member, a path with a curve: filled, outlined */
    single[0] = path_square(10, 10, 40);
    factory->CreateGeometryGroup(D2D1_FILL_MODE_ALTERNATE, single, 1, &group);
    draw(group, FALSE, red);
    CHECK(pixel(30, 30) == 0xffff0000, "a one-path group filled: %08lx", pixel(30, 30));
    CHECK(pixel(70, 70) == 0, "outside the group untouched: %08lx", pixel(70, 70));
    draw(group, TRUE, red);
    CHECK(pixel(10, 30) == 0xffff0000, "a group outlined: %08lx", pixel(10, 30));
    CHECK(pixel(30, 30) == 0, "an outline is not a fill: %08lx", pixel(30, 30));
    draw(group, FALSE, red);   /* again: the same figures */
    CHECK(pixel(30, 30) == 0xffff0000, "a group filled twice: %08lx", pixel(30, 30));
    group->Release();

    /* two overlapping rectangles: the overlap is a hole under ALTERNATE, filled under WINDING */
    members[0] = rect(10, 10, 60, 60);
    members[1] = rect(40, 40, 90, 90);
    factory->CreateGeometryGroup(D2D1_FILL_MODE_ALTERNATE, members, 2, &group);
    draw(group, FALSE, red);
    CHECK(pixel(20, 20) == 0xffff0000 && pixel(80, 80) == 0xffff0000, "both members filled: %08lx %08lx",
          pixel(20, 20), pixel(80, 80));
    CHECK(pixel(50, 50) == 0, "alternate: the overlap is a hole: %08lx", pixel(50, 50));
    group->Release();
    factory->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, members, 2, &group);
    draw(group, FALSE, red);
    CHECK(pixel(50, 50) == 0xffff0000, "winding: the overlap is filled: %08lx", pixel(50, 50));
    group->Release();

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
