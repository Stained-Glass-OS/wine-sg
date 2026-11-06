/* vkshare gate helper (native): exit 0 when a host Vulkan device shares timeline semaphores as opaque fds */
#include <vulkan/vulkan.h>
#include <stdio.h>

int main(void)
{
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkPhysicalDevice devices[8];
    uint32_t n = 8, i;
    VkInstance inst;

    app.apiVersion = VK_API_VERSION_1_2;
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, NULL, &inst)) return 1;
    if (vkEnumeratePhysicalDevices(inst, &n, devices) < 0 || !n) return 1;
    /* the probe uses the first device */
    for (i = 0; i < 1; i++)
    {
        VkSemaphoreTypeCreateInfo type = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
        VkPhysicalDeviceExternalSemaphoreInfo info = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO };
        VkExternalSemaphoreProperties props = { VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES };
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        info.pNext = &type;
        info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        vkGetPhysicalDeviceExternalSemaphoreProperties(devices[i], &info, &props);
        if ((props.externalSemaphoreFeatures & 3) == 3) { printf("opaque fd timeline semaphores\n"); return 0; }
    }
    return 1;
}
