/* A shared D3D11 texture (MISC_SHARED, KMT and NT handles), as Chromium, Qt WebEngine and
 * WebView2 create for their GPU compositing. Prints which d3d11 answered and whether the
 * process survived creating and sharing it. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <stdio.h>
int main(void)
{
    ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; HRESULT hr;
    HMODULE m = LoadLibraryA("d3d11.dll");
    printf("d3d11=%s\n", m && !memcmp((char *)m + 0x40, "Wine builtin DLL", 16) ? "builtin" : "native");
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                           D3D11_SDK_VERSION, &dev, &fl, &ctx);
    printf("device=%08lx fl=%x\n", hr, fl); fflush(stdout);
    if (FAILED(hr)) return 2;
    UINT flags[2] = { D3D11_RESOURCE_MISC_SHARED, D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX };
    for (int i = 0; i < 2; i++)
    {
        D3D11_TEXTURE2D_DESC d = { 256, 256, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
                                   D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, flags[i] };
        ID3D11Texture2D *tex = NULL; IDXGIResource *res; IDXGIResource1 *res1; HANDLE h = NULL;
        hr = ID3D11Device_CreateTexture2D(dev, &d, NULL, &tex);
        printf("shared%d create=%08lx\n", i, hr); fflush(stdout);
        if (FAILED(hr)) continue;
        if (i == 0 && SUCCEEDED(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res)))
        {
            hr = IDXGIResource_GetSharedHandle(res, &h);
            printf("shared%d handle=%08lx\n", i, hr); IDXGIResource_Release(res);
        }
        if (i == 1 && SUCCEEDED(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource1, (void **)&res1)))
        {
            hr = IDXGIResource1_CreateSharedHandle(res1, NULL, GENERIC_ALL, NULL, &h);
            printf("shared%d handle=%08lx\n", i, hr); IDXGIResource1_Release(res1);
        }
        fflush(stdout);
        ID3D11Texture2D_Release(tex);
    }
    printf("survived=1\n");
    return 0;
}
