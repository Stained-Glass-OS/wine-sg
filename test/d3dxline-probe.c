/* d3dx9_36: ID3DXLine::Draw and DrawTransform (patches/sg/2629) on a render target of a
 * Direct3D 9 device: thick lines, polylines, patterns, drawing outside Begin and End leaving
 * the device state alone, the state setters. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

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

static IDirect3DDevice9 *device;
static IDirect3DSurface9 *target, *sysmem;
#define W 64
#define H 32

static void clear(D3DCOLOR c)
{
    IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET, c, 1.0f, 0);
}

static D3DCOLOR pixel(int x, int y)
{
    D3DLOCKED_RECT lr;
    D3DCOLOR c = 0xdeadbeef;

    IDirect3DDevice9_GetRenderTargetData(device, target, sysmem);
    if (SUCCEEDED(IDirect3DSurface9_LockRect(sysmem, &lr, NULL, D3DLOCK_READONLY)))
    {
        c = *(D3DCOLOR *)((BYTE *)lr.pBits + y * lr.Pitch + x * 4);
        IDirect3DSurface9_UnlockRect(sysmem);
    }
    return c & 0xffffff;
}

static int count_row(int y, D3DCOLOR c)
{
    int x, n = 0;

    for (x = 0; x < W; ++x) if (pixel(x, y) == c) ++n;
    return n;
}

int main(void)
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3D9 *d3d;
    ID3DXLine *line = NULL;
    HRESULT hr;
    HWND window;
    D3DXVECTOR2 pts[3];
    D3DXVECTOR3 pts3[2];
    D3DXMATRIX identity;
    DWORD value;
    const D3DCOLOR red = 0xffff0000, black = 0xff000000;

    window = CreateWindowA("static", "line", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = W; pp.BackBufferHeight = H; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;
    IDirect3DDevice9_GetRenderTarget(device, 0, &target);
    IDirect3DDevice9_CreateOffscreenPlainSurface(device, W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sysmem, NULL);

    hr = D3DXCreateLine(device, &line);
    check(hr == D3D_OK && line, "line (%#lx)", hr);
    if (!line) goto done;

    /* defaults and setters */
    check(ID3DXLine_GetPattern(line) == 0xffffffff, "default pattern");
    check(ID3DXLine_GetPatternScale(line) == 1.0f, "default pattern scale");
    check(!ID3DXLine_GetAntialias(line) && !ID3DXLine_GetGLLines(line), "no antialias, no GL lines");
    check(ID3DXLine_SetPattern(line, 0xaaaaaaaa) == D3D_OK && ID3DXLine_GetPattern(line) == 0xaaaaaaaa, "pattern is kept");
    check(ID3DXLine_SetPatternScale(line, 2.5f) == D3D_OK && ID3DXLine_GetPatternScale(line) == 2.5f, "pattern scale is kept");
    check(ID3DXLine_SetPatternScale(line, 0.0f) == D3DERR_INVALIDCALL && ID3DXLine_GetPatternScale(line) == 2.5f, "scale 0 is refused");
    check(ID3DXLine_SetAntialias(line, TRUE) == D3D_OK && ID3DXLine_GetAntialias(line), "antialias is kept");
    check(ID3DXLine_SetGLLines(line, TRUE) == D3D_OK && ID3DXLine_GetGLLines(line), "GL lines are kept");
    ID3DXLine_SetAntialias(line, FALSE); ID3DXLine_SetGLLines(line, FALSE);
    ID3DXLine_SetPattern(line, 0xffffffff); ID3DXLine_SetPatternScale(line, 1.0f);
    check(ID3DXLine_OnLostDevice(line) == D3D_OK && ID3DXLine_OnResetDevice(line) == D3D_OK, "lost and reset");

    /* argument checks */
    pts[0].x = 4; pts[0].y = 16; pts[1].x = 40; pts[1].y = 16;
    check(ID3DXLine_Draw(line, NULL, 2, red) == D3DERR_INVALIDCALL, "no vertices");
    check(ID3DXLine_Draw(line, pts, 1, red) == D3DERR_INVALIDCALL, "one vertex");

    /* a horizontal line of width 4 between Begin and End */
    ID3DXLine_SetWidth(line, 4.0f);
    clear(black);
    IDirect3DDevice9_BeginScene(device);
    hr = ID3DXLine_Begin(line);
    check(hr == D3D_OK, "Begin (%#lx)", hr);
    hr = ID3DXLine_Draw(line, pts, 2, red);
    check(hr == D3D_OK, "Draw (%#lx)", hr);
    ID3DXLine_End(line);
    IDirect3DDevice9_EndScene(device);
    check(pixel(20, 16) == 0xff0000 && pixel(20, 15) == 0xff0000 && pixel(20, 17) == 0xff0000, "the line is red in the middle");
    check(pixel(20, 12) == 0 && pixel(20, 20) == 0, "and not beside it");
    check(pixel(2, 16) == 0 && pixel(44, 16) == 0, "nor beyond its ends");
    check(pixel(20, 18) == 0 && pixel(20, 13) != 0xff0000 || pixel(20, 14) == 0xff0000, "its width is 4 rows (%x %x)", pixel(20, 13), pixel(20, 14));

    /* outside Begin and End the device state is left alone */
    IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, D3DZB_TRUE);
    clear(black);
    IDirect3DDevice9_BeginScene(device);
    hr = ID3DXLine_Draw(line, pts, 2, 0xff00ff00);
    IDirect3DDevice9_EndScene(device);
    check(hr == D3D_OK, "Draw without Begin (%#lx)", hr);
    check(pixel(20, 16) == 0x00ff00, "draws too (%#x)", pixel(20, 16));
    IDirect3DDevice9_GetRenderState(device, D3DRS_ALPHABLENDENABLE, &value);
    check(value == FALSE, "alpha blending is as it was");
    IDirect3DDevice9_GetRenderState(device, D3DRS_ZENABLE, &value);
    check(value == D3DZB_TRUE, "the depth test too");

    /* a polyline with a corner */
    pts[0].x = 8; pts[0].y = 6; pts[1].x = 8; pts[1].y = 26; pts[2].x = 50; pts[2].y = 26;
    clear(black);
    IDirect3DDevice9_BeginScene(device);
    ID3DXLine_Draw(line, pts, 3, red);
    IDirect3DDevice9_EndScene(device);
    check(pixel(8, 12) == 0xff0000 && pixel(30, 26) == 0xff0000, "both stretches of a polyline");
    check(pixel(30, 12) == 0 && pixel(20, 6) == 0, "and nothing else");

    /* a pattern leaves gaps */
    ID3DXLine_SetPattern(line, 0x0000ffff);
    ID3DXLine_SetPatternScale(line, 2.0f);
    pts[0].x = 2; pts[0].y = 16; pts[1].x = 62; pts[1].y = 16;
    clear(black);
    IDirect3DDevice9_BeginScene(device);
    ID3DXLine_Draw(line, pts, 2, red);
    IDirect3DDevice9_EndScene(device);
    {
        int n = count_row(16, 0xff0000);

        check(n > 10 && n < 55, "a pattern: some of the row is drawn (%d pixels)", n);
    }
    ID3DXLine_SetPattern(line, 0xffffffff);
    ID3DXLine_SetPatternScale(line, 1.0f);

    /* DrawTransform: clip space coordinates through the identity */
    D3DXMatrixIdentity(&identity);
    pts3[0].x = -0.5f; pts3[0].y = 0.0f; pts3[0].z = 0.0f;
    pts3[1].x = 0.5f; pts3[1].y = 0.0f; pts3[1].z = 0.0f;
    clear(black);
    IDirect3DDevice9_BeginScene(device);
    hr = ID3DXLine_DrawTransform(line, pts3, 2, &identity, red);
    IDirect3DDevice9_EndScene(device);
    check(hr == D3D_OK, "DrawTransform (%#lx)", hr);
    check(pixel(32, 16) == 0xff0000 && pixel(20, 16) == 0xff0000 && pixel(44, 16) == 0xff0000, "from a quarter to three quarters of the viewport");
    check(pixel(6, 16) == 0 && pixel(58, 16) == 0, "not to its edges");
    check(ID3DXLine_DrawTransform(line, pts3, 2, NULL, red) == D3DERR_INVALIDCALL, "no matrix");

    ID3DXLine_Release(line);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
