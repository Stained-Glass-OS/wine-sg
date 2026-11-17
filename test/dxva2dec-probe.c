/* dxva2dec-probe: Wine's dxva2.dll hands its decoder calls to the D3D9 device's own decoder service
 * (7849), for dxva2dec-gate.sh. A stand-in device answers the private interface id with a stand-in
 * service (one decoder GUID, NV12, one configuration, a decoder object); one without it has none.
 * Prints one line per case. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <stdio.h>

DEFINE_GUID(IID_ISgD3D9VideoDecoderService, 0x1f6b2c7e, 0x3a8d, 0x4f05, 0x9c, 0x21, 0x5e, 0x4d, 0x7b, 0x9a, 0x0c, 0x13);
DEFINE_GUID(GUID_TEST_DECODER, 0x11112222, 0x3333, 0x4444, 0x55, 0x55, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66);

/* ---- the stand-in decoder service ---- */
static IDirectXVideoDecoder fake_decoder;
static HRESULT WINAPI svc_qi(IDirectXVideoDecoderService *iface, REFIID riid, void **obj) { *obj = iface; return S_OK; }
static ULONG WINAPI svc_addref(IDirectXVideoDecoderService *iface) { return 2; }
static ULONG WINAPI svc_release(IDirectXVideoDecoderService *iface) { return 1; }
static HRESULT WINAPI svc_create_surface(IDirectXVideoDecoderService *iface, UINT w, UINT h, UINT bb, D3DFORMAT f, D3DPOOL p,
    DWORD u, DWORD t, IDirect3DSurface9 **s, HANDLE *sh) { return E_NOTIMPL; }
static HRESULT WINAPI svc_guids(IDirectXVideoDecoderService *iface, UINT *count, GUID **guids)
{
    *guids = CoTaskMemAlloc(sizeof(GUID));
    **guids = GUID_TEST_DECODER;
    *count = 1;
    return S_OK;
}
static HRESULT WINAPI svc_rts(IDirectXVideoDecoderService *iface, REFGUID guid, UINT *count, D3DFORMAT **formats)
{
    if (!IsEqualGUID(guid, &GUID_TEST_DECODER)) return E_INVALIDARG;
    *formats = CoTaskMemAlloc(sizeof(D3DFORMAT));
    **formats = (D3DFORMAT)MAKEFOURCC('N','V','1','2');
    *count = 1;
    return S_OK;
}
static HRESULT WINAPI svc_configs(IDirectXVideoDecoderService *iface, REFGUID guid, const DXVA2_VideoDesc *desc, void *r,
    UINT *count, DXVA2_ConfigPictureDecode **configs)
{
    *configs = CoTaskMemAlloc(sizeof(**configs));
    memset(*configs, 0, sizeof(**configs));
    (*configs)->ConfigBitstreamRaw = 2;
    *count = 1;
    return S_OK;
}
static HRESULT WINAPI svc_create_decoder(IDirectXVideoDecoderService *iface, REFGUID guid, const DXVA2_VideoDesc *desc,
    const DXVA2_ConfigPictureDecode *config, IDirect3DSurface9 **rts, UINT n, IDirectXVideoDecoder **dec)
{
    *dec = &fake_decoder;
    return S_OK;
}
static const IDirectXVideoDecoderServiceVtbl svc_vtbl = { svc_qi, svc_addref, svc_release, svc_create_surface,
    svc_guids, svc_rts, svc_configs, svc_create_decoder };
static IDirectXVideoDecoderService fake_service = { &svc_vtbl };

/* ---- the stand-in device: IUnknown only, and the private interface when asked to have it ---- */
static int has_decoders;
static HRESULT WINAPI dev_qi(IDirect3DDevice9 *iface, REFIID riid, void **obj)
{
    if (has_decoders && IsEqualGUID(riid, &IID_ISgD3D9VideoDecoderService)) { *obj = &fake_service; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI dev_addref(IDirect3DDevice9 *iface) { return 2; }
static ULONG WINAPI dev_release(IDirect3DDevice9 *iface) { return 1; }
static void *dev_vtbl[128];
static IDirect3DDevice9 fake_device = { (IDirect3DDevice9Vtbl *)dev_vtbl };

static void run(int with)
{
    IDirect3DDeviceManager9 *mgr;
    IDirectXVideoDecoderService *svc;
    UINT token, count = 0;
    HANDLE h;
    GUID *guids = NULL;
    D3DFORMAT *fmts = NULL;
    DXVA2_ConfigPictureDecode *configs = NULL;
    DXVA2_VideoDesc desc = { 320, 240 };
    IDirectXVideoDecoder *dec = NULL;
    IDirect3DSurface9 *rts[1] = { NULL };
    HRESULT hr;

    has_decoders = with;
    DXVA2CreateDirect3DDeviceManager9(&token, &mgr);
    IDirect3DDeviceManager9_ResetDevice(mgr, &fake_device, token);
    IDirect3DDeviceManager9_OpenDeviceHandle(mgr, &h);
    hr = IDirect3DDeviceManager9_GetVideoService(mgr, h, &IID_IDirectXVideoDecoderService, (void **)&svc);
    if (FAILED(hr)) { printf("%s: no service %#lx\n", with ? "with" : "without", hr); return; }
    hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(svc, &count, &guids);
    printf("%s: guids=%u", with ? "with" : "without", count);
    if (count && IsEqualGUID(&guids[0], &GUID_TEST_DECODER)) printf(" (its)");
    if (count)
    {
        hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(svc, &guids[0], &count, &fmts);
        printf(" formats=%u%s", count, count && fmts[0] == (D3DFORMAT)MAKEFOURCC('N','V','1','2') ? " (NV12)" : "");
        hr = IDirectXVideoDecoderService_GetDecoderConfigurations(svc, &guids[0], &desc, NULL, &count, &configs);
        printf(" configs=%u raw=%u", count, count ? configs[0].ConfigBitstreamRaw : 0);
        hr = IDirectXVideoDecoderService_CreateVideoDecoder(svc, &guids[0], &desc, configs, rts, 1, &dec);
        printf(" decoder=%s", SUCCEEDED(hr) && dec == &fake_decoder ? "its" : "none");
    }
    printf("\n");
    IDirectXVideoDecoderService_Release(svc);
    IDirect3DDeviceManager9_CloseDeviceHandle(mgr, h);
    IDirect3DDeviceManager9_Release(mgr);
}

int main(void)
{
    int i;
    for (i = 0; i < 128; i++) dev_vtbl[i] = NULL;
    dev_vtbl[0] = (void *)dev_qi; dev_vtbl[1] = (void *)dev_addref; dev_vtbl[2] = (void *)dev_release;
    CoInitialize(NULL);
    run(1);
    run(0);
    return 0;
}
