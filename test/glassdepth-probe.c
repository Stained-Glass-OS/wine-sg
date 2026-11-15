/* glassdepth-probe: drawing into a window with alpha (wine-sg 7786). A top-level that is a
 * sheet of glass (DwmExtendFrameIntoClientArea with -1 margins) or layered has an X window of
 * 32 bits; a child's frame (24 bits) was put there with XCopyArea and drawing through a DC as
 * 24 bits -- BadMatch, an X error that ends the process. DevToys 2 (WPF with Mica, a WebView2
 * control) lost its Direct3D renderer and Chromium's GPU process at the first frame.
 *
 * Usage: glassdepth-probe.exe vk|gl|gdi [layered] -- opens "glassdepth" (400x300 white, glass
 * or layered) with a child (200x150 at 50,50), and fills the child:
 *   vk   red, presented with Vulkan;
 *   gl   green, with OpenGL (SwapBuffers);
 *   gdi  blue, drawn by another process (glassdepth-probe.exe draw HWND) through the child's
 *        DC with SetDIBitsToDevice every 50 ms for 5 s, as DXVK's composition swap chains draw
 *        Chromium's frames.
 * Prints "<mode>=<1|0>" and "alive=1" after 4 s, then keeps running for 6 s. */
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <GL/gl.h>

#define LOAD(name) PFN_##name p##name = (PFN_##name)pvkGetInstanceProcAddr(inst, #name)

typedef struct { int l, r, t, b; } MARGINS_;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int present_color(HWND child, float red_c, float green_c, float blue_c);

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


static int present_gl(HWND child)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 24 };
    HDC dc = GetDC(child);
    int fmt = ChoosePixelFormat(dc, &pfd);
    HGLRC rc;
    if (!fmt || !SetPixelFormat(dc, fmt, &pfd)) return 0;
    if (!(rc = wglCreateContext(dc)) || !wglMakeCurrent(dc, rc)) return 0;
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    SwapBuffers(dc);
    glFinish();
    return 1;
}

static int draw_gdi(HWND child)
{
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), 200, -150, 1, 32, BI_RGB }};
    DWORD *bits = malloc(200 * 150 * 4);
    HDC dc;
    int i, ret;
    for (i = 0; i < 200 * 150; i++) bits[i] = 0x0000ff;
    dc = GetDCEx(child, NULL, DCX_CACHE | DCX_CLIPSIBLINGS);
    ret = SetDIBitsToDevice(dc, 0, 0, 200, 150, 0, 0, 0, 150, bits, &bi, DIB_RGB_COLORS);
    GdiFlush();
    ReleaseDC(child, dc);
    free(bits);
    return ret == 150;
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg); Sleep(20); }
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = { 0 };
    HWND top, child;
    const char *mode = argc > 1 ? argv[1] : "vk";
    BOOL layered = argc > 2 && !strcmp(argv[2], "layered");
    int ok = 0;

    if (!strcmp(mode, "draw") && argc > 2)
    {
        /* as a swap chain does: a frame every 50 ms, for 5 s */
        HWND child = (HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16);
        int i;
        for (i = 0, ok = 1; i < 100; i++) { ok &= draw_gdi(child); Sleep(50); }
        Sleep(500);  /* errors arrive after the request */
        printf("draw=%d\n", ok); fflush(stdout);
        return ok ? 0 : 1;
    }
    wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH); wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"glassdepth"; RegisterClassW(&wc);
    wc.lpszClassName = L"glassdepthchild"; wc.hbrBackground = NULL; RegisterClassW(&wc);
    top = CreateWindowExW(layered ? WS_EX_LAYERED : 0, L"glassdepth", L"glassdepth", WS_POPUP | WS_CLIPCHILDREN,
                          100, 100, 400, 300, NULL, NULL, NULL, NULL);
    if (layered) SetLayeredWindowAttributes(top, 0, 255, LWA_ALPHA);
    else
    {
        HRESULT (WINAPI *pDwmExtend)(HWND, const MARGINS_ *) =
            (void *)GetProcAddress(LoadLibraryA("dwmapi.dll"), "DwmExtendFrameIntoClientArea");
        MARGINS_ m = { -1, -1, -1, -1 };
        printf("glass=%08lx\n", pDwmExtend ? (unsigned long)pDwmExtend(top, &m) : 0xffffffffUL);
    }
    ShowWindow(top, SW_SHOWNOACTIVATE);
    child = CreateWindowW(L"glassdepthchild", L"pane", WS_CHILD | WS_VISIBLE, 50, 50, 200, 150, top, NULL, NULL, NULL);
    UpdateWindow(top);
    pump(800);
    if (!strcmp(mode, "vk")) ok = present_color(child, 1.0f, 0.0f, 0.0f);
    else if (!strcmp(mode, "gl")) ok = present_gl(child);
    else if (!strcmp(mode, "gdi"))
    {
        char cmd[MAX_PATH + 64], self[MAX_PATH];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 99;
        GetModuleFileNameA(NULL, self, MAX_PATH);
        sprintf(cmd, "\"%s\" draw %p", self, child);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            while (MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 15000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) pump(10);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        ok = code == 0;
        printf("drawer exit=%lu\n", code);
    }
    printf("%s=%d\n", mode, ok); fflush(stdout);
    pump(4000);
    printf("alive=1\n"); fflush(stdout);
    pump(6000);
    return 0;
}
