/* vkshare-probe: VK_KHR_external_memory_win32 (wine-sg 6261). An image made in one VkDevice with
 * exportable memory is cleared to a colour; its memory is exported as a Win32 handle and imported by
 * a second device, which copies the image to a buffer and reads the pixels:
 *   - a KMT (global D3DKMT) handle, imported by a second device of the same process;
 *   - an NT handle, duplicated into a child process (DuplicateHandle), imported there.
 * DXVK shares Direct3D 11 textures so (Qt WebEngine and Chromium hand their frames to the browser
 * process as shared textures); Wine 10 had no such extension and the frames were lost.
 * Also: the extension is listed, the image format says the handle types are exportable and
 * importable, and the D3DKMT resource behind the memory keeps runtime data that
 * D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE gives it (DXVK keeps a texture's description there).
 *
 * Usage: vkshare-probe.exe            (prints PASS/FAIL lines and RESULT)
 *        vkshare-probe.exe child      (reads an NT handle value on stdin, imports it, prints the pixel) */
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

/* the few D3DKMT structures used here (layouts of the Windows DDK) */
typedef UINT D3DKMT_HANDLE;
typedef struct { HDC hDc; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; UINT VidPnSourceId; } D3DKMT_OPENADAPTERFROMHDC;
typedef struct { union { D3DKMT_HANDLE hAdapter; void *pAdapter; }; UINT Flags; D3DKMT_HANDLE hDevice; void *pCommandBuffer; UINT CommandBufferSize;
                 void *pAllocationList; UINT AllocationListSize; void *pPatchLocationList; UINT PatchLocationListSize; } D3DKMT_CREATEDEVICE;
typedef struct { D3DKMT_HANDLE hDevice; HANDLE hNtHandle; void *pPrivateRuntimeData; UINT PrivateRuntimeDataSize;
                 UINT TotalPrivateDriverDataSize; UINT ResourcePrivateDriverDataSize; UINT NumAllocations; } D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE;
typedef struct { D3DKMT_HANDLE hAllocation; const void *pPrivateDriverData; UINT PrivateDriverDataSize;
                 UINT64 GpuVirtualAddress; ULONG_PTR Reserved[6]; } D3DDDI_OPENALLOCATIONINFO2;
typedef struct { D3DKMT_HANDLE hDevice; HANDLE hNtHandle; UINT NumAllocations; D3DDDI_OPENALLOCATIONINFO2 *pOpenAllocationInfo2;
                 UINT PrivateRuntimeDataSize; void *pPrivateRuntimeData; UINT ResourcePrivateDriverDataSize;
                 void *pResourcePrivateDriverData; UINT TotalPrivateDriverDataBufferSize; void *pTotalPrivateDriverDataBuffer;
                 D3DKMT_HANDLE hResource; D3DKMT_HANDLE hKeyedMutex; void *pKeyedMutexPrivateRuntimeData;
                 UINT KeyedMutexPrivateRuntimeDataSize; D3DKMT_HANDLE hSyncObject; } D3DKMT_OPENRESOURCEFROMNTHANDLE;
typedef struct { D3DKMT_HANDLE hAdapter; D3DKMT_HANDLE hDevice; UINT Type; UINT Flags; void *pPrivateDriverData;
                 UINT PrivateDriverDataSize; D3DKMT_HANDLE hContext; } D3DKMT_ESCAPE;

static int fails;
static void check(int ok, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("%s  ", ok ? "PASS" : "FAIL");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    if (!ok) fails++;
}

#define W 64
#define H 32

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
static VkPhysicalDevice phys;
static uint32_t qf;

#define F(name) PFN_##name p##name = (PFN_##name)gipa(inst, #name)

struct dev
{
    VkDevice dev;
    VkQueue queue;
    VkCommandPool pool;
};

static int init_instance(void)
{
    HMODULE vk = LoadLibraryA("vulkan-1.dll");
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkPhysicalDevice list[8];
    uint32_t n = 8, i, nq = 16;
    VkQueueFamilyProperties qprops[16];

    if (!vk || !(gipa = (PFN_vkGetInstanceProcAddr)GetProcAddress(vk, "vkGetInstanceProcAddr"))) return 0;
    app.apiVersion = VK_API_VERSION_1_1;
    ici.pApplicationInfo = &app;
    if (((PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance"))(&ici, NULL, &inst)) return 0;
    {
        F(vkEnumeratePhysicalDevices); F(vkGetPhysicalDeviceQueueFamilyProperties);
        if (pvkEnumeratePhysicalDevices(inst, &n, list) < 0 || !n) return 0;
        phys = list[0];
        pvkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qprops);
        for (i = 0; i < nq; i++) if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qf = i; break; }
    }
    return 1;
}

static int has_semaphores;

static int has_extension(const char *name)
{
    F(vkEnumerateDeviceExtensionProperties);
    VkExtensionProperties props[512];
    uint32_t n = 512, i;
    pvkEnumerateDeviceExtensionProperties(phys, NULL, &n, props);
    for (i = 0; i < n; i++) if (!strcmp(props[i].extensionName, name)) return 1;
    return 0;
}

static int make_device(struct dev *d)
{
    F(vkCreateDevice); F(vkGetDeviceQueue); F(vkCreateCommandPool);
    const char *exts[] = { "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32" };
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };

    qci.queueFamilyIndex = qf;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = has_semaphores ? 2 : 1;
    timeline.timelineSemaphore = VK_TRUE;
    if (has_semaphores) dci.pNext = &timeline;
    dci.ppEnabledExtensionNames = exts;
    if (pvkCreateDevice(phys, &dci, NULL, &d->dev)) return 0;
    pvkGetDeviceQueue(d->dev, qf, 0, &d->queue);
    pci.queueFamilyIndex = qf;
    return !pvkCreateCommandPool(d->dev, &pci, NULL, &d->pool);
}

static uint32_t memory_type(uint32_t bits, VkMemoryPropertyFlags want)
{
    F(vkGetPhysicalDeviceMemoryProperties);
    VkPhysicalDeviceMemoryProperties mp;
    uint32_t i;
    pvkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return ~0u;
}

static VkImage make_image(struct dev *d, VkExternalMemoryHandleTypeFlags types)
{
    F(vkCreateImage);
    VkExternalMemoryImageCreateInfo ext = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkImage image = VK_NULL_HANDLE;

    ext.handleTypes = types;
    ii.pNext = &ext;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent.width = W;
    ii.extent.height = H;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (pvkCreateImage(d->dev, &ii, NULL, &image)) return VK_NULL_HANDLE;
    return image;
}

/* memory for the image: exported (types) or imported (handle) */
static VkDeviceMemory image_memory(struct dev *d, VkImage image, VkExternalMemoryHandleTypeFlags export_types,
                                   VkExternalMemoryHandleTypeFlagBits import_type, HANDLE import_handle, VkResult *res)
{
    F(vkGetImageMemoryRequirements); F(vkAllocateMemory); F(vkBindImageMemory);
    VkMemoryRequirements req;
    VkMemoryDedicatedAllocateInfo dedicated = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
    VkExportMemoryAllocateInfo exp = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
    VkImportMemoryWin32HandleInfoKHR imp = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkDeviceMemory mem = VK_NULL_HANDLE;

    pvkGetImageMemoryRequirements(d->dev, image, &req);
    dedicated.image = image;
    if (export_types)
    {
        exp.handleTypes = export_types;
        exp.pNext = &dedicated;
        ai.pNext = &exp;
    }
    else
    {
        imp.handleType = import_type;
        imp.handle = import_handle;
        imp.pNext = &dedicated;
        ai.pNext = &imp;
    }
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == ~0u) ai.memoryTypeIndex = memory_type(req.memoryTypeBits, 0);
    *res = pvkAllocateMemory(d->dev, &ai, NULL, &mem);
    if (*res) return VK_NULL_HANDLE;
    if (pvkBindImageMemory(d->dev, image, mem, 0)) return VK_NULL_HANDLE;
    return mem;
}

static VkCommandBuffer begin(struct dev *d)
{
    F(vkAllocateCommandBuffers); F(vkBeginCommandBuffer);
    VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkCommandBuffer cb;
    ai.commandPool = d->pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    pvkAllocateCommandBuffers(d->dev, &ai, &cb);
    pvkBeginCommandBuffer(cb, &bi);
    return cb;
}

static void submit(struct dev *d, VkCommandBuffer cb)
{
    F(vkEndCommandBuffer); F(vkQueueSubmit); F(vkQueueWaitIdle);
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    pvkEndCommandBuffer(cb);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    pvkQueueSubmit(d->queue, 1, &si, VK_NULL_HANDLE);
    pvkQueueWaitIdle(d->queue);
}

static void barrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to)
{
    F(vkCmdPipelineBarrier);
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
}

static void clear(struct dev *d, VkImage image, float r, float g, float b)
{
    F(vkCmdClearColorImage);
    VkCommandBuffer cb = begin(d);
    VkClearColorValue color = {{ r, g, b, 1.0f }};
    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier(cb, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    pvkCmdClearColorImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
    barrier(cb, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    submit(d, cb);
}

/* the pixel at (x, y) of the image, through a copy to a host-visible buffer */
static uint32_t read_pixel(struct dev *d, VkImage image, int x, int y)
{
    F(vkCreateBuffer); F(vkGetBufferMemoryRequirements); F(vkAllocateMemory); F(vkBindBufferMemory);
    F(vkCmdCopyImageToBuffer); F(vkMapMemory);
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements req;
    VkBuffer buf;
    VkDeviceMemory mem;
    VkBufferImageCopy copy = { 0 };
    VkCommandBuffer cb;
    uint32_t *data = NULL;

    bci.size = W * H * 4;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (pvkCreateBuffer(d->dev, &bci, NULL, &buf)) return 0xdeadbeef;
    pvkGetBufferMemoryRequirements(d->dev, buf, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (pvkAllocateMemory(d->dev, &ai, NULL, &mem)) return 0xdeadbeef;
    pvkBindBufferMemory(d->dev, buf, mem, 0);
    cb = begin(d);
    barrier(cb, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = W;
    copy.imageExtent.height = H;
    copy.imageExtent.depth = 1;
    pvkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &copy);
    submit(d, cb);
    if (pvkMapMemory(d->dev, mem, 0, VK_WHOLE_SIZE, 0, (void **)&data) || !data) return 0xdeadbeef;
    return data[y * W + x];
}

/* the colour a clear to (0.6, 0.4, 0.2) gives in R8G8B8A8_UNORM, as a little-endian word */
static uint32_t expected(float r, float g, float b)
{
    return ((uint32_t)(r * 255 + 0.5f)) | ((uint32_t)(g * 255 + 0.5f) << 8) | ((uint32_t)(b * 255 + 0.5f) << 16) | 0xff000000u;
}

/* a timeline semaphore, exportable as the given Win32 handle type when it is not 0 */
static VkSemaphore make_semaphore(struct dev *d, VkExternalSemaphoreHandleTypeFlags types)
{
    F(vkCreateSemaphore);
    VkSemaphoreTypeCreateInfo type = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    VkExportSemaphoreCreateInfo export = { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphore sem = VK_NULL_HANDLE;

    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type.initialValue = 1;
    sci.pNext = &type;
    if (types)
    {
        export.handleTypes = types;
        type.pNext = &export;
    }
    if (pvkCreateSemaphore(d->dev, &sci, NULL, &sem)) return VK_NULL_HANDLE;
    return sem;
}

static VkResult import_semaphore(struct dev *d, VkSemaphore sem, VkExternalSemaphoreHandleTypeFlagBits type, HANDLE handle)
{
    PFN_vkImportSemaphoreWin32HandleKHR import = (PFN_vkImportSemaphoreWin32HandleKHR)
            ((PFN_vkGetDeviceProcAddr)gipa(inst, "vkGetDeviceProcAddr"))(d->dev, "vkImportSemaphoreWin32HandleKHR");
    VkImportSemaphoreWin32HandleInfoKHR info = { VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR };

    if (!import) return VK_ERROR_EXTENSION_NOT_PRESENT;
    info.semaphore = sem;
    info.handleType = type;
    info.handle = handle;
    return import(d->dev, &info);
}

static VkResult export_semaphore(struct dev *d, VkSemaphore sem, VkExternalSemaphoreHandleTypeFlagBits type, HANDLE *handle)
{
    PFN_vkGetSemaphoreWin32HandleKHR get = (PFN_vkGetSemaphoreWin32HandleKHR)
            ((PFN_vkGetDeviceProcAddr)gipa(inst, "vkGetDeviceProcAddr"))(d->dev, "vkGetSemaphoreWin32HandleKHR");
    VkSemaphoreGetWin32HandleInfoKHR info = { VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR };

    if (!get) return VK_ERROR_EXTENSION_NOT_PRESENT;
    info.semaphore = sem;
    info.handleType = type;
    return get(d->dev, &info, handle);
}

static uint64_t counter(struct dev *d, VkSemaphore sem)
{
    F(vkGetSemaphoreCounterValue);
    uint64_t value = 0;
    if (pvkGetSemaphoreCounterValue(d->dev, sem, &value)) return ~(uint64_t)0;
    return value;
}

static VkResult signal_semaphore(struct dev *d, VkSemaphore sem, uint64_t value)
{
    F(vkSignalSemaphore);
    VkSemaphoreSignalInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
    info.semaphore = sem;
    info.value = value;
    return pvkSignalSemaphore(d->dev, &info);
}

static VkResult wait_semaphore(struct dev *d, VkSemaphore sem, uint64_t value)
{
    F(vkWaitSemaphores);
    VkSemaphoreWaitInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    info.semaphoreCount = 1;
    info.pSemaphores = &sem;
    info.pValues = &value;
    return pvkWaitSemaphores(d->dev, &info, 5000000000ull);
}

static int child(HANDLE handle, HANDLE sem_handle)
{
    struct dev d;
    VkImage image;
    VkDeviceMemory mem;
    VkResult res = VK_ERROR_UNKNOWN;
    uint32_t pixel = 0;

    if (!init_instance()) { printf("child: no instance\n"); return 2; }
    has_semaphores = sem_handle && has_extension("VK_KHR_external_semaphore_win32");
    if (!make_device(&d)) { printf("child: no device\n"); return 2; }
    image = make_image(&d, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);
    mem = image ? image_memory(&d, image, 0, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT, handle, &res) : VK_NULL_HANDLE;
    if (mem) pixel = read_pixel(&d, image, 10, 20);
    printf("child import %d pixel %08x", res, pixel);
    if (sem_handle)
    {
        VkSemaphore sem = make_semaphore(&d, 0);
        VkResult sres = sem ? import_semaphore(&d, sem, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT, sem_handle)
                            : VK_ERROR_UNKNOWN;
        uint64_t before = sres ? 0 : counter(&d, sem);
        /* the parent waits for this signal */
        VkResult wres = sres ? sres : signal_semaphore(&d, sem, 21);
        printf(" sem %d value %llu signal %d", sres, (unsigned long long)before, wres);
    }
    printf("\n");
    return 0;
}

int main(int argc, char **argv)
{
    struct dev a, b;
    VkImage image_a = VK_NULL_HANDLE, image_b = VK_NULL_HANDLE, image_c = VK_NULL_HANDLE;
    VkDeviceMemory mem_a = VK_NULL_HANDLE, mem_b = VK_NULL_HANDLE, mem_c = VK_NULL_HANDLE;
    VkResult res = VK_ERROR_UNKNOWN;
    HANDLE kmt = NULL, nt = NULL;
    uint32_t pixel, want;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc >= 2 && !strcmp(argv[1], "child"))
    {
        char line[64] = "", *end;
        DWORD got = 0;
        unsigned long long h1, h2;
        ReadFile(GetStdHandle(STD_INPUT_HANDLE), line, sizeof(line) - 1, &got, NULL);
        line[got] = 0;
        h1 = strtoull(line, &end, 0);
        h2 = strtoull(end, NULL, 0);
        return child((HANDLE)(ULONG_PTR)h1, (HANDLE)(ULONG_PTR)h2);
    }

    if (!init_instance()) { printf("FAIL  no Vulkan instance\nRESULT: FAIL\n"); return 1; }
    check(has_extension("VK_KHR_external_memory_win32"), "VK_KHR_external_memory_win32 is listed");
    has_semaphores = has_extension("VK_KHR_external_semaphore_win32");
    if (GetEnvironmentVariableA("VKSHARE_SEMAPHORES", NULL, 0))
        check(has_semaphores, "VK_KHR_external_semaphore_win32 is listed");
    else if (!has_semaphores)
        printf("SKIP  VK_KHR_external_semaphore_win32 is not listed (the host has no opaque fd semaphores)\n");
    if (has_semaphores)
    {
        F(vkGetPhysicalDeviceExternalSemaphoreProperties);
        VkSemaphoreTypeCreateInfo type = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
        VkPhysicalDeviceExternalSemaphoreInfo si = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO };
        VkExternalSemaphoreProperties sp = { VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES };
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        si.pNext = &type;
        si.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        pvkGetPhysicalDeviceExternalSemaphoreProperties(phys, &si, &sp);
        check((sp.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) &&
              (sp.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) &&
              (sp.compatibleHandleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT),
              "timeline semaphores export and import as Win32 handles: features %#x compatible %#x",
              sp.externalSemaphoreFeatures, sp.compatibleHandleTypes);
    }
    {
        F(vkGetPhysicalDeviceImageFormatProperties2);
        VkPhysicalDeviceExternalImageFormatInfo ext = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO };
        VkPhysicalDeviceImageFormatInfo2 fi = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 };
        VkExternalImageFormatProperties eprops = { VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES };
        VkImageFormatProperties2 props = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };
        VkExternalMemoryFeatureFlags f;
        ext.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        fi.pNext = &ext;
        fi.format = VK_FORMAT_R8G8B8A8_UNORM;
        fi.type = VK_IMAGE_TYPE_2D;
        fi.tiling = VK_IMAGE_TILING_OPTIMAL;
        fi.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        props.pNext = &eprops;
        res = pvkGetPhysicalDeviceImageFormatProperties2(phys, &fi, &props);
        f = eprops.externalMemoryProperties.externalMemoryFeatures;
        check(!res && (f & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) && (f & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) &&
              (eprops.externalMemoryProperties.compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT),
              "an image can export and import Win32 memory: %d features %#x compatible %#x", res, f,
              eprops.externalMemoryProperties.compatibleHandleTypes);
    }
    if (!make_device(&a) || !make_device(&b)) { printf("FAIL  devices with the extension\nRESULT: FAIL\n"); return 1; }

    /* KMT, same process, second device */
    image_a = make_image(&a, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT);
    mem_a = image_a ? image_memory(&a, image_a, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT, 0, NULL, &res) : 0;
    check(mem_a != VK_NULL_HANDLE, "exportable memory (KMT): %d", res);
    if (mem_a)
    {
        F(vkGetDeviceProcAddr);
        PFN_vkGetMemoryWin32HandleKHR pvkGetMemoryWin32HandleKHR =
                (PFN_vkGetMemoryWin32HandleKHR)pvkGetDeviceProcAddr(a.dev, "vkGetMemoryWin32HandleKHR");
        VkMemoryGetWin32HandleInfoKHR gi = { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
        clear(&a, image_a, 0.6f, 0.4f, 0.2f);
        gi.memory = mem_a;
        gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
        res = pvkGetMemoryWin32HandleKHR ? pvkGetMemoryWin32HandleKHR(a.dev, &gi, &kmt) : VK_ERROR_EXTENSION_NOT_PRESENT;
        check(!res && kmt, "a KMT handle: %d %p", res, kmt);
        image_b = make_image(&b, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT);
        mem_b = image_b ? image_memory(&b, image_b, 0, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT, kmt, &res) : 0;
        check(mem_b != VK_NULL_HANDLE, "the second device imports it: %d", res);
        if (mem_b)
        {
            pixel = read_pixel(&b, image_b, 5, 7);
            want = expected(0.6f, 0.4f, 0.2f);
            check(pixel == want, "the second device sees the first's pixels: %08x, want %08x", pixel, want);
        }
    }

    /* NT handle, other process */
    image_c = make_image(&a, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);
    /* timeline semaphores shared between the two devices, by KMT and by NT handle */
    if (has_semaphores)
    {
        static const VkExternalSemaphoreHandleTypeFlagBits types[2] =
            { VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT };
        static const char *names[2] = { "KMT", "NT" };
        int t;

        for (t = 0; t < 2; t++)
        {
            VkSemaphore sa = make_semaphore(&a, types[t]), sb = make_semaphore(&b, 0);
            HANDLE h = NULL;
            VkResult er = VK_ERROR_UNKNOWN, ir = VK_ERROR_UNKNOWN, w1 = VK_ERROR_UNKNOWN, w2 = VK_ERROR_UNKNOWN;
            uint64_t seen = 0, seen2 = 0;

            if (sa && sb && !(er = export_semaphore(&a, sa, types[t], &h)) && h &&
                !(ir = import_semaphore(&b, sb, types[t], h)))
            {
                signal_semaphore(&a, sa, 5);
                w1 = wait_semaphore(&b, sb, 5);
                seen = counter(&b, sb);
                signal_semaphore(&b, sb, 8);
                w2 = wait_semaphore(&a, sa, 8);
                seen2 = counter(&a, sa);
            }
            check(sa && sb && !er && h, "a timeline semaphore exports a %s handle: %d %p", names[t], er, h);
            check(!ir && !w1 && seen == 5 && !w2 && seen2 == 8,
                  "the second device shares the semaphore by %s handle: import %d wait %d value %llu, back %d value %llu",
                  names[t], ir, w1, (unsigned long long)seen, w2, (unsigned long long)seen2);
            /* the imported semaphore shares what it imported: the same KMT handle */
            if (!ir)
            {
                HANDLE again = NULL;
                VkResult ar = export_semaphore(&b, sb, types[t], &again);
                check(!ar && again && (t == 1 || again == h),
                      "the imported semaphore exports a %s handle of its own sync object: %d %p (was %p)",
                      names[t], ar, again, h);
                if (t == 1 && again) CloseHandle(again);
            }
            if (t == 1 && h) CloseHandle(h);
        }
    }

    mem_c = image_c ? image_memory(&a, image_c, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT, 0, NULL, &res) : 0;
    check(mem_c != VK_NULL_HANDLE, "exportable memory (NT): %d", res);
    if (mem_c)
    {
        F(vkGetDeviceProcAddr);
        PFN_vkGetMemoryWin32HandleKHR pvkGetMemoryWin32HandleKHR =
                (PFN_vkGetMemoryWin32HandleKHR)pvkGetDeviceProcAddr(a.dev, "vkGetMemoryWin32HandleKHR");
        VkMemoryGetWin32HandleInfoKHR gi = { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 64], exe[MAX_PATH], out[256] = "";
        HANDLE in_child = NULL, rd, wr, sem_nt = NULL, sem_child = NULL;
        VkSemaphore sem_x = VK_NULL_HANDLE;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        DWORD got = 0;

        clear(&a, image_c, 0.2f, 0.8f, 0.4f);
        gi.memory = mem_c;
        gi.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        res = pvkGetMemoryWin32HandleKHR ? pvkGetMemoryWin32HandleKHR(a.dev, &gi, &nt) : VK_ERROR_EXTENSION_NOT_PRESENT;
        check(!res && nt, "an NT handle: %d %p", res, nt);
        /* the D3DKMT resource behind it keeps runtime data (DXVK's texture description) */
        {
            HMODULE gdi = GetModuleHandleA("gdi32.dll");
            NTSTATUS (WINAPI *open_adapter)(D3DKMT_OPENADAPTERFROMHDC *) = (void *)GetProcAddress(gdi, "D3DKMTOpenAdapterFromHdc");
            NTSTATUS (WINAPI *create_device)(D3DKMT_CREATEDEVICE *) = (void *)GetProcAddress(gdi, "D3DKMTCreateDevice");
            NTSTATUS (WINAPI *query)(D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *) = (void *)GetProcAddress(gdi, "D3DKMTQueryResourceInfoFromNtHandle");
            NTSTATUS (WINAPI *escape)(const D3DKMT_ESCAPE *) = (void *)GetProcAddress(gdi, "D3DKMTEscape");
            NTSTATUS (WINAPI *open)(D3DKMT_OPENRESOURCEFROMNTHANDLE *) = (void *)GetProcAddress(gdi, "D3DKMTOpenResourceFromNtHandle");
            D3DKMT_OPENADAPTERFROMHDC adapter = { 0 };
            D3DKMT_CREATEDEVICE device = { 0 };
            D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE q = { 0 };
            D3DKMT_OPENRESOURCEFROMNTHANDLE o = { 0 };
            D3DDDI_OPENALLOCATIONINFO2 alloc = { 0 };
            D3DKMT_ESCAPE e = { 0 };
            char runtime[64], driver[64], desc[24] = "texture description!";
            NTSTATUS s0 = -1, s1 = -1, s2 = -1, s3 = -1;

            adapter.hDc = GetDC(NULL);
            if (open_adapter && !(s0 = open_adapter(&adapter)))
            {
                device.hAdapter = adapter.hAdapter;
                s0 = create_device(&device);
            }
            q.hDevice = device.hDevice;
            q.hNtHandle = nt;
            q.pPrivateRuntimeData = runtime;
            q.PrivateRuntimeDataSize = sizeof(runtime);
            if (query) s1 = query(&q);
            o.hDevice = device.hDevice;
            o.hNtHandle = nt;
            o.NumAllocations = 1;
            o.pOpenAllocationInfo2 = &alloc;
            o.pPrivateRuntimeData = runtime;
            o.PrivateRuntimeDataSize = sizeof(runtime);
            o.pTotalPrivateDriverDataBuffer = driver;
            o.TotalPrivateDriverDataBufferSize = sizeof(driver);
            if (open) s2 = open(&o);
            e.Type = 0x80000000; /* D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE */
            e.pPrivateDriverData = desc;
            e.PrivateDriverDataSize = sizeof(desc);
            e.hContext = o.hResource;
            if (escape) s3 = escape(&e);
            q.PrivateRuntimeDataSize = sizeof(runtime);
            memset(runtime, 0, sizeof(runtime));
            if (query) query(&q);
            check(!s0 && !s1 && !s2 && !s3 && q.PrivateRuntimeDataSize == sizeof(desc) && !memcmp(runtime, desc, sizeof(desc)),
                  "the resource keeps the runtime data the escape gives it: %#lx %#lx %#lx %#lx size %u",
                  s0, s1, s2, s3, q.PrivateRuntimeDataSize);
        }

        if (has_semaphores && (sem_x = make_semaphore(&a, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)))
        {
            signal_semaphore(&a, sem_x, 9);
            if (export_semaphore(&a, sem_x, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT, &sem_nt)) sem_nt = NULL;
        }
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        /* the child reads the value of its copy of the handle on stdin, and answers on stdout */
        {
            HANDLE in_rd, in_wr;
            char line[32];
            DWORD put;

            CreatePipe(&rd, &wr, &sa, 0);
            CreatePipe(&in_rd, &in_wr, &sa, 0);
            SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = wr;
            si.hStdError = wr;
            si.hStdInput = in_rd;
            snprintf(cmd, sizeof(cmd), "\"%s\" child", exe);
            if (CreateProcessA(exe, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
            {
                DuplicateHandle(GetCurrentProcess(), nt, pi.hProcess, &in_child, 0, FALSE, DUPLICATE_SAME_ACCESS);
                if (sem_nt) DuplicateHandle(GetCurrentProcess(), sem_nt, pi.hProcess, &sem_child, 0, FALSE, DUPLICATE_SAME_ACCESS);
                snprintf(line, sizeof(line), "%#lx %#lx\n", (unsigned long)(ULONG_PTR)in_child,
                         (unsigned long)(ULONG_PTR)sem_child);
                WriteFile(in_wr, line, strlen(line), &put, NULL);
                CloseHandle(wr);
                wr = NULL;
                WaitForSingleObject(pi.hProcess, 60000);
                ReadFile(rd, out, sizeof(out) - 1, &got, NULL);
                out[got] = 0;
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            CloseHandle(in_rd);
            CloseHandle(in_wr);
            if (wr) CloseHandle(wr);
        }
        printf("      child said: %s", out);
        want = expected(0.2f, 0.8f, 0.4f);
        {
            int ires = -1, n;
            unsigned int px = 0;
            n = sscanf(out, "child import %d pixel %x", &ires, &px);
            check(n == 2 && ires == 0 && px == want,
                  "another process imports the NT handle and sees the pixels: %d %08x, want %08x", ires, px, want);
            if (has_semaphores)
            {
                int sres = -1, wres = -1;
                unsigned long long before = 0;
                VkResult pw = sem_x ? wait_semaphore(&a, sem_x, 21) : VK_ERROR_UNKNOWN;
                const char *p = strstr(out, " sem ");
                n = p ? sscanf(p, " sem %d value %llu signal %d", &sres, &before, &wres) : 0;
                check(sem_nt && n == 3 && !sres && before == 9 && !wres && !pw && counter(&a, sem_x) == 21,
                      "another process shares a semaphore by NT handle: import %d saw %llu, its signal %d, our wait %d",
                      sres, before, wres, pw);
            }
        }
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
