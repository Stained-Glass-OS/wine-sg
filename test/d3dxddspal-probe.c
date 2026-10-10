/* d3dx9_36 (patches/sg/2659): saving a P8 or A8P8 surface as DDS without a palette writes 32-bit pixels
 * (what the Wine tests record from native); with a palette the file stays paletted. */
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

int main(void)
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d;
    IDirect3DSurface9 *surf, *back;
    HWND window;
    HRESULT hr;
    ID3DXBuffer *buf;
    D3DXIMAGE_INFO info;
    D3DLOCKED_RECT lr;
    PALETTEENTRY pal[256];
    unsigned int i;

    window = CreateWindowA("static", "dds", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;

    for (i = 0; i < 256; i++) { pal[i].peRed = i; pal[i].peGreen = 255 - i; pal[i].peBlue = 7; pal[i].peFlags = 0xff; }

    /* P8 */
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, 4, 4, D3DFMT_P8, D3DPOOL_SCRATCH, &surf, NULL);
    check(hr == D3D_OK, "P8 surface (%#lx)", hr);
    if (FAILED(hr)) goto done;
    IDirect3DSurface9_LockRect(surf, &lr, NULL, 0);
    for (i = 0; i < 16; i++) ((BYTE *)lr.pBits)[(i / 4) * lr.Pitch + i % 4] = i + 1;
    IDirect3DSurface9_UnlockRect(surf);

    hr = D3DXSaveSurfaceToFileInMemory(&buf, D3DXIFF_DDS, surf, NULL, NULL);
    check(hr == D3D_OK, "P8 without a palette (%#lx)", hr);
    if (SUCCEEDED(hr))
    {
        hr = D3DXGetImageInfoFromFileInMemory(ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf), &info);
        check(hr == D3D_OK && info.Format == D3DFMT_A8R8G8B8 && info.Width == 4 && info.Height == 4,
              "the file is 4x4 A8R8G8B8 (format %d, %#lx)", info.Format, hr);
        check(ID3DXBuffer_GetBufferSize(buf) == 4 + 124 + 4 * 4 * 4, "its size is a header and 16 pixels (%lu)", ID3DXBuffer_GetBufferSize(buf));
        IDirect3DDevice9_CreateOffscreenPlainSurface(device, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &back, NULL);
        hr = D3DXLoadSurfaceFromFileInMemory(back, NULL, NULL, ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf),
                NULL, D3DX_FILTER_NONE, 0, NULL);
        check(hr == D3D_OK, "it loads (%#lx)", hr);
        IDirect3DSurface9_LockRect(back, &lr, NULL, D3DLOCK_READONLY);
        check(*(DWORD *)lr.pBits == 0xff000000, "indices read the default black palette (%08lx)", *(DWORD *)lr.pBits);
        IDirect3DSurface9_UnlockRect(back);
        IDirect3DSurface9_Release(back);
        ID3DXBuffer_Release(buf);
    }

    hr = D3DXSaveSurfaceToFileInMemory(&buf, D3DXIFF_DDS, surf, pal, NULL);
    check(hr == D3D_OK, "P8 with a palette (%#lx)", hr);
    if (SUCCEEDED(hr))
    {
        hr = D3DXGetImageInfoFromFileInMemory(ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf), &info);
        check(hr == D3D_OK && info.Format == D3DFMT_P8, "it stays P8 (format %d, %#lx)", info.Format, hr);
        ID3DXBuffer_Release(buf);
    }
    IDirect3DSurface9_Release(surf);

    /* A8P8: the alpha is the surface's */
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, 4, 4, D3DFMT_A8P8, D3DPOOL_SCRATCH, &surf, NULL);
    check(hr == D3D_OK, "A8P8 surface (%#lx)", hr);
    if (SUCCEEDED(hr))
    {
        IDirect3DSurface9_LockRect(surf, &lr, NULL, 0);
        memset(lr.pBits, 0, 4 * lr.Pitch);
        *(WORD *)lr.pBits = 0x8002;
        IDirect3DSurface9_UnlockRect(surf);
        hr = D3DXSaveSurfaceToFileInMemory(&buf, D3DXIFF_DDS, surf, NULL, NULL);
        check(hr == D3D_OK, "A8P8 without a palette (%#lx)", hr);
        if (SUCCEEDED(hr))
        {
            IDirect3DDevice9_CreateOffscreenPlainSurface(device, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &back, NULL);
            D3DXLoadSurfaceFromFileInMemory(back, NULL, NULL, ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf),
                    NULL, D3DX_FILTER_NONE, 0, NULL);
            IDirect3DSurface9_LockRect(back, &lr, NULL, D3DLOCK_READONLY);
            check((*(DWORD *)lr.pBits >> 24) == 0x80, "the first pixel keeps the surface's alpha (%08lx)", *(DWORD *)lr.pBits);
            IDirect3DSurface9_UnlockRect(back);
            IDirect3DSurface9_Release(back);
            ID3DXBuffer_Release(buf);
        }
        IDirect3DSurface9_Release(surf);
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
