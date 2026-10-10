/* d3dx9_36 (patches/sg/2641): D3DXSaveSurfaceToFileInMemory writes TGA and PPM files (the Wine tests
 * record that native accepts both); the pixels read back or parsed are the surface's. */
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
    unsigned int x, y;
    static const RECT sub = { 1, 1, 4, 3 };

    window = CreateWindowA("static", "tga", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;

    IDirect3DDevice9_CreateOffscreenPlainSurface(device, 5, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surf, NULL);
    IDirect3DSurface9_LockRect(surf, &lr, NULL, 0);
    for (y = 0; y < 4; y++) for (x = 0; x < 5; x++)
        *(DWORD *)((BYTE *)lr.pBits + y * lr.Pitch + x * 4) = 0x80000000 | (x * 40 + 10) << 16 | (y * 50 + 20) << 8 | (x + y * 5 + 100);
    IDirect3DSurface9_UnlockRect(surf);

    /* TGA */
    hr = D3DXSaveSurfaceToFileInMemory(&buf, D3DXIFF_TGA, surf, NULL, NULL);
    check(hr == D3D_OK, "TGA (%#lx)", hr);
    if (SUCCEEDED(hr))
    {
        hr = D3DXGetImageInfoFromFileInMemory(ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf), &info);
        check(hr == D3D_OK && info.Width == 5 && info.Height == 4 && info.ImageFileFormat == D3DXIFF_TGA, "TGA info %ux%u fmt %d (%#lx)",
              info.Width, info.Height, info.ImageFileFormat, hr);
        IDirect3DDevice9_CreateOffscreenPlainSurface(device, 5, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &back, NULL);
        hr = D3DXLoadSurfaceFromFileInMemory(back, NULL, NULL, ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf),
                NULL, D3DX_FILTER_NONE, 0, NULL);
        check(hr == D3D_OK, "TGA loaded (%#lx)", hr);
        if (SUCCEEDED(hr))
        {
            int bad = 0;
            D3DLOCKED_RECT l1, l2;

            IDirect3DSurface9_LockRect(surf, &l1, NULL, D3DLOCK_READONLY);
            IDirect3DSurface9_LockRect(back, &l2, NULL, D3DLOCK_READONLY);
            for (y = 0; y < 4; y++) for (x = 0; x < 5; x++)
                if (*(DWORD *)((BYTE *)l1.pBits + y * l1.Pitch + x * 4) != *(DWORD *)((BYTE *)l2.pBits + y * l2.Pitch + x * 4)) bad++;
            IDirect3DSurface9_UnlockRect(surf);
            IDirect3DSurface9_UnlockRect(back);
            check(!bad, "TGA pixels come back (%d differ)", bad);
        }
        IDirect3DSurface9_Release(back);
        ID3DXBuffer_Release(buf);
    }

    /* PPM of a sub-rectangle */
    hr = D3DXSaveSurfaceToFileInMemory(&buf, D3DXIFF_PPM, surf, NULL, &sub);
    check(hr == D3D_OK, "PPM (%#lx)", hr);
    if (SUCCEEDED(hr))
    {
        const char *p = ID3DXBuffer_GetBufferPointer(buf);
        unsigned int w = 0, h = 0, max = 0, n = 0;
        const BYTE *pix;
        int bad = 0;

        check(!strncmp(p, "P6", 2), "P6 signature");
        sscanf(p, "P6 %u %u %u%n", &w, &h, &max, &n);
        check(w == 3 && h == 2 && max == 255, "size %ux%u max %u", w, h, max);
        pix = (const BYTE *)p + n + 1;
        IDirect3DSurface9_LockRect(surf, &lr, NULL, D3DLOCK_READONLY);
        for (y = 0; y < 2; y++) for (x = 0; x < 3; x++)
        {
            DWORD c = *(DWORD *)((BYTE *)lr.pBits + (y + 1) * lr.Pitch + (x + 1) * 4);
            const BYTE *q = pix + (y * 3 + x) * 3;
            if (q[0] != ((c >> 16) & 0xff) || q[1] != ((c >> 8) & 0xff) || q[2] != (c & 0xff)) bad++;
        }
        IDirect3DSurface9_UnlockRect(surf);
        check(!bad, "PPM pixels are the rectangle's (%d differ)", bad);
        ID3DXBuffer_Release(buf);
    }
    IDirect3DSurface9_Release(surf);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
