/* DirectComposition surfaces (patch 2679), run by test/dcompsurface-gate.sh on Xvfb.
 * Checks the surface API (argument validation, the BeginDraw / SuspendDraw / ResumeDraw / EndDraw
 * state machine, the texture BeginDraw hands out and its offset, Scroll, a virtual surface's
 * Resize and Trim) by reading the texture back, then commits a visual whose content is a surface
 * and prints WIN <X window id> so the gate can look at the pixels the window shows: green, then
 * (after the gate says "go2") red from a second BeginDraw/EndDraw. Prints "PHASE1" / "PHASE2"
 * when a phase is on screen, and waits for the file "done". C++: mingw's dcomp.h is. */
#include <windows.h>
#include <stdio.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>

#ifndef DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED
#define DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED ((HRESULT)0x88980801)
#define DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED ((HRESULT)0x88980802)
#endif

static const GUID sg_IID_IDCompositionSurface = { 0xbb8a4953, 0x2c99, 0x4f5a, { 0x96, 0xf5, 0x48, 0x19, 0x02, 0x7f, 0xa3, 0xac } };
static const GUID sg_IID_IDCompositionVirtualSurface = { 0xae471c51, 0x5f53, 0x4a24, { 0x8d, 0x3e, 0xd0, 0xc3, 0x9c, 0x30, 0xb3, 0xf0 } };

static int failures;
static void check(bool ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static ID3D11Device *dev;
static ID3D11DeviceContext *ctx;

static UINT32 read_pixel(ID3D11Texture2D *tex, UINT x, UINT y)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D *staging;
    D3D11_MAPPED_SUBRESOURCE map;
    UINT32 px = 0xdeadbeef;

    tex->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    if (FAILED(dev->CreateTexture2D(&desc, NULL, &staging))) return px;
    ctx->CopyResource(staging, tex);
    if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &map)))
    {
        px = *(UINT32 *)((BYTE *)map.pData + y * map.RowPitch + x * 4);
        ctx->Unmap(staging, 0);
    }
    staging->Release();
    return px;
}

static void fill(ID3D11Texture2D *tex, UINT l, UINT t, UINT r, UINT b, UINT32 color)
{
    UINT32 *row = (UINT32 *)malloc((r - l) * (b - t) * 4);
    D3D11_BOX box = { l, t, 0, r, b, 1 };

    for (UINT i = 0; i < (r - l) * (b - t); i++) row[i] = color;
    ctx->UpdateSubresource(tex, 0, &box, row, (r - l) * 4, 0);
    free(row);
}

static void pump(DWORD ms)
{
    MSG msg;
    for (DWORD start = GetTickCount(); GetTickCount() - start < ms; Sleep(5))
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = {};
    IDXGIDevice *dxgidev;
    IDCompositionDevice *dcomp;
    IDCompositionTarget *target;
    IDCompositionVisual *visual;
    IDCompositionSurface *surf = NULL, *surf2 = NULL;
    IDCompositionVirtualSurface *vsurf = NULL;
    ID3D11Texture2D *tex = NULL;
    IDXGISurface *dxgisurf = NULL;
    D3D11_TEXTURE2D_DESC desc;
    POINT off;
    RECT rc;
    char done[MAX_PATH], go2[MAX_PATH];
    HRESULT hr;
    void *unk;

    snprintf(done, sizeof(done), "%s\\done", argc > 1 ? argv[1] : ".");
    snprintf(go2, sizeof(go2), "%s\\go2", argc > 1 ? argv[1] : ".");
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"DcompSurface";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, L"DcompSurface", L"dcompsurface", WS_POPUP | WS_VISIBLE,
                                100, 100, 200, 100, NULL, NULL, wc.hInstance, NULL);
    if (FAILED(hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                      D3D11_SDK_VERSION, &dev, NULL, &ctx))) { printf("NODEVICE %08lx\n", hr); return 1; }
    dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgidev);
    if (FAILED(hr = DCompositionCreateDevice(dxgidev, __uuidof(IDCompositionDevice), (void **)&dcomp)))
    { printf("NODCOMP %08lx\n", hr); return 1; }

    /* arguments */
    hr = dcomp->CreateSurface(0, 100, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &surf);
    check(hr == E_INVALIDARG && !surf, "CreateSurface: no width is E_INVALIDARG");
    hr = dcomp->CreateSurface(100, 16385, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &surf);
    check(hr == E_INVALIDARG && !surf, "CreateSurface: too high is E_INVALIDARG");
    hr = dcomp->CreateSurface(100, 100, DXGI_FORMAT_R32_FLOAT, DXGI_ALPHA_MODE_PREMULTIPLIED, &surf);
    check(hr == E_INVALIDARG && !surf, "CreateSurface: R32_FLOAT is E_INVALIDARG");
    hr = dcomp->CreateSurface(100, 100, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_STRAIGHT, &surf);
    check(hr == E_INVALIDARG && !surf, "CreateSurface: straight alpha is E_INVALIDARG");
    hr = dcomp->CreateSurface(100, 100, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, NULL);
    check(hr == E_INVALIDARG, "CreateSurface: no output is E_INVALIDARG");

    hr = dcomp->CreateSurface(200, 100, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &surf);
    check(hr == S_OK && surf, "CreateSurface");
    if (!surf) return 1;
    hr = surf->QueryInterface(sg_IID_IDCompositionVirtualSurface, &unk);
    check(FAILED(hr), "a plain surface is not a virtual surface");

    /* the state machine */
    check(surf->EndDraw() == DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED, "EndDraw before BeginDraw");
    check(surf->SuspendDraw() == DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED, "SuspendDraw before BeginDraw");
    check(surf->ResumeDraw() == DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED, "ResumeDraw before BeginDraw");
    SetRect(&rc, 0, 0, 300, 100);
    hr = surf->BeginDraw(&rc, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
    check(hr == E_INVALIDARG && !tex, "BeginDraw: a rectangle outside the surface");
    SetRect(&rc, 10, 10, 10, 20);
    hr = surf->BeginDraw(&rc, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
    check(hr == E_INVALIDARG && !tex, "BeginDraw: an empty rectangle");
    hr = surf->BeginDraw(NULL, __uuidof(ID3D11Texture2D), (void **)&tex, NULL);
    check(hr == E_INVALIDARG, "BeginDraw: no offset");

    off.x = off.y = 99;
    hr = surf->BeginDraw(NULL, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
    check(hr == S_OK && tex && off.x == 0 && off.y == 0, "BeginDraw(NULL): the texture, offset 0,0");
    if (tex)
    {
        tex->GetDesc(&desc);
        check(desc.Width == 200 && desc.Height == 100 && desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM
              && (desc.BindFlags & D3D11_BIND_RENDER_TARGET), "it is 200x100 BGRA and a render target");
    }
    hr = surf->BeginDraw(NULL, __uuidof(ID3D11Texture2D), &unk, &off);
    check(hr == DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED, "BeginDraw while drawing");
    check(surf->Scroll(NULL, NULL, 1, 1) == DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED, "Scroll while drawing");
    check(surf->ResumeDraw() == DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED, "ResumeDraw while drawing");
    check(surf->SuspendDraw() == S_OK, "SuspendDraw");
    check(surf->SuspendDraw() == DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED, "SuspendDraw twice");
    check(surf->BeginDraw(NULL, __uuidof(ID3D11Texture2D), &unk, &off) == DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED,
          "BeginDraw while suspended");
    check(surf->ResumeDraw() == S_OK, "ResumeDraw");
    /* left half red, right half blue */
    fill(tex, 0, 0, 100, 100, 0xffff0000);
    fill(tex, 100, 0, 200, 100, 0xff0000ff);
    check(surf->EndDraw() == S_OK, "EndDraw");
    check(surf->EndDraw() == DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED, "EndDraw twice");
    tex->Release(); tex = NULL;

    SetRect(&rc, 20, 10, 60, 50);
    hr = surf->BeginDraw(&rc, __uuidof(IDXGISurface), (void **)&dxgisurf, &off);
    check(hr == S_OK && dxgisurf && off.x == 20 && off.y == 10, "BeginDraw(rect, IDXGISurface): offset is the rectangle's corner");
    if (dxgisurf) { dxgisurf->Release(); dxgisurf = NULL; }
    surf->EndDraw();
    hr = surf->BeginDraw(NULL, IID_IUnknown, &unk, &off);
    check(hr == S_OK, "BeginDraw(IUnknown)");
    if (hr == S_OK) { ((IUnknown *)unk)->Release(); surf->EndDraw(); }
    hr = surf->BeginDraw(NULL, __uuidof(IDCompositionDevice), &unk, &off);
    check(FAILED(hr) && !unk, "BeginDraw(an interface the texture does not have) fails");
    check(surf->EndDraw() == DCOMPOSITION_ERROR_SURFACE_NOT_BEING_RENDERED, "and does not leave the surface being drawn");

    /* Scroll: the pixels move, only inside the clip rectangle */
    surf->BeginDraw(NULL, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
    UINT32 left = read_pixel(tex, 10, 50), right = read_pixel(tex, 150, 50);
    check(left == 0xffff0000 && right == 0xff0000ff, "the pixels written are the pixels read");
    surf->EndDraw();
    SetRect(&rc, 0, 0, 100, 100);
    check(surf->Scroll(&rc, NULL, 50, 0) == S_OK, "Scroll the left half 50 to the right");
    check(read_pixel(tex, 120, 50) == 0xffff0000, "it covers the right half's left quarter");
    check(read_pixel(tex, 25, 50) == 0xffff0000, "what was left behind is not cleared");
    check(read_pixel(tex, 180, 50) == 0xff0000ff, "and the rest is untouched");
    fill(tex, 0, 0, 100, 100, 0xff00ff00);
    fill(tex, 100, 0, 200, 100, 0xff0000ff);
    SetRect(&rc, 0, 0, 100, 100);
    RECT clip = { 0, 0, 120, 100 };
    check(surf->Scroll(&rc, &clip, 50, 0) == S_OK, "Scroll with a clip rectangle");
    check(read_pixel(tex, 110, 50) == 0xff00ff00 && read_pixel(tex, 130, 50) == 0xff0000ff,
          "only the part inside the clip changes");
    SetRect(&rc, 0, 0, 500, 100);
    check(surf->Scroll(&rc, NULL, 1, 0) == E_INVALIDARG, "Scroll: a scroll rectangle outside the surface");
    tex->Release(); tex = NULL;
    surf->Release(); surf = NULL;

    /* a virtual surface */
    hr = dcomp->CreateVirtualSurface(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &vsurf);
    check(hr == S_OK && vsurf, "CreateVirtualSurface");
    if (vsurf)
    {
        hr = vsurf->QueryInterface(sg_IID_IDCompositionSurface, &unk);
        check(hr == S_OK, "a virtual surface is a surface");
        if (hr == S_OK) ((IUnknown *)unk)->Release();
        vsurf->BeginDraw(NULL, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
        fill(tex, 0, 0, 64, 64, 0xff123456);
        vsurf->EndDraw();
        tex->Release(); tex = NULL;
        check(vsurf->Resize(0, 10) == E_INVALIDARG, "Resize: no width is E_INVALIDARG");
        check(vsurf->Resize(20000, 10) == E_INVALIDARG, "Resize: too wide is E_INVALIDARG");
        check(vsurf->Resize(128, 96) == S_OK, "Resize to 128x96");
        vsurf->BeginDraw(NULL, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
        tex->GetDesc(&desc);
        check(desc.Width == 128 && desc.Height == 96, "the texture has the new size");
        check(read_pixel(tex, 30, 30) == 0xff123456, "and the pixels that stay");
        check(vsurf->Resize(10, 10) == DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED, "Resize while drawing");
        tex->Release(); tex = NULL;
        vsurf->EndDraw();
        SetRect(&rc, 0, 0, 32, 32);
        check(vsurf->Trim(&rc, 1) == S_OK, "Trim");
        check(vsurf->Trim(NULL, 1) == E_INVALIDARG, "Trim: no rectangles");
        vsurf->Release();
    }

    /* showing a surface */
    hr = dcomp->CreateSurface(200, 100, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_IGNORE, &surf2);
    check(hr == S_OK && surf2, "CreateSurface (alpha ignored) for the window");
    if (!surf2) return 1;
    surf2->BeginDraw(NULL, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
    fill(tex, 0, 0, 200, 100, 0xff00ff00);
    tex->Release(); tex = NULL;
    surf2->EndDraw();
    dcomp->CreateTargetForHwnd(hwnd, TRUE, &target);
    dcomp->CreateVisual(&visual);
    check(visual->SetContent(surf2) == S_OK, "SetContent(a surface)");
    target->SetRoot(visual);
    check(dcomp->Commit() == S_OK, "Commit");
    pump(1000);
    printf("PHASE1 %lx\n", (unsigned long)(ULONG_PTR)GetPropA(hwnd, "__wine_x11_whole_window"));
    fflush(stdout);
    for (DWORD start = GetTickCount(); GetFileAttributesA(go2) == INVALID_FILE_ATTRIBUTES && GetTickCount() - start < 60000; )
        pump(20);

    surf2->BeginDraw(NULL, __uuidof(ID3D11Texture2D), (void **)&tex, &off);
    fill(tex, 0, 0, 200, 100, 0xffff0000);
    tex->Release(); tex = NULL;
    surf2->EndDraw();
    dcomp->Commit();
    pump(1000);
    printf("PHASE2\n");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    for (DWORD start = GetTickCount(); GetFileAttributesA(done) == INVALID_FILE_ATTRIBUTES && GetTickCount() - start < 60000; )
        pump(20);
    return failures != 0;
}
