/* Probe for patches/sg/5050: GetPixel gives CLR_INVALID for a pixel outside
 * the DC's clipping region ("If the pixel is outside of the current clipping
 * region, the return value is CLR_INVALID"), in logical coordinates; inside it
 * and without a clip region it reads the pixel as before.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { int x, y; BOOL inside; } points[] = {{2, 2, TRUE}, {9, 9, TRUE}, {10, 10, FALSE}, {15, 3, FALSE}, {3, 15, FALSE}};
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bmp = CreateCompatibleBitmap(GetDC(0), 20, 20);
    HRGN rgn = CreateRectRgn(0, 0, 10, 10);
    RECT all = {0, 0, 20, 20};
    unsigned int i;
    COLORREF c;

    SelectObject(dc, bmp);
    FillRect(dc, &all, GetStockObject(WHITE_BRUSH));
    SetPixel(dc, 15, 3, RGB(1, 2, 3));
    c = GetPixel(dc, 15, 3);
    check(c == RGB(1, 2, 3), "no clip region: %08lx", c);

    SelectClipRgn(dc, rgn);
    for (i = 0; i < ARRAYSIZE(points); i++)
    {
        c = GetPixel(dc, points[i].x, points[i].y);
        check(points[i].inside ? c != CLR_INVALID : c == CLR_INVALID, "clipped to 0,0-10,10: (%d,%d) %08lx", points[i].x, points[i].y, c);
    }
    SetViewportOrgEx(dc, 5, 5, NULL);
    c = GetPixel(dc, 2, 2);
    check(c != CLR_INVALID, "logical (2,2) is device (7,7), inside: %08lx", c);
    c = GetPixel(dc, 6, 6);
    check(c == CLR_INVALID, "logical (6,6) is device (11,11), outside: %08lx", c);
    SetViewportOrgEx(dc, 0, 0, NULL);
    SelectClipRgn(dc, NULL);
    c = GetPixel(dc, 15, 3);
    check(c == RGB(1, 2, 3), "clip region removed: %08lx", c);
    c = GetPixel(dc, 25, 3);
    check(c == CLR_INVALID, "outside the bitmap: %08lx", c);
    DeleteDC(dc); DeleteObject(bmp); DeleteObject(rgn);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
