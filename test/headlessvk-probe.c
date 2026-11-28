/* headlessvk-probe: Direct3D 11 (DXVK, on Vulkan) and Vulkan with no display
 * driver -- a service in session 0, a program started without a desktop
 * (wine-sg 9559). Creates a Vulkan instance with VK_KHR_surface and
 * VK_KHR_win32_surface (what DXVK asks for), a D3D11 device, clears a render
 * target and reads it back. Prints its steps; DONE at the end. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <stdio.h>
typedef int VkResult;
typedef struct { int sType; const void *pNext; const char *name; unsigned ver; const char *engine; unsigned ever; unsigned api; } VkAppInfo;
typedef struct { int sType; const void *pNext; unsigned flags; const VkAppInfo *app; unsigned nlayers; const char *const *layers; unsigned next; const char *const *exts; } VkInstInfo;
typedef VkResult (WINAPI *PFN_create)(const VkInstInfo *, const void *, void **);
typedef void (WINAPI *PFN_destroy)(void *, const void *);
int main(void)
{
    HMODULE vk = LoadLibraryA("vulkan-1.dll");
    PFN_create create = vk ? (PFN_create)GetProcAddress(vk, "vkCreateInstance") : NULL;
    PFN_destroy destroy = vk ? (PFN_destroy)GetProcAddress(vk, "vkDestroyInstance") : NULL;
    const char *exts[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    VkAppInfo app = { 0, NULL, "probe", 1, "probe", 1, (1u << 22) | (1u << 12) };
    VkInstInfo info = { 1, NULL, 0, &app, 0, NULL, 2, exts };
    void *inst = NULL; VkResult r = -1;
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0; HRESULT hr;
    if (create) r = create(&info, NULL, &inst);
    printf("vkCreateInstance with win32_surface %d\n", r); fflush(stdout);
    if (!r && destroy) destroy(inst, NULL);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    printf("D3D11CreateDevice %#lx level %#x\n", hr, fl); fflush(stdout);
    if (SUCCEEDED(hr))
    {
        D3D11_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
        ID3D11Texture2D *tex, *stg; ID3D11RenderTargetView *rtv; D3D11_MAPPED_SUBRESOURCE m; float c[4] = {0, 1, 0, 1};
        ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex);
        td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Device_CreateTexture2D(dev, &td, NULL, &stg);
        ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)tex, NULL, &rtv);
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, c);
        ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)stg, (ID3D11Resource *)tex);
        if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)stg, 0, D3D11_MAP_READ, 0, &m)))
        {
            printf("pixel %08x\n", *(unsigned *)m.pData);
            ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)stg, 0);
        }
    }
    printf("DONE\n"); fflush(stdout);
    return 0;
}
