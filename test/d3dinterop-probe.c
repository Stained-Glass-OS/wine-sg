/* d3dinterop-probe.c: CreateDirect3D11DeviceFromDXGIDevice / CreateDirect3D11SurfaceFromDXGISurface
 * (wine-sg 7854; DXVK's d3d11.dll forwards them, sg-image dxvk/patches/0011) used as a screen capture
 * client uses them (Windows.Graphics.Capture: OBS's, PotPlayer's and Zoom's capture plugins): the
 * D3D11 device wrapped as the WinRT IDirect3DDevice and back; a rendered texture wrapped as the
 * IDirect3DSurface a capture frame hands out, its description, and its pixels read back through
 * IDirect3DDxgiInterfaceAccess; IClosable. The interop interfaces are declared here (the SDK's
 * windows.graphics.directx.direct3d11.interop.h). Prints PASS/FAIL/SKIP and RESULT. */
#define COBJMACROS
#define INITGUID
#include <stdio.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <inspectable.h>
#include <winstring.h>

DEFINE_GUID(IID_IDirect3DDxgiInterfaceAccess_, 0xa9b3d012, 0x3df2, 0x4ee3, 0xb8, 0xd1, 0x86, 0x95, 0xf4, 0x57, 0xd3, 0xc1);
DEFINE_GUID(IID_IDirect3DDevice_, 0xa37624ab, 0x8d5f, 0x4650, 0x9d, 0x3e, 0x9e, 0xae, 0x3d, 0x9b, 0xc6, 0x70);
DEFINE_GUID(IID_IDirect3DSurface_, 0x0bf4a146, 0x13c1, 0x4694, 0xbe, 0xe3, 0x7a, 0xbf, 0x15, 0xea, 0xf5, 0x86);
DEFINE_GUID(IID_IClosable_, 0x30d5a829, 0x7fa4, 0x4026, 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e);
#define RO_E_CLOSED_ ((HRESULT)0x80000013)

typedef struct { void *qi, *addref; ULONG (WINAPI *release)(void *); HRESULT (WINAPI *get)(void *, REFIID, void **); } access_vtbl;
typedef struct { void *qi, *addref; ULONG (WINAPI *release)(void *); void *iids;
                 HRESULT (WINAPI *class_name)(void *, HSTRING *); void *trust; HRESULT (WINAPI *call)(void *, void *); } winrt_vtbl;
typedef struct { INT32 width, height; INT32 format; INT32 count, quality; } surface_desc;
typedef struct { void *qi, *addref; ULONG (WINAPI *release)(void *); void *iids, *class_name, *trust; HRESULT (WINAPI *close)(void *); } closable_vtbl;

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } \
                           else { printf("PASS  " __VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HMODULE d3d11 = LoadLibraryA("d3d11.dll");
    HRESULT (WINAPI *create_device)(IDXGIDevice *, IInspectable **) = (void *)GetProcAddress(d3d11, "CreateDirect3D11DeviceFromDXGIDevice");
    HRESULT (WINAPI *create_surface)(IDXGISurface *, IInspectable **) = (void *)GetProcAddress(d3d11, "CreateDirect3D11SurfaceFromDXGISurface");
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    ID3D11Device *device = NULL, *back = NULL;
    ID3D11DeviceContext *ctx = NULL;
    IDXGIDevice *dxgi_device = NULL, *dxgi_back = NULL;
    IInspectable *gdevice = NULL, *gsurface = NULL, *other = NULL;
    void **access = NULL, **closable = NULL, **wsurf = NULL;
    D3D11_TEXTURE2D_DESC td = { 64, 32, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
                                D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    ID3D11Texture2D *tex = NULL, *frame = NULL, *staging = NULL;
    ID3D11RenderTargetView *rtv = NULL;
    IDXGISurface *dxgi_surface = NULL;
    surface_desc desc = {0};
    HSTRING name = NULL;
    HRESULT hr;
    char path[MAX_PATH] = "";

    setvbuf(stdout, NULL, _IONBF, 0);
    GetModuleFileNameA(d3d11, path, sizeof(path));
    printf("      d3d11.dll: %s\n", path);
    CHECK(create_device && create_surface, "d3d11.dll exports CreateDirect3D11DeviceFromDXGIDevice and CreateDirect3D11SurfaceFromDXGISurface");
    if (!create_device || !create_surface) { puts("RESULT: FAIL"); return 1; }

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2,
                           D3D11_SDK_VERSION, &device, NULL, &ctx);
    if (FAILED(hr)) { printf("SKIP  no Direct3D 11 device here (%#lx)\nRESULT: SKIP\n", hr); return 77; }
    ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);

    /* the device, wrapped and back */
    hr = create_device(dxgi_device, &gdevice);
    CHECK(hr == S_OK && gdevice, "CreateDirect3D11DeviceFromDXGIDevice wraps the device (%#lx)", hr);
    if (!gdevice) { puts("RESULT: FAIL"); return 1; }
    CHECK(IInspectable_QueryInterface(gdevice, &IID_IDirect3DDevice_, (void **)&other) == S_OK, "it is an IDirect3DDevice");
    if (other) IInspectable_Release(other);
    hr = IInspectable_GetRuntimeClassName(gdevice, &name);
    CHECK(hr == S_OK && name && WindowsGetStringLen(name), "with a runtime class name (%#lx)", hr);
    if (name) WindowsDeleteString(name);
    hr = IInspectable_QueryInterface(gdevice, &IID_IDirect3DDxgiInterfaceAccess_, (void **)&access);
    CHECK(hr == S_OK, "it answers IDirect3DDxgiInterfaceAccess (%#lx)", hr);
    if (access)
    {
        hr = ((access_vtbl *)*access)->get(access, &IID_ID3D11Device, (void **)&back);
        CHECK(hr == S_OK && back == device, "GetInterface(ID3D11Device) is the device it wraps (%#lx)", hr);
        hr = ((access_vtbl *)*access)->get(access, &IID_IDXGIDevice, (void **)&dxgi_back);
        CHECK(hr == S_OK && dxgi_back == dxgi_device, "GetInterface(IDXGIDevice) too (%#lx)", hr);
        if (back) ID3D11Device_Release(back);
        if (dxgi_back) IDXGIDevice_Release(dxgi_back);
        ((access_vtbl *)*access)->release(access);
    }
    hr = ((winrt_vtbl *)*(void **)gdevice)->call(gdevice, NULL);   /* IDirect3DDevice::Trim */
    CHECK(hr == S_OK, "IDirect3DDevice::Trim (%#lx)", hr);
    CHECK(create_device(NULL, &other) == E_INVALIDARG, "no device: E_INVALIDARG");
    CHECK(create_device((IDXGIDevice *)ctx, &other) == E_INVALIDARG, "a device context is no DXGI device: E_INVALIDARG");

    /* a captured frame: a texture the program renders into, handed out as an IDirect3DSurface */
    ID3D11Device_CreateTexture2D(device, &td, NULL, &tex);
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)tex, NULL, &rtv);
    {
        static const float color[4] = { 0.0f, 128.0f / 255.0f, 1.0f, 1.0f };   /* r g b a */
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, color);
    }
    ID3D11Texture2D_QueryInterface(tex, &IID_IDXGISurface, (void **)&dxgi_surface);
    ID3D11Texture2D_AddRef(tex);
    {
        ULONG before = ID3D11Texture2D_Release(tex);
    hr = create_surface(dxgi_surface, &gsurface);
    ID3D11Texture2D_AddRef(tex);
    CHECK(ID3D11Texture2D_Release(tex) == before + 1, "the wrapper holds the texture (one reference)");
    if (gsurface)
    {
        void **c = NULL;
        IInspectable_QueryInterface(gsurface, &IID_IClosable_, (void **)&c);
        if (c) ((closable_vtbl *)*c)->release(c);
    }
    }
    CHECK(hr == S_OK && gsurface, "CreateDirect3D11SurfaceFromDXGISurface wraps the texture (%#lx)", hr);
    if (gsurface)
    {
        IInspectable_QueryInterface(gsurface, &IID_IDirect3DSurface_, (void **)&wsurf);
        hr = wsurf ? ((HRESULT (WINAPI *)(void *, surface_desc *))((winrt_vtbl *)*wsurf)->call)(wsurf, &desc) : E_NOINTERFACE;
        CHECK(hr == S_OK && desc.width == 64 && desc.height == 32 && desc.format == DXGI_FORMAT_B8G8R8A8_UNORM && desc.count == 1,
              "IDirect3DSurface::Description: %dx%d format %d, %d sample(s) (%#lx)", desc.width, desc.height, desc.format, desc.count, hr);
        IInspectable_QueryInterface(gsurface, &IID_IDirect3DDxgiInterfaceAccess_, (void **)&access);
        hr = access ? ((access_vtbl *)*access)->get(access, &IID_ID3D11Texture2D, (void **)&frame) : E_NOINTERFACE;
        CHECK(hr == S_OK && frame == tex, "the frame's texture comes back through GetInterface (%#lx)", hr);
        if (frame)
        {
            D3D11_TEXTURE2D_DESC sd = td;
            D3D11_MAPPED_SUBRESOURCE map;
            DWORD pixel = 0;
            sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Device_CreateTexture2D(device, &sd, NULL, &staging);
            ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)staging, (ID3D11Resource *)frame);
            if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map)))
            {
                pixel = ((DWORD *)((BYTE *)map.pData + 7 * map.RowPitch))[9];
                ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)staging, 0);
            }
            CHECK((pixel & 0xffffff) == 0x0080ff || (pixel & 0xffffff) == 0x0081ff,
                  "and its pixels read back are what was rendered (%#lx, want 0x0080ff BGRA)", pixel);
            ID3D11Texture2D_Release(staging);
            ID3D11Texture2D_Release(frame);
        }
        /* closed, the wrapper lets the texture go */
        IInspectable_QueryInterface(gsurface, &IID_IClosable_, (void **)&closable);
        hr = closable ? ((closable_vtbl *)*closable)->close(closable) : E_NOINTERFACE;
        CHECK(hr == S_OK, "IClosable::Close (%#lx)", hr);
        {
            ULONG n;
            ID3D11Texture2D_AddRef(tex);
            n = ID3D11Texture2D_Release(tex);
            ID3D11Texture2D_AddRef(tex);
            ID3D11Texture2D_Release(tex);
            CHECK(n == 2, "closed, the wrapper's reference to the texture is gone (%lu left: ours and the view's)", n);
        }
        frame = NULL;
        hr = access ? ((access_vtbl *)*access)->get(access, &IID_ID3D11Texture2D, (void **)&frame) : E_FAIL;
        CHECK(hr == RO_E_CLOSED_ && !frame, "closed: GetInterface says RO_E_CLOSED (%#lx)", hr);
        hr = wsurf ? ((HRESULT (WINAPI *)(void *, surface_desc *))((winrt_vtbl *)*wsurf)->call)(wsurf, &desc) : E_FAIL;
        CHECK(hr == RO_E_CLOSED_, "and Description too (%#lx)", hr);
        if (closable) ((closable_vtbl *)*closable)->release(closable);
        if (access) ((access_vtbl *)*access)->release(access);
        if (wsurf) ((winrt_vtbl *)*wsurf)->release(wsurf);
        IInspectable_Release(gsurface);
    }
    IInspectable_Release(gdevice);
    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
