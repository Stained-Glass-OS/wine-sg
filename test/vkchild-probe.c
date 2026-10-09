/* vkchild-probe: a child window that presents with Vulkan keeps its picture
 * when the window is exposed (patches/sg/1523). Office draws its panes with
 * Direct3D (DXVK: Vulkan) into child windows; Windows' compositor keeps a
 * child's last frame on screen whatever GDI paints. Under X a child's frames
 * are rendered offscreen and copied into the top-level's X window, and the
 * top-level's window surface (GDI) was put over them whenever it was flushed
 * (an expose: a dialog closed over the window): white boxes where Excel's
 * formula bar and sheet tabs were, a white window until the mouse moved.
 *
 * With "nest" a window inside the child presents green before the child
 * presents red (patches/sg/1524): the inner frame stays over the outer one.
 *
 * Usage: vkchild-probe.exe [nest] -- opens "vkchild" (400x300 white) with a child
 * (200x150 at 50,50) cleared red once by Vulkan, prints "presented=<0|1>",
 * then keeps running (the gate covers and uncovers it, and reads pixels). */
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define LOAD(name) PFN_##name p##name = (PFN_##name)pvkGetInstanceProcAddr(inst, #name)

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int present_color(HWND child, float red_c, float green_c, float blue_c)
{
    HMODULE vk = LoadLibraryA("vulkan-1.dll");
    PFN_vkGetInstanceProcAddr pvkGetInstanceProcAddr;
    const char *iext[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    const char *dext[] = { "VK_KHR_swapchain" };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstance inst; VkPhysicalDevice phys[8]; uint32_t nphys = 8, qf = 0, nfmt = 8, nimg = 8, idx;
    VkWin32SurfaceCreateInfoKHR sci = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    VkSurfaceKHR surf; VkDevice dev; VkQueue queue; VkSwapchainKHR sc; VkImage imgs[8];
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    VkSurfaceFormatKHR fmts[8]; VkSurfaceCapabilitiesKHR caps;
    VkSwapchainCreateInfoKHR swci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkImageMemoryBarrier bar = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    VkClearColorValue red = {{ red_c, green_c, blue_c, 1.0f }};
    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkCommandPool pool; VkCommandBuffer cb; VkFence fence;
    VkResult r;

    if (!vk) return 0;
    pvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vk, "vkGetInstanceProcAddr");
    {
        PFN_vkCreateInstance pvkCreateInstance = (PFN_vkCreateInstance)pvkGetInstanceProcAddr(NULL, "vkCreateInstance");
        app.apiVersion = VK_API_VERSION_1_1;
        ici.pApplicationInfo = &app; ici.enabledExtensionCount = 2; ici.ppEnabledExtensionNames = iext;
        if (pvkCreateInstance(&ici, NULL, &inst)) return 0;
    }
    {
        LOAD(vkEnumeratePhysicalDevices); LOAD(vkCreateWin32SurfaceKHR); LOAD(vkCreateDevice);
        LOAD(vkGetDeviceQueue); LOAD(vkGetPhysicalDeviceSurfaceFormatsKHR); LOAD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
        LOAD(vkCreateSwapchainKHR); LOAD(vkGetSwapchainImagesKHR); LOAD(vkAcquireNextImageKHR);
        LOAD(vkCreateCommandPool); LOAD(vkAllocateCommandBuffers); LOAD(vkBeginCommandBuffer);
        LOAD(vkCmdPipelineBarrier); LOAD(vkCmdClearColorImage); LOAD(vkEndCommandBuffer);
        LOAD(vkQueueSubmit); LOAD(vkQueuePresentKHR); LOAD(vkCreateFence); LOAD(vkWaitForFences);
        LOAD(vkQueueWaitIdle); LOAD(vkResetFences);

        if (pvkEnumeratePhysicalDevices(inst, &nphys, phys) < 0 || !nphys) return 0;
        sci.hinstance = GetModuleHandleW(NULL); sci.hwnd = child;
        if (pvkCreateWin32SurfaceKHR(inst, &sci, NULL, &surf)) return 0;
        qci.queueFamilyIndex = qf; qci.queueCount = 1; qci.pQueuePriorities = &prio;
        dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = dext;
        if (pvkCreateDevice(phys[0], &dci, NULL, &dev)) return 0;
        pvkGetDeviceQueue(dev, qf, 0, &queue);
        pvkGetPhysicalDeviceSurfaceFormatsKHR(phys[0], surf, &nfmt, fmts);
        pvkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys[0], surf, &caps);
        swci.surface = surf; swci.minImageCount = caps.minImageCount; swci.imageFormat = fmts[0].format;
        swci.imageColorSpace = fmts[0].colorSpace; swci.imageExtent = caps.currentExtent;
        if (swci.imageExtent.width == 0xffffffff) { swci.imageExtent.width = 200; swci.imageExtent.height = 150; }
        swci.imageArrayLayers = 1; swci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swci.preTransform = caps.currentTransform; swci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swci.presentMode = VK_PRESENT_MODE_FIFO_KHR; swci.clipped = VK_TRUE;
        if ((r = pvkCreateSwapchainKHR(dev, &swci, NULL, &sc))) return 0;
        pvkGetSwapchainImagesKHR(dev, sc, &nimg, imgs);
        pvkCreateFence(dev, &fci, NULL, &fence);
        if (pvkAcquireNextImageKHR(dev, sc, UINT64_MAX, VK_NULL_HANDLE, fence, &idx) < 0) return 0;
        pvkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
        cpci.queueFamilyIndex = qf; pvkCreateCommandPool(dev, &cpci, NULL, &pool);
        cbai.commandPool = pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        pvkAllocateCommandBuffers(dev, &cbai, &cb);
        pvkBeginCommandBuffer(cb, &cbbi);
        bar.srcAccessMask = 0; bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.srcQueueFamilyIndex = bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.image = imgs[idx]; bar.subresourceRange = range;
        pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &bar);
        pvkCmdClearColorImage(cb, imgs[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &red, 1, &range);
        bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; bar.dstAccessMask = 0;
        bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; bar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &bar);
        pvkEndCommandBuffer(cb);
        si.commandBufferCount = 1; si.pCommandBuffers = &cb;
        pvkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
        pvkQueueWaitIdle(queue);
        pi.swapchainCount = 1; pi.pSwapchains = &sc; pi.pImageIndices = &idx;
        r = pvkQueuePresentKHR(queue, &pi);
        pvkQueueWaitIdle(queue);
        return r >= 0;
    }
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = { 0 };
    HWND top, child;
    MSG msg;
    int ok;

    if (argc > 1 && !strcmp(argv[1], "cover"))
    {
        /* a window over the probe's, gone after a second: what a closing dialog does */
        HWND w = CreateWindowW(L"STATIC", L"cover", WS_POPUP | WS_VISIBLE, 0, 0, 800, 600, NULL, NULL, NULL, NULL);
        DWORD end = GetTickCount() + 1000;
        while (GetTickCount() < end) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg); Sleep(20); }
        DestroyWindow(w);
        return 0;
    }
    wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH); wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"vkchild"; RegisterClassW(&wc);
    top = CreateWindowW(L"vkchild", L"vkchild", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, 100, 100, 400, 300, NULL, NULL, NULL, NULL);
    child = CreateWindowW(L"vkchild", L"pane", WS_CHILD | WS_VISIBLE, 50, 50, 200, 150, top, NULL, NULL, NULL);
    UpdateWindow(top);
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    Sleep(500);
    if (argc > 1 && !strcmp(argv[1], "nest"))
    {
        /* a window inside the child (60x40 at 20,20) presents green first,
         * then the child red: the inner frame must stay over the outer one */
        HWND inner = CreateWindowW(L"vkchild", L"inner", WS_CHILD | WS_VISIBLE, 20, 20, 60, 40, child, NULL, NULL, NULL);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        present_color(inner, 0.0f, 1.0f, 0.0f);
    }
    ok = present_color(child, 1.0f, 0.0f, 0.0f);
    printf("presented=%d\n", ok); fflush(stdout);
    if (argc > 1 && !strcmp(argv[1], "restack"))
    {
        /* the top-level is restacked (what showing an owned dialog does to
         * its owner): its child's frame is still where it was */
        Sleep(1000);
        SetWindowPos(top, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetWindowPos(top, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        printf("restacked\n"); fflush(stdout);
    }
    while (GetMessageW(&msg, NULL, 0, 0)) DispatchMessageW(&msg);
    return 0;
}
