// dcrt-opaque-probe: an ID2D1DCRenderTarget bound to a coloured memory DC leaves
// what it does not draw as the DC had it -- an opaque target (alpha mode
// IGNORE) through its own BeginDraw and through the ID2D1DeviceContext it
// gives out (Paint.NET draws its toolbar labels so), and a premultiplied one
// through the device context too (a second draw does not bring back the
// first's ink).
#include <windows.h>
#include <d2d1_1.h>
#include <stdio.h>

static const COLORREF BG = RGB(0x70, 0x30, 0xc0);
static DWORD *bits;

static void fill_dc(void)
{
    for (int i = 0; i < 64 * 32; ++i) bits[i] = 0x7030c0;   // BGRX of BG
}

static DWORD px(int x, int y) { return bits[y * 64 + x] & 0xffffff; }

static void draw(ID2D1DCRenderTarget *rt, bool via_context, float left, D2D1_COLOR_F c)
{
    ID2D1SolidColorBrush *brush;
    if (via_context)
    {
        ID2D1DeviceContext *ctx = NULL;
        if (FAILED(rt->QueryInterface(__uuidof(ID2D1DeviceContext), (void **)&ctx)) || !ctx) { printf("NOCTX\n"); return; }
        ctx->BeginDraw();
        ctx->CreateSolidColorBrush(c, &brush);
        ctx->FillRectangle(D2D1::RectF(left, 0, left + 10, 10), brush);
        brush->Release();
        ctx->EndDraw();
        ctx->Release();
    }
    else
    {
        rt->BeginDraw();
        rt->CreateSolidColorBrush(c, &brush);
        rt->FillRectangle(D2D1::RectF(left, 0, left + 10, 10), brush);
        brush->Release();
        rt->EndDraw();
    }
}

int main()
{
    ID2D1Factory *factory;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), NULL, (void **)&factory)))
    { printf("NOFACTORY\n"); return 1; }
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = 64; bi.bmiHeader.biHeight = -32;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    HBITMAP dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(mem, dib);
    RECT r = { 0, 0, 64, 32 };
    D2D1_COLOR_F red = { 1, 0, 0, 1 }, green = { 0, 1, 0, 1 };

    for (int premul = 0; premul < 2; ++premul)
    {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, premul ? D2D1_ALPHA_MODE_PREMULTIPLIED : D2D1_ALPHA_MODE_IGNORE));
        ID2D1DCRenderTarget *rt;
        if (FAILED(factory->CreateDCRenderTarget(&props, &rt))) { printf("NORT %d\n", premul); continue; }
        for (int via = 0; via < 2; ++via)
        {
            fill_dc();
            rt->BindDC(mem, &r);
            draw(rt, via, 0, red);
            GdiFlush();
            printf("%s %s keep=%06lx ink=%06lx\n", premul ? "PREMUL" : "OPAQUE", via ? "CTX" : "SELF", px(40, 20), px(5, 5));
            // a second draw elsewhere: the first's ink does not come back over a fresh DC
            fill_dc();
            rt->BindDC(mem, &r);
            draw(rt, via, 20, green);
            GdiFlush();
            printf("%s %s2 first=%06lx keep=%06lx ink=%06lx\n", premul ? "PREMUL" : "OPAQUE", via ? "CTX" : "SELF", px(5, 5), px(40, 20), px(25, 5));
        }
        rt->Release();
    }
    (void)BG;
    return 0;
}
