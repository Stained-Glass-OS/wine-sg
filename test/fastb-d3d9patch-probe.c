/* d3d9 (patch 3681): DrawRectPatch, DrawTriPatch, DeletePatch. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define SETSRC9(dev, vb, stride) IDirect3DDevice9_SetStreamSource(dev, 0, vb, 0, stride)
#define GETSRC9(dev, vb, stride) do { UINT o; IDirect3DDevice9_GetStreamSource(dev, 0, vb, &o, stride); } while (0)
#define SETFVF9(dev, f) IDirect3DDevice9_SetFVF(dev, f)
#define GETIB9(dev, ib) IDirect3DDevice9_GetIndices(dev, ib)
#define CREATEVB9(dev, len, vb) IDirect3DDevice9_CreateVertexBuffer(dev, len, 0, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, D3DPOOL_MANAGED, vb, NULL)
static HRESULT GETPIX9(IDirect3DDevice9 *dev, IDirect3DSurface9 *back, IDirect3DSurface9 **surf)
{
    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(dev, 64, 64, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, surf, NULL);
    if (SUCCEEDED(hr)) hr = IDirect3DDevice9_GetRenderTargetData(dev, back, *surf);
    return hr;
}
/* ---- D3D9 ---- */
static IDirect3DDevice9 *dev9;

static DWORD pixel9(int x, int y)
{
    IDirect3DSurface9 *back = NULL, *surf = NULL;
    D3DLOCKED_RECT lr;
    DWORD ret = 0xdeadbeef;

    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev9, 0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) return ret;
    if (SUCCEEDED(GETPIX9(dev9, back, &surf)))
    {
        if (SUCCEEDED(IDirect3DSurface9_LockRect(surf, &lr, NULL, D3DLOCK_READONLY)))
        {
            ret = ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x] & 0xffffff;
            IDirect3DSurface9_UnlockRect(surf);
        }
        IDirect3DSurface9_Release(surf);
    }
    IDirect3DSurface9_Release(back);
    return ret;
}

struct vtx { float x, y, z, rhw; DWORD color; };

static IDirect3DVertexBuffer9 *make_vb9(const struct vtx *v, UINT count)
{
    IDirect3DVertexBuffer9 *vb = NULL;
    void *p;

    if (FAILED(CREATEVB9(dev9, count * sizeof(*v), &vb))) return NULL;
    IDirect3DVertexBuffer9_Lock(vb, 0, 0, (void *)&p, 0);
    memcpy(p, v, count * sizeof(*v));
    IDirect3DVertexBuffer9_Unlock(vb);
    return vb;
}

static void clear9(void)
{
    IDirect3DDevice9_Clear(dev9, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
}

static DWORD red9(DWORD c) { return (c >> 16) & 0xff; }

static void test_patch9(void)
{
    static const float segs[4] = {16, 16, 16, 16};
    struct vtx v[28];
    IDirect3DVertexBuffer9 *vb, *vb_after = NULL;
    IDirect3DIndexBuffer9 *ib_after = NULL;
    D3DRECTPATCH_INFO info;
    D3DTRIPATCH_INFO tri;
    unsigned i, j;
    HRESULT hr;
    DWORD c;
    UINT s;

    if (!dev9) { printf("SKIP  no d3d9 device\n"); return; }
    IDirect3DDevice9_SetRenderState(dev9, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev9, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev9, D3DRS_CULLMODE, D3DCULL_NONE);
    SETFVF9(dev9, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

    /* 4x4 bezier patch: x = 8 + 16 i, y = 8 + 16 j, red = 0, 0, 255, 255 across i */
    for (j = 0; j < 4; ++j) for (i = 0; i < 4; ++i)
    {
        v[j * 4 + i].x = 8 + 16 * i; v[j * 4 + i].y = 8 + 16 * j; v[j * 4 + i].z = 0; v[j * 4 + i].rhw = 1;
        v[j * 4 + i].color = 0xff000000 | ((i >= 2 ? 255 : 0) << 16);
    }
    vb = make_vb9(v, 16);
    check(vb != NULL, "9 vertex buffer");
    if (!vb) return;
    SETSRC9(dev9, vb, sizeof(struct vtx));
    memset(&info, 0, sizeof(info));
    info.Width = 4; info.Height = 4; info.Stride = 4; info.Basis = D3DBASIS_BEZIER; info.Degree = 3;

    clear9();
    IDirect3DDevice9_BeginScene(dev9);
    hr = IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, &info);
    IDirect3DDevice9_EndScene(dev9);
    check(hr == D3D_OK, "9 DrawRectPatch cubic bezier");
    check(pixel9(2, 2) == 0 && pixel9(60, 60) == 0, "9 nothing outside the patch");
    c = pixel9(32, 32);
    check(c != 0 && (c >> 8 & 0xff) == 0, "9 the patch is filled");
    c = red9(pixel9(20, 32));
    check(c >= 32 && c <= 50, "9 colour follows the cubic Bernstein weights at t = 1/4 (about 40, not 64)");
    c = red9(pixel9(44, 32));
    check(c >= 205 && c <= 225, "9 colour at t = 3/4 (about 215, not 191)");
    check(pixel9(8 + 1, 8 + 1) != 0 && pixel9(55, 55) != 0, "9 the corners reach the corner control points");

    /* the caller's stream and index bindings are untouched */
    GETSRC9(dev9, &vb_after, &s);
    check(vb_after == vb && s == sizeof(struct vtx), "9 stream 0 is still the control point buffer");
    if (vb_after) IDirect3DVertexBuffer9_Release(vb_after);
    GETIB9(dev9, &ib_after);
    check(ib_after == NULL, "9 no index buffer appeared");
    if (ib_after) IDirect3DIndexBuffer9_Release(ib_after);

    /* uniform cubic B-spline: one patch, it covers only the middle of the control grid */
    info.Basis = D3DBASIS_BSPLINE;
    clear9();
    IDirect3DDevice9_BeginScene(dev9);
    hr = IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, &info);
    IDirect3DDevice9_EndScene(dev9);
    check(hr == D3D_OK, "9 DrawRectPatch cubic B-spline");
    check(pixel9(14, 32) == 0 && pixel9(50, 32) == 0 && pixel9(32, 32) != 0, "9 the B-spline stays inside the control polygon");
    check(pixel9(26, 32) != 0 && pixel9(38, 32) != 0, "9 the B-spline spans [24, 40]");

    /* catmull-rom (D3DBASIS_INTERPOLATE): passes through the inner control points: covers [24, 40] too */
    info.Basis = D3DBASIS_INTERPOLATE;
    clear9();
    IDirect3DDevice9_BeginScene(dev9);
    hr = IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, &info);
    IDirect3DDevice9_EndScene(dev9);
    check(hr == D3D_OK, "9 DrawRectPatch Catmull-Rom");
    check(pixel9(14, 32) == 0 && pixel9(32, 32) != 0 && pixel9(26, 26) != 0, "9 Catmull-Rom spans the inner control points");

    /* a 7 x 4 grid is two bezier patches */
    for (j = 0; j < 4; ++j) for (i = 0; i < 7; ++i)
    {
        v[j * 7 + i].x = 4 + 8 * i; v[j * 7 + i].y = 8 + 16 * j; v[j * 7 + i].z = 0; v[j * 7 + i].rhw = 1; v[j * 7 + i].color = 0xffffffff;
    }
    IDirect3DVertexBuffer9_Release(vb);
    vb = make_vb9(v, 28);
    SETSRC9(dev9, vb, sizeof(struct vtx));
    info.Basis = D3DBASIS_BEZIER; info.Width = 7; info.Stride = 7;
    clear9();
    IDirect3DDevice9_BeginScene(dev9);
    hr = IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, &info);
    IDirect3DDevice9_EndScene(dev9);
    check(hr == D3D_OK, "9 DrawRectPatch with two patches");
    check(pixel9(10, 32) != 0 && pixel9(26, 32) != 0 && pixel9(46, 32) != 0 && pixel9(58, 32) == 0, "9 both patches are drawn");

    /* a width that is not 3k + 1 is no bezier patch grid */
    info.Width = 5;
    check(IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, &info) == D3DERR_INVALIDCALL, "9 invalid control point count is refused");
    info.Width = 7; info.Stride = 7;
    info.Degree = 4;
    check(IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, &info) == D3DERR_INVALIDCALL, "9 invalid degree is refused");
    info.Degree = 3;
    info.StartVertexOffsetHeight = 3;
    check(IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, &info) == D3DERR_INVALIDCALL, "9 control points past the buffer are refused");
    info.StartVertexOffsetHeight = 0;
    check(IDirect3DDevice9_DrawRectPatch(dev9, 0, NULL, &info) == D3DERR_INVALIDCALL, "9 no segment counts is refused");
    check(IDirect3DDevice9_DrawRectPatch(dev9, 0, segs, NULL) == D3DERR_INVALIDCALL, "9 handle 0 without a description is refused");

    /* handles: a patch with a handle is remembered and drawn again without a description */
    clear9();
    IDirect3DDevice9_BeginScene(dev9);
    hr = IDirect3DDevice9_DrawRectPatch(dev9, 5, segs, &info);
    IDirect3DDevice9_EndScene(dev9);
    check(hr == D3D_OK && pixel9(26, 32) != 0, "9 DrawRectPatch with handle 5");
    clear9();
    check(pixel9(26, 32) == 0, "9 cleared");
    IDirect3DDevice9_BeginScene(dev9);
    hr = IDirect3DDevice9_DrawRectPatch(dev9, 5, segs, NULL);
    IDirect3DDevice9_EndScene(dev9);
    check(hr == D3D_OK && pixel9(26, 32) != 0, "9 the cached patch draws again");
    check(IDirect3DDevice9_DeletePatch(dev9, 5) == D3D_OK, "9 DeletePatch");
    check(IDirect3DDevice9_DeletePatch(dev9, 5) == D3DERR_INVALIDCALL, "9 DeletePatch twice");
    check(IDirect3DDevice9_DrawRectPatch(dev9, 5, segs, NULL) == D3DERR_INVALIDCALL, "9 a deleted patch cannot be drawn");
    IDirect3DVertexBuffer9_Release(vb);

    /* triangle patch, cubic: 10 control points on the triangle (32,8) (8,56) (56,56), row by row */
    {
        static const float ax = 32, ay = 8, bx = 8, by = 56, cx = 56, cy = 56;
        unsigned r, cc, k = 0;

        for (r = 0; r <= 3; ++r) for (cc = 0; cc <= r; ++cc)
        {
            float ii = 3 - r, jj = r - cc, kk = cc;
            v[k].x = (ii * ax + jj * bx + kk * cx) / 3; v[k].y = (ii * ay + jj * by + kk * cy) / 3;
            v[k].z = 0; v[k].rhw = 1; v[k].color = 0xffffffff;
            ++k;
        }
    }
    vb = make_vb9(v, 10);
    SETSRC9(dev9, vb, sizeof(struct vtx));
    memset(&tri, 0, sizeof(tri));
    tri.NumVertices = 10; tri.Basis = D3DBASIS_BEZIER; tri.Degree = 3;
    clear9();
    IDirect3DDevice9_BeginScene(dev9);
    hr = IDirect3DDevice9_DrawTriPatch(dev9, 0, segs, &tri);
    IDirect3DDevice9_EndScene(dev9);
    check(hr == D3D_OK, "9 DrawTriPatch cubic");
    check(pixel9(32, 40) != 0 && pixel9(14, 52) != 0 && pixel9(50, 52) != 0 && pixel9(32, 14) != 0, "9 the triangle is filled");
    check(pixel9(8, 8) == 0 && pixel9(56, 8) == 0 && pixel9(32, 60) == 0, "9 nothing outside the triangle");
    tri.NumVertices = 9;
    check(IDirect3DDevice9_DrawTriPatch(dev9, 0, segs, &tri) == D3DERR_INVALIDCALL, "9 too few control points are refused");
    tri.NumVertices = 10; tri.Degree = 2;
    clear9();
    check(IDirect3DDevice9_DrawTriPatch(dev9, 0, segs, &tri) == D3D_OK, "9 quadratic uses the first six control points");
    IDirect3DVertexBuffer9_Release(vb);
}

int main(void)
{
    HWND win = CreateWindowA("static", "patch", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    IDirect3D9 *d9;
    D3DPRESENT_PARAMETERS pp;
    IDirect3D9 *(WINAPI *c9)(UINT) = (void *)GetProcAddress(LoadLibraryA("d3d9.dll"), "Direct3DCreate9");

    if (c9 && (d9 = c9(D3D_SDK_VERSION)))
    {
        memset(&pp, 0, sizeof(pp));
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.hDeviceWindow = win;
        if (FAILED(IDirect3D9_CreateDevice(d9, 0, D3DDEVTYPE_HAL, win, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev9)))
            dev9 = NULL;
    }
    test_patch9();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
