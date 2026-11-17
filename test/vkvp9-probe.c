/* vkvp9-probe: is VK_KHR_video_decode_vp9 offered on the first Vulkan device that decodes video, and
 * does a device with its feature enabled come up? For vkvp9-gate.sh. Prints "vp9 offered=N device=R". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#define VK_NO_PROTOTYPES
#include <stdint.h>

typedef uint32_t VkFlags;
typedef int VkResult;
typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkDevice_T *VkDevice;
typedef void (WINAPI *PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (WINAPI *PFN_vkGetInstanceProcAddr)(VkInstance, const char *);
typedef struct { int sType; const void *pNext; const char *pApplicationName; uint32_t applicationVersion;
                 const char *pEngineName; uint32_t engineVersion; uint32_t apiVersion; } VkApplicationInfo;
typedef struct { int sType; const void *pNext; VkFlags flags; const VkApplicationInfo *pApplicationInfo;
                 uint32_t enabledLayerCount; const char *const *ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames; } VkInstanceCreateInfo;
typedef struct { char extensionName[256]; uint32_t specVersion; } VkExtensionProperties;
typedef struct { int sType; void *pNext; uint32_t videoDecodeVP9; } VkPhysicalDeviceVideoDecodeVP9FeaturesKHR;
typedef struct { int sType; const void *pNext; VkFlags flags; uint32_t queueFamilyIndex; uint32_t queueCount;
                 const float *pQueuePriorities; } VkDeviceQueueCreateInfo;
typedef struct { int sType; const void *pNext; VkFlags flags; uint32_t queueCreateInfoCount;
                 const VkDeviceQueueCreateInfo *pQueueCreateInfos; uint32_t enabledLayerCount; const char *const *ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames; const void *pEnabledFeatures; } VkDeviceCreateInfo;

int main(void)
{
    HMODULE vk = LoadLibraryA("vulkan-1.dll");
    PFN_vkGetInstanceProcAddr gipa = vk ? (void *)GetProcAddress(vk, "vkGetInstanceProcAddr") : NULL;
    VkApplicationInfo app = { 0, NULL, "vkvp9", 1, NULL, 0, (1u << 22) | (3u << 12) };
    VkInstanceCreateInfo ici = { 1, NULL, 0, &app, 0, NULL, 0, NULL };
    VkInstance inst;
    VkPhysicalDevice devs[8];
    uint32_t n = 8, i, j, count;
    VkResult (WINAPI *createInstance)(const VkInstanceCreateInfo *, const void *, VkInstance *);
    VkResult (WINAPI *enumDevs)(VkInstance, uint32_t *, VkPhysicalDevice *);
    VkResult (WINAPI *enumExts)(VkPhysicalDevice, const char *, uint32_t *, VkExtensionProperties *);
    VkResult (WINAPI *createDevice)(VkPhysicalDevice, const VkDeviceCreateInfo *, const void *, VkDevice *);
    void (WINAPI *destroyDevice)(VkDevice, const void *);

    if (!gipa) { printf("no vulkan\n"); return 0; }
    createInstance = (void *)gipa(NULL, "vkCreateInstance");
    if (!createInstance || createInstance(&ici, NULL, &inst)) { printf("no instance\n"); return 0; }
    enumDevs = (void *)gipa(inst, "vkEnumeratePhysicalDevices");
    enumExts = (void *)gipa(inst, "vkEnumerateDeviceExtensionProperties");
    createDevice = (void *)gipa(inst, "vkCreateDevice");
    destroyDevice = (void *)gipa(inst, "vkDestroyDevice");
    if (enumDevs(inst, &n, devs)) { printf("no devices\n"); return 0; }
    for (i = 0; i < n; i++)
    {
        VkExtensionProperties *props;
        int video = 0, vp9 = 0;
        count = 0;
        enumExts(devs[i], NULL, &count, NULL);
        props = calloc(count, sizeof(*props));
        enumExts(devs[i], NULL, &count, props);
        for (j = 0; j < count; j++)
        {
            if (!strcmp(props[j].extensionName, "VK_KHR_video_decode_queue")) video = 1;
            if (!strcmp(props[j].extensionName, "VK_KHR_video_decode_vp9")) vp9 = 1;
        }
        free(props);
        if (!video) continue;
        {
            static const char *exts[] = { "VK_KHR_video_queue", "VK_KHR_video_decode_queue", "VK_KHR_video_decode_vp9" };
            VkPhysicalDeviceVideoDecodeVP9FeaturesKHR feat = { 1000514000, NULL, 1 };
            float prio = 1.0f;
            VkDeviceQueueCreateInfo qci = { 2, NULL, 0, 0, 1, &prio };
            VkDeviceCreateInfo dci = { 3, &feat, 0, 1, &qci, 0, NULL, vp9 ? 3 : 2, exts, NULL };
            VkDevice dev;
            VkResult r = createDevice(devs[i], &dci, NULL, &dev);
            if (!r) destroyDevice(dev, NULL);
            printf("vp9 offered=%d device=%d\n", vp9, r);
        }
        return 0;
    }
    printf("no video decode\n");
    return 0;
}
