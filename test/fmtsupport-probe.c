/* D3D11 format support flags (patches/sg/1627), under Xvfb.
 * CheckFormatSupport was a partial stub: no DISPLAY, BACK_BUFFER_CAST,
 * CPU_LOCKABLE or SO_BUFFER for any format, and SHADER_SAMPLE and
 * MULTISAMPLE_RESOLVE for integer formats, which can be neither sampled
 * nor resolved (ANGLE, Chromium and engines pick their formats by these). */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

int main(void)
{
    ID3D11Device *device;
    D3D_FEATURE_LEVEL fl;
    UINT rgba = 0, bgra = 0, uint4 = 0, r32f = 0, r8 = 0;
    HRESULT hr;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &device, &fl, NULL);
    if (FAILED(hr))
    {
        printf("FAIL  no D3D11 device (%#lx)\nRESULT: FAIL\n", hr);
        return 1;
    }
    printf("feature level %#x\n", fl);
    ID3D11Device_CheckFormatSupport(device, DXGI_FORMAT_R8G8B8A8_UNORM, &rgba);
    ID3D11Device_CheckFormatSupport(device, DXGI_FORMAT_B8G8R8A8_UNORM, &bgra);
    ID3D11Device_CheckFormatSupport(device, DXGI_FORMAT_R32G32B32A32_UINT, &uint4);
    ID3D11Device_CheckFormatSupport(device, DXGI_FORMAT_R32_FLOAT, &r32f);
    ID3D11Device_CheckFormatSupport(device, DXGI_FORMAT_R8_UNORM, &r8);
    printf("R8G8B8A8_UNORM %#x, B8G8R8A8_UNORM %#x, R32G32B32A32_UINT %#x, R32_FLOAT %#x, R8_UNORM %#x\n",
           rgba, bgra, uint4, r32f, r8);

    check(rgba & D3D11_FORMAT_SUPPORT_DISPLAY, "R8G8B8A8_UNORM can be displayed");
    check((bgra & D3D11_FORMAT_SUPPORT_DISPLAY) && (bgra & D3D11_FORMAT_SUPPORT_BACK_BUFFER_CAST),
          "B8G8R8A8_UNORM can be displayed and cast as a back buffer");
    check(!(r8 & D3D11_FORMAT_SUPPORT_DISPLAY), "R8_UNORM cannot be displayed");
    check((rgba & D3D11_FORMAT_SUPPORT_CPU_LOCKABLE) && (uint4 & D3D11_FORMAT_SUPPORT_CPU_LOCKABLE),
          "resources of these formats can be mapped (CPU_LOCKABLE)");
    check(rgba & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE, "R8G8B8A8_UNORM is sampled");
    check(!(uint4 & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) && (uint4 & D3D11_FORMAT_SUPPORT_SHADER_LOAD),
          "R32G32B32A32_UINT is loaded, not sampled");
    check(!(uint4 & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE), "... and not resolved");
    if (fl >= D3D_FEATURE_LEVEL_10_0)
        check(r32f & D3D11_FORMAT_SUPPORT_SO_BUFFER, "R32_FLOAT can be a stream output buffer");
    ID3D11Device_Release(device);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
