/* compcapture-probe (wine-sg 7857/7856, test/compcapture-gate.sh): a window of
 * four known colours, read back through Windows' screen capture APIs.
 *
 *   probe show                 the window only, for five minutes (another process's)
 *   probe API [nowin] [X Y]    capture; nowin: no window of its own; X Y: a
 *                              point also reported (a Linux window's)
 *     gdi        GetDC(NULL) + BitBlt (what older programs do)
 *     dxgi       IDXGIOutput1::DuplicateOutput + AcquireNextFrame
 *     wgcmon     Windows.Graphics.Capture, the primary monitor's item
 *     wgcwin     Windows.Graphics.Capture, the pattern window's item
 *     wgcstatic  ...and no new frame while nothing changes
 *     wgccursor  ...monitor with IsCursorCaptureEnabled: the pointer drawn
 *     wgchold    ...window, the session held six seconds (its border)
 *     pixloop    10000 GetPixel calls timed, then a repaint seen at once
 *
 * Prints "<api> [atX,Y=RRGGBB] q0=RRGGBB q1=... q2=... q3=..." (the quadrants'
 * centres: red, green, blue, white expected), "wgcstatic new=N",
 * "wgccursor dark=N", or "<api> FAIL <why>". The WinRT interfaces are reached
 * through hand-declared vtables (mingw has no Windows.Graphics.Capture headers).
 */
#define COBJMACROS
#define INITGUID
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inspectable.h>
#include <winstring.h>
#include <roapi.h>

#define WX 100
#define WY 100
#define WW 400
#define WH 300
static COLORREF quad[4] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 255) };
static const POINT centre[4] = { { WX + WW / 4, WY + WH / 4 }, { WX + 3 * WW / 4, WY + WH / 4 },
                                 { WX + WW / 4, WY + 3 * WH / 4 }, { WX + 3 * WW / 4, WY + 3 * WH / 4 } };

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        int i;
        for (i = 0; i < 4; i++)
        {
            RECT r = { (i % 2) * WW / 2, (i / 2) * WH / 2, (i % 2 + 1) * WW / 2, (i / 2 + 1) * WH / 2 };
            HBRUSH b = CreateSolidBrush(quad[i]);
            FillRect(dc, &r, b);
            DeleteObject(b);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG m;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        Sleep(20);
    }
}

static int extra_x = -1, extra_y = -1;
static void report(const char *api, const BYTE *bgra, int pitch, int ox, int oy, int w, int h)
{
    int i;
    printf("%s", api);
    if (extra_x >= ox && extra_y >= oy && extra_x < ox + w && extra_y < oy + h)
    {
        const BYTE *p = bgra + (extra_y - oy) * pitch + (extra_x - ox) * 4;
        printf(" at%d,%d=%02X%02X%02X", extra_x, extra_y, p[2], p[1], p[0]);
    }
    for (i = 0; i < 4; i++)
    {
        const BYTE *p = bgra + (centre[i].y - oy) * pitch + (centre[i].x - ox) * 4;
        printf(" q%d=%02X%02X%02X", i, p[2], p[1], p[0]);
    }
    printf("\n");
}

static int do_gdi(void)
{
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    BITMAPINFO bi = { { sizeof(bi.bmiHeader), sw, -sh, 1, 32, BI_RGB } };
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    void *bits;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(mem, bmp);
    if (!BitBlt(mem, 0, 0, sw, sh, screen, 0, 0, SRCCOPY | CAPTUREBLT))
    {
        printf("gdi FAIL BitBlt %lu\n", GetLastError());
        return 1;
    }
    GdiFlush();
    report("gdi", bits, sw * 4, 0, 0, sw, sh);
    return 0;
}

static int do_dxgi(void)
{
    ID3D11Device *dev; ID3D11DeviceContext *ctx;
    IDXGIDevice *dd; IDXGIAdapter *ad; IDXGIOutput *out; IDXGIOutput1 *out1;
    IDXGIOutputDuplication *dup; DXGI_OUTDUPL_FRAME_INFO fi; IDXGIResource *res;
    ID3D11Texture2D *tex, *stage; D3D11_TEXTURE2D_DESC td; D3D11_MAPPED_SUBRESOURCE map;
    DXGI_OUTPUT_DESC od; HRESULT hr; int tries;

    if (FAILED(hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, NULL, &ctx)))
    { printf("dxgi FAIL D3D11CreateDevice %#lx\n", hr); return 1; }
    ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd);
    IDXGIDevice_GetAdapter(dd, &ad);
    if (FAILED(hr = IDXGIAdapter_EnumOutputs(ad, 0, &out))) { printf("dxgi FAIL EnumOutputs %#lx\n", hr); return 1; }
    IDXGIOutput_GetDesc(out, &od);
    IDXGIOutput_QueryInterface(out, &IID_IDXGIOutput1, (void **)&out1);
    if (FAILED(hr = IDXGIOutput1_DuplicateOutput(out1, (IUnknown *)dev, &dup)))
    { printf("dxgi FAIL DuplicateOutput %#lx\n", hr); return 1; }
    for (tries = 0; tries < 20; tries++)
    {
        hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 500, &fi, &res);
        if (SUCCEEDED(hr)) break;
        if (hr != DXGI_ERROR_WAIT_TIMEOUT) { printf("dxgi FAIL AcquireNextFrame %#lx\n", hr); return 1; }
    }
    if (FAILED(hr)) { printf("dxgi FAIL no frame\n"); return 1; }
    IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&tex);
    ID3D11Texture2D_GetDesc(tex, &td);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; td.MiscFlags = 0;
    ID3D11Device_CreateTexture2D(dev, &td, NULL, &stage);
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)stage, (ID3D11Resource *)tex);
    if (FAILED(hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)stage, 0, D3D11_MAP_READ, 0, &map)))
    { printf("dxgi FAIL Map %#lx\n", hr); return 1; }
    report("dxgi", map.pData, map.RowPitch, od.DesktopCoordinates.left, od.DesktopCoordinates.top, td.Width, td.Height);
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)stage, 0);
    {
        /* Desktop Duplication's rules: a held frame first released, the whole
         * output dirty, nothing new while nothing changes, the formats list */
        IDXGIResource *res2 = NULL; IDXGIOutput5 *out5; IDXGIOutputDuplication *dup2 = NULL;
        RECT dirty = {0}; UINT need = 0; HRESULT held, timeout, formats = E_FAIL;
        DXGI_FORMAT rgba = DXGI_FORMAT_R8G8B8A8_UNORM;
        held = IDXGIOutputDuplication_AcquireNextFrame(dup, 0, &fi, &res2);
        IDXGIOutputDuplication_GetFrameDirtyRects(dup, sizeof(dirty), &dirty, &need);
        IDXGIOutputDuplication_ReleaseFrame(dup);
        timeout = IDXGIOutputDuplication_AcquireNextFrame(dup, 300, &fi, &res2);
        if (SUCCEEDED(timeout)) IDXGIOutputDuplication_ReleaseFrame(dup);
        if (SUCCEEDED(IDXGIOutput_QueryInterface(out, &IID_IDXGIOutput5, (void **)&out5)))
            formats = IDXGIOutput5_DuplicateOutput1(out5, (IUnknown *)dev, 0, 1, &rgba, &dup2);
        printf("dxgirules held=%s dirty=%s timeout=%s formats=%s\n",
               held == DXGI_ERROR_INVALID_CALL ? "ok" : "bad",
               (dirty.right - dirty.left == (LONG)td.Width && dirty.bottom - dirty.top == (LONG)td.Height) ? "ok" : "bad",
               timeout == DXGI_ERROR_WAIT_TIMEOUT ? "ok" : "bad",
               formats == DXGI_ERROR_UNSUPPORTED ? "ok" : "bad");
    }
    return 0;
}

/* ---- Windows.Graphics.Capture, through hand-declared vtables ---- */
DEFINE_GUID(IID_IGraphicsCaptureItemInterop_, 0x3628e81b, 0x3cac, 0x4c60, 0xb7, 0xf4, 0x23, 0xce, 0x0e, 0x0c, 0x33, 0x56);
DEFINE_GUID(IID_IGraphicsCaptureItem_, 0x79c3f95b, 0x31f7, 0x4ec2, 0xa4, 0x64, 0x63, 0x2e, 0xf5, 0xd3, 0x07, 0x60);
DEFINE_GUID(IID_IFramePoolStatics2_, 0x589b103f, 0x6bbc, 0x5df5, 0xa9, 0x91, 0x02, 0xe2, 0x8b, 0x3b, 0x66, 0xd5);
DEFINE_GUID(IID_IGraphicsCaptureSession2_, 0x2c39ae40, 0x7d2e, 0x5044, 0x80, 0x4e, 0x8b, 0x67, 0x99, 0xd4, 0xcf, 0x9e);
DEFINE_GUID(IID_IGraphicsCaptureSession3_, 0xf2cdd966, 0x22ae, 0x5ea1, 0x95, 0x96, 0x3a, 0x28, 0x93, 0x44, 0xc3, 0xbe);
DEFINE_GUID(IID_ISessionStatics_, 0x2224a540, 0x5974, 0x49aa, 0xb2, 0x32, 0x08, 0x82, 0x53, 0x6f, 0x4c, 0xb5);
DEFINE_GUID(IID_IDirect3DDevice_, 0xa37624ab, 0x8d5f, 0x4650, 0x9d, 0x3e, 0x9e, 0xae, 0x3d, 0x9b, 0xc6, 0x70);
DEFINE_GUID(IID_IDxgiAccess_, 0xa9b3d012, 0x3df2, 0x4ee3, 0xb8, 0xd1, 0x86, 0x95, 0xf4, 0x57, 0xd3, 0xc1);
DEFINE_GUID(IID_IClosable_, 0x30d5a829, 0x7fa4, 0x4026, 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e);

typedef struct { INT32 w, h; } size32;
#define SLOT(obj, i) ((*(void ***)(obj))[i])
#define QI(obj, iid, out) ((HRESULT (WINAPI *)(void *, REFIID, void **))SLOT(obj, 0))(obj, iid, (void **)(out))
#define REL(obj) ((ULONG (WINAPI *)(void *))SLOT(obj, 2))(obj)

static HRESULT factory(const WCHAR *cls, REFIID iid, void **out)
{
    HSTRING s; HRESULT hr;
    WindowsCreateString(cls, lstrlenW(cls), &s);
    hr = RoGetActivationFactory(s, iid, out);
    WindowsDeleteString(s);
    return hr;
}

static int do_wgc(BOOL window_item, int mode)
{
    void *interop, *item, *statics2, *pool, *session, *session2, *frame = NULL, *surface, *access, *sstat;
    HRESULT (WINAPI *create_device)(IDXGIDevice *, IInspectable **);
    ID3D11Device *dev; ID3D11DeviceContext *ctx; IDXGIDevice *dd; IInspectable *gdev; void *d3ddev;
    ID3D11Texture2D *tex, *stage; D3D11_TEXTURE2D_DESC td; D3D11_MAPPED_SUBRESOURCE map;
    const char *api = window_item ? "wgcwin" : "wgcmon";
    unsigned char supported = 0; size32 size; HRESULT hr; RECT origin; int i;

    RoInitialize(RO_INIT_MULTITHREADED);
    if (SUCCEEDED(factory(L"Windows.Graphics.Capture.GraphicsCaptureSession", &IID_ISessionStatics_, &sstat)))
        ((HRESULT (WINAPI *)(void *, unsigned char *))SLOT(sstat, 6))(sstat, &supported);
    if (!supported) { printf("%s FAIL IsSupported is false\n", api); return 1; }
    if (FAILED(hr = factory(L"Windows.Graphics.Capture.GraphicsCaptureItem", &IID_IGraphicsCaptureItemInterop_, &interop)))
    { printf("%s FAIL no item interop %#lx\n", api, hr); return 1; }
    if (window_item)
    {
        HWND hwnd = FindWindowA("capprobe", NULL);
        GetWindowRect(hwnd, &origin);
        hr = ((HRESULT (WINAPI *)(void *, HWND, REFIID, void **))SLOT(interop, 3))(interop, hwnd, &IID_IGraphicsCaptureItem_, &item);
    }
    else
    {
        POINT pt = { 0, 0 };
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoA(mon, &mi);
        origin = mi.rcMonitor;
        hr = ((HRESULT (WINAPI *)(void *, HMONITOR, REFIID, void **))SLOT(interop, 4))(interop, mon, &IID_IGraphicsCaptureItem_, &item);
    }
    if (FAILED(hr)) { printf("%s FAIL CreateFor %#lx\n", api, hr); return 1; }
    ((HRESULT (WINAPI *)(void *, size32 *))SLOT(item, 7))(item, &size);

    if (FAILED(hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &dev, NULL, &ctx)))
    { printf("%s FAIL D3D11CreateDevice %#lx\n", api, hr); return 1; }
    ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd);
    create_device = (void *)GetProcAddress(LoadLibraryA("d3d11.dll"), "CreateDirect3D11DeviceFromDXGIDevice");
    if (!create_device || FAILED(create_device(dd, &gdev)) || FAILED(QI(gdev, &IID_IDirect3DDevice_, &d3ddev)))
    { printf("%s FAIL no WinRT device\n", api); return 1; }

    if (FAILED(hr = factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool", &IID_IFramePoolStatics2_, &statics2)))
    { printf("%s FAIL no pool statics %#lx\n", api, hr); return 1; }
    if (FAILED(hr = ((HRESULT (WINAPI *)(void *, void *, INT32, INT32, size32, void **))SLOT(statics2, 6))(statics2, d3ddev, 87, 2, size, &pool)))
    { printf("%s FAIL CreateFreeThreaded %#lx\n", api, hr); return 1; }
    if (FAILED(hr = ((HRESULT (WINAPI *)(void *, void *, void **))SLOT(pool, 10))(pool, item, &session)))
    { printf("%s FAIL CreateCaptureSession %#lx\n", api, hr); return 1; }
    if (SUCCEEDED(QI(session, &IID_IGraphicsCaptureSession2_, &session2)))
        ((HRESULT (WINAPI *)(void *, unsigned char))SLOT(session2, 7))(session2, mode == 2);   /* the pointer only for the cursor check */
    if (mode == 2) SetCursorPos(centre[3].x, centre[3].y);
    if (FAILED(hr = ((HRESULT (WINAPI *)(void *))SLOT(session, 6))(session)))
    { printf("%s FAIL StartCapture %#lx\n", api, hr); return 1; }
    for (i = 0; i < 100 && !frame; i++)
    {
        ((HRESULT (WINAPI *)(void *, void **))SLOT(pool, 7))(pool, &frame);
        if (!frame) Sleep(50);
    }
    if (!frame) { printf("%s FAIL no frame in 5 s\n", api); return 1; }
    ((HRESULT (WINAPI *)(void *, void **))SLOT(frame, 6))(frame, &surface);
    if (FAILED(QI(surface, &IID_IDxgiAccess_, &access)) ||
        FAILED(((HRESULT (WINAPI *)(void *, REFIID, void **))SLOT(access, 3))(access, &IID_ID3D11Texture2D, (void **)&tex)))
    { printf("%s FAIL no texture in the frame\n", api); return 1; }
    ID3D11Texture2D_GetDesc(tex, &td);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; td.MiscFlags = 0;
    ID3D11Device_CreateTexture2D(dev, &td, NULL, &stage);
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)stage, (ID3D11Resource *)tex);
    if (FAILED(hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)stage, 0, D3D11_MAP_READ, 0, &map)))
    { printf("%s FAIL Map %#lx\n", api, hr); return 1; }
    if (mode == 2)
    {
        /* the pointer drawn at its place in the white quadrant: not all white there */
        int x, y, dark = 0;
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++)
            {
                const BYTE *p = (const BYTE *)map.pData + (centre[3].y - origin.top + y) * map.RowPitch + (centre[3].x - origin.left + x) * 4;
                if (p[0] < 0x80 && p[1] < 0x80 && p[2] < 0x80) dark++;
            }
        {
            CURSORINFO ci = { sizeof(ci) };
            GetCursorInfo(&ci);
            printf("wgccursor dark=%d (cursor flags %lx at %ld,%ld handle %p)\n", dark, ci.flags, ci.ptScreenPos.x, ci.ptScreenPos.y, ci.hCursor);
        }
    }
    else
    {
        printf("%s size=%dx%d ", api, size.w, size.h);
        report(api, map.pData, map.RowPitch, origin.left, origin.top, td.Width, td.Height);
    }
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)stage, 0);
    if (mode == 3) Sleep(6000);   /* held: the border is looked at meanwhile */
    if (mode == 1)
    {
        /* nothing changes now: no frame arrives */
        void *more = NULL, *closable;
        int n = 0;
        if (SUCCEEDED(QI(frame, &IID_IClosable_, &closable))) ((HRESULT (WINAPI *)(void *))SLOT(closable, 6))(closable);
        for (i = 0; i < 10; i++)   /* frames queued before this one */
        {
            ((HRESULT (WINAPI *)(void *, void **))SLOT(pool, 7))(pool, &more);
            if (more) REL(more), more = NULL;
        }
        Sleep(800);
        for (i = 0; i < 3; i++)
        {
            ((HRESULT (WINAPI *)(void *, void **))SLOT(pool, 7))(pool, &more);
            if (more) { n++; REL(more); more = NULL; }
        }
        printf("wgcstatic new=%d\n", n);
    }
    {
        void *closable;
        if (SUCCEEDED(QI(session, &IID_IClosable_, &closable))) ((HRESULT (WINAPI *)(void *))SLOT(closable, 6))(closable);
    }
    return 0;
}

/* a loop of GetPixel calls on the screen DC (AutoHotkey's PixelGetColor, colour
 * pickers): fast -- the compositor's picture is reused for a frame -- and a
 * change on the screen seen on the next read after it is shown */
static int do_pixloop(HWND hwnd)
{
    HDC dc = GetDC(NULL);
    DWORD start = GetTickCount(), ms;
    COLORREF c = 0, before, after;
    int i;

    for (i = 0; i < 10000; i++) c = GetPixel(dc, centre[0].x, centre[0].y);
    ms = GetTickCount() - start;
    before = c;
    quad[0] = RGB(0, 255, 0);
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
    pump(150);
    after = GetPixel(dc, centre[0].x, centre[0].y);
    ReleaseDC(NULL, dc);
    printf("pixloop ms=%lu before=%02X%02X%02X after=%02X%02X%02X\n", ms, GetRValue(before), GetGValue(before),
           GetBValue(before), GetRValue(after), GetGValue(after), GetBValue(after));
    return 0;
}

int main(int argc, char **argv)
{
    WNDCLASSA wc = { 0 };
    HWND hwnd;
    int rc = 2;

    BOOL own = !(argc > 2 && !strcmp(argv[2], "nowin"));
    if (argc > 4) { extra_x = atoi(argv[3]); extra_y = atoi(argv[4]); }
    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "capprobe"; wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    RegisterClassA(&wc);
    hwnd = own ? CreateWindowExA(WS_EX_TOPMOST, "capprobe", "capprobe", WS_POPUP | WS_VISIBLE, WX, WY, WW, WH, NULL, NULL, wc.hInstance, NULL) : NULL;
    if (hwnd) { SetForegroundWindow(hwnd); UpdateWindow(hwnd); }
    pump(2500);
    if (argc > 1 && !strcmp(argv[1], "show")) { pump(300000); return 0; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "gdi")) rc = do_gdi();
    else if (argc > 1 && !strcmp(argv[1], "dxgi")) rc = do_dxgi();
    else if (argc > 1 && !strcmp(argv[1], "wgcmon")) rc = do_wgc(FALSE, 0);
    else if (argc > 1 && !strcmp(argv[1], "wgcwin")) rc = do_wgc(TRUE, 0);
    else if (argc > 1 && !strcmp(argv[1], "wgcstatic")) rc = do_wgc(TRUE, 1);
    else if (argc > 1 && !strcmp(argv[1], "wgccursor")) rc = do_wgc(FALSE, 2);
    else if (argc > 1 && !strcmp(argv[1], "wgchold")) rc = do_wgc(TRUE, 3);
    else if (argc > 1 && !strcmp(argv[1], "pixloop")) rc = do_pixloop(hwnd);
    else printf("usage: capprobe gdi|dxgi\n");
    pump(300);
    if (hwnd) DestroyWindow(hwnd);
    return rc;
}
