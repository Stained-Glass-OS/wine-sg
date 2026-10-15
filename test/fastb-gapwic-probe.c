/* windowscodecs smoke: pixel format colour context, class factory LockServer */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void check_ctx(IWICImagingFactory *f, REFGUID fmt, UINT expected, const char *name)
{
    IWICComponentInfo *info; IWICPixelFormatInfo2 *pfi; IWICColorContext *ctx = NULL;
    WICColorContextType type; UINT cs = 0; HRESULT hr;
    hr = IWICImagingFactory_CreateComponentInfo(f, fmt, &info);
    CHECK(hr == S_OK, "%s: CreateComponentInfo %#lx", name, hr);
    if (FAILED(hr)) return;
    hr = IWICComponentInfo_QueryInterface(info, &IID_IWICPixelFormatInfo2, (void **)&pfi);
    CHECK(hr == S_OK, "%s: QI %#lx", name, hr);
    hr = IWICPixelFormatInfo2_GetColorContext(pfi, NULL);
    CHECK(hr == E_INVALIDARG, "%s: NULL arg %#lx", name, hr);
    hr = IWICPixelFormatInfo2_GetColorContext(pfi, &ctx);
    CHECK(hr == S_OK && ctx, "%s: GetColorContext %#lx", name, hr);
    if (ctx)
    {
        hr = IWICColorContext_GetType(ctx, &type);
        CHECK(hr == S_OK && type == WICColorContextExifColorSpace, "%s: type %d", name, type);
        hr = IWICColorContext_GetExifColorSpace(ctx, &cs);
        CHECK(hr == S_OK && cs == expected, "%s: exif %u != %u", name, cs, expected);
        IWICColorContext_Release(ctx);
    }
    IWICPixelFormatInfo2_Release(pfi);
    IWICComponentInfo_Release(info);
}

int main(void)
{
    IWICImagingFactory *factory; IClassFactory *cf; HRESULT hr;
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) { printf("FAIL factory %#lx\n", hr); return 1; }
    check_ctx(factory, &GUID_WICPixelFormat32bppBGRA, 1, "32bppBGRA");
    check_ctx(factory, &GUID_WICPixelFormat24bppRGB, 1, "24bppRGB");
    check_ctx(factory, &GUID_WICPixelFormat128bppRGBAFloat, 0xffff, "128bppRGBAFloat");
    hr = CoGetClassObject(&CLSID_WICImagingFactory, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    CHECK(hr == S_OK, "CoGetClassObject %#lx", hr);
    if (SUCCEEDED(hr))
    {
        hr = IClassFactory_LockServer(cf, TRUE);  CHECK(hr == S_OK, "LockServer(TRUE) %#lx", hr);
        hr = IClassFactory_LockServer(cf, FALSE); CHECK(hr == S_OK, "LockServer(FALSE) %#lx", hr);
        IClassFactory_Release(cf);
    }
    IWICImagingFactory_Release(factory);
    printf("RESULT: %s\n", failed ? "FAIL" : "PASS");
    return failed != 0;
}
