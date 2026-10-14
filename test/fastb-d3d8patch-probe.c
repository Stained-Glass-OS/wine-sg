/* d3d8 (patch 3681): DrawRectPatch, DrawTriPatch, DeletePatch. */
#define COBJMACROS
#include <windows.h>
#include <d3d8.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define SETSRC8(dev, vb, stride) IDirect3DDevice8_SetStreamSource(dev, 0, vb, stride)
#define GETSRC8(dev, vb, stride) IDirect3DDevice8_GetStreamSource(dev, 0, vb, stride)
#define SETFVF8(dev, f) IDirect3DDevice8_SetVertexShader(dev, f)
#define GETIB8(dev, ib) do { UINT b; IDirect3DDevice8_GetIndices(dev, ib, &b); } while (0)
#define CREATEVB8(dev, len, vb) IDirect3DDevice8_CreateVertexBuffer(dev, len, 0, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, D3DPOOL_MANAGED, vb)
static HRESULT GETPIX8(IDirect3DDevice8 *dev, IDirect3DSurface8 *back, IDirect3DSurface8 **surf)
{
    HRESULT hr = IDirect3DDevice8_CreateImageSurface(dev, 64, 64, D3DFMT_X8R8G8B8, surf);
    if (SUCCEEDED(hr)) hr = IDirect3DDevice8_CopyRects(dev, back, NULL, 0, *surf, NULL);
    return hr;
}
/* ---- D3D8 ---- */
static IDirect3DDevice8 *dev8;

static DWORD pixel8(int x, int y)
{
    IDirect3DSurface8 *back = NULL, *surf = NULL;
    D3DLOCKED_RECT lr;
    DWORD ret = 0xdeadbeef;

    if (FAILED(IDirect3DDevice8_GetBackBuffer(dev8, 0, D3DBACKBUFFER_TYPE_MONO, &back))) return ret;
    if (SUCCEEDED(GETPIX8(dev8, back, &surf)))
    {
        if (SUCCEEDED(IDirect3DSurface8_LockRect(surf, &lr, NULL, D3DLOCK_READONLY)))
        {
            ret = ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x] & 0xffffff;
            IDirect3DSurface8_UnlockRect(surf);
        }
        IDirect3DSurface8_Release(surf);
    }
    IDirect3DSurface8_Release(back);
    return ret;
}

struct vtx { float x, y, z, rhw; DWORD color; };

static IDirect3DVertexBuffer8 *make_vb8(const struct vtx *v, UINT count)
{
    IDirect3DVertexBuffer8 *vb = NULL;
    void *p;

    if (FAILED(CREATEVB8(dev8, count * sizeof(*v), &vb))) return NULL;
    IDirect3DVertexBuffer8_Lock(vb, 0, 0, (void *)&p, 0);
    memcpy(p, v, count * sizeof(*v));
    IDirect3DVertexBuffer8_Unlock(vb);
    return vb;
}

static void clear8(void)
{
    IDirect3DDevice8_Clear(dev8, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
}

static DWORD red8(DWORD c) { return (c >> 16) & 0xff; }

static void test_patch8(void)
{
    static const float segs[4] = {16, 16, 16, 16};
    struct vtx v[28];
    IDirect3DVertexBuffer8 *vb, *vb_after = NULL;
    IDirect3DIndexBuffer8 *ib_after = NULL;
    D3DRECTPATCH_INFO info;
    D3DTRIPATCH_INFO tri;
    unsigned i, j;
    HRESULT hr;
    DWORD c;
    UINT s;

    if (!dev8) { printf("SKIP  no d3d8 device\n"); return; }
    IDirect3DDevice8_SetRenderState(dev8, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice8_SetRenderState(dev8, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice8_SetRenderState(dev8, D3DRS_CULLMODE, D3DCULL_NONE);
    SETFVF8(dev8, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

    /* 4x4 bezier patch: x = 8 + 16 i, y = 8 + 16 j, red = 0, 0, 255, 255 across i */
    for (j = 0; j < 4; ++j) for (i = 0; i < 4; ++i)
    {
        v[j * 4 + i].x = 8 + 16 * i; v[j * 4 + i].y = 8 + 16 * j; v[j * 4 + i].z = 0; v[j * 4 + i].rhw = 1;
        v[j * 4 + i].color = 0xff000000 | ((i >= 2 ? 255 : 0) << 16);
    }
    vb = make_vb8(v, 16);
    check(vb != NULL, "8 vertex buffer");
    if (!vb) return;
    SETSRC8(dev8, vb, sizeof(struct vtx));
    memset(&info, 0, sizeof(info));
    info.Width = 4; info.Height = 4; info.Stride = 4; info.Basis = D3DBASIS_BEZIER; info.Order = 3;

    clear8();
    IDirect3DDevice8_BeginScene(dev8);
    hr = IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, &info);
    IDirect3DDevice8_EndScene(dev8);
    check(hr == D3D_OK, "8 DrawRectPatch cubic bezier");
    check(pixel8(2, 2) == 0 && pixel8(60, 60) == 0, "8 nothing outside the patch");
    c = pixel8(32, 32);
    check(c != 0 && (c >> 8 & 0xff) == 0, "8 the patch is filled");
    c = red8(pixel8(20, 32));
    check(c >= 32 && c <= 50, "8 colour follows the cubic Bernstein weights at t = 1/4 (about 40, not 64)");
    c = red8(pixel8(44, 32));
    check(c >= 205 && c <= 225, "8 colour at t = 3/4 (about 215, not 191)");
    check(pixel8(8 + 1, 8 + 1) != 0 && pixel8(55, 55) != 0, "8 the corners reach the corner control points");

    /* the caller's stream and index bindings are untouched */
    GETSRC8(dev8, &vb_after, &s);
    check(vb_after == vb && s == sizeof(struct vtx), "8 stream 0 is still the control point buffer");
    if (vb_after) IDirect3DVertexBuffer8_Release(vb_after);
    GETIB8(dev8, &ib_after);
    check(ib_after == NULL, "8 no index buffer appeared");
    if (ib_after) IDirect3DIndexBuffer8_Release(ib_after);

    /* uniform cubic B-spline: one patch, it covers only the middle of the control grid */
    info.Basis = D3DBASIS_BSPLINE;
    clear8();
    IDirect3DDevice8_BeginScene(dev8);
    hr = IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, &info);
    IDirect3DDevice8_EndScene(dev8);
    check(hr == D3D_OK, "8 DrawRectPatch cubic B-spline");
    check(pixel8(14, 32) == 0 && pixel8(50, 32) == 0 && pixel8(32, 32) != 0, "8 the B-spline stays inside the control polygon");
    check(pixel8(26, 32) != 0 && pixel8(38, 32) != 0, "8 the B-spline spans [24, 40]");

    /* catmull-rom (D3DBASIS_INTERPOLATE): passes through the inner control points: covers [24, 40] too */
    info.Basis = D3DBASIS_INTERPOLATE;
    clear8();
    IDirect3DDevice8_BeginScene(dev8);
    hr = IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, &info);
    IDirect3DDevice8_EndScene(dev8);
    check(hr == D3D_OK, "8 DrawRectPatch Catmull-Rom");
    check(pixel8(14, 32) == 0 && pixel8(32, 32) != 0 && pixel8(26, 26) != 0, "8 Catmull-Rom spans the inner control points");

    /* a 7 x 4 grid is two bezier patches */
    for (j = 0; j < 4; ++j) for (i = 0; i < 7; ++i)
    {
        v[j * 7 + i].x = 4 + 8 * i; v[j * 7 + i].y = 8 + 16 * j; v[j * 7 + i].z = 0; v[j * 7 + i].rhw = 1; v[j * 7 + i].color = 0xffffffff;
    }
    IDirect3DVertexBuffer8_Release(vb);
    vb = make_vb8(v, 28);
    SETSRC8(dev8, vb, sizeof(struct vtx));
    info.Basis = D3DBASIS_BEZIER; info.Width = 7; info.Stride = 7;
    clear8();
    IDirect3DDevice8_BeginScene(dev8);
    hr = IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, &info);
    IDirect3DDevice8_EndScene(dev8);
    check(hr == D3D_OK, "8 DrawRectPatch with two patches");
    check(pixel8(10, 32) != 0 && pixel8(26, 32) != 0 && pixel8(46, 32) != 0 && pixel8(58, 32) == 0, "8 both patches are drawn");

    /* a width that is not 3k + 1 is no bezier patch grid */
    info.Width = 5;
    check(IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, &info) == D3DERR_INVALIDCALL, "8 invalid control point count is refused");
    info.Width = 7; info.Stride = 7;
    info.Order = 4;
    check(IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, &info) == D3DERR_INVALIDCALL, "8 invalid degree is refused");
    info.Order = 3;
    info.StartVertexOffsetHeight = 3;
    check(IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, &info) == D3DERR_INVALIDCALL, "8 control points past the buffer are refused");
    info.StartVertexOffsetHeight = 0;
    check(IDirect3DDevice8_DrawRectPatch(dev8, 0, NULL, &info) == D3DERR_INVALIDCALL, "8 no segment counts is refused");
    check(IDirect3DDevice8_DrawRectPatch(dev8, 0, segs, NULL) == D3DERR_INVALIDCALL, "8 handle 0 without a description is refused");

    /* handles: a patch with a handle is remembered and drawn again without a description */
    clear8();
    IDirect3DDevice8_BeginScene(dev8);
    hr = IDirect3DDevice8_DrawRectPatch(dev8, 5, segs, &info);
    IDirect3DDevice8_EndScene(dev8);
    check(hr == D3D_OK && pixel8(26, 32) != 0, "8 DrawRectPatch with handle 5");
    clear8();
    check(pixel8(26, 32) == 0, "8 cleared");
    IDirect3DDevice8_BeginScene(dev8);
    hr = IDirect3DDevice8_DrawRectPatch(dev8, 5, segs, NULL);
    IDirect3DDevice8_EndScene(dev8);
    check(hr == D3D_OK && pixel8(26, 32) != 0, "8 the cached patch draws again");
    check(IDirect3DDevice8_DeletePatch(dev8, 5) == D3D_OK, "8 DeletePatch");
    check(IDirect3DDevice8_DeletePatch(dev8, 5) == D3DERR_INVALIDCALL, "8 DeletePatch twice");
    check(IDirect3DDevice8_DrawRectPatch(dev8, 5, segs, NULL) == D3DERR_INVALIDCALL, "8 a deleted patch cannot be drawn");
    IDirect3DVertexBuffer8_Release(vb);

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
    vb = make_vb8(v, 10);
    SETSRC8(dev8, vb, sizeof(struct vtx));
    memset(&tri, 0, sizeof(tri));
    tri.NumVertices = 10; tri.Basis = D3DBASIS_BEZIER; tri.Order = 3;
    clear8();
    IDirect3DDevice8_BeginScene(dev8);
    hr = IDirect3DDevice8_DrawTriPatch(dev8, 0, segs, &tri);
    IDirect3DDevice8_EndScene(dev8);
    check(hr == D3D_OK, "8 DrawTriPatch cubic");
    check(pixel8(32, 40) != 0 && pixel8(14, 52) != 0 && pixel8(50, 52) != 0 && pixel8(32, 14) != 0, "8 the triangle is filled");
    check(pixel8(8, 8) == 0 && pixel8(56, 8) == 0 && pixel8(32, 60) == 0, "8 nothing outside the triangle");
    tri.NumVertices = 9;
    check(IDirect3DDevice8_DrawTriPatch(dev8, 0, segs, &tri) == D3DERR_INVALIDCALL, "8 too few control points are refused");
    tri.NumVertices = 10; tri.Order = 2;
    clear8();
    check(IDirect3DDevice8_DrawTriPatch(dev8, 0, segs, &tri) == D3D_OK, "8 quadratic uses the first six control points");
    IDirect3DVertexBuffer8_Release(vb);
}

int main(void)
{
    HWND win = CreateWindowA("static", "patch", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    IDirect3D8 *d8;
    D3DPRESENT_PARAMETERS pp;
    IDirect3D8 *(WINAPI *c8)(UINT) = (void *)GetProcAddress(LoadLibraryA("d3d8.dll"), "Direct3DCreate8");

    if (c8 && (d8 = c8(220)))
    {
        memset(&pp, 0, sizeof(pp));
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.hDeviceWindow = win;
        if (FAILED(IDirect3D8_CreateDevice(d8, 0, D3DDEVTYPE_HAL, win, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev8)))
            dev8 = NULL;
    }
    test_patch8();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
