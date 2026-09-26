/* d2dlayer-probe: Direct2D layers, primitive blends and DrawImage's
 * composite modes (patches/sg/0340).
 *
 * Draws on a 64x64 WIC bitmap render target (premultiplied BGRA) and reads
 * the pixels back:
 *
 *  - PushLayer/PopLayer: what is drawn inside a layer is put on the target
 *    with the layer's opacity, only inside its content bounds and its
 *    geometric mask, through an opacity brush; after the pop, drawing goes
 *    to the target again; nested layers; the ID2D1RenderTarget PushLayer and
 *    the ID2D1DeviceContext one;
 *  - SetPrimitiveBlend: COPY replaces (alpha included), ADD adds, MIN keeps
 *    the smaller;
 *  - DrawImage's composite modes: DESTINATION_OVER puts the image behind
 *    what is there, SOURCE_COPY replaces, DESTINATION_OUT cuts out, XOR.
 *
 * Prints name=value lines; see test/d2dlayer-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

/* ID2D1DeviceContext::PushLayer (mingw's C header misnames it) */
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
#define DC_PUSH_LAYER 86

#define SIZE 64

static IWICImagingFactory *wic;
static ID2D1Factory1 *factory;
static IWICBitmap *wic_bitmap;
static ID2D1RenderTarget *rt;
static ID2D1DeviceContext *dc;

static D2D1_COLOR_F color(float r, float g, float b, float a)
{
    D2D1_COLOR_F c = { r, g, b, a };
    return c;
}

static D2D1_RECT_F rect(float l, float t, float r, float b)
{
    D2D1_RECT_F x = { l, t, r, b };
    return x;
}

static HRESULT begin(D2D1_COLOR_F clear)
{
    D2D1_RENDER_TARGET_PROPERTIES props = { D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f,
            D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT };
    HRESULT hr;

    if (dc) { ID2D1RenderTarget_Release((ID2D1RenderTarget *)dc); dc = NULL; }
    if (rt) { ID2D1RenderTarget_Release(rt); rt = NULL; }
    if (wic_bitmap) { IWICBitmap_Release(wic_bitmap); wic_bitmap = NULL; }
    if (FAILED(hr = IWICImagingFactory_CreateBitmap(wic, SIZE, SIZE, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapCacheOnDemand, &wic_bitmap)))
        return hr;
    if (FAILED(hr = ID2D1Factory_CreateWicBitmapRenderTarget((ID2D1Factory *)factory, wic_bitmap, &props, &rt)))
        return hr;
    if (FAILED(hr = ID2D1RenderTarget_QueryInterface(rt, &IID_ID2D1DeviceContext, (void **)&dc)))
        return hr;
    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_Clear(rt, &clear);
    return S_OK;
}

/* the pixel at x,y as 0xAARRGGBB (premultiplied) */
static DWORD pixel(int x, int y)
{
    WICRect r = { 0, 0, SIZE, SIZE };
    IWICBitmapLock *lock;
    UINT stride, size;
    BYTE *data;
    DWORD v = 0xdeadbeef;

    if (FAILED(IWICBitmap_Lock(wic_bitmap, &r, WICBitmapLockRead, &lock)))
        return v;
    if (SUCCEEDED(IWICBitmapLock_GetStride(lock, &stride))
            && SUCCEEDED(IWICBitmapLock_GetDataPointer(lock, &size, &data)))
        v = *(DWORD *)(data + y * stride + x * 4);
    IWICBitmapLock_Release(lock);
    return v;
}

/* channels within 3 of each other */
static BOOL close_to(DWORD a, DWORD b)
{
    int i;
    for (i = 0; i < 32; i += 8)
        if (abs((int)((a >> i) & 0xff) - (int)((b >> i) & 0xff)) > 3) return FALSE;
    return TRUE;
}

static ID2D1SolidColorBrush *brush(D2D1_COLOR_F c)
{
    ID2D1SolidColorBrush *b = NULL;
    ID2D1RenderTarget_CreateSolidColorBrush(rt, &c, NULL, &b);
    return b;
}

static void fill(D2D1_RECT_F r, D2D1_COLOR_F c)
{
    ID2D1SolidColorBrush *b = brush(c);
    ID2D1RenderTarget_FillRectangle(rt, &r, (ID2D1Brush *)b);
    ID2D1SolidColorBrush_Release(b);
}

static void end(void)
{
    ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
}

static D2D1_LAYER_PARAMETERS layer_params(void)
{
    D2D1_LAYER_PARAMETERS p;
    memset(&p, 0, sizeof(p));
    p.contentBounds = rect(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);
    p.maskAntialiasMode = D2D1_ANTIALIAS_MODE_ALIASED;
    p.maskTransform._11 = p.maskTransform._22 = 1.0f;
    p.opacity = 1.0f;
    return p;
}

static void report(const char *name, BOOL ok, DWORD got)
{
    if (ok) printf("%s=1\n", name);
    else printf("%s=0 (0x%08lx)\n", name, got);
}

int main(void)
{
    static const D2D1_COLOR_F white = { 1, 1, 1, 1 }, red = { 1, 0, 0, 1 }, transparent = { 0, 0, 0, 0 };
    D2D1_LAYER_PARAMETERS params;
    ID2D1Layer *layer = NULL;
    ID2D1EllipseGeometry *ellipse;
    ID2D1Bitmap *bitmap;
    D2D1_ELLIPSE e;
    DWORD p, q;
    HRESULT hr;

    CoInitialize(NULL);
    if (FAILED(hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void **)&wic)))
    {
        printf("no_wic=%#lx\n", hr);
        return 1;
    }
    if (FAILED(hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL,
            (void **)&factory)))
    {
        printf("no_d2d=%#lx\n", hr);
        return 1;
    }
    if (FAILED(hr = begin(white)))
    {
        printf("no_target=%#lx\n", hr);
        return 1;
    }
    end();

    /* a layer at half opacity: red drawn in it is pink on white */
    begin(white);
    ID2D1RenderTarget_CreateLayer(rt, NULL, &layer);
    params = layer_params();
    params.opacity = 0.5f;
    ID2D1RenderTarget_PushLayer(rt, &params, layer);
    fill(rect(0, 0, SIZE, SIZE), red);
    ID2D1RenderTarget_PopLayer(rt);
    end();
    p = pixel(32, 32);
    report("layer_opacity", close_to(p, 0xffff8080), p);

    /* only inside its content bounds */
    begin(white);
    params = layer_params();
    params.contentBounds = rect(0, 0, 32, 64);
    ID2D1RenderTarget_PushLayer(rt, &params, layer);
    fill(rect(0, 0, SIZE, SIZE), red);
    ID2D1RenderTarget_PopLayer(rt);
    end();
    p = pixel(16, 32); q = pixel(48, 32);
    report("layer_bounds_inside", close_to(p, 0xffff0000), p);
    report("layer_bounds_outside", close_to(q, 0xffffffff), q);

    /* only inside its geometric mask: a circle leaves the corners */
    begin(white);
    e.point.x = e.point.y = 32.0f; e.radiusX = e.radiusY = 24.0f;
    ID2D1Factory_CreateEllipseGeometry((ID2D1Factory *)factory, &e, &ellipse);
    params = layer_params();
    params.geometricMask = (ID2D1Geometry *)ellipse;
    ID2D1RenderTarget_PushLayer(rt, &params, layer);
    fill(rect(0, 0, SIZE, SIZE), red);
    ID2D1RenderTarget_PopLayer(rt);
    end();
    p = pixel(32, 32); q = pixel(3, 3);
    report("layer_mask_inside", close_to(p, 0xffff0000), p);
    report("layer_mask_outside", close_to(q, 0xffffffff), q);

    /* an opacity brush: a transparent brush hides the layer */
    begin(white);
    params = layer_params();
    params.opacityBrush = (ID2D1Brush *)brush(color(0, 0, 0, 0));
    ID2D1RenderTarget_PushLayer(rt, &params, layer);
    fill(rect(0, 0, SIZE, SIZE), red);
    ID2D1RenderTarget_PopLayer(rt);
    end();
    p = pixel(32, 32);
    report("layer_opacity_brush", close_to(p, 0xffffffff), p);
    ID2D1Brush_Release(params.opacityBrush);

    /* nested: an inner layer at half opacity within an outer one at half */
    begin(white);
    params = layer_params();
    params.opacity = 0.5f;
    ID2D1RenderTarget_PushLayer(rt, &params, NULL);
    ID2D1RenderTarget_PushLayer(rt, &params, NULL);
    fill(rect(0, 0, SIZE, SIZE), red);
    ID2D1RenderTarget_PopLayer(rt);
    ID2D1RenderTarget_PopLayer(rt);
    fill(rect(0, 0, 8, 8), red);    /* after the pop: on the target */
    end();
    p = pixel(32, 32); q = pixel(4, 4);
    report("layer_nested", close_to(p, 0xffffbfbf), p);
    report("layer_popped", close_to(q, 0xffff0000), q);

    /* the device context's PushLayer (D2D1_LAYER_PARAMETERS1) */
    begin(white);
    {
        D2D1_LAYER_PARAMETERS1 p1;
        memset(&p1, 0, sizeof(p1));
        p1.contentBounds = rect(32, 0, 64, 64);
        p1.maskTransform._11 = p1.maskTransform._22 = 1.0f;
        p1.opacity = 1.0f;
        SLOT(dc, DC_PUSH_LAYER, void (WINAPI *)(ID2D1DeviceContext *, const D2D1_LAYER_PARAMETERS1 *, ID2D1Layer *))(dc, &p1, NULL);
        fill(rect(0, 0, SIZE, SIZE), red);
        ID2D1RenderTarget_PopLayer((ID2D1RenderTarget *)dc);
    }
    end();
    p = pixel(48, 32); q = pixel(16, 32);
    report("layer1_inside", close_to(p, 0xffff0000), p);
    report("layer1_outside", close_to(q, 0xffffffff), q);

    /* primitive blends */
    begin(white);
    ID2D1DeviceContext_SetPrimitiveBlend(dc, D2D1_PRIMITIVE_BLEND_COPY);
    fill(rect(0, 0, SIZE, SIZE), color(1, 0, 0, 0.5f));
    end();
    p = pixel(32, 32);
    report("blend_copy", close_to(p, 0x80800000), p);

    begin(color(0.25f, 0.25f, 0.25f, 1.0f));
    ID2D1DeviceContext_SetPrimitiveBlend(dc, D2D1_PRIMITIVE_BLEND_ADD);
    fill(rect(0, 0, SIZE, SIZE), color(0.5f, 0.0f, 0.0f, 1.0f));
    end();
    p = pixel(32, 32);
    report("blend_add", close_to(p, 0xffbf4040), p);

    begin(color(0.25f, 0.75f, 0.25f, 1.0f));
    ID2D1DeviceContext_SetPrimitiveBlend(dc, D2D1_PRIMITIVE_BLEND_MIN);
    fill(rect(0, 0, SIZE, SIZE), color(0.5f, 0.5f, 0.5f, 1.0f));
    end();
    p = pixel(32, 32);
    report("blend_min", close_to(p, 0xff408040), p);

    /* DrawImage composite modes, with an opaque red bitmap */
    {
        D2D1_BITMAP_PROPERTIES bp = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f };
        static DWORD reds[16 * 16];
        D2D1_SIZE_U sz = { 16, 16 };
        D2D1_POINT_2F at = { 8, 8 };
        int i;

        for (i = 0; i < 16 * 16; ++i) reds[i] = 0xffff0000;

        /* behind an opaque white: stays white; where there was nothing: red */
        begin(transparent);
        fill(rect(0, 0, 16, SIZE), white);
        ID2D1RenderTarget_CreateBitmap(rt, sz, reds, 16 * 4, &bp, &bitmap);
        ID2D1DeviceContext_DrawImage(dc, (ID2D1Image *)bitmap, &at, NULL, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1_COMPOSITE_MODE_DESTINATION_OVER);
        end();
        p = pixel(12, 12); q = pixel(20, 20);
        report("composite_dest_over_kept", close_to(p, 0xffffffff), p);
        report("composite_dest_over_behind", close_to(q, 0xffff0000), q);

        /* destination out: where the image is, what was there goes */
        ID2D1Bitmap_Release(bitmap);
        begin(white);
        ID2D1RenderTarget_CreateBitmap(rt, sz, reds, 16 * 4, &bp, &bitmap);
        ID2D1DeviceContext_DrawImage(dc, (ID2D1Image *)bitmap, &at, NULL, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1_COMPOSITE_MODE_DESTINATION_OUT);
        end();
        p = pixel(12, 12); q = pixel(40, 40);
        report("composite_dest_out", close_to(p, 0x00000000) && close_to(q, 0xffffffff), p);

        /* xor of two opaque: nothing */
        ID2D1Bitmap_Release(bitmap);
        begin(white);
        ID2D1RenderTarget_CreateBitmap(rt, sz, reds, 16 * 4, &bp, &bitmap);
        ID2D1DeviceContext_DrawImage(dc, (ID2D1Image *)bitmap, &at, NULL, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1_COMPOSITE_MODE_XOR);
        end();
        p = pixel(12, 12);
        report("composite_xor", close_to(p, 0x00000000), p);

        /* the primitive blend does not stay changed by DrawImage */
        ID2D1Bitmap_Release(bitmap);
        begin(white);
        ID2D1RenderTarget_CreateBitmap(rt, sz, reds, 16 * 4, &bp, &bitmap);
        ID2D1DeviceContext_DrawImage(dc, (ID2D1Image *)bitmap, &at, NULL, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1_COMPOSITE_MODE_DESTINATION_OUT);
        fill(rect(0, 0, SIZE, SIZE), color(0, 0, 1, 1));
        end();
        p = pixel(12, 12);
        report("composite_reset", close_to(p, 0xff0000ff), p);
        ID2D1Bitmap_Release(bitmap);
    }

    printf("done=1\n");
    return 0;
}
