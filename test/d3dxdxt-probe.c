/* d3dx9_36 (patches/sg/2640): the colours of DXT2 and DXT4 surfaces are multiplied by their alpha
 * in the file and the multiplication is undone when they are read; DXT3 and DXT5 are not (the Wine
 * tests record this from Windows). Blocks and expected values are from those tests. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

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

static const uint32_t expected_pma[] =
{
    0x00000000, 0x22ffffff, 0x44ffffff, 0x66ffffff, 0x88f7f3f7, 0xaac5c2c5, 0xcca5a2a5, 0xff848284,
    0x00000000, 0x22ffffff, 0x44ffffff, 0x66ffffff, 0x88f7f3f7, 0xaac5c2c5, 0xcca5a2a5, 0xff848284,
};
static const uint32_t expected_plain[] =
{
    0x00848284, 0x22848284, 0x44848284, 0x66848284, 0x88848284, 0xaa848284, 0xcc848284, 0xff848284,
    0x00848284, 0x22848284, 0x44848284, 0x66848284, 0x88848284, 0xaa848284, 0xcc848284, 0xff848284,
};
static const uint8_t dxt3_block[] = { 0x20,0x64,0xa8,0xfc,0x20,0x64,0xa8,0xfc,0x10,0x84,0x10,0x84,0x00,0x00,0x00,0x00 };
static const uint8_t dxt5_block[] = { 0x22,0xcc,0x86,0xc6,0xe6,0x86,0xc6,0xe6,0x10,0x84,0x10,0x84,0x00,0x00,0x00,0x00 };
static const uint32_t compress_src[] =
{
    0xffffffff, 0x00ffffff, 0xffffffff, 0x00ffffff, 0xffffffff, 0x00ffffff, 0xffffffff, 0x00ffffff,
    0xffffffff, 0x00ffffff, 0xffffffff, 0x00ffffff, 0xffffffff, 0x00ffffff, 0xffffffff, 0x00ffffff,
};

static int surface_matches(IDirect3DSurface9 *surf, const uint32_t *want, const char *what)
{
    D3DLOCKED_RECT lr;
    int x, y, bad = 0;

    IDirect3DSurface9_LockRect(surf, &lr, NULL, D3DLOCK_READONLY);
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
        {
            uint32_t got = *(uint32_t *)((BYTE *)lr.pBits + y * lr.Pitch + x * 4);
            if (got != want[y * 4 + x]) { if (!bad) printf("  %s (%d,%d): %08x, want %08x\n", what, x, y, got, want[y * 4 + x]); bad++; }
        }
    IDirect3DSurface9_UnlockRect(surf);
    return !bad;
}

int main(void)
{
    static const struct { D3DFORMAT pma, plain; const uint8_t *block; const char *name; } tests[] =
    {
        { D3DFMT_DXT2, D3DFMT_DXT3, dxt3_block, "DXT2/DXT3" },
        { D3DFMT_DXT4, D3DFMT_DXT5, dxt5_block, "DXT4/DXT5" },
    };
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d;
    IDirect3DSurface9 *out;
    HWND window;
    HRESULT hr;
    unsigned int i;
    static const RECT rect = { 0, 0, 4, 4 };

    window = CreateWindowA("static", "dxt", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;
    IDirect3DDevice9_CreateOffscreenPlainSurface(device, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &out, NULL);

    for (i = 0; i < 2; i++)
    {
        IDirect3DTexture9 *tex;
        IDirect3DSurface9 *surf;

        hr = D3DXLoadSurfaceFromMemory(out, NULL, NULL, tests[i].block, tests[i].pma, 16, NULL, &rect, D3DX_FILTER_NONE, 0);
        check(hr == D3D_OK && surface_matches(out, expected_pma, tests[i].name), "%s: block read as premultiplied (%#lx)", tests[i].name, hr);
        hr = D3DXLoadSurfaceFromMemory(out, NULL, NULL, tests[i].block, tests[i].plain, 16, NULL, &rect, D3DX_FILTER_NONE, 0);
        check(hr == D3D_OK && surface_matches(out, expected_plain, tests[i].name), "%s: same block read as plain (%#lx)", tests[i].name, hr);

        /* write and read back */
        IDirect3DDevice9_CreateTexture(device, 4, 4, 1, 0, tests[i].pma, D3DPOOL_SYSTEMMEM, &tex, NULL);
        IDirect3DTexture9_GetSurfaceLevel(tex, 0, &surf);
        hr = D3DXLoadSurfaceFromMemory(surf, NULL, NULL, compress_src, D3DFMT_A8B8G8R8, 16, NULL, &rect, D3DX_FILTER_NONE, 0);
        check(hr == D3D_OK, "%s: compress (%#lx)", tests[i].name, hr);
        hr = D3DXLoadSurfaceFromSurface(out, NULL, NULL, surf, NULL, NULL, D3DX_FILTER_NONE, 0);
        {
            uint32_t want[16];
            int k;

            for (k = 0; k < 16; k++) want[k] = (k & 1) ? 0x00000000 : 0xffffffff;
            check(hr == D3D_OK && surface_matches(out, want, tests[i].name), "%s: white and nothing survive (%#lx)", tests[i].name, hr);
        }
        IDirect3DSurface9_Release(surf);
        IDirect3DTexture9_Release(tex);
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
