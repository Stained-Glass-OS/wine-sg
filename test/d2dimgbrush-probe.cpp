// d2dimgbrush-probe: Direct2D image brushes of any image and BlendImage in command lists (Stained Glass OS
// wine-sg 9504). An image brush paints the source rectangle of an effect's output, a command list or part
// of a bitmap (only whole bitmaps were painted; anything else drew nothing); a bitmap brush with a rotated
// transform maps a non-square bitmap right; a command list records BlendImage (was dropped). Prints
// name=1/0 lines with the pixels; see test/d2dimgbrush-gate.sh.
//
// SPDX-License-Identifier: LGPL-2.1-or-later
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A00000C
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effects.h>
#include <cstdio>
#include <cstdlib>

static ID2D1DeviceContext6 *dc;
static ID2D1Bitmap1 *target, *readback;

static const UINT32 RED = 0xffff0000, BLUE = 0xff0000ff, GREEN = 0xff00ff00, GRAY = 0xff808080;

/* a bitmap W x H, the left LEFT columns one colour, the rest another (0xAARRGGBB, opaque) */
static ID2D1Bitmap1 *split(UINT w, UINT h, UINT left, UINT32 a, UINT32 b)
{
    UINT32 *px = (UINT32 *)malloc(w * h * 4);
    for (UINT i = 0; i < w * h; i++) px[i] = (i % w) < left ? a : b;
    D2D1_BITMAP_PROPERTIES1 p = {};
    p.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    p.dpiX = p.dpiY = 96.0f;
    ID2D1Bitmap1 *bmp = NULL;
    dc->CreateBitmap(D2D1::SizeU(w, h), px, w * 4, &p, &bmp);
    free(px);
    return bmp;
}

static UINT32 pixel(int x, int y)
{
    D2D1_MAPPED_RECT m;
    D2D1_POINT_2U at = { 0, 0 };
    UINT32 v = 0xdeadbeef;
    dc->Flush(NULL, NULL);
    if (FAILED(readback->CopyFromBitmap(&at, target, NULL))) return v;
    if (SUCCEEDED(readback->Map(D2D1_MAP_OPTIONS_READ, &m)))
    {
        v = *(UINT32 *)(m.bits + y * m.pitch + x * 4);
        readback->Unmap();
    }
    return v;
}

static int close_to(UINT32 got, UINT32 want)
{
    for (int s = 0; s < 32; s += 8)
        if (abs((int)((got >> s) & 0xff) - (int)((want >> s) & 0xff)) > 3) return 0;
    return 1;
}

static void expect(const char *name, int x, int y, UINT32 want)
{
    UINT32 got = pixel(x, y);
    printf("%s=%d at %d,%d got %08x want %08x\n", name, close_to(got, want), x, y, got, want);
}

static void fill_with(ID2D1Image *image, const D2D1_RECT_F &source, D2D1_EXTEND_MODE extend, const D2D1_RECT_F &area)
{
    ID2D1ImageBrush *brush = NULL;
    D2D1_IMAGE_BRUSH_PROPERTIES props = { source, extend, extend, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR };
    dc->CreateImageBrush(image, &props, NULL, &brush);
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0, 1.0f));
    if (brush) dc->FillRectangle(area, brush);
    dc->EndDraw();
    if (brush) brush->Release();
}

int main()
{
    ID3D11Device *d3d;
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                 D3D11_SDK_VERSION, &d3d, NULL, NULL))) { printf("no_d3d=1\n"); return 1; }
    IDXGIDevice *dxgi; d3d->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi);
    ID2D1Factory1 *factory;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), NULL, (void **)&factory)))
        { printf("no_factory=1\n"); return 1; }
    ID2D1Device *dev; factory->CreateDevice(dxgi, &dev);
    ID2D1DeviceContext *dc0; dev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc0);
    if (FAILED(dc0->QueryInterface(__uuidof(ID2D1DeviceContext6), (void **)&dc))) { printf("no_dc6=1\n"); return 1; }
    D2D1_BITMAP_PROPERTIES1 p = {};
    p.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    p.dpiX = p.dpiY = 96.0f;
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    dc->CreateBitmap(D2D1::SizeU(64, 64), NULL, 0, &p, &target);
    p.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    dc->CreateBitmap(D2D1::SizeU(64, 64), NULL, 0, &p, &readback);
    if (!target || !readback) { printf("no_bitmaps=1\n"); return 1; }
    dc->SetTarget(target);

    /* an effect's output: a green flood, its 8 x 8 corner tiled */
    {
        ID2D1Effect *flood = NULL;
        ID2D1Image *out = NULL;
        D2D1_VECTOR_4F green = { 0.0f, 1.0f, 0.0f, 1.0f };
        dc->CreateEffect(CLSID_D2D1Flood, &flood);
        if (flood)
        {
            flood->SetValue(D2D1_FLOOD_PROP_COLOR, green);
            flood->GetOutput(&out);
        }
        fill_with(out, D2D1::RectF(0, 0, 8, 8), D2D1_EXTEND_MODE_WRAP, D2D1::RectF(0, 0, 64, 64));
        expect("effect_brush", 40, 40, GREEN);
        if (out) out->Release();
        if (flood) flood->Release();
    }

    /* a command list: red 0-4 and blue 4-8 across, 4 high, tiled */
    {
        ID2D1CommandList *list = NULL;
        ID2D1SolidColorBrush *red = NULL, *blue = NULL;
        dc->CreateCommandList(&list);
        dc->SetTarget(list);
        dc->CreateSolidColorBrush(D2D1::ColorF(1, 0, 0, 1), &red);
        dc->CreateSolidColorBrush(D2D1::ColorF(0, 0, 1, 1), &blue);
        dc->BeginDraw();
        dc->FillRectangle(D2D1::RectF(0, 0, 4, 4), red);
        dc->FillRectangle(D2D1::RectF(4, 0, 8, 4), blue);
        dc->EndDraw();
        list->Close();
        dc->SetTarget(target);
        fill_with(list, D2D1::RectF(0, 0, 8, 4), D2D1_EXTEND_MODE_WRAP, D2D1::RectF(0, 0, 64, 64));
        expect("list_brush_red", 1, 1, RED);
        expect("list_brush_blue", 5, 1, BLUE);
        expect("list_brush_wrap_x", 9, 1, RED);
        expect("list_brush_wrap_y", 5, 6, BLUE);
        red->Release(); blue->Release(); list->Release();
    }

    /* part of a bitmap: the right half (blue) of a red | blue bitmap, clamped: blue left of it too */
    {
        ID2D1Bitmap1 *rb = split(16, 16, 8, RED, BLUE);
        fill_with(rb, D2D1::RectF(8, 0, 16, 16), D2D1_EXTEND_MODE_CLAMP, D2D1::RectF(0, 0, 32, 16));
        expect("bitmap_part_inside", 12, 4, BLUE);
        expect("bitmap_part_clamped", 4, 4, BLUE);
        /* the whole bitmap still paints both halves */
        fill_with(rb, D2D1::RectF(0, 0, 16, 16), D2D1_EXTEND_MODE_CLAMP, D2D1::RectF(0, 0, 32, 16));
        expect("bitmap_whole_left", 4, 4, RED);
        expect("bitmap_whole_right", 12, 4, BLUE);
        rb->Release();
    }

    /* a bitmap brush, 16 x 4 (red | blue at 8), turned 90 degrees and moved to (30,10): bitmap x runs down
     * the screen from y 10 to 26, bitmap y leftwards from x 30 to 26 */
    {
        ID2D1Bitmap1 *wide = split(16, 4, 8, RED, BLUE);
        ID2D1BitmapBrush *brush = NULL;
        dc->CreateBitmapBrush(wide, &brush);
        if (brush)
        {
            brush->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
            brush->SetTransform(D2D1::Matrix3x2F(0, 1, -1, 0, 30, 10));
            dc->BeginDraw();
            dc->Clear(D2D1::ColorF(0, 1.0f));
            dc->FillRectangle(D2D1::RectF(26, 10, 30, 26), brush);
            dc->EndDraw();
            brush->Release();
        }
        expect("rotated_bitmap_red", 28, 13, RED);
        expect("rotated_bitmap_blue", 28, 23, BLUE);
        wide->Release();
    }

    /* BlendImage recorded in a command list: multiply red into gray when the list is drawn */
    {
        ID2D1CommandList *list = NULL;
        ID2D1SolidColorBrush *gray = NULL;
        ID2D1Bitmap1 *red = split(64, 64, 64, RED, RED);
        dc->CreateCommandList(&list);
        dc->SetTarget(list);
        dc->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.5f, 1), &gray);
        dc->BeginDraw();
        dc->FillRectangle(D2D1::RectF(0, 0, 64, 64), gray);
        D2D1_POINT_2F at = { 0, 0 };
        D2D1_RECT_F part = { 0, 0, 32, 64 };
        dc->BlendImage(red, D2D1_BLEND_MODE_MULTIPLY, &at, &part, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        dc->EndDraw();
        list->Close();
        dc->SetTarget(target);
        dc->BeginDraw();
        dc->Clear(D2D1::ColorF(0, 1.0f));
        dc->DrawImage(list);
        dc->EndDraw();
        expect("list_blendimage_inside", 16, 16, 0xff800000);
        expect("list_blendimage_outside", 48, 16, GRAY);
        gray->Release(); red->Release(); list->Release();
    }
    return 0;
}
