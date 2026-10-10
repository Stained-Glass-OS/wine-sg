/* dxgi: output duplication, display surface data, gamma control, overlay and
 * statistics queries on IDXGIOutput (patches/sg/2611). The desktop is painted
 * with a window of known colours and read back through the duplication. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const GUID iid_dxgidevice = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID iid_output1 = {0x00cddea8, 0x939b, 0x4b83, {0xa3, 0x40, 0xa6, 0x85, 0x22, 0x66, 0x66, 0xcc}};
static const GUID iid_output2 = {0x595e39d1, 0x2724, 0x4663, {0x99, 0xb1, 0xda, 0x96, 0x9d, 0xe2, 0x83, 0x64}};
static const GUID iid_output4 = {0xdc7dca35, 0x2196, 0x414d, {0x9f, 0x53, 0x61, 0x78, 0x84, 0x03, 0x2a, 0x60}};
static const GUID iid_output5 = {0x80a07424, 0xab52, 0x42eb, {0x83, 0x3c, 0x0c, 0x42, 0xfd, 0x28, 0x2d, 0x98}};
static const GUID iid_output3 = {0x8a6bb301, 0x7e7e, 0x41f4, {0xa8, 0xe0, 0x5b, 0x32, 0xf7, 0xf9, 0x9b, 0x18}};
static const GUID iid_tex2d = {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};
static const GUID iid_dxgiresource = {0x035f3ab4, 0x482e, 0x4e50, {0xb4, 0x1f, 0x8a, 0x7f, 0x8b, 0xd8, 0x96, 0x0b}};
static const GUID iid_dxgisurface = {0xcafcb56c, 0x6ac3, 0x4889, {0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec}};
static const GUID iid_dxgiobject = {0xaec22fb8, 0x76f3, 0x4639, {0x9b, 0xe0, 0x28, 0xeb, 0x43, 0xa6, 0x7a, 0x2e}};
static const GUID priv_guid = {0x1234, 0x5678, 0x9abc, {1, 2, 3, 4, 5, 6, 7, 8}};

static int failures;
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static HWND hwnd;

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

static void do_fill(COLORREF colour)
{
    HDC dc = GetDC(hwnd);
    RECT rc;
    HBRUSH brush = CreateSolidBrush(colour);

    GetClientRect(hwnd, &rc);
    FillRect(dc, &rc, brush);
    DeleteObject(brush);
    GdiFlush();
    ReleaseDC(hwnd, dc);
}

#define WM_FILL (WM_APP + 1)

static void fill_window(COLORREF colour)
{
    SendMessageW(hwnd, WM_FILL, 0, colour);
}

static DWORD WINAPI change_later(void *arg)
{
    Sleep(400);
    fill_window(RGB(200, 20, 100));
    return 0;
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_FILL)
    {
        do_fill((COLORREF)lp);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static HANDLE window_ready;

/* The window lives on a thread of its own that keeps pumping while the test waits in AcquireNextFrame. */
static DWORD WINAPI window_thread(void *arg)
{
    WNDCLASSW wc = {0};
    MSG msg;

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"dxgidup";
    wc.hbrBackground = CreateSolidBrush(RGB(10, 200, 30));
    RegisterClassW(&wc);
    hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"dxgidup", L"dup", WS_POPUP | WS_VISIBLE, 100, 100, 200, 150,
            NULL, NULL, wc.hInstance, NULL);
    SetEvent(window_ready);
    while (GetMessageW(&msg, NULL, 0, 0))
        DispatchMessageW(&msg);
    return 0;
}

static ID3D11Texture2D *make_staging(UINT w, UINT h, DXGI_FORMAT format)
{
    D3D11_TEXTURE2D_DESC desc = {0};
    ID3D11Texture2D *tex = NULL;

    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    ID3D11Device_CreateTexture2D(device, &desc, NULL, &tex);
    return tex;
}

/* BGRA of one pixel of a texture (copied through a staging texture when needed) */
static BOOL read_pixel(ID3D11Texture2D *tex, UINT x, UINT y, BYTE out[4])
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D *staging;
    D3D11_MAPPED_SUBRESOURCE map;
    BOOL ret = FALSE;

    ID3D11Texture2D_GetDesc(tex, &desc);
    staging = make_staging(desc.Width, desc.Height, desc.Format);
    if (!staging) return FALSE;
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)tex);
    if (SUCCEEDED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map)))
    {
        memcpy(out, (BYTE *)map.pData + y * map.RowPitch + x * 4, 4);
        ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
        ret = TRUE;
    }
    ID3D11Texture2D_Release(staging);
    return ret;
}

static IDXGIOutput1 *output1;
static IDXGIOutput *output;
static UINT screen_w, screen_h;

static void test_duplication(void)
{
    IDXGIOutputDuplication *dup = NULL, *dup2 = NULL;
    DXGI_OUTDUPL_DESC desc;
    DXGI_OUTDUPL_FRAME_INFO info;
    IDXGIResource *resource = NULL;
    ID3D11Texture2D *tex;
    D3D11_TEXTURE2D_DESC tdesc;
    BYTE px[4];
    RECT rects[2];
    UINT required;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape;
    IUnknown *obj = NULL;
    IDXGIObject *dxgiobj;
    HRESULT hr;
    DWORD t0, elapsed;
    HANDLE thread;
    UINT data = 0x31337, size, i;
    BOOL got;

    hr = IDXGIOutput1_DuplicateOutput(output1, NULL, &dup);
    check(hr == E_INVALIDARG && !dup, "DuplicateOutput(NULL device) = %#lx", hr);
    hr = IDXGIOutput1_DuplicateOutput(output1, (IUnknown *)device, NULL);
    check(hr == E_INVALIDARG, "DuplicateOutput(NULL result) = %#lx", hr);
    hr = IDXGIOutput1_DuplicateOutput(output1, (IUnknown *)output, &dup);
    check(hr == E_INVALIDARG && !dup, "DuplicateOutput with something that is no device = %#lx", hr);

    hr = IDXGIOutput1_DuplicateOutput(output1, (IUnknown *)device, &dup);
    check(hr == S_OK && dup, "DuplicateOutput = %#lx", hr);
    if (!dup) return;

    memset(&desc, 0xcc, sizeof(desc));
    IDXGIOutputDuplication_GetDesc(dup, &desc);
    check(desc.ModeDesc.Width == screen_w && desc.ModeDesc.Height == screen_h, "GetDesc size is the display's (%ux%u vs %ux%u)",
          desc.ModeDesc.Width, desc.ModeDesc.Height, screen_w, screen_h);
    check(desc.ModeDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM, "GetDesc format is B8G8R8A8 (%d)", desc.ModeDesc.Format);
    check(desc.DesktopImageInSystemMemory == FALSE, "the desktop image is not in system memory");
    check(desc.Rotation == DXGI_MODE_ROTATION_IDENTITY, "rotation identity (%d)", desc.Rotation);

    /* nothing acquired yet */
    hr = IDXGIOutputDuplication_ReleaseFrame(dup);
    check(hr == DXGI_ERROR_INVALID_CALL, "ReleaseFrame without a frame = %#lx", hr);
    hr = IDXGIOutputDuplication_GetFrameDirtyRects(dup, sizeof(rects), rects, &required);
    check(hr == DXGI_ERROR_INVALID_CALL, "GetFrameDirtyRects without a frame = %#lx", hr);
    hr = IDXGIOutputDuplication_UnMapDesktopSurface(dup);
    check(hr == DXGI_ERROR_INVALID_CALL, "UnMapDesktopSurface when not mapped = %#lx", hr);
    {
        DXGI_MAPPED_RECT mapped;
        hr = IDXGIOutputDuplication_MapDesktopSurface(dup, &mapped);
        check(hr == DXGI_ERROR_UNSUPPORTED, "MapDesktopSurface = %#lx", hr);
        hr = IDXGIOutputDuplication_MapDesktopSurface(dup, NULL);
        check(hr == E_INVALIDARG, "MapDesktopSurface(NULL) = %#lx", hr);
    }

    /* the first frame */
    hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 100, NULL, &resource);
    check(hr == E_INVALIDARG, "AcquireNextFrame(NULL info) = %#lx", hr);
    hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 100, &info, NULL);
    check(hr == E_INVALIDARG, "AcquireNextFrame(NULL resource) = %#lx", hr);
    memset(&info, 0xcc, sizeof(info));
    hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 1000, &info, &resource);
    check(hr == S_OK && resource, "first AcquireNextFrame = %#lx", hr);
    if (hr != S_OK) goto out;
    check(info.AccumulatedFrames == 1, "AccumulatedFrames = %u", info.AccumulatedFrames);
    check(info.LastPresentTime.QuadPart != 0, "LastPresentTime is set");
    check(info.PointerShapeBufferSize == 0 && !info.PointerPosition.Visible, "no pointer shape or position");
    check(info.TotalMetadataBufferSize == 0 || info.TotalMetadataBufferSize == 16, "metadata size %u", info.TotalMetadataBufferSize);

    hr = IDXGIResource_QueryInterface(resource, &iid_tex2d, (void **)&tex);
    check(hr == S_OK, "the frame is a Direct3D 11 texture (%#lx)", hr);
    if (SUCCEEDED(hr))
    {
        ID3D11Texture2D_GetDesc(tex, &tdesc);
        check(tdesc.Width == screen_w && tdesc.Height == screen_h && tdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM,
              "the texture is the display image (%ux%u fmt %d)", tdesc.Width, tdesc.Height, tdesc.Format);
        memset(px, 0, 4);
        got = read_pixel(tex, 150, 150, px);
        check(got && px[0] == 30 && px[1] == 200 && px[2] == 10 && px[3] == 255,
              "the painted window is in the frame as BGRA (%u,%u,%u,%u)", px[0], px[1], px[2], px[3]);
        ID3D11Texture2D_Release(tex);
    }
    IDXGIResource_Release(resource);

    hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 10, &info, &resource);
    check(hr == DXGI_ERROR_INVALID_CALL, "second AcquireNextFrame before ReleaseFrame = %#lx", hr);

    required = 0;
    hr = IDXGIOutputDuplication_GetFrameDirtyRects(dup, 0, NULL, &required);
    check(hr == DXGI_ERROR_MORE_DATA && required == sizeof(RECT), "GetFrameDirtyRects with no buffer = %#lx, %u needed", hr, required);
    memset(rects, 0, sizeof(rects));
    hr = IDXGIOutputDuplication_GetFrameDirtyRects(dup, sizeof(rects), rects, &required);
    check(hr == S_OK && rects[0].left == 0 && rects[0].top == 0 && rects[0].right == (LONG)screen_w && rects[0].bottom == (LONG)screen_h,
          "the whole screen is dirty in a new frame (%#lx, %ld %ld %ld %ld)", hr, rects[0].left, rects[0].top, rects[0].right, rects[0].bottom);
    hr = IDXGIOutputDuplication_GetFrameDirtyRects(dup, sizeof(rects), rects, NULL);
    check(hr == E_INVALIDARG, "GetFrameDirtyRects(NULL required) = %#lx", hr);
    required = 99;
    hr = IDXGIOutputDuplication_GetFrameMoveRects(dup, 0, NULL, &required);
    check(hr == S_OK && required == 0, "no move rectangles (%#lx, %u)", hr, required);
    required = 99;
    hr = IDXGIOutputDuplication_GetFramePointerShape(dup, 0, NULL, &required, &shape);
    check(hr == S_OK && required == 0, "no pointer shape (%#lx, %u)", hr, required);

    hr = IDXGIOutputDuplication_ReleaseFrame(dup);
    check(hr == S_OK, "ReleaseFrame = %#lx", hr);
    hr = IDXGIOutputDuplication_ReleaseFrame(dup);
    check(hr == DXGI_ERROR_INVALID_CALL, "second ReleaseFrame = %#lx", hr);

    /* nothing changed: the wait ends in a timeout */
    memset(&info, 0xcc, sizeof(info));
    resource = (IDXGIResource *)0x1234;
    t0 = GetTickCount();
    hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 150, &info, &resource);
    elapsed = GetTickCount() - t0;
    check(hr == DXGI_ERROR_WAIT_TIMEOUT, "AcquireNextFrame on an unchanged desktop times out (%#lx)", hr);
    check(elapsed >= 120 && elapsed < 2000, "and waited for the timeout (%lu ms)", elapsed);
    check(!resource && info.AccumulatedFrames == 0, "no resource and no frame count on a timeout");

    /* the desktop changes while we wait */
    thread = CreateThread(NULL, 0, change_later, NULL, 0, NULL);
    t0 = GetTickCount();
    hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 5000, &info, &resource);
    elapsed = GetTickCount() - t0;
    check(hr == S_OK && resource, "AcquireNextFrame returns when the desktop changes (%#lx)", hr);
    check(elapsed >= 250 && elapsed < 4000, "after the change, not before (%lu ms)", elapsed);
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);
    if (resource)
    {
        if (SUCCEEDED(IDXGIResource_QueryInterface(resource, &iid_tex2d, (void **)&tex)))
        {
            memset(px, 0, 4);
            got = read_pixel(tex, 150, 150, px);
            check(got && px[0] == 100 && px[1] == 20 && px[2] == 200,
                  "the new colour is in the frame (%u,%u,%u)", px[0], px[1], px[2]);
            ID3D11Texture2D_Release(tex);
        }
        IDXGIResource_Release(resource);
        IDXGIOutputDuplication_ReleaseFrame(dup);
    }

    /* IDXGIObject */
    hr = IDXGIOutputDuplication_QueryInterface(dup, &iid_dxgiobject, (void **)&dxgiobj);
    check(hr == S_OK, "QueryInterface(IDXGIObject) = %#lx", hr);
    if (SUCCEEDED(hr))
    {
        hr = IDXGIObject_SetPrivateData(dxgiobj, &priv_guid, sizeof(data), &data);
        data = 0;
        size = sizeof(data);
        hr = IDXGIObject_GetPrivateData(dxgiobj, &priv_guid, &size, &data);
        check(hr == S_OK && data == 0x31337, "private data round trips (%#lx)", hr);
        hr = IDXGIObject_GetParent(dxgiobj, &iid_output1, (void **)&obj);
        check(hr == S_OK && obj, "GetParent is the output (%#lx)", hr);
        if (obj) IUnknown_Release(obj);
        IDXGIObject_Release(dxgiobj);
    }
    hr = IDXGIOutputDuplication_QueryInterface(dup, &iid_output1, (void **)&obj);
    check(hr == E_NOINTERFACE, "QueryInterface(IDXGIOutput1) = %#lx", hr);

    /* a second duplication of the same output works too, and a held frame can be dropped by Release */
    hr = IDXGIOutput1_DuplicateOutput(output1, (IUnknown *)device, &dup2);
    check(hr == S_OK && dup2, "a second DuplicateOutput = %#lx", hr);
    if (dup2)
    {
        hr = IDXGIOutputDuplication_AcquireNextFrame(dup2, 1000, &info, &resource);
        check(hr == S_OK, "the second one has its own first frame (%#lx)", hr);
        if (resource) IDXGIResource_Release(resource);
        check(IDXGIOutputDuplication_Release(dup2) == 0, "releasing a duplication with a frame held");
    }

out:
    i = IDXGIOutputDuplication_Release(dup);
    check(i == 0, "the duplication is released (%u)", i);
}

static void test_surface_data(void)
{
    ID3D11Texture2D *tex, *small, *wrong;
    IDXGISurface *surface;
    IDXGIResource *res;
    IDXGIOutput1 *o1 = output1;
    BYTE px[4] = {0};
    D3D11_MAPPED_SUBRESOURCE map;
    HRESULT hr;

    tex = make_staging(screen_w, screen_h, DXGI_FORMAT_B8G8R8A8_UNORM);
    small = make_staging(16, 16, DXGI_FORMAT_B8G8R8A8_UNORM);
    wrong = make_staging(screen_w, screen_h, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!tex || !small || !wrong) { check(0, "staging textures created"); return; }

    hr = IDXGIOutput_GetDisplaySurfaceData(output, NULL);
    check(hr == E_INVALIDARG, "GetDisplaySurfaceData(NULL) = %#lx", hr);
    hr = IDXGIOutput1_GetDisplaySurfaceData1(o1, NULL);
    check(hr == E_INVALIDARG, "GetDisplaySurfaceData1(NULL) = %#lx", hr);

    ID3D11Texture2D_QueryInterface(small, &iid_dxgisurface, (void **)&surface);
    hr = IDXGIOutput_GetDisplaySurfaceData(output, surface);
    check(hr == DXGI_ERROR_INVALID_CALL, "a surface of the wrong size = %#lx", hr);
    IDXGISurface_Release(surface);
    ID3D11Texture2D_QueryInterface(wrong, &iid_dxgisurface, (void **)&surface);
    hr = IDXGIOutput_GetDisplaySurfaceData(output, surface);
    check(hr == DXGI_ERROR_INVALID_CALL, "a surface of the wrong format = %#lx", hr);
    IDXGISurface_Release(surface);

    ID3D11Texture2D_QueryInterface(tex, &iid_dxgisurface, (void **)&surface);
    hr = IDXGIOutput_GetDisplaySurfaceData(output, surface);
    check(hr == S_OK, "GetDisplaySurfaceData = %#lx", hr);
    IDXGISurface_Release(surface);
    if (SUCCEEDED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)tex, 0, D3D11_MAP_READ, 0, &map)))
    {
        memcpy(px, (BYTE *)map.pData + 150 * map.RowPitch + 150 * 4, 4);
        ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)tex, 0);
    }
    check(px[0] == 100 && px[1] == 20 && px[2] == 200 && px[3] == 255, "it holds the screen (%u,%u,%u,%u)", px[0], px[1], px[2], px[3]);

    fill_window(RGB(1, 2, 3));
    Sleep(100);
    ID3D11Texture2D_QueryInterface(tex, &iid_dxgiresource, (void **)&res);
    hr = IDXGIOutput1_GetDisplaySurfaceData1(o1, res);
    check(hr == S_OK, "GetDisplaySurfaceData1 = %#lx", hr);
    IDXGIResource_Release(res);
    memset(px, 0, 4);
    if (SUCCEEDED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)tex, 0, D3D11_MAP_READ, 0, &map)))
    {
        memcpy(px, (BYTE *)map.pData + 150 * map.RowPitch + 150 * 4, 4);
        ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)tex, 0);
    }
    check(px[0] == 3 && px[1] == 2 && px[2] == 1, "the second copy follows the screen (%u,%u,%u)", px[0], px[1], px[2]);

    ID3D11Texture2D_Release(tex);
    ID3D11Texture2D_Release(small);
    ID3D11Texture2D_Release(wrong);
}

static void test_output_queries(void)
{
    DXGI_GAMMA_CONTROL gamma;
    DXGI_FRAME_STATISTICS stats;
    IDXGIOutput2 *o2 = NULL;
    IDXGIOutput3 *o3 = NULL;
    IDXGIOutput4 *o4 = NULL;
    IDXGIOutput5 *o5 = NULL;
    IDXGIOutputDuplication *dup = NULL;
    ID3D11Texture2D *tex;
    IDXGISurface *surface;
    UINT flags;
    HRESULT hr;
    int i, mono;
    static const DXGI_FORMAT only_rgba[] = {DXGI_FORMAT_R8G8B8A8_UNORM};
    static const DXGI_FORMAT with_bgra[] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_B8G8R8A8_UNORM};

    hr = IDXGIOutput_GetGammaControl(output, NULL);
    check(hr == E_INVALIDARG, "GetGammaControl(NULL) = %#lx", hr);
    memset(&gamma, 0, sizeof(gamma));
    hr = IDXGIOutput_GetGammaControl(output, &gamma);
    check(hr == S_OK, "GetGammaControl = %#lx", hr);
    if (hr == S_OK)
    {
        mono = 1;
        for (i = 1; i < 256; i++)
            if (gamma.GammaCurve[i].Red < gamma.GammaCurve[i - 1].Red) mono = 0;
        check(gamma.Scale.Red == 1.0f && gamma.Scale.Green == 1.0f && gamma.Scale.Blue == 1.0f &&
              gamma.Offset.Red == 0.0f && gamma.Offset.Green == 0.0f && gamma.Offset.Blue == 0.0f, "scale 1 and offset 0");
        check(mono && gamma.GammaCurve[255].Red > gamma.GammaCurve[0].Red, "the curve rises (%g .. %g)", gamma.GammaCurve[0].Red, gamma.GammaCurve[255].Red);
        check(gamma.GammaCurve[255].Red <= 1.0f && gamma.GammaCurve[0].Red >= 0.0f, "and stays within 0..1");
    }

    hr = IDXGIOutput_GetFrameStatistics(output, NULL);
    check(hr == E_INVALIDARG, "output GetFrameStatistics(NULL) = %#lx", hr);
    hr = IDXGIOutput_GetFrameStatistics(output, &stats);
    check(hr == DXGI_ERROR_INVALID_CALL, "output GetFrameStatistics without a full-screen owner = %#lx", hr);

    tex = make_staging(screen_w, screen_h, DXGI_FORMAT_B8G8R8A8_UNORM);
    ID3D11Texture2D_QueryInterface(tex, &iid_dxgisurface, (void **)&surface);
    hr = IDXGIOutput_SetDisplaySurface(output, NULL);
    check(hr == E_INVALIDARG, "SetDisplaySurface(NULL) = %#lx", hr);
    hr = IDXGIOutput_SetDisplaySurface(output, surface);
    check(hr == DXGI_ERROR_INVALID_CALL, "SetDisplaySurface without a full-screen owner = %#lx", hr);
    IDXGISurface_Release(surface);
    ID3D11Texture2D_Release(tex);

    hr = IDXGIOutput_QueryInterface(output, &iid_output2, (void **)&o2);
    if (SUCCEEDED(hr))
    {
        check(IDXGIOutput2_SupportsOverlays(o2) == FALSE, "SupportsOverlays is FALSE");
        IDXGIOutput2_Release(o2);
    }
    hr = IDXGIOutput_QueryInterface(output, &iid_output3, (void **)&o3);
    if (SUCCEEDED(hr))
    {
        flags = 0xff;
        hr = IDXGIOutput3_CheckOverlaySupport(o3, DXGI_FORMAT_B8G8R8A8_UNORM, (IUnknown *)device, &flags);
        check(hr == S_OK && flags == 0, "CheckOverlaySupport: no overlay (%#lx, %#x)", hr, flags);
        hr = IDXGIOutput3_CheckOverlaySupport(o3, DXGI_FORMAT_B8G8R8A8_UNORM, (IUnknown *)device, NULL);
        check(hr == E_INVALIDARG, "CheckOverlaySupport(NULL flags) = %#lx", hr);
        flags = 0xff;
        hr = IDXGIOutput3_CheckOverlaySupport(o3, DXGI_FORMAT_B8G8R8A8_UNORM, NULL, &flags);
        check(hr == DXGI_ERROR_INVALID_CALL && flags == 0, "CheckOverlaySupport(NULL device) = %#lx", hr);
        IDXGIOutput3_Release(o3);
    }
    hr = IDXGIOutput_QueryInterface(output, &iid_output4, (void **)&o4);
    if (SUCCEEDED(hr))
    {
        flags = 0xff;
        hr = IDXGIOutput4_CheckOverlayColorSpaceSupport(o4, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
                (IUnknown *)device, &flags);
        check(hr == S_OK && flags == 0, "CheckOverlayColorSpaceSupport: no overlay (%#lx, %#x)", hr, flags);
        hr = IDXGIOutput4_CheckOverlayColorSpaceSupport(o4, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
                (IUnknown *)device, NULL);
        check(hr == E_INVALIDARG, "CheckOverlayColorSpaceSupport(NULL flags) = %#lx", hr);
        IDXGIOutput4_Release(o4);
    }

    hr = IDXGIOutput_QueryInterface(output, &iid_output5, (void **)&o5);
    if (SUCCEEDED(hr))
    {
        hr = IDXGIOutput5_DuplicateOutput1(o5, (IUnknown *)device, 0, 0, NULL, &dup);
        check(hr == E_INVALIDARG && !dup, "DuplicateOutput1 with no formats = %#lx", hr);
        hr = IDXGIOutput5_DuplicateOutput1(o5, (IUnknown *)device, 0, 1, only_rgba, &dup);
        check(hr == DXGI_ERROR_UNSUPPORTED && !dup, "DuplicateOutput1 with only R8G8B8A8 = %#lx", hr);
        hr = IDXGIOutput5_DuplicateOutput1(o5, (IUnknown *)device, 0, 2, with_bgra, &dup);
        check(hr == S_OK && dup, "DuplicateOutput1 with B8G8R8A8 among the formats = %#lx", hr);
        if (dup) IDXGIOutputDuplication_Release(dup);
        IDXGIOutput5_Release(o5);
    }
}

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    DXGI_OUTPUT_DESC odesc;
    HRESULT hr;

    window_ready = CreateEventW(NULL, FALSE, FALSE, NULL);
    CreateThread(NULL, 0, window_thread, NULL, 0, NULL);
    WaitForSingleObject(window_ready, 10000);
    Sleep(400);
    fill_window(RGB(10, 200, 30));
    Sleep(200);

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
            D3D11_SDK_VERSION, &device, NULL, &context);
    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
                D3D11_SDK_VERSION, &device, NULL, &context);
    check(SUCCEEDED(hr), "D3D11CreateDevice");
    if (FAILED(hr)) goto done;
    ID3D11Device_QueryInterface(device, &iid_dxgidevice, (void **)&dxgi_device);
    IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    hr = IDXGIAdapter_EnumOutputs(adapter, 0, &output);
    check(hr == S_OK, "the adapter has an output (%#lx)", hr);
    if (FAILED(hr)) goto done;
    hr = IDXGIOutput_QueryInterface(output, &iid_output1, (void **)&output1);
    check(hr == S_OK, "IDXGIOutput1 (%#lx)", hr);
    if (FAILED(hr)) goto done;
    IDXGIOutput_GetDesc(output, &odesc);
    screen_w = odesc.DesktopCoordinates.right - odesc.DesktopCoordinates.left;
    screen_h = odesc.DesktopCoordinates.bottom - odesc.DesktopCoordinates.top;

    test_duplication();
    test_surface_data();
    test_output_queries();

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
