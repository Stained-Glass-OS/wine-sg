/* dxgi: IDXGISurface1 GetDC/ReleaseDC errors, GetSharedResourceAdapterLuid, CreateSwapChainForCoreWindow, display mode flags (patch 3719) */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define ERR_INVALID_CALL ((HRESULT)0x887A0001)

static const GUID IID_CoreInterop = {0x45d64a29, 0xa63e, 0x4cb6, {0xb4, 0x98, 0x57, 0x81, 0xd2, 0x98, 0xcb, 0x4f}};

struct fake { void **vt; LONG ref; HWND hwnd; int with_interop; };
static HRESULT WINAPI f_qi(void *t, REFIID iid, void **o)
{
    struct fake *w = t;
    if (IsEqualGUID(iid, &IID_IUnknown) || (w->with_interop && IsEqualGUID(iid, &IID_CoreInterop))) { *o = t; InterlockedIncrement(&w->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_addref(void *t) { return InterlockedIncrement(&((struct fake *)t)->ref); }
static ULONG WINAPI f_release(void *t) { return InterlockedDecrement(&((struct fake *)t)->ref); }
static HRESULT WINAPI f_hwnd(void *t, HWND *h) { *h = ((struct fake *)t)->hwnd; return S_OK; }
static HRESULT WINAPI f_handled(void *t, unsigned char b) { return S_OK; }
static void *fake_vt[] = { f_qi, f_addref, f_release, f_hwnd, f_handled };

static ID3D11Device *make_device(void)
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    ID3D11Device *d = NULL;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &d, NULL, NULL);
    if (FAILED(hr)) hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &d, NULL, NULL);
    return SUCCEEDED(hr) ? d : NULL;
}

int main(void)
{
    ID3D11Device *dev = make_device();
    IDXGIDevice *dxdev = NULL; IDXGIAdapter *adapter = NULL; IDXGIFactory2 *factory = NULL;
    D3D11_TEXTURE2D_DESC td = {0}; ID3D11Texture2D *tex = NULL, *shared = NULL; IDXGISurface1 *surf = NULL;
    HRESULT hr; HDC dc = NULL, dc2 = NULL;
    DXGI_ADAPTER_DESC ad;

    if (!dev) { printf("SKIP no d3d11 device\nRESULT: PASS\n"); return 0; }
    IUnknown_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dxdev);
    IDXGIDevice_GetAdapter(dxdev, &adapter);
    IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, (void **)&factory);
    CHECK(factory != NULL, "no IDXGIFactory2");
    IDXGIAdapter_GetDesc(adapter, &ad);

    /* surface GetDC / ReleaseDC */
    td.Width = td.Height = 64; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    hr = ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex); CHECK(hr == S_OK, "create %#lx", hr);
    ID3D11Texture2D_QueryInterface(tex, &IID_IDXGISurface1, (void **)&surf);
    hr = IDXGISurface1_GetDC(surf, FALSE, &dc); CHECK(hr == ERR_INVALID_CALL, "GetDC on a non-GDI surface %#lx", hr);
    IDXGISurface1_Release(surf); ID3D11Texture2D_Release(tex);
    td.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    hr = ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex); CHECK(hr == S_OK, "create gdi %#lx", hr);
    ID3D11Texture2D_QueryInterface(tex, &IID_IDXGISurface1, (void **)&surf);
    hr = IDXGISurface1_ReleaseDC(surf, NULL); CHECK(hr == ERR_INVALID_CALL, "ReleaseDC without GetDC %#lx", hr);
    hr = IDXGISurface1_GetDC(surf, TRUE, &dc); CHECK(hr == S_OK && dc, "GetDC %#lx", hr);
    hr = IDXGISurface1_GetDC(surf, FALSE, &dc2); CHECK(hr == ERR_INVALID_CALL, "second GetDC %#lx", hr);
    hr = IDXGISurface1_ReleaseDC(surf, NULL); CHECK(hr == S_OK, "ReleaseDC %#lx", hr);
    hr = IDXGISurface1_ReleaseDC(surf, NULL); CHECK(hr == ERR_INVALID_CALL, "second ReleaseDC %#lx", hr);
    hr = IDXGISurface1_GetDC(surf, FALSE, &dc); CHECK(hr == S_OK && dc, "GetDC after release %#lx", hr);
    { RECT r = {0, 0, 4, 4}; hr = IDXGISurface1_ReleaseDC(surf, &r); CHECK(hr == S_OK, "ReleaseDC with a dirty rect %#lx", hr); }
    IDXGISurface1_Release(surf); ID3D11Texture2D_Release(tex);

    /* shared resource adapter */
    td.MiscFlags = D3D11_RESOURCE_MISC_SHARED; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    hr = ID3D11Device_CreateTexture2D(dev, &td, NULL, &shared);
    if (hr == S_OK)
    {
        IDXGIResource *res = NULL; HANDLE h = NULL; LUID luid = { 5, 5 };
        IDXGIResource *r2; ID3D11Texture2D_QueryInterface(shared, &IID_IDXGIResource, (void **)&res);
        r2 = res; hr = IDXGIResource_GetSharedHandle(r2, &h); CHECK(hr == S_OK && h, "GetSharedHandle %#lx %p", hr, h);
        hr = IDXGIFactory2_GetSharedResourceAdapterLuid(factory, h, &luid);
        CHECK(hr == S_OK && luid.LowPart == ad.AdapterLuid.LowPart && luid.HighPart == ad.AdapterLuid.HighPart,
              "luid %#lx %lx:%lx want %lx:%lx", hr, luid.HighPart, luid.LowPart, ad.AdapterLuid.HighPart, ad.AdapterLuid.LowPart);
        hr = IDXGIFactory2_GetSharedResourceAdapterLuid(factory, (HANDLE)0x1234, &luid); CHECK(hr == ERR_INVALID_CALL, "bad handle %#lx", hr);
        hr = IDXGIFactory2_GetSharedResourceAdapterLuid(factory, NULL, &luid); CHECK(hr == ERR_INVALID_CALL, "null handle %#lx", hr);
        hr = IDXGIFactory2_GetSharedResourceAdapterLuid(factory, h, NULL); CHECK(hr == ERR_INVALID_CALL, "null luid %#lx", hr);
        IDXGIResource_Release(res); ID3D11Texture2D_Release(shared);
    }
    else CHECK(0, "shared texture %#lx", hr);

    /* CreateSwapChainForCoreWindow */
    {
        WNDCLASSA wc = { 0 }; HWND win; struct fake w = { fake_vt, 1, NULL, 1 }, plain = { fake_vt, 1, NULL, 0 };
        DXGI_SWAP_CHAIN_DESC1 sd = {0}; IDXGISwapChain1 *sc = NULL; IUnknown *unk = (IUnknown *)&w;
        wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "fastbdxgi"; wc.hInstance = GetModuleHandleA(NULL);
        RegisterClassA(&wc);
        win = CreateWindowA("fastbdxgi", "x", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
        w.hwnd = win;
        sd.Width = 64; sd.Height = 64; sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM; sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        hr = IDXGIFactory2_CreateSwapChainForCoreWindow(factory, (IUnknown *)dev, NULL, &sd, NULL, &sc); CHECK(hr == ERR_INVALID_CALL && !sc, "null window %#lx", hr);
        sc = (void *)0xdeadbeef; hr = IDXGIFactory2_CreateSwapChainForCoreWindow(factory, NULL, unk, &sd, NULL, &sc); CHECK(hr == ERR_INVALID_CALL && !sc, "null device %#lx", hr);
        sc = (void *)0xdeadbeef; hr = IDXGIFactory2_CreateSwapChainForCoreWindow(factory, (IUnknown *)dev, unk, NULL, NULL, &sc); CHECK(hr == ERR_INVALID_CALL && !sc, "null desc %#lx", hr);
        hr = IDXGIFactory2_CreateSwapChainForCoreWindow(factory, (IUnknown *)dev, unk, &sd, NULL, NULL); CHECK(hr == ERR_INVALID_CALL, "null out %#lx", hr);
        sc = (void *)0xdeadbeef; hr = IDXGIFactory2_CreateSwapChainForCoreWindow(factory, (IUnknown *)dev, (IUnknown *)&plain, &sd, NULL, &sc); CHECK(hr == ERR_INVALID_CALL && !sc, "no interop %#lx", hr);
        w.hwnd = NULL;
        hr = IDXGIFactory2_CreateSwapChainForCoreWindow(factory, (IUnknown *)dev, unk, &sd, NULL, &sc); CHECK(hr == ERR_INVALID_CALL && !sc, "NULL hwnd %#lx", hr);
        w.hwnd = win;
        hr = IDXGIFactory2_CreateSwapChainForCoreWindow(factory, (IUnknown *)dev, unk, &sd, NULL, &sc);
        CHECK(hr == S_OK && sc, "CoreWindow swap chain %#lx", hr);
        CHECK(w.ref == 1, "interop reference leaked: %ld", w.ref);
        if (sc)
        {
            HWND got = NULL; IDXGISwapChain1_GetHwnd(sc, &got); CHECK(got == win, "swap chain window %p want %p", got, win);
            IDXGISwapChain1_Release(sc);
        }
        DestroyWindow(win);
    }

    /* mode list flags */
    {
        IDXGIOutput *out = NULL;
        if (SUCCEEDED(IDXGIAdapter_EnumOutputs(adapter, 0, &out)))
        {
            UINT c0 = 0, c1 = 0, c2 = 0;
            hr = IDXGIOutput_GetDisplayModeList(out, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &c0, NULL); CHECK(hr == S_OK, "list 0 %#lx", hr);
            hr = IDXGIOutput_GetDisplayModeList(out, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &c1, NULL); CHECK(hr == S_OK, "list interlaced %#lx", hr);
            hr = IDXGIOutput_GetDisplayModeList(out, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED | DXGI_ENUM_MODES_SCALING, &c2, NULL); CHECK(hr == S_OK, "list both %#lx", hr);
            CHECK(c0 > 0 && c1 >= c0 && c2 >= c0, "mode counts %u %u %u", c0, c1, c2);
            IDXGIOutput_Release(out);
        }
    }
    IDXGIFactory2_Release(factory); IDXGIAdapter_Release(adapter); IDXGIDevice_Release(dxdev); ID3D11Device_Release(dev);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
