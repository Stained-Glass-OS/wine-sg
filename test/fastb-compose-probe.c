/* d3d9 IDirect3DDevice9Ex::ComposeRects (patch 3693) */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct sd { USHORT x, y, w, h; };
struct dd { USHORT src, reserved, x, y; };

static DWORD px(IDirect3DSurface9 *s, int x, int y)
{
    D3DLOCKED_RECT lr; DWORD v = 0xdead;
    if (SUCCEEDED(IDirect3DSurface9_LockRect(s, &lr, NULL, D3DLOCK_READONLY)))
    { v = ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x]; IDirect3DSurface9_UnlockRect(s); }
    return v;
}

static void fill(IDirect3DSurface9 *s, DWORD v, int w, int h)
{
    D3DLOCKED_RECT lr; int x, y;
    IDirect3DSurface9_LockRect(s, &lr, NULL, 0);
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x] = v;
    IDirect3DSurface9_UnlockRect(s);
}

int main(void)
{
    HWND win = CreateWindowA("static", "compose", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    HRESULT (WINAPI *c9ex)(UINT, IDirect3D9Ex **) = (void *)GetProcAddress(LoadLibraryA("d3d9.dll"), "Direct3DCreate9Ex");
    IDirect3D9Ex *d3d = NULL;
    IDirect3DDevice9Ex *dev = NULL;
    D3DPRESENT_PARAMETERS pp;
    IDirect3DSurface9 *src = NULL, *dst = NULL;
    IDirect3DVertexBuffer9 *sv = NULL, *dv = NULL;
    HRESULT hr;
    void *p;
    struct sd srcs[2] = { { 0, 0, 4, 4 }, { 4, 4, 2, 2 } };
    struct dd dsts[2] = { { 0, 0, 10, 10 }, { 1, 0, 20, 12 } };

    if (!c9ex || FAILED(c9ex(D3D_SDK_VERSION, &d3d))) { printf("SKIP no d3d9ex\nRESULT: PASS\n"); return 0; }
    memset(&pp, 0, sizeof(pp));
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.hDeviceWindow = win;
    hr = IDirect3D9Ex_CreateDeviceEx(d3d, 0, D3DDEVTYPE_HAL, win, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL, &dev);
    CHECK(hr == S_OK, "CreateDeviceEx %#lx", hr);
    if (!dev) return 1;

    IDirect3DDevice9Ex_CreateOffscreenPlainSurface(dev, 32, 32, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &src, NULL);
    IDirect3DDevice9Ex_CreateOffscreenPlainSurface(dev, 32, 32, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &dst, NULL);
    IDirect3DDevice9Ex_CreateVertexBuffer(dev, sizeof(srcs), 0, 0, D3DPOOL_SYSTEMMEM, &sv, NULL);
    IDirect3DDevice9Ex_CreateVertexBuffer(dev, sizeof(dsts), 0, 0, D3DPOOL_SYSTEMMEM, &dv, NULL);
    IDirect3DVertexBuffer9_Lock(sv, 0, 0, &p, 0); memcpy(p, srcs, sizeof(srcs)); IDirect3DVertexBuffer9_Unlock(sv);
    IDirect3DVertexBuffer9_Lock(dv, 0, 0, &p, 0); memcpy(p, dsts, sizeof(dsts)); IDirect3DVertexBuffer9_Unlock(dv);

    fill(src, 0xff0000ff, 32, 32);
    fill(dst, 0, 32, 32);
    hr = IDirect3DDevice9Ex_ComposeRects(dev, src, dst, sv, 2, dv, D3DCOMPOSERECTS_COPY, 0, 0);
    CHECK(hr == S_OK, "COPY %#lx", hr);
    CHECK(px(dst, 10, 10) == 0xff0000ff && px(dst, 13, 13) == 0xff0000ff, "first rect copied");
    CHECK(px(dst, 14, 10) == 0 && px(dst, 9, 10) == 0, "first rect size 4x4");
    CHECK(px(dst, 20, 12) == 0xff0000ff && px(dst, 21, 13) == 0xff0000ff && px(dst, 22, 12) == 0, "second rect 2x2 from source index 1");

    fill(dst, 0, 32, 32);
    hr = IDirect3DDevice9Ex_ComposeRects(dev, src, dst, sv, 2, dv, D3DCOMPOSERECTS_COPY, 5, 3);
    CHECK(hr == S_OK && px(dst, 15, 13) == 0xff0000ff && px(dst, 10, 10) == 0, "offset applied: %#lx", (unsigned long)px(dst, 15, 13));

    fill(dst, 0x00ff0000, 32, 32);
    IDirect3DDevice9Ex_ComposeRects(dev, src, dst, sv, 2, dv, D3DCOMPOSERECTS_OR, 0, 0);
    CHECK(px(dst, 10, 10) == 0xffff00ff && px(dst, 0, 0) == 0x00ff0000, "OR: %#lx", (unsigned long)px(dst, 10, 10));
    fill(dst, 0xffff0000, 32, 32);
    IDirect3DDevice9Ex_ComposeRects(dev, src, dst, sv, 2, dv, D3DCOMPOSERECTS_AND, 0, 0);
    CHECK(px(dst, 10, 10) == 0xff000000, "AND: %#lx", (unsigned long)px(dst, 10, 10));
    IDirect3DDevice9Ex_ComposeRects(dev, src, dst, sv, 2, dv, D3DCOMPOSERECTS_NEG, 0, 0);
    CHECK(px(dst, 10, 10) == 0x00ffff00, "NEG: %#lx", (unsigned long)px(dst, 10, 10));

    hr = IDirect3DDevice9Ex_ComposeRects(dev, src, dst, sv, 2, dv, 0, 0, 0);
    CHECK(hr == D3DERR_INVALIDCALL, "bad operation %#lx", hr);
    hr = IDirect3DDevice9Ex_ComposeRects(dev, NULL, dst, sv, 2, dv, D3DCOMPOSERECTS_COPY, 0, 0);
    CHECK(hr == D3DERR_INVALIDCALL, "NULL surface %#lx", hr);
    hr = IDirect3DDevice9Ex_ComposeRects(dev, src, dst, sv, 3, dv, D3DCOMPOSERECTS_COPY, 0, 0);
    CHECK(hr == D3DERR_INVALIDCALL, "too many rects for the descriptor buffer %#lx", hr);

    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
