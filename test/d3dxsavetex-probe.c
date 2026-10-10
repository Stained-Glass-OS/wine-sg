/* d3dx9_36 (patches/sg/2639): D3DXSaveTextureToFileInMemory writes a DDS file of a texture with all
 * its levels, of a cube texture with its six faces and of a volume texture with its depth;
 * the pixels read back are the ones that were in the levels. */
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

static DWORD level_color(UINT face, UINT level) { return 0xff000000 | (face * 40 + 10) << 16 | (level * 30 + 5) << 8 | 0x77; }

static void fill_surface_levels(IDirect3DBaseTexture9 *tex, D3DRESOURCETYPE type, UINT levels, UINT faces)
{
    UINT f, l, x, y, z;

    for (f = 0; f < faces; f++)
        for (l = 0; l < levels; l++)
        {
            if (type == D3DRTYPE_VOLUMETEXTURE)
            {
                D3DLOCKED_BOX box;
                D3DVOLUME_DESC d;

                IDirect3DVolumeTexture9_GetLevelDesc((IDirect3DVolumeTexture9 *)tex, l, &d);
                IDirect3DVolumeTexture9_LockBox((IDirect3DVolumeTexture9 *)tex, l, &box, NULL, 0);
                for (z = 0; z < d.Depth; z++) for (y = 0; y < d.Height; y++) for (x = 0; x < d.Width; x++)
                    *(DWORD *)((BYTE *)box.pBits + z * box.SlicePitch + y * box.RowPitch + x * 4) = level_color(z, l);
                IDirect3DVolumeTexture9_UnlockBox((IDirect3DVolumeTexture9 *)tex, l);
            }
            else
            {
                D3DLOCKED_RECT lr;
                D3DSURFACE_DESC d;

                if (type == D3DRTYPE_TEXTURE)
                {
                    IDirect3DTexture9_GetLevelDesc((IDirect3DTexture9 *)tex, l, &d);
                    IDirect3DTexture9_LockRect((IDirect3DTexture9 *)tex, l, &lr, NULL, 0);
                }
                else
                {
                    IDirect3DCubeTexture9_GetLevelDesc((IDirect3DCubeTexture9 *)tex, l, &d);
                    IDirect3DCubeTexture9_LockRect((IDirect3DCubeTexture9 *)tex, f, l, &lr, NULL, 0);
                }
                for (y = 0; y < d.Height; y++) for (x = 0; x < d.Width; x++)
                    *(DWORD *)((BYTE *)lr.pBits + y * lr.Pitch + x * 4) = level_color(f, l);
                if (type == D3DRTYPE_TEXTURE) IDirect3DTexture9_UnlockRect((IDirect3DTexture9 *)tex, l);
                else IDirect3DCubeTexture9_UnlockRect((IDirect3DCubeTexture9 *)tex, f, l);
            }
        }
}

int main(void)
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d;
    HWND window;
    HRESULT hr;
    ID3DXBuffer *buf;
    D3DXIMAGE_INFO info;
    UINT f, l;

    window = CreateWindowA("static", "tex", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;

    /* a texture with 6 levels */
    {
        IDirect3DTexture9 *tex, *back;
        D3DLOCKED_RECT lr;

        IDirect3DDevice9_CreateTexture(device, 32, 32, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
        fill_surface_levels((IDirect3DBaseTexture9 *)tex, D3DRTYPE_TEXTURE, 6, 1);
        hr = D3DXSaveTextureToFileInMemory(&buf, D3DXIFF_DDS, (IDirect3DBaseTexture9 *)tex, NULL);
        check(hr == D3D_OK, "texture to DDS (%#lx)", hr);
        if (SUCCEEDED(hr))
        {
            hr = D3DXGetImageInfoFromFileInMemory(ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf), &info);
            check(hr == D3D_OK && info.Width == 32 && info.Height == 32 && info.MipLevels == 6 && info.ResourceType == D3DRTYPE_TEXTURE &&
                  info.ImageFileFormat == D3DXIFF_DDS, "info: %ux%u, %u levels, type %d (%#lx)", info.Width, info.Height, info.MipLevels, info.ResourceType, hr);
            hr = D3DXCreateTextureFromFileInMemoryEx(device, ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf),
                    D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, D3DX_FROM_FILE, 0, D3DFMT_FROM_FILE, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &back);
            check(hr == D3D_OK, "read back (%#lx)", hr);
            if (SUCCEEDED(hr))
            {
                int ok = 1;

                for (l = 0; l < 6; l++)
                {
                    IDirect3DTexture9_LockRect(back, l, &lr, NULL, D3DLOCK_READONLY);
                    if (*(DWORD *)lr.pBits != level_color(0, l)) ok = 0;
                    IDirect3DTexture9_UnlockRect(back, l);
                }
                check(ok, "the pixels of all 6 levels are those that were saved");
                IDirect3DTexture9_Release(back);
            }
            ID3DXBuffer_Release(buf);
        }
        IDirect3DTexture9_Release(tex);
    }

    /* a cube texture */
    {
        IDirect3DCubeTexture9 *cube, *back;
        D3DLOCKED_RECT lr;

        IDirect3DDevice9_CreateCubeTexture(device, 16, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cube, NULL);
        fill_surface_levels((IDirect3DBaseTexture9 *)cube, D3DRTYPE_CUBETEXTURE, 5, 6);
        hr = D3DXSaveTextureToFileInMemory(&buf, D3DXIFF_DDS, (IDirect3DBaseTexture9 *)cube, NULL);
        check(hr == D3D_OK, "cube to DDS (%#lx)", hr);
        if (SUCCEEDED(hr))
        {
            hr = D3DXGetImageInfoFromFileInMemory(ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf), &info);
            check(hr == D3D_OK && info.ResourceType == D3DRTYPE_CUBETEXTURE && info.MipLevels == 5 && info.Width == 16,
                  "cube info: type %d, %u levels (%#lx)", info.ResourceType, info.MipLevels, hr);
            hr = D3DXCreateCubeTextureFromFileInMemoryEx(device, ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf),
                    D3DX_DEFAULT, D3DX_FROM_FILE, 0, D3DFMT_FROM_FILE, D3DPOOL_MANAGED, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &back);
            check(hr == D3D_OK, "cube read back (%#lx)", hr);
            if (SUCCEEDED(hr))
            {
                int ok = 1;

                for (f = 0; f < 6; f++)
                    for (l = 0; l < 5; l++)
                    {
                        IDirect3DCubeTexture9_LockRect(back, f, l, &lr, NULL, D3DLOCK_READONLY);
                        if (*(DWORD *)lr.pBits != level_color(f, l)) ok = 0;
                        IDirect3DCubeTexture9_UnlockRect(back, f, l);
                    }
                check(ok, "the pixels of all 6 faces and 5 levels are those that were saved");
                IDirect3DCubeTexture9_Release(back);
            }
            ID3DXBuffer_Release(buf);
        }
        IDirect3DCubeTexture9_Release(cube);
    }

    /* a volume texture */
    {
        IDirect3DVolumeTexture9 *vol, *back;
        D3DLOCKED_BOX box;

        IDirect3DDevice9_CreateVolumeTexture(device, 8, 8, 8, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &vol, NULL);
        fill_surface_levels((IDirect3DBaseTexture9 *)vol, D3DRTYPE_VOLUMETEXTURE, 4, 1);
        hr = D3DXSaveTextureToFileInMemory(&buf, D3DXIFF_DDS, (IDirect3DBaseTexture9 *)vol, NULL);
        check(hr == D3D_OK, "volume to DDS (%#lx)", hr);
        if (SUCCEEDED(hr))
        {
            hr = D3DXGetImageInfoFromFileInMemory(ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf), &info);
            check(hr == D3D_OK && info.ResourceType == D3DRTYPE_VOLUMETEXTURE && info.Depth == 8 && info.MipLevels == 4,
                  "volume info: type %d, depth %u, %u levels (%#lx)", info.ResourceType, info.Depth, info.MipLevels, hr);
            hr = D3DXCreateVolumeTextureFromFileInMemoryEx(device, ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf),
                    D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_FROM_FILE, 0, D3DFMT_FROM_FILE, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &back);
            check(hr == D3D_OK, "volume read back (%#lx)", hr);
            if (SUCCEEDED(hr))
            {
                int ok = 1;
                UINT z;

                IDirect3DVolumeTexture9_LockBox(back, 0, &box, NULL, D3DLOCK_READONLY);
                for (z = 0; z < 8; z++)
                    if (*(DWORD *)((BYTE *)box.pBits + z * box.SlicePitch) != level_color(z, 0)) ok = 0;
                IDirect3DVolumeTexture9_UnlockBox(back, 0);
                check(ok, "the 8 slices are those that were saved");
                IDirect3DVolumeTexture9_Release(back);
            }
            ID3DXBuffer_Release(buf);
        }
        /* as an image: the first slice */
        hr = D3DXSaveTextureToFileInMemory(&buf, D3DXIFF_BMP, (IDirect3DBaseTexture9 *)vol, NULL);
        check(hr == D3D_OK, "volume to BMP (%#lx)", hr);
        if (SUCCEEDED(hr))
        {
            hr = D3DXGetImageInfoFromFileInMemory(ID3DXBuffer_GetBufferPointer(buf), ID3DXBuffer_GetBufferSize(buf), &info);
            check(hr == D3D_OK && info.Width == 8 && info.Height == 8 && info.ResourceType == D3DRTYPE_TEXTURE && info.ImageFileFormat == D3DXIFF_BMP,
                  "BMP info %ux%u (%#lx)", info.Width, info.Height, hr);
            ID3DXBuffer_Release(buf);
        }
        IDirect3DVolumeTexture9_Release(vol);
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
