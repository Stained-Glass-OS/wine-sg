/* dxgi: results the Wine tests record from Windows (patches/sg/2628): ResizeBuffers1 of a
 * Direct3D 11 swap chain (node masks, present queues) and FindClosestMatchingMode with an
 * empty mode and a device. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
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
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    DXGI_SWAP_CHAIN_DESC desc = {0};
    IDXGISwapChain *swapchain = NULL;
    IDXGISwapChain3 *swapchain3 = NULL;
    ID3D11Device *device = NULL;
    IDXGIOutput *output = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIDevice *dxgi_device = NULL;
    DXGI_MODE_DESC mode = {0}, matching;
    UINT node_mask[2] = {1, 1};
    IUnknown *queue[2];
    HWND window;
    HRESULT hr;

    window = CreateWindowA("static", "dxgi", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    desc.BufferDesc.Width = 128; desc.BufferDesc.Height = 128; desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2;
    desc.OutputWindow = window; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3, D3D11_SDK_VERSION,
            &desc, &swapchain, &device, NULL, NULL);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, levels, 3, D3D11_SDK_VERSION,
                &desc, &swapchain, &device, NULL, NULL);
    check(SUCCEEDED(hr), "device and swap chain (%#lx)", hr);
    if (FAILED(hr)) goto done;

    hr = IDXGISwapChain_QueryInterface(swapchain, &IID_IDXGISwapChain3, (void **)&swapchain3);
    check(hr == S_OK, "IDXGISwapChain3 (%#lx)", hr);
    if (swapchain3)
    {
        queue[0] = queue[1] = (IUnknown *)device;
        hr = IDXGISwapChain3_ResizeBuffers1(swapchain3, 2, 320, 240, DXGI_FORMAT_B8G8R8A8_UNORM, 0, node_mask, queue);
        check(hr == DXGI_ERROR_INVALID_CALL, "a present queue on a Direct3D 11 swap chain = %#lx", hr);
        hr = IDXGISwapChain3_ResizeBuffers1(swapchain3, 2, 320, 240, DXGI_FORMAT_B8G8R8A8_UNORM, 0, NULL, queue);
        check(hr == DXGI_ERROR_INVALID_CALL, "a present queue without masks = %#lx", hr);
        node_mask[0] = node_mask[1] = 2;
        hr = IDXGISwapChain3_ResizeBuffers1(swapchain3, 2, 320, 240, DXGI_FORMAT_B8G8R8A8_UNORM, 0, node_mask, NULL);
        check(hr == DXGI_ERROR_INVALID_CALL, "node mask 2 on a single node = %#lx", hr);
        hr = IDXGISwapChain3_ResizeBuffers1(swapchain3, 0, 320, 240, DXGI_FORMAT_B8G8R8A8_UNORM, 0, node_mask, NULL);
        check(hr == DXGI_ERROR_INVALID_CALL, "and with the buffer count of the swap chain = %#lx", hr);
        node_mask[0] = node_mask[1] = 1;
        hr = IDXGISwapChain3_ResizeBuffers1(swapchain3, 2, 320, 240, DXGI_FORMAT_B8G8R8A8_UNORM, 0, node_mask, NULL);
        check(hr == S_OK, "node mask 1 resizes = %#lx", hr);
        {
            DXGI_SWAP_CHAIN_DESC now;

            IDXGISwapChain_GetDesc(swapchain, &now);
            check(now.BufferDesc.Width == 320 && now.BufferDesc.Height == 240, "to %ux%u", now.BufferDesc.Width, now.BufferDesc.Height);
        }
        hr = IDXGISwapChain3_ResizeBuffers1(swapchain3, 2, 200, 100, DXGI_FORMAT_B8G8R8A8_UNORM, 0, NULL, NULL);
        check(hr == S_OK, "without masks and queues it is ResizeBuffers = %#lx", hr);
        IDXGISwapChain3_Release(swapchain3);
    }

    ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    hr = IDXGIAdapter_EnumOutputs(adapter, 0, &output);
    if (SUCCEEDED(hr))
    {
        hr = IDXGIOutput_FindClosestMatchingMode(output, &mode, &matching, (IUnknown *)device);
        check(hr == S_OK, "an empty mode and a device = %#lx", hr);
        check(matching.Width > 0 && matching.Height > 0, "gives a resolution (%ux%u)", matching.Width, matching.Height);
        hr = IDXGIOutput_FindClosestMatchingMode(output, &mode, &matching, NULL);
        check(hr == DXGI_ERROR_INVALID_CALL, "and without a device = %#lx", hr);
        IDXGIOutput_Release(output);
    }
    else
        printf("note: no output (%#lx)\n", hr);
    IDXGIAdapter_Release(adapter);
    IDXGIDevice_Release(dxgi_device);
    IDXGISwapChain_Release(swapchain);
    ID3D11Device_Release(device);
done:
    DestroyWindow(window);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
