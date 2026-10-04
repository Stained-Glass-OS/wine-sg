/* test/stretchzero-gate.sh's probe: StretchBlt and AlphaBlend with a source
 * or a destination of no width or height draw nothing and return -- from a
 * memory DC, a DC with no bitmap, and the screen (whose DC has no device
 * rectangle, so an empty source reached the scaling and divided by zero in
 * win32u: MeediOS ended with 0x94, 2026-10-03). A normal stretch still draws.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

static void t(const char *what, HDC dst, HDC src, int dw, int dh, int sw, int sh)
{
    BOOL r = StretchBlt(dst, 0, 0, dw, dh, src, 0, 0, sw, sh, SRCCOPY);
    printf("%s %dx%d<-%dx%d %d\n", what, dw, dh, sw, sh, r);
    fflush(stdout);
}

int main(void)
{
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen), bare = CreateCompatibleDC(screen), src = CreateCompatibleDC(screen);
    HBITMAP b = CreateCompatibleBitmap(screen, 20, 20), sb = CreateCompatibleBitmap(screen, 4, 4);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, 0 };
    COLORREF c;

    SelectObject(mem, b);
    SelectObject(src, sb);
    t("mem<-mem", mem, mem, 10, 10, 0, 10);
    t("mem<-bare", mem, bare, 10, 10, 0, 10);
    t("mem<-screen", mem, screen, 10, 10, 0, 10);
    t("mem<-screen", mem, screen, 10, 10, 10, 0);
    t("screen<-mem", screen, mem, 10, 10, 0, 10);
    t("mem<-screen", mem, screen, 0, 10, 10, 10);
    printf("alpha %d\n", AlphaBlend(mem, 0, 0, 10, 10, screen, 0, 0, 0, 10, bf));
    fflush(stdout);
    /* a real stretch still draws: a red 4x4 source onto 16x16 */
    PatBlt(mem, 0, 0, 20, 20, WHITENESS);
    SetPixel(src, 0, 0, RGB(255, 0, 0)); SetPixel(src, 3, 3, RGB(255, 0, 0));
    { HBRUSH r = CreateSolidBrush(RGB(255, 0, 0)); RECT rc = { 0, 0, 4, 4 }; FillRect(src, &rc, r); DeleteObject(r); }
    StretchBlt(mem, 0, 0, 16, 16, src, 0, 0, 4, 4, SRCCOPY);
    c = GetPixel(mem, 12, 12);
    printf("stretched %06lx\n", (unsigned long)c);
    printf("DONE\n");
    return 0;
}
