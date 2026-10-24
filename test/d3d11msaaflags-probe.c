/* d3d11msaaflags-probe.c (wine-sg 6097): CheckFormatSupport's multisample flags agree with
 * CheckMultisampleQualityLevels -- a format that some sample count above 1 works for is reported
 * multisample-renderable (ANGLE builds its sample counts only for such formats). KEY=VALUE lines. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>

int main(void)
{
    static const struct { DXGI_FORMAT format; const char *name; } formats[] =
    {
        {DXGI_FORMAT_R8G8B8A8_UNORM, "rgba8"},
        {DXGI_FORMAT_B8G8R8A8_UNORM, "bgra8"},
        {DXGI_FORMAT_R16G16B16A16_FLOAT, "rgba16f"},
        {DXGI_FORMAT_D24_UNORM_S8_UINT, "d24s8"},
    };
    D3D11_FEATURE_DATA_FORMAT_SUPPORT data;
    ID3D11Device *device;
    D3D_FEATURE_LEVEL level;
    UINT i, count, levels, support, any;
    HRESULT hr;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &device, &level, NULL);
    printf("device=%#lx\n", hr);
    if (FAILED(hr)) return 1;
    for (i = 0; i < ARRAYSIZE(formats); i++)
    {
        any = 0;
        printf("%s_counts=", formats[i].name);
        for (count = 2; count <= 8; count *= 2)
        {
            levels = 0;
            ID3D11Device_CheckMultisampleQualityLevels(device, formats[i].format, count, &levels);
            if (levels) { any = 1; printf("%u ", count); }
        }
        printf("\n");
        support = 0;
        ID3D11Device_CheckFormatSupport(device, formats[i].format, &support);
        data.InFormat = formats[i].format;
        data.OutFormatSupport = 0;
        ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_FORMAT_SUPPORT, &data, sizeof(data));
        printf("%s_any=%u\n", formats[i].name, any);
        printf("%s_msrt=%u\n", formats[i].name, !!(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET));
        printf("%s_msload=%u\n", formats[i].name, !!(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD));
        printf("%s_feature_same=%u\n", formats[i].name, data.OutFormatSupport == support);
    }
    ID3D11Device_Release(device);
    printf("done=1\n");
    return 0;
}
