/* vkvp9-probe: is VK_KHR_video_decode_vp9 offered on the first Vulkan device that decodes video, and
 * does a device with its feature enabled come up? For vkvp9-gate.sh. Prints "vp9 offered=N device=R".
 * Then (wine-sg 8203: the structures 32-bit programs pass are converted) "vp9 feature=F caps=R level=L
 * session=S": the feature as vkGetPhysicalDeviceFeatures2 reports it, the VP9 profile 0 decode
 * capabilities (VkVideoDecodeVP9CapabilitiesKHR's maxLevel) and a video session for that profile. */
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
typedef struct { int sType; void *pNext; VkFlags features[64]; } VkPhysicalDeviceFeatures2;
typedef struct { int sType; const void *pNext; uint32_t stdProfile; } VkVideoDecodeVP9ProfileInfoKHR;
typedef struct { int sType; void *pNext; uint32_t maxLevel; } VkVideoDecodeVP9CapabilitiesKHR;
typedef struct { int sType; void *pNext; VkFlags flags; } VkVideoDecodeCapabilitiesKHR;
typedef struct { int sType; const void *pNext; VkFlags videoCodecOperation, chromaSubsampling, lumaBitDepth, chromaBitDepth; } VkVideoProfileInfoKHR;
typedef struct { uint32_t width, height; } VkExtent2D;
typedef struct { int sType; void *pNext; VkFlags flags; uint64_t minBitstreamBufferOffsetAlignment, minBitstreamBufferSizeAlignment;
                 VkExtent2D pictureAccessGranularity, minCodedExtent, maxCodedExtent; uint32_t maxDpbSlots, maxActiveReferencePictures;
                 VkExtensionProperties stdHeaderVersion; } VkVideoCapabilitiesKHR;
typedef struct { int sType; const void *pNext; uint32_t queueFamilyIndex; VkFlags flags; const VkVideoProfileInfoKHR *pVideoProfile;
                 int pictureFormat; VkExtent2D maxCodedExtent; int referencePictureFormat; uint32_t maxDpbSlots, maxActiveReferencePictures;
                 const VkExtensionProperties *pStdHeaderVersion; } VkVideoSessionCreateInfoKHR;
typedef struct { VkFlags queueFlags; uint32_t queueCount, timestampValidBits; uint32_t minImageTransferGranularity[3]; } VkQueueFamilyProperties;
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
            VkResult r;
            VkQueueFamilyProperties qf[16];
            uint32_t nqf = 16, q;
            void (WINAPI *getQueueFamilies)(VkPhysicalDevice, uint32_t *, VkQueueFamilyProperties *) = (void *)gipa(inst, "vkGetPhysicalDeviceQueueFamilyProperties");

            getQueueFamilies(devs[i], &nqf, qf);
            for (q = 0; q < nqf; q++) if (qf[q].queueFlags & 0x20 /* VIDEO_DECODE */) break;
            if (q < nqf) qci.queueFamilyIndex = q;
            r = createDevice(devs[i], &dci, NULL, &dev);
            printf("vp9 offered=%d device=%d\n", vp9, r);
            if (!r && vp9)
            {
                void (WINAPI *getFeatures2)(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *) = (void *)gipa(inst, "vkGetPhysicalDeviceFeatures2");
                VkResult (WINAPI *getCaps)(VkPhysicalDevice, const VkVideoProfileInfoKHR *, VkVideoCapabilitiesKHR *) =
                    (void *)gipa(inst, "vkGetPhysicalDeviceVideoCapabilitiesKHR");
                PFN_vkVoidFunction (WINAPI *gdpa)(VkDevice, const char *) = (void *)gipa(inst, "vkGetDeviceProcAddr");
                VkResult (WINAPI *createSession)(VkDevice, const VkVideoSessionCreateInfoKHR *, const void *, uint64_t *);
                void (WINAPI *destroySession)(VkDevice, uint64_t, const void *);
                VkPhysicalDeviceVideoDecodeVP9FeaturesKHR qfeat = { 1000514000, NULL, 0 };
                VkPhysicalDeviceFeatures2 f2 = { 1000059000, &qfeat };
                VkVideoDecodeVP9ProfileInfoKHR vp9prof = { 1000514003, NULL, 0 /* profile 0 */ };
                VkVideoProfileInfoKHR prof = { 1000023000, &vp9prof, 0x8 /* DECODE_VP9 */, 0x2 /* 420 */, 0x1 /* 8 bit */, 0x1 };
                VkVideoDecodeVP9CapabilitiesKHR vp9caps = { 1000514001, NULL, 0xdead };
                VkVideoDecodeCapabilitiesKHR dcaps = { 1000024001, &vp9caps, 0 };
                VkVideoCapabilitiesKHR caps;
                VkVideoSessionCreateInfoKHR sci;
                uint64_t session = 0;
                VkResult rc, rs = -1;

                getFeatures2(devs[i], &f2);
                memset(&caps, 0, sizeof(caps));
                caps.sType = 1000023001;
                caps.pNext = &dcaps;
                rc = getCaps(devs[i], &prof, &caps);
                createSession = (void *)gdpa(dev, "vkCreateVideoSessionKHR");
                destroySession = (void *)gdpa(dev, "vkDestroyVideoSessionKHR");
                if (!rc && createSession)
                {
                    memset(&sci, 0, sizeof(sci));
                    sci.sType = 1000023005;
                    sci.queueFamilyIndex = qci.queueFamilyIndex;
                    sci.pVideoProfile = &prof;
                    sci.pictureFormat = 1000156003;   /* G8_B8R8_2PLANE_420_UNORM */
                    sci.referencePictureFormat = 1000156003;
                    sci.maxCodedExtent = caps.maxCodedExtent;
                    sci.maxDpbSlots = caps.maxDpbSlots;
                    sci.maxActiveReferencePictures = caps.maxActiveReferencePictures;
                    sci.pStdHeaderVersion = &caps.stdHeaderVersion;
                    rs = createSession(dev, &sci, NULL, &session);
                    if (!rs) destroySession(dev, session, NULL);
                }
                printf("vp9 feature=%u caps=%d level=%d session=%d\n", qfeat.videoDecodeVP9, rc,
                       vp9caps.maxLevel == 0xdead ? -1 : (int)vp9caps.maxLevel, rs);
            }
            if (!r) destroyDevice(dev, NULL);
        }
        return 0;
    }
    printf("no video decode\n");
    return 0;
}
