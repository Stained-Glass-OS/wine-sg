/* Buffered painting (patches/sg/1611): BeginBufferedPaint's painting
 * parameters (an excluded rectangle, a blend function) were ignored, and
 * BufferedPaintClear / BufferedPaintSetAlpha failed with E_NOTIMPL. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static HDC make_target(DWORD **bits)
{
    BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER), 100, -100, 1, 32, BI_RGB }};
    HDC dc = CreateCompatibleDC(NULL);
    SelectObject(dc, CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)bits, NULL, 0));
    for (int i = 0; i < 100 * 100; i++) (*bits)[i] = 0x00ffffff;
    return dc;
}

int main(void)
{
    RECT all = { 0, 0, 100, 100 }, left = { 0, 0, 50, 100 }, corner = { 0, 0, 10, 10 };
    BLENDFUNCTION half = { AC_SRC_OVER, 0, 128, 0 };
    BP_PAINTPARAMS params = { sizeof(params) };
    HPAINTBUFFER pb;
    HBRUSH red = CreateSolidBrush(RGB(255, 0, 0)), blue = CreateSolidBrush(RGB(0, 0, 255));
    DWORD *target, p;
    RGBQUAD *bits;
    HDC dc, bdc;
    int width, i, opaque;
    HRESULT hr;

    check(BufferedPaintInit() == S_OK, "BufferedPaintInit");

    /* an excluded rectangle keeps the target's pixels */
    dc = make_target(&target);
    params.dwFlags = BPPF_ERASE;
    params.prcExclude = &left;
    pb = BeginBufferedPaint(dc, &all, BPBF_TOPDOWNDIB, &params, &bdc);
    FillRect(bdc, &all, red);
    EndBufferedPaint(pb, TRUE);
    printf("exclude: left %06lx, right %06lx\n", target[50 * 100 + 25] & 0xffffff, target[50 * 100 + 75] & 0xffffff);
    check((target[50 * 100 + 25] & 0xffffff) == 0xffffff, "the excluded rectangle is left as it was");
    check((target[50 * 100 + 75] & 0xffffff) == 0xff0000, "the rest is painted");
    DeleteDC(dc);

    /* a blend function blends the buffer in */
    dc = make_target(&target);
    params.prcExclude = NULL;
    params.pBlendFunction = &half;
    pb = BeginBufferedPaint(dc, &all, BPBF_TOPDOWNDIB, &params, &bdc);
    FillRect(bdc, &all, blue);
    EndBufferedPaint(pb, TRUE);
    p = target[50 * 100 + 50] & 0xffffff;
    printf("blend: %06lx\n", p);
    check(((p >> 16) & 0xff) > 0x60 && ((p >> 16) & 0xff) < 0xa0 && (p & 0xff) > 0xf0, "the blend function blends the buffer in");
    DeleteDC(dc);

    /* BufferedPaintClear and BufferedPaintSetAlpha */
    dc = make_target(&target);
    pb = BeginBufferedPaint(dc, &all, BPBF_TOPDOWNDIB, NULL, &bdc);
    FillRect(bdc, &all, red);
    GetBufferedPaintBits(pb, &bits, &width);
    hr = BufferedPaintClear(pb, &corner);
    printf("clear: hr %#lx, (0,0) %08lx, (20,20) %08lx\n", hr, *(DWORD *)&bits[0], *(DWORD *)&bits[20 * width + 20]);
    check(hr == S_OK && *(DWORD *)&bits[0] == 0 && (*(DWORD *)&bits[20 * width + 20] & 0xffffff) == 0xff0000,
          "BufferedPaintClear clears the rectangle only");
    hr = BufferedPaintSetAlpha(pb, NULL, 255);
    for (i = opaque = 0; i < 100 * 100; i++) if (bits[i].rgbReserved == 255) opaque++;
    printf("set alpha: hr %#lx, %d opaque\n", hr, opaque);
    check(hr == S_OK && opaque == 100 * 100, "BufferedPaintSetAlpha sets every pixel's alpha");
    EndBufferedPaint(pb, FALSE);
    DeleteDC(dc);

    /* a bottom-up buffer clears the same rectangle */
    dc = make_target(&target);
    pb = BeginBufferedPaint(dc, &all, BPBF_DIB, NULL, &bdc);
    FillRect(bdc, &all, red);
    BufferedPaintClear(pb, &corner);
    printf("bottom-up clear: (0,0) %06lx, (20,20) %06lx\n", GetPixel(bdc, 0, 0), GetPixel(bdc, 20, 20));
    check(GetPixel(bdc, 0, 0) == 0 && GetPixel(bdc, 20, 20) == RGB(255, 0, 0), "... in a bottom-up buffer too");
    EndBufferedPaint(pb, FALSE);
    DeleteDC(dc);

    BufferedPaintUnInit();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
