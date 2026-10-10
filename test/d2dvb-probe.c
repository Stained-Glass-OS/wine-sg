/* d2d1 (patches/sg/2645): the Wine tests record from Windows that a vertex buffer of an effect context can
 * be mapped (the same memory every time, a size over the buffer's is refused) and that a WIC bitmap
 * render target needs a pixel format that fits the bitmap's channel order and alpha. */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d2d1_3.h>
#include <d2d1effectauthor.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

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

static const GUID CLSID_VbEffect = {0x4a9b8c01, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};

static const WCHAR *effect_xml =
L"<?xml version='1.0'?><Effect><Property name='DisplayName' type='string' value='VbEffect'/>"
 "<Property name='Author' type='string' value='Wine'/><Property name='Category' type='string' value='Test'/>"
 "<Property name='Description' type='string' value='Test effect.'/><Inputs><Input name='Source'/></Inputs>"
 "<Property name='Context' type='iunknown'><Property name='DisplayName' type='string' value='Context'/></Property></Effect>";

struct impl { ID2D1EffectImpl iface; LONG ref; ID2D1EffectContext *context; ID2D1TransformGraph *graph; };
static struct impl *impl_from(ID2D1EffectImpl *i) { return (struct impl *)i; }
static HRESULT WINAPI impl_QI(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_ID2D1EffectImpl)) { *out = iface; ID2D1EffectImpl_AddRef(iface); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI impl_AddRef(ID2D1EffectImpl *iface) { return InterlockedIncrement(&impl_from(iface)->ref); }
static ULONG WINAPI impl_Release(ID2D1EffectImpl *iface)
{
    struct impl *impl = impl_from(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    if (!ref) { if (impl->context) ID2D1EffectContext_Release(impl->context); if (impl->graph) ID2D1TransformGraph_Release(impl->graph); free(impl); }
    return ref;
}
static HRESULT WINAPI impl_Initialize(ID2D1EffectImpl *iface, ID2D1EffectContext *ctx, ID2D1TransformGraph *graph)
{
    struct impl *impl = impl_from(iface);
    ID2D1EffectContext_AddRef(impl->context = ctx);
    ID2D1TransformGraph_AddRef(impl->graph = graph);
    return S_OK;
}
static HRESULT WINAPI impl_Prepare(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE t) { return S_OK; }
static HRESULT WINAPI impl_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *g) { return S_OK; }
static const ID2D1EffectImplVtbl impl_vtbl = { impl_QI, impl_AddRef, impl_Release, impl_Initialize, impl_Prepare, impl_SetGraph };
static HRESULT WINAPI impl_create(IUnknown **out)
{
    struct impl *o = calloc(1, sizeof(*o));

    o->iface.lpVtbl = &impl_vtbl; o->ref = 1;
    *out = (IUnknown *)&o->iface;
    return S_OK;
}
static HRESULT WINAPI impl_get_context(const IUnknown *iface, BYTE *data, UINT32 size, UINT32 *actual)
{
    *(ID2D1EffectContext **)data = impl_from((ID2D1EffectImpl *)iface)->context;
    if (actual) *actual = sizeof(void *);
    return S_OK;
}

int main(void)
{
    static const struct { DXGI_FORMAT dxgi; D2D1_ALPHA_MODE alpha; const GUID *wic; HRESULT hr; } fmts[] =
    {
        { DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED, &GUID_WICPixelFormat32bppPBGRA, S_OK },
        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED, &GUID_WICPixelFormat32bppPBGRA, S_OK },
        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, &GUID_WICPixelFormat32bppBGR, S_OK },
        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_UNKNOWN, &GUID_WICPixelFormat32bppBGR, S_OK },
        { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED, &GUID_WICPixelFormat32bppPRGBA, S_OK },
        { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, &GUID_WICPixelFormat32bppRGB, S_OK },
        { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED, &GUID_WICPixelFormat32bppPBGRA, E_INVALIDARG },
        { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, &GUID_WICPixelFormat32bppBGR, E_INVALIDARG },
        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED, &GUID_WICPixelFormat32bppBGR, E_INVALIDARG },
        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, &GUID_WICPixelFormat32bppPBGRA, E_INVALIDARG },
    };
    D2D1_PROPERTY_BINDING binding = { L"Context", NULL, impl_get_context };
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *factory = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1Effect *effect = NULL;
    ID2D1EffectContext *ectx = NULL;
    ID2D1VertexBuffer *buffer = NULL;
    IWICImagingFactory *wic = NULL;
    D2D1_VERTEX_BUFFER_PROPERTIES bp;
    FLOAT data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    BYTE *ptr = NULL, *ptr2 = NULL;
    unsigned int i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d3d, NULL, NULL);
    check(hr == S_OK, "d3d11 device (%#lx)", hr);
    if (FAILED(hr)) goto done;
    ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    check(hr == S_OK, "factory (%#lx)", hr);
    if (FAILED(hr)) goto done;
    hr = ID2D1Factory1_CreateDevice(factory, dxgi, &device);
    hr = ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);
    check(hr == S_OK, "device context (%#lx)", hr);

    hr = ID2D1Factory1_RegisterEffectFromString(factory, &CLSID_VbEffect, effect_xml, &binding, 1, impl_create);
    check(hr == S_OK, "register effect (%#lx)", hr);
    hr = ID2D1DeviceContext_CreateEffect(dc, &CLSID_VbEffect, &effect);
    check(hr == S_OK, "create effect (%#lx)", hr);
    if (effect)
    {
        hr = ID2D1Effect_GetValueByName(effect, L"Context", D2D1_PROPERTY_TYPE_IUNKNOWN, (BYTE *)&ectx, sizeof(ectx));
        check(hr == S_OK && ectx, "effect context (%#lx)", hr);
    }
    if (ectx)
    {
        bp.inputCount = 1; bp.usage = D2D1_VERTEX_USAGE_STATIC; bp.data = (const BYTE *)data; bp.byteWidth = sizeof(data);
        hr = ID2D1EffectContext_CreateVertexBuffer(ectx, &bp, NULL, NULL, &buffer);
        check(hr == S_OK && buffer, "vertex buffer (%#lx)", hr);
        if (buffer)
        {
            hr = ID2D1VertexBuffer_Map(buffer, &ptr, sizeof(data));
            check(hr == S_OK && ptr, "Map (%#lx)", hr);
            check(ptr && !memcmp(ptr, data, sizeof(data)), "it holds the initial data");
            check(ID2D1VertexBuffer_Unmap(buffer) == S_OK, "Unmap");
            hr = ID2D1VertexBuffer_Map(buffer, &ptr, sizeof(data) + 1);
            check(hr == E_INVALIDARG, "too large (%#lx)", hr);
            hr = ID2D1VertexBuffer_Map(buffer, &ptr, sizeof(data) - 1);
            check(hr == S_OK && ptr, "smaller (%#lx)", hr);
            check(ID2D1VertexBuffer_Unmap(buffer) == S_OK, "Unmap again");
            hr = ID2D1VertexBuffer_Map(buffer, &ptr, sizeof(data));
            check(hr == S_OK, "Map for the pair (%#lx)", hr);
            hr = ID2D1VertexBuffer_Map(buffer, &ptr2, sizeof(data));
            check(hr == S_OK && ptr == ptr2, "mapped twice, same memory (%#lx)", hr);
            check(ID2D1VertexBuffer_Unmap(buffer) == S_OK, "Unmap of the pair");
            ID2D1VertexBuffer_Release(buffer);
        }
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic);
    check(hr == S_OK, "WIC (%#lx)", hr);
    for (i = 0; wic && i < sizeof(fmts) / sizeof(fmts[0]); i++)
    {
        IWICBitmap *bitmap;
        ID2D1RenderTarget *rt = NULL;
        D2D1_RENDER_TARGET_PROPERTIES rtp = {0};

        IWICImagingFactory_CreateBitmap(wic, 16, 16, fmts[i].wic, WICBitmapCacheOnDemand, &bitmap);
        rtp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
        rtp.pixelFormat.format = fmts[i].dxgi; rtp.pixelFormat.alphaMode = fmts[i].alpha;
        rtp.dpiX = rtp.dpiY = 96.0f;
        hr = ID2D1Factory1_CreateWicBitmapRenderTarget(factory, bitmap, &rtp, &rt);
        check(hr == fmts[i].hr, "WIC target %u: %#lx (want %#lx)", i, hr, fmts[i].hr);
        if (rt) ID2D1RenderTarget_Release(rt);
        IWICBitmap_Release(bitmap);
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
