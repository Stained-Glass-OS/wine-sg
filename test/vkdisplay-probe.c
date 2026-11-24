/* vulkan-1's VK_KHR_display entry points (patches/sg/9554), 64- and 32-bit.
 * They were spec stubs: a program that called one without first checking for
 * the extension died of the stub's exception. Windows' vulkan-1.dll (the
 * Khronos loader) answers them itself when the driver lacks the extension:
 * no displays, planes or modes, zeroed plane capabilities, and an error for
 * what it cannot create. The calls use a real physical device when Vulkan
 * gives one, a placeholder handle otherwise (the answers do not depend on it).
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef int VkResult;
typedef void *VkInstance, *VkPhysicalDevice;
typedef ULONG64 VkDisplayKHR, VkDisplayModeKHR, VkSurfaceKHR;
#define VK_SUCCESS 0
#define VK_ERROR_INITIALIZATION_FAILED (-3)
#define VK_ERROR_EXTENSION_NOT_PRESENT (-7)

typedef struct { int sType; const void *pNext; unsigned flags; const void *app; unsigned layers; const char *const *pl;
                 unsigned exts; const char *const *pe; } VkInstanceCreateInfo;

int main(void)
{
    HMODULE vk = LoadLibraryA("vulkan-1.dll");
    VkResult (WINAPI *pCreateInstance)(const VkInstanceCreateInfo *, const void *, VkInstance *);
    VkResult (WINAPI *pEnumeratePhysicalDevices)(VkInstance, UINT32 *, VkPhysicalDevice *);
    VkResult (WINAPI *pDisplayProps)(VkPhysicalDevice, UINT32 *, void *);
    VkResult (WINAPI *pPlaneProps)(VkPhysicalDevice, UINT32 *, void *);
    VkResult (WINAPI *pPlaneDisplays)(VkPhysicalDevice, UINT32, UINT32 *, VkDisplayKHR *);
    VkResult (WINAPI *pModeProps)(VkPhysicalDevice, VkDisplayKHR, UINT32 *, void *);
    VkResult (WINAPI *pPlaneCaps)(VkPhysicalDevice, VkDisplayModeKHR, UINT32, void *);
    VkResult (WINAPI *pCreateMode)(VkPhysicalDevice, VkDisplayKHR, const void *, const void *, VkDisplayModeKHR *);
    VkResult (WINAPI *pCreateSurface)(VkInstance, const void *, const void *, VkSurfaceKHR *);
    VkInstanceCreateInfo ici = { 1 /* VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO */ };
    VkInstance inst = NULL;
    VkPhysicalDevice dev = (VkPhysicalDevice)(ULONG_PTR)0x1000;
    UINT32 n = 1, count;
    BYTE caps[68] /* VkDisplayPlaneCapabilitiesKHR */, info[64];
    VkDisplayModeKHR mode = 0x1234;
    VkSurfaceKHR surface = 0x5678;
    VkResult r;
    unsigned i;

    printf("info  %d-bit\n", (int)sizeof(void *) * 8);
    CHECK(vk != NULL, "vulkan-1.dll loads");
    if (!vk) goto done;
#define GET(p, name) (*(FARPROC *)&p = GetProcAddress(vk, name))
    GET(pCreateInstance, "vkCreateInstance");
    GET(pEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
    CHECK(GET(pDisplayProps, "vkGetPhysicalDeviceDisplayPropertiesKHR") && GET(pPlaneProps, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR") &&
          GET(pPlaneDisplays, "vkGetDisplayPlaneSupportedDisplaysKHR") && GET(pModeProps, "vkGetDisplayModePropertiesKHR") &&
          GET(pPlaneCaps, "vkGetDisplayPlaneCapabilitiesKHR") && GET(pCreateMode, "vkCreateDisplayModeKHR") &&
          GET(pCreateSurface, "vkCreateDisplayPlaneSurfaceKHR"), "the seven VK_KHR_display entry points are exported");
    if (!pCreateSurface) goto done;
    if (pCreateInstance && !pCreateInstance(&ici, NULL, &inst) && pEnumeratePhysicalDevices &&
        !pEnumeratePhysicalDevices(inst, &n, &dev) && n)
        printf("info  a real physical device\n");
    else
    {
        printf("info  no Vulkan device here; a placeholder handle\n");
        dev = (VkPhysicalDevice)(ULONG_PTR)0x1000;
    }

    count = 0xdeadbeef;
    r = pDisplayProps(dev, &count, NULL);
    CHECK(r == VK_SUCCESS && count == 0, "vkGetPhysicalDeviceDisplayPropertiesKHR: no displays (%d, %u)", r, count);
    count = 0xdeadbeef;
    r = pPlaneProps(dev, &count, NULL);
    CHECK(r == VK_SUCCESS && count == 0, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR: no planes (%d, %u)", r, count);
    count = 0xdeadbeef;
    r = pPlaneDisplays(dev, 0, &count, NULL);
    CHECK(r == VK_SUCCESS && count == 0, "vkGetDisplayPlaneSupportedDisplaysKHR: no displays (%d, %u)", r, count);
    count = 0xdeadbeef;
    r = pModeProps(dev, 0x42, &count, NULL);
    CHECK(r == VK_SUCCESS && count == 0, "vkGetDisplayModePropertiesKHR: no modes (%d, %u)", r, count);
    memset(caps, 0xff, sizeof(caps));
    r = pPlaneCaps(dev, 0x42, 0, caps);
    for (i = 0; i < sizeof(caps) && !caps[i]; i++);
    CHECK(r == VK_SUCCESS && i == sizeof(caps), "vkGetDisplayPlaneCapabilitiesKHR: zeroed capabilities (%d)", r);
    memset(info, 0, sizeof(info));
    r = pCreateMode(dev, 0x42, info, NULL, &mode);
    CHECK(r == VK_ERROR_INITIALIZATION_FAILED && mode == 0x1234, "vkCreateDisplayModeKHR: VK_ERROR_INITIALIZATION_FAILED (%d)", r);
    r = pCreateSurface(inst ? inst : (VkInstance)(ULONG_PTR)0x1000, info, NULL, &surface);
    CHECK(r == VK_ERROR_EXTENSION_NOT_PRESENT && surface == 0x5678, "vkCreateDisplayPlaneSurfaceKHR: VK_ERROR_EXTENSION_NOT_PRESENT (%d)", r);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
