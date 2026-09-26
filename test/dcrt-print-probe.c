/* Direct2D on a GDI DC and alpha-blended bitmaps on a printer (0300, 0301).
 *
 *   dcrt-print-probe.exe screen
 *       A premultiplied ID2D1DCRenderTarget bound to a white 64x64 DIB,
 *       filling only its top-left 16x16 red: the rest must stay white (it
 *       was painted black -- the whole bound rectangle was BitBlt'ed).
 *       Prints "inside=RRGGBB outside=RRGGBB".
 *
 *   dcrt-print-probe.exe print PRINTER
 *       One page on PRINTER: a grey band, then over it
 *        - AlphaBlend of a 200x100 bitmap whose left half is opaque black
 *          and right half fully transparent (AC_SRC_ALPHA), at 300,300;
 *        - a premultiplied DC render target bound to 300,700 400x100 that
 *          fills only its left half black, as LibreOffice prints text.
 *       The gate renders the PostScript and samples the four halves.
 *       Prints "printed=1" when the job went through.
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d2d1.h>
#include <stdio.h>

static ID2D1DCRenderTarget *create_dcrt(void)
{
    D2D1_RENDER_TARGET_PROPERTIES props = {D2D1_RENDER_TARGET_TYPE_DEFAULT,
            {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0.0f, 0.0f,
            D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT};
    ID2D1Factory *factory;
    ID2D1DCRenderTarget *rt = NULL;

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&factory)))
        return NULL;
    if (FAILED(ID2D1Factory_CreateDCRenderTarget(factory, &props, &rt))) rt = NULL;
    ID2D1Factory_Release(factory);
    return rt;
}

/* bind, fill the left part (width fill) with colour, end */
static HRESULT dcrt_fill(ID2D1DCRenderTarget *rt, HDC hdc, const RECT *bound, float fill_w, float fill_h,
        float r, float g, float b)
{
    D2D1_COLOR_F colour = {r, g, b, 1.0f};
    D2D1_RECT_F fill = {0.0f, 0.0f, fill_w, fill_h};
    ID2D1SolidColorBrush *brush;
    HRESULT hr;

    if (FAILED(hr = ID2D1DCRenderTarget_BindDC(rt, hdc, bound))) return hr;
    if (FAILED(hr = ID2D1DCRenderTarget_CreateSolidColorBrush(rt, &colour, NULL, &brush))) return hr;
    ID2D1DCRenderTarget_BeginDraw(rt);
    ID2D1DCRenderTarget_FillRectangle(rt, &fill, (ID2D1Brush *)brush);
    hr = ID2D1DCRenderTarget_EndDraw(rt, NULL, NULL);
    ID2D1SolidColorBrush_Release(brush);
    return hr;
}

static int screen(void)
{
    BITMAPINFO bi = {{sizeof(bi.bmiHeader), 64, -64, 1, 32, BI_RGB}};
    RECT rc = {0, 0, 64, 64};
    ID2D1DCRenderTarget *rt;
    HBITMAP bmp;
    DWORD *bits;
    HRESULT hr;
    HDC hdc;

    if (!(rt = create_dcrt())) { printf("no-dc-render-target\n"); return 1; }
    hdc = CreateCompatibleDC(NULL);
    bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(hdc, bmp);
    FillRect(hdc, &rc, GetStockObject(WHITE_BRUSH));
    hr = dcrt_fill(rt, hdc, &rc, 16.0f, 16.0f, 1.0f, 0.0f, 0.0f);
    GdiFlush();
    printf("hr=%#lx inside=%06lx outside=%06lx\n", hr, bits[8 * 64 + 8] & 0xffffff, bits[40 * 64 + 40] & 0xffffff);
    ID2D1DCRenderTarget_Release(rt);
    return 0;
}

static int print(const WCHAR *printer)
{
    BITMAPINFO bi = {{sizeof(bi.bmiHeader), 200, -100, 1, 32, BI_RGB}};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    DOCINFOW doc = {sizeof(doc), L"dcrt-print-probe"};
    RECT band = {100, 200, 1100, 900}, bound = {300, 700, 700, 800};
    ID2D1DCRenderTarget *rt;
    HDC hdc, mem;
    HBITMAP bmp;
    HBRUSH grey;
    DWORD *bits;
    HRESULT hr;
    int x, y, dpi;

    if (!(hdc = CreateDCW(NULL, printer, NULL, NULL))) { printf("no-printer-dc\n"); return 1; }
    dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    if (StartDocW(hdc, &doc) <= 0 || StartPage(hdc) <= 0) { printf("startdoc-failed %lu\n", GetLastError()); return 1; }

    grey = CreateSolidBrush(RGB(128, 128, 128));
    FillRect(hdc, &band, grey);

    mem = CreateCompatibleDC(NULL);
    bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(mem, bmp);
    for (y = 0; y < 100; y++)
        for (x = 0; x < 200; x++)
            bits[y * 200 + x] = x < 100 ? 0xff000000 : 0x00000000;   /* premultiplied */
    if (!GdiAlphaBlend(hdc, 300, 300, 200, 100, mem, 0, 0, 200, 100, blend)) printf("alphablend-failed\n");

    if ((rt = create_dcrt()))
    {
        hr = dcrt_fill(rt, hdc, &bound, 200.0f, 100.0f, 0.0f, 0.0f, 0.0f);
        printf("dcrt hr=%#lx\n", hr);
        ID2D1DCRenderTarget_Release(rt);
    }
    else printf("no-dc-render-target\n");

    EndPage(hdc);
    EndDoc(hdc);
    DeleteDC(hdc);
    printf("dpi=%d printed=1\n", dpi);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    if (argc >= 2 && !lstrcmpW(argv[1], L"screen")) return screen();
    if (argc >= 3 && !lstrcmpW(argv[1], L"print")) return print(argv[2]);
    printf("usage: dcrt-print-probe screen | print PRINTER\n");
    return 2;
}
