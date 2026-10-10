/* d3dx9_36 (patches/sg/2638): what the Wine tests record from Windows about the glyph cache of
 * ID3DXFont (texture mip levels follow the cell size, a glyph's box holds a pixel of margin
 * all around it) and ID3DXSprite::SetWorldViewLH/RH accepting missing matrices. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
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
    static const struct { int height; unsigned int size, levels; } tests[] =
    {
        { 2, 32, 2 }, { 6, 128, 4 }, { 10, 256, 5 }, { 12, 256, 5 }, { 72, 256, 8 }, { 250, 256, 9 }, { 258, 512, 10 },
    };
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d;
    HWND window;
    HRESULT hr;
    unsigned int i;
    ID3DXSprite *sprite;
    D3DXMATRIX mat;

    window = CreateWindowA("static", "font", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        ID3DXFont *font = NULL;
        IDirect3DTexture9 *texture = NULL;
        D3DSURFACE_DESC desc;
        GLYPHMETRICS gm;
        static const MAT2 m2 = { {0,1}, {0,0}, {0,0}, {0,1} };
        RECT box;
        POINT cell;
        WORD glyph;
        char c = 'a';
        HDC hdc;
        DWORD levels;

        hr = D3DXCreateFontA(device, tests[i].height, 0, FW_DONTCARE, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH, "Tahoma", &font);
        if (FAILED(hr)) { check(0, "font %d (%#lx)", tests[i].height, hr); continue; }
        hdc = ID3DXFont_GetDC(font);
        GetGlyphIndicesA(hdc, &c, 1, &glyph, 0);
        hr = ID3DXFont_GetGlyphData(font, glyph, &texture, &box, &cell);
        if (FAILED(hr) || !texture) { check(0, "glyph data (%#lx)", hr); ID3DXFont_Release(font); continue; }
        levels = IDirect3DTexture9_GetLevelCount(texture);
        IDirect3DTexture9_GetLevelDesc(texture, 0, &desc);
        check(levels == tests[i].levels && desc.Width == tests[i].size, "height %d: %lu levels, %ux%u (want %u, %u)",
              tests[i].height, levels, desc.Width, desc.Height, tests[i].levels, tests[i].size);
        if (GetGlyphOutlineW(hdc, glyph, GGO_GLYPH_INDEX | GGO_METRICS, &gm, 0, NULL, &m2) != GDI_ERROR)
            check(box.right - box.left == (LONG)gm.gmBlackBoxX + 2 && box.bottom - box.top == (LONG)gm.gmBlackBoxY + 2,
                  "height %d: the box %ldx%ld holds a margin of one pixel around %ux%u", tests[i].height,
                  box.right - box.left, box.bottom - box.top, gm.gmBlackBoxX, gm.gmBlackBoxY);
        IDirect3DTexture9_Release(texture);
        ID3DXFont_Release(font);
    }

    hr = D3DXCreateSprite(device, &sprite);
    check(hr == D3D_OK && sprite, "sprite (%#lx)", hr);
    if (sprite)
    {
        D3DXMatrixIdentity(&mat);
        mat._11 = 2.0f;
        check(ID3DXSprite_SetWorldViewLH(sprite, &mat, &mat) == D3D_OK, "LH both");
        check(ID3DXSprite_SetWorldViewLH(sprite, NULL, &mat) == D3D_OK, "LH no world");
        check(ID3DXSprite_SetWorldViewLH(sprite, &mat, NULL) == D3D_OK, "LH no view");
        check(ID3DXSprite_SetWorldViewLH(sprite, NULL, NULL) == D3D_OK, "LH none");
        check(ID3DXSprite_SetWorldViewRH(sprite, &mat, &mat) == D3D_OK, "RH both");
        check(ID3DXSprite_SetWorldViewRH(sprite, NULL, NULL) == D3D_OK, "RH none");
        ID3DXSprite_Release(sprite);
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
