/* Image list effects and IImageList2 (patches/sg/1669), run by
 * test/imglist2-gate.sh: ILS_GLOW and ILS_SHADOW draw an effect around
 * and under the image; ILD_SCALE draws it to another size; IImageList2's
 * Resize, original sizes, callback, statistics, Replace2 and
 * ReplaceFromImageList; ImageList_WriteEx and ImageList_ReadEx.
 * These were FIXMEs (E_NOTIMPL; the effects and scaling were ignored). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HBITMAP square_bitmap(int size, int lo, int hi, DWORD argb)
{
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB }};
    DWORD *bits;
    HBITMAP bmp = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    int x, y;
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++)
            bits[y * size + x] = (x >= lo && x < hi && y >= lo && y < hi) ? argb : 0;
    return bmp;
}

/* a white 64x64 canvas */
static HDC canvas(DWORD **bits)
{
    BITMAPINFO info = {{ sizeof(BITMAPINFOHEADER), 64, -64, 1, 32, BI_RGB }};
    HDC dc = CreateCompatibleDC(0);
    HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)bits, NULL, 0);
    int i;
    SelectObject(dc, bmp);
    for (i = 0; i < 64 * 64; i++) (*bits)[i] = 0xffffff;
    return dc;
}

static DWORD px(DWORD *bits, int x, int y) { return bits[y * 64 + x] & 0xffffff; }

static void draw(HIMAGELIST himl, int i, HDC dc, int cx, int cy, UINT style, DWORD state, COLORREF effect)
{
    IMAGELISTDRAWPARAMS p = { sizeof(p) };
    p.himl = himl;
    p.i = i;
    p.hdcDst = dc;
    p.x = 8;
    p.y = 8;
    p.cx = cx;
    p.cy = cy;
    p.rgbBk = CLR_NONE;
    p.rgbFg = CLR_DEFAULT;
    p.fStyle = style;
    p.fState = state;
    p.crEffect = effect;
    ImageList_DrawIndirect(&p);
}

int main(void)
{
    HRESULT (WINAPI *query)(HIMAGELIST, REFIID, void **);
    HRESULT (WINAPI *read_ex)(DWORD, IStream *, REFIID, void **);
    HRESULT (WINAPI *write_ex)(HIMAGELIST, DWORD, IStream *);
    HMODULE comctl = LoadLibraryA("comctl32.dll");
    HIMAGELIST himl, other;
    IImageList2 *il2 = NULL;
    IImageList *read_back = NULL, *cb = NULL;
    IMAGELISTSTATS stats = { sizeof(stats) };
    IStream *stream;
    LARGE_INTEGER zero = {{0}};
    DWORD *bits;
    HDC dc;
    int cx, cy, count;
    HBITMAP red, green;

    InitCommonControls();
    query = (void *)GetProcAddress(comctl, "HIMAGELIST_QueryInterface");
    read_ex = (void *)GetProcAddress(comctl, "ImageList_ReadEx");
    write_ex = (void *)GetProcAddress(comctl, "ImageList_WriteEx");

    himl = ImageList_Create(16, 16, ILC_COLOR32, 1, 1);
    red = square_bitmap(16, 4, 12, 0xffff0000);
    check(ImageList_Add(himl, red, NULL) == 0, "a red square with alpha");

    /* effects */
    dc = canvas(&bits);
    draw(himl, 0, dc, 0, 0, ILD_TRANSPARENT, ILS_NORMAL, 0);
    check(px(bits, 15, 15) == 0xff0000 && px(bits, 10, 15) == 0xffffff, "drawn plainly: red, white around");
    DeleteDC(dc);
    dc = canvas(&bits);
    draw(himl, 0, dc, 0, 0, ILD_TRANSPARENT, ILS_GLOW, RGB(0, 0, 255));
    printf("      glow beside: %06lx, far: %06lx\n", px(bits, 11, 15), px(bits, 4, 4));
    check(px(bits, 15, 15) == 0xff0000 && px(bits, 11, 15) != 0xffffff && (px(bits, 11, 15) & 0xff) > 0x80 &&
          px(bits, 4, 4) == 0xffffff, "ILS_GLOW: blue around the square");
    DeleteDC(dc);
    dc = canvas(&bits);
    draw(himl, 0, dc, 0, 0, ILD_TRANSPARENT, ILS_SHADOW, RGB(0, 0, 0));
    printf("      shadow below-right: %06lx, above-left: %06lx\n", px(bits, 21, 21), px(bits, 11, 11));
    check(px(bits, 21, 21) != 0xffffff && px(bits, 11, 11) == 0xffffff, "ILS_SHADOW: below and right only");
    DeleteDC(dc);

    /* scaling */
    dc = canvas(&bits);
    draw(himl, 0, dc, 32, 32, ILD_TRANSPARENT | ILD_SCALE, ILS_NORMAL, 0);
    check(px(bits, 8 + 28, 8 + 28) == 0xffffff && px(bits, 8 + 20, 8 + 20) == 0xff0000 &&
          px(bits, 8 + 9, 8 + 9) == 0xff0000, "ILD_SCALE: the square drawn twice the size");
    DeleteDC(dc);

    /* IImageList2 */
    check(query && query(himl, &IID_IImageList2, (void **)&il2) == S_OK, "IImageList2");
    if (!il2) goto done;
    check(IImageList2_GetStatistics(il2, &stats) == S_OK && stats.cUsed == 1 && stats.cAlloc >= 1,
          "GetStatistics");
    check(IImageList2_SetOriginalSize(il2, 0, 48, 48) == S_OK &&
          IImageList2_GetOriginalSize(il2, 0, 0, &cx, &cy) == S_OK && cx == 48 && cy == 48, "original size");
    check(IImageList2_SetOriginalSize(il2, 5, 48, 48) == E_INVALIDARG, "of an image that is not: E_INVALIDARG");
    check(IImageList2_ForceImagePresent(il2, 0, 0) == S_OK && IImageList2_DiscardImages(il2, 0, 0, 0) == S_OK,
          "ForceImagePresent, DiscardImages");
    other = ImageList_Create(8, 8, ILC_COLOR32, 1, 1);
    check(IImageList2_SetCallback(il2, (IUnknown *)other) == S_OK &&
          IImageList2_GetCallback(il2, &IID_IImageList, (void **)&cb) == S_OK && cb, "the callback");
    if (cb) IImageList_Release(cb);
    IImageList2_SetCallback(il2, NULL);
    ImageList_Destroy(other);

    check(IImageList2_Resize(il2, 32, 32) == S_OK && ImageList_GetIconSize(himl, &cx, &cy) && cx == 32 &&
          ImageList_GetImageCount(himl) == 1, "Resize: 32 by 32, the image kept");
    dc = canvas(&bits);
    draw(himl, 0, dc, 0, 0, ILD_TRANSPARENT, ILS_NORMAL, 0);
    check(px(bits, 8 + 20, 8 + 20) == 0xff0000 && px(bits, 8 + 2, 8 + 2) == 0xffffff, "and stretched");
    DeleteDC(dc);

    green = square_bitmap(32, 0, 32, 0xff00ff00);
    check(IImageList2_Replace2(il2, 0, green, NULL, NULL, 0) == S_OK, "Replace2");
    dc = canvas(&bits);
    draw(himl, 0, dc, 0, 0, ILD_TRANSPARENT, ILS_NORMAL, 0);
    check(px(bits, 8 + 2, 8 + 2) == 0x00ff00, "with the new image");
    DeleteDC(dc);
    other = ImageList_Create(32, 32, ILC_COLOR32, 1, 1);
    ImageList_Add(other, red == NULL ? NULL : square_bitmap(32, 0, 32, 0xff0000ff), NULL);
    check(IImageList2_ReplaceFromImageList(il2, 0, (IImageList *)other, 0, NULL, 0) == S_OK, "ReplaceFromImageList");
    dc = canvas(&bits);
    draw(himl, 0, dc, 0, 0, ILD_TRANSPARENT, ILS_NORMAL, 0);
    check(px(bits, 8 + 2, 8 + 2) == 0x0000ff, "with the other list's image");
    DeleteDC(dc);

    /* streams */
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    check(write_ex && write_ex(himl, 7, stream) == E_INVALIDARG, "WriteEx: unknown flags refused");
    check(write_ex && write_ex(himl, ILP_DOWNLEVEL, stream) == S_OK, "WriteEx");
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    check(read_ex && read_ex(ILP_NORMAL, stream, &IID_IImageList, (void **)&read_back) == S_OK && read_back &&
          IImageList_GetImageCount(read_back, &count) == S_OK && count == 1, "ReadEx: the list again");
    if (read_back) IImageList_Release(read_back);
    IStream_Release(stream);
    IImageList2_Release(il2);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
