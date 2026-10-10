/* d2d1 (patches/sg/2646): DrawTextLayout with a drawing effect brush made by another factory ends in
 * D2DERR_WRONG_FACTORY (the Wine tests record Windows); with a brush of the same factory it draws. */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdarg.h>

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

int main(void)
{
    ID2D1Factory *f1 = NULL, *f2 = NULL;
    IDWriteFactory *dw = NULL;
    IDWriteTextFormat *format = NULL;
    IDWriteTextLayout *layout = NULL;
    IWICImagingFactory *wic = NULL;
    IWICBitmap *bitmap = NULL;
    ID2D1RenderTarget *rt = NULL, *rt2 = NULL;
    ID2D1SolidColorBrush *brush = NULL, *same = NULL, *other = NULL;
    D2D1_RENDER_TARGET_PROPERTIES rtp = {0};
    D2D1_COLOR_F color = { 1, 0, 0, 1 };
    D2D1_POINT_2F origin = { 0, 0 };
    DWRITE_TEXT_RANGE range = { 0, 4 };
    IWICBitmap *bitmap2 = NULL;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&f1);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&f2);
    check(hr == S_OK && f2, "factories (%#lx)", hr);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&dw);
    CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic);
    if (!f1 || !f2 || !dw || !wic) goto done;
    IWICImagingFactory_CreateBitmap(wic, 64, 32, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bitmap);
    IWICImagingFactory_CreateBitmap(wic, 64, 32, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bitmap2);
    rtp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT; rtp.dpiX = rtp.dpiY = 96.0f;
    hr = ID2D1Factory_CreateWicBitmapRenderTarget(f1, bitmap, &rtp, &rt);
    check(hr == S_OK && rt, "render target (%#lx)", hr);
    ID2D1Factory_CreateWicBitmapRenderTarget(f2, bitmap2, &rtp, &rt2);
    if (!rt || !rt2) goto done;
    ID2D1RenderTarget_CreateSolidColorBrush(rt, &color, NULL, &brush);
    ID2D1RenderTarget_CreateSolidColorBrush(rt, &color, NULL, &same);
    ID2D1RenderTarget_CreateSolidColorBrush(rt2, &color, NULL, &other);

    IDWriteFactory_CreateTextFormat(dw, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &format);
    IDWriteFactory_CreateTextLayout(dw, L"Wine text", 9, format, 60.0f, 30.0f, &layout);

    IDWriteTextLayout_SetDrawingEffect(layout, (IUnknown *)same, range);
    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_DrawTextLayout(rt, origin, layout, (ID2D1Brush *)brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    check(hr == S_OK, "effect of the same factory (%#lx)", hr);

    IDWriteTextLayout_SetDrawingEffect(layout, (IUnknown *)other, range);
    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_DrawTextLayout(rt, origin, layout, (ID2D1Brush *)brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    check(hr == D2DERR_WRONG_FACTORY, "effect of another factory (%#lx)", hr);

    /* and the target can be used again */
    IDWriteTextLayout_SetDrawingEffect(layout, (IUnknown *)same, range);
    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_DrawTextLayout(rt, origin, layout, (ID2D1Brush *)brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
    hr = ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    check(hr == S_OK, "and then the same factory again (%#lx)", hr);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
