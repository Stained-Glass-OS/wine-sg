/* d3dx9_36 (patches/sg/2650): D3DXIntersect tells whether a ray hits a mesh, the closest face with its
 * barycentric coordinates and distance, and every hit. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

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
    HWND window;
    HRESULT hr;
    ID3DXMesh *box = NULL;

    window = CreateWindowA("static", "isect", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;
    hr = D3DXCreateBox(device, 1.0f, 1.0f, 1.0f, &box, NULL);
    check(hr == D3D_OK && box, "box (%#lx)", hr);
    if (!box) goto done;

    {
        D3DXVECTOR3 pos = { 0.1f, 0.2f, -5.0f }, dir = { 0.0f, 0.0f, 1.0f };
        BOOL hit = FALSE;
        DWORD face = 99, count = 99;
        float u = -1, v = -1, dist = -1;
        ID3DXBuffer *all = NULL;
        D3DXINTERSECTINFO *infos;

        hr = D3DXIntersect((ID3DXBaseMesh *)box, &pos, &dir, &hit, &face, &u, &v, &dist, &all, &count);
        check(hr == D3D_OK && hit, "the ray hits the box (%#lx)", hr);
        check(count == 2, "it goes in and out: %lu hits", count);
        check(fabsf(dist - 4.5f) < 0.001f, "the closest hit is 4.5 away (%g)", dist);
        check(face < 12 && u >= 0 && v >= 0 && u + v <= 1.0f, "face %lu, barycentric %g, %g", face, u, v);
        if (all)
        {
            infos = ID3DXBuffer_GetBufferPointer(all);
            check(ID3DXBuffer_GetBufferSize(all) == 2 * sizeof(*infos), "the buffer holds the two hits");
            check((fabsf(infos[0].Dist - 4.5f) < 0.001f && fabsf(infos[1].Dist - 5.5f) < 0.001f)
                    || (fabsf(infos[1].Dist - 4.5f) < 0.001f && fabsf(infos[0].Dist - 5.5f) < 0.001f), "at 4.5 and 5.5 (%g, %g)", infos[0].Dist, infos[1].Dist);
            ID3DXBuffer_Release(all);
        }
        else check(0, "all hits");

        /* the far face first in the mesh order when the ray comes from behind */
        pos.z = 5.0f; dir.z = -1.0f;
        hit = FALSE;
        hr = D3DXIntersect((ID3DXBaseMesh *)box, &pos, &dir, &hit, &face, &u, &v, &dist, NULL, &count);
        check(hr == D3D_OK && hit && fabsf(dist - 4.5f) < 0.001f && count == 2, "from the other side: %g, %lu hits", dist, count);

        /* a miss */
        pos.x = 3.0f; pos.z = -5.0f; dir.z = 1.0f;
        hit = TRUE; count = 99;
        hr = D3DXIntersect((ID3DXBaseMesh *)box, &pos, &dir, &hit, &face, &u, &v, &dist, &all, &count);
        check(hr == D3D_OK && !hit && count == 0 && !all, "a ray beside the box (%#lx, hit %d, %lu hits)", hr, hit, count);

        /* away from the box */
        pos.x = 0.0f; pos.z = -5.0f; dir.z = -1.0f;
        hr = D3DXIntersect((ID3DXBaseMesh *)box, &pos, &dir, &hit, NULL, NULL, NULL, NULL, NULL, &count);
        check(hr == D3D_OK && !hit && count == 0, "a ray going away");

        check(D3DXIntersect(NULL, &pos, &dir, &hit, NULL, NULL, NULL, NULL, NULL, NULL) == D3DERR_INVALIDCALL, "no mesh");
        check(D3DXIntersect((ID3DXBaseMesh *)box, NULL, &dir, &hit, NULL, NULL, NULL, NULL, NULL, NULL) == D3DERR_INVALIDCALL, "no position");
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
