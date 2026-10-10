/* d3dx9_36 (patches/sg/2642): D3DXLoadSurfaceFromMemory to a format with fewer bits in a channel
 * rounds to the nearest value; the values are those the Wine tests record from Windows. */
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

static const DWORD g16r16[] = { 0x07d23fbe, 0xdc7f44a4, 0xe4d8976b, 0x9a84fe89 };
static const DWORD a8b8g8r8[] = { 0xc3394cf0, 0x235ae892, 0x09b197fd, 0x8dc32bf6 };
static const DWORD a2r10g10b10[] = { 0x57395aff, 0x5b7668fd, 0xb0d856b5, 0xff2c61d6 };

static const struct
{
    const char *name;
    const DWORD *src;
    D3DFORMAT src_fmt;
    D3DFORMAT dst_fmt;
    DWORD want[4];
} tests[] =
{
    { "G16R16 -> A8R8G8B8", g16r16, D3DFMT_G16R16, D3DFMT_A8R8G8B8, { 0xff3f08ff, 0xff44dcff, 0xff97e4ff, 0xfffe9aff } },
    { "A2R10G10B10 -> A8R8G8B8", a2r10g10b10, D3DFMT_A2R10G10B10, D3DFMT_A8R8G8B8, { 0x555c95bf, 0x556d663f, 0xaac385ad, 0xfffcc575 } },
    { "G16R16 -> A1R5G5B5", g16r16, D3DFMT_G16R16, D3DFMT_A1R5G5B5, { 0xa03f, 0xa37f, 0xcb9f, 0xfe7f } },
    { "A8B8G8R8 -> A1R5G5B5", a8b8g8r8, D3DFMT_A8B8G8R8, D3DFMT_A1R5G5B5, { 0xf527, 0x4b8b, 0x7e56, 0xf8b8 } },
    { "A2R10G10B10 -> A1R5G5B5", a2r10g10b10, D3DFMT_A2R10G10B10, D3DFMT_A1R5G5B5, { 0x2e57, 0x3588, 0xe215, 0xff0e } },
};

int main(void)
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d;
    HWND window;
    HRESULT hr;
    unsigned int i, k;
    static const RECT rect = { 0, 0, 2, 2 };

    window = CreateWindowA("static", "conv", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
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
        IDirect3DSurface9 *surf;
        D3DLOCKED_RECT lr;
        DWORD got[4];
        BOOL wide = tests[i].dst_fmt == D3DFMT_A8R8G8B8;

        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, 2, 2, tests[i].dst_fmt, D3DPOOL_SYSTEMMEM, &surf, NULL);
        if (FAILED(hr)) { check(0, "%s: surface (%#lx)", tests[i].name, hr); continue; }
        hr = D3DXLoadSurfaceFromMemory(surf, NULL, NULL, tests[i].src, tests[i].src_fmt, 8, NULL, &rect, D3DX_FILTER_NONE, 0);
        IDirect3DSurface9_LockRect(surf, &lr, NULL, D3DLOCK_READONLY);
        for (k = 0; k < 4; k++)
        {
            BYTE *row = (BYTE *)lr.pBits + (k / 2) * lr.Pitch;
            got[k] = wide ? ((DWORD *)row)[k % 2] : ((WORD *)row)[k % 2];
        }
        IDirect3DSurface9_UnlockRect(surf);
        check(hr == D3D_OK && got[0] == tests[i].want[0] && got[1] == tests[i].want[1] && got[2] == tests[i].want[2] && got[3] == tests[i].want[3],
              "%s: %x %x %x %x (want %x %x %x %x)", tests[i].name, got[0], got[1], got[2], got[3],
              tests[i].want[0], tests[i].want[1], tests[i].want[2], tests[i].want[3]);
        IDirect3DSurface9_Release(surf);
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
