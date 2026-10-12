/* d2d1 (patches/sg/2671): ID2D1Bitmap1::GetColorContext returns the color context the bitmap was created with
 * (a counted reference), or NULL when it was given none. */
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

int main(void)
{
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Factory1 *factory = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1ColorContext *cc = NULL, *got = NULL;
    ID2D1Bitmap1 *bitmap = NULL, *plain = NULL;
    D2D1_BITMAP_PROPERTIES1 props;
    D2D1_SIZE_U size = { 4, 4 };
    ULONG before, after;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d3d, NULL, NULL);
    check(hr == S_OK, "d3d11 device (%#lx)", hr);
    if (FAILED(hr)) goto done;
    ID3D11Device_QueryInterface(d3d, &IID_IDXGIDevice, (void **)&dxgi);
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL, (void **)&factory);
    check(hr == S_OK, "factory (%#lx)", hr);
    if (FAILED(hr)) goto done;
    ID2D1Factory1_CreateDevice(factory, dxgi, &device);
    hr = ID2D1Device_CreateDeviceContext(device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);
    check(hr == S_OK, "device context (%#lx)", hr);
    if (FAILED(hr)) goto done;

    hr = ID2D1DeviceContext_CreateColorContext(dc, D2D1_COLOR_SPACE_SRGB, NULL, 0, &cc);
    check(hr == S_OK && cc, "color context (%#lx)", hr);
    if (!cc) goto done;

    memset(&props, 0, sizeof(props));
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX = props.dpiY = 96.0f;
    hr = ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &props, &plain);
    check(hr == S_OK && plain, "a bitmap without a context (%#lx)", hr);
    got = (ID2D1ColorContext *)1;
    if (plain) ID2D1Bitmap1_GetColorContext(plain, &got);
    check(got == NULL, "it has none (%p)", got);

    props.colorContext = cc;
    before = (ID2D1ColorContext_AddRef(cc), ID2D1ColorContext_Release(cc));
    hr = ID2D1DeviceContext_CreateBitmap(dc, size, NULL, 0, &props, &bitmap);
    check(hr == S_OK && bitmap, "a bitmap with one (%#lx)", hr);
    got = NULL;
    if (bitmap) ID2D1Bitmap1_GetColorContext(bitmap, &got);
    check(got == cc, "GetColorContext returns it (%p, %p)", got, cc);
    after = (ID2D1ColorContext_AddRef(cc), ID2D1ColorContext_Release(cc));
    check(after == before + 2, "the bitmap holds one and GetColorContext gave another (%lu -> %lu)", before, after);
    if (got) ID2D1ColorContext_Release(got);
    if (bitmap) ID2D1Bitmap1_Release(bitmap);
    after = (ID2D1ColorContext_AddRef(cc), ID2D1ColorContext_Release(cc));
    check(after == before, "releasing the bitmap lets it go (%lu)", after);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
