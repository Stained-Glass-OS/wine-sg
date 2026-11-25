/* D3DPRASTERCAPS_PAT (patches/sg/9307): wined3d's OpenGL core contexts have no glLineStipple, so the
 * device must not report line patterns; a pattern set anyway is ignored (the line is drawn solid),
 * as on a Direct3D driver without the capability (Direct3D 7 and 8 report the same wined3d caps). */
#define COBJMACROS
#include <windows.h>
#include <d3d8.h>
#include <stdio.h>

typedef IDirect3D8 *(WINAPI *create8_t)(UINT);

struct v { float x, y, z, rhw; DWORD c; };

int main(void)
{
    HWND hwnd = CreateWindowA("static", "linepat", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
    HMODULE m8 = LoadLibraryA("d3d8.dll");
    create8_t create8 = m8 ? (create8_t)GetProcAddress(m8, "Direct3DCreate8") : NULL;
    IDirect3D8 *d3d8 = create8 ? create8(D3D_SDK_VERSION) : NULL;
    D3DCAPS8 caps8;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!d3d8) { printf("no_d3d=1\n"); return 0; }

    hr = IDirect3D8_GetDeviceCaps(d3d8, 0, D3DDEVTYPE_HAL, &caps8);
    printf("d3d8_hr=%#lx raster=%#lx\n", hr, caps8.RasterCaps);
    printf("d3d8_pat=%d\n", hr == S_OK && (caps8.RasterCaps & D3DPRASTERCAPS_PAT));

    /* a pattern set anyway: the line is drawn whole */
    {
        D3DPRESENT_PARAMETERS pp = {0};
        IDirect3DDevice8 *dev;
        IDirect3DSurface8 *rt, *sys;
        D3DLOCKED_RECT lr;
        D3DLINEPATTERN pat = {1, 0x00ff};
        struct v line[2] = {{2.0f, 16.5f, 0.0f, 1.0f, 0xffffffff}, {62.0f, 16.5f, 0.0f, 1.0f, 0xffffffff}};
        int x, lit = 0;

        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_COPY; pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.hDeviceWindow = hwnd;
        hr = IDirect3D8_CreateDevice(d3d8, 0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
        if (FAILED(hr)) { printf("device8_hr=%#lx\n", hr); printf("done=1\n"); return 0; }
        IDirect3DDevice8_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
        IDirect3DDevice8_SetRenderState(dev, D3DRS_LINEPATTERN, *(DWORD *)&pat);
        IDirect3DDevice8_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 0.0f, 0);
        IDirect3DDevice8_BeginScene(dev);
        IDirect3DDevice8_SetVertexShader(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        IDirect3DDevice8_DrawPrimitiveUP(dev, D3DPT_LINELIST, 1, line, sizeof(line[0]));
        IDirect3DDevice8_EndScene(dev);
        IDirect3DDevice8_GetBackBuffer(dev, 0, D3DBACKBUFFER_TYPE_MONO, &rt);
        IDirect3DDevice8_CreateImageSurface(dev, 64, 64, D3DFMT_X8R8G8B8, &sys);
        IDirect3DDevice8_CopyRects(dev, rt, NULL, 0, sys, NULL);
        IDirect3DSurface8_LockRect(sys, &lr, NULL, D3DLOCK_READONLY);
        for (x = 8; x < 56; ++x)
            if ((((DWORD *)((BYTE *)lr.pBits + 16 * lr.Pitch))[x] & 0xffffff) == 0xffffff) ++lit;
        IDirect3DSurface8_UnlockRect(sys);
        printf("lit=%d\n", lit);
        printf("pattern_ignored=%d\n", lit == 48);
        /* reported and honoured (a legacy OpenGL context), or neither */
        printf("consistent=%d\n", (caps8.RasterCaps & D3DPRASTERCAPS_PAT) ? lit < 48 : lit == 48);
    }
    printf("done=1\n");
    return 0;
}
