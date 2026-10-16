/* wined3d smoke (d3d9): clip status, N-patch mode and software vertex processing round-trip;
   ProcessVertices with a destination vertex declaration. */
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device = NULL;
    D3DPRESENT_PARAMETERS pp = {0};
    D3DCLIPSTATUS9 cs, got;
    IDirect3DVertexBuffer9 *src, *dst;
    IDirect3DVertexDeclaration9 *decl;
    HWND hwnd;
    HRESULT hr;
    float *f;
    void *p;
    int i;
    struct { float x, y, z; } verts[3] = {{-1, -1, 0}, {0, 1, 0}, {1, -1, 0}};
    static const D3DVERTEXELEMENT9 elems[] =
    {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0},
        {0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END()
    };
    struct { float x, y, z, w; DWORD color; } out[3];

    hwnd = CreateWindowA("static", "p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, NULL, NULL);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { printf("FAIL no d3d9\n"); return 1; }
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = hwnd; pp.BackBufferWidth = 640; pp.BackBufferHeight = 480;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr)) { printf("SKIP no device %#lx\nRESULT: PASS\n", hr); return 0; }

    cs.ClipUnion = 0x3f; cs.ClipIntersection = 0x15;
    hr = IDirect3DDevice9_SetClipStatus(device, &cs);
    CHECK(hr == D3D_OK, "SetClipStatus %#lx", hr);
    memset(&got, 0, sizeof(got));
    hr = IDirect3DDevice9_GetClipStatus(device, &got);
    CHECK(hr == D3D_OK && got.ClipUnion == 0x3f && got.ClipIntersection == 0x15, "GetClipStatus %#lx %#lx %#lx", hr, got.ClipUnion, got.ClipIntersection);

    hr = IDirect3DDevice9_SetNPatchMode(device, 3.0f);
    CHECK(hr == D3D_OK, "SetNPatchMode %#lx", hr);
    CHECK(IDirect3DDevice9_GetNPatchMode(device) == 3.0f, "GetNPatchMode %f", IDirect3DDevice9_GetNPatchMode(device));
    IDirect3DDevice9_SetNPatchMode(device, 0.0f);
    CHECK(IDirect3DDevice9_GetNPatchMode(device) == 0.0f, "GetNPatchMode reset");

    hr = IDirect3DDevice9_SetSoftwareVertexProcessing(device, TRUE);
    CHECK(hr == D3D_OK && IDirect3DDevice9_GetSoftwareVertexProcessing(device) == TRUE, "swvp on %#lx", hr);

    IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE);
    hr = IDirect3DDevice9_CreateVertexBuffer(device, sizeof(verts), 0, D3DFVF_XYZ, D3DPOOL_SYSTEMMEM, &src, NULL);
    CHECK(hr == D3D_OK, "src vb %#lx", hr);
    IDirect3DVertexBuffer9_Lock(src, 0, 0, &p, 0); memcpy(p, verts, sizeof(verts)); IDirect3DVertexBuffer9_Unlock(src);
    hr = IDirect3DDevice9_CreateVertexBuffer(device, sizeof(out), 0, 0, D3DPOOL_SYSTEMMEM, &dst, NULL);
    CHECK(hr == D3D_OK, "dst vb %#lx", hr);
    hr = IDirect3DDevice9_CreateVertexDeclaration(device, elems, &decl);
    CHECK(hr == D3D_OK, "decl %#lx", hr);
    IDirect3DDevice9_SetFVF(device, D3DFVF_XYZ);
    IDirect3DDevice9_SetStreamSource(device, 0, src, 0, sizeof(verts[0]));
    hr = IDirect3DDevice9_ProcessVertices(device, 0, 0, 3, dst, decl, 0);
    CHECK(hr == D3D_OK, "ProcessVertices %#lx", hr);
    IDirect3DVertexBuffer9_Lock(dst, 0, 0, &p, 0);
    memcpy(out, p, sizeof(out));
    IDirect3DVertexBuffer9_Unlock(dst);
    for (i = 0; i < 3; ++i)
    {
        float ex = verts[i].x * 320.0f + 320.0f, ey = -verts[i].y * 240.0f + 240.0f;
        CHECK(fabsf(out[i].x - ex) < 1.0f && fabsf(out[i].y - ey) < 1.0f && out[i].w > 0.5f,
                "vertex %d {%f %f %f %f}", i, out[i].x, out[i].y, out[i].z, out[i].w);
    }
    (void)f;
    printf("%s\nRESULT: %s\n", failed ? "failures" : "ok", failed ? "FAIL" : "PASS");
    IDirect3DDevice9_Release(device); IDirect3D9_Release(d3d);
    return failed != 0;
}
