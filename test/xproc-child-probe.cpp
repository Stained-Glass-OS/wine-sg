// xproc-child-probe: another process's frames in a window stay when the window repaints, and
// PrintWindow (PW_RENDERFULLCONTENT: the taskbar's thumbnails, Task View, DWM thumbnails) shows them
// (Stained Glass OS wine-sg 6500, 6501).
//   host [child|top]  a top-level (white) with two child windows (white); a render process presents
//                     red into the big child (child) or into the top-level (top), a few times, then
//                     stops presenting (a still page). The host repaints itself (erase and all, as an
//                     expose, a lock or a compositor starting would), and another process prints the
//                     top-level (as it is, then covered by another window). Then the host moves
//                     the big child away and repaints again.
//   render HWND       the render process, at low integrity as Chromium's sandboxed GPU process (a
//                     Vulkan swap chain on HWND, as DXVK makes for Chromium's Direct3D 11: DXVK's own
//                     frames do not reach a virtual X server's screen in this probe)
//   print HWND FILE   PrintWindow(HWND, PW_RENDERFULLCONTENT) from another process: the colour at the
//                     child's centre and at a spot of the top-level outside its children, to FILE
// Phases are written to C:\phase.txt for the gate, which reads the screen's pixels.
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 2
#endif

static void write_file(const char *name, const char *text)
{
    FILE *f = fopen(name, "w");
    if (f) { fputs(text, f); fclose(f); }
}

/* WM_WINE_CLIENT_SURFACE (Stained Glass OS 6500) as another program would send it */
#define SG_WM_CLIENT_SURFACE 0x8000000f

static void phase(const char *text)
{
    FILE *f = fopen("C:\\phase.txt", "w");
    if (f) { fputs(text, f); fclose(f); }
}

static void pump(DWORD ms)
{
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < ms)
    {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(20);
    }
}

#define F(n) static PFN_##n p##n
F(vkCreateInstance); F(vkCreateWin32SurfaceKHR); F(vkEnumeratePhysicalDevices); F(vkCreateDevice); F(vkGetDeviceQueue);
F(vkCreateSwapchainKHR); F(vkGetSwapchainImagesKHR); F(vkCreateCommandPool); F(vkCreateFence); F(vkAcquireNextImageKHR);
F(vkWaitForFences); F(vkResetFences); F(vkAllocateCommandBuffers); F(vkBeginCommandBuffer); F(vkCmdPipelineBarrier);
F(vkCmdClearColorImage); F(vkEndCommandBuffer); F(vkQueueSubmit); F(vkQueuePresentKHR);
#undef F

/* a Vulkan swap chain on TARGET (as DXVK makes for Chromium's Direct3D 11), presenting red */
static int render(HWND target)
{
    HMODULE vulkan = LoadLibraryA("vulkan-1.dll");
    PFN_vkGetInstanceProcAddr gipa = vulkan ? (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkan, "vkGetInstanceProcAddr") : NULL;
    VkInstance in = VK_NULL_HANDLE;
    if (!gipa) return 2;
#define L(n) p##n = (PFN_##n)gipa(in, #n)
    L(vkCreateInstance);
    const char *ie[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.enabledExtensionCount = 2; ici.ppEnabledExtensionNames = ie;
    if (pvkCreateInstance(&ici, NULL, &in)) return 2;
    L(vkCreateWin32SurfaceKHR); L(vkEnumeratePhysicalDevices); L(vkCreateDevice); L(vkGetDeviceQueue);
    L(vkCreateSwapchainKHR); L(vkGetSwapchainImagesKHR); L(vkCreateCommandPool); L(vkCreateFence);
    L(vkAcquireNextImageKHR); L(vkWaitForFences); L(vkResetFences); L(vkAllocateCommandBuffers);
    L(vkBeginCommandBuffer); L(vkCmdPipelineBarrier); L(vkCmdClearColorImage); L(vkEndCommandBuffer);
    L(vkQueueSubmit); L(vkQueuePresentKHR);
#undef L
    RECT cr; GetClientRect(target, &cr);
    VkWin32SurfaceCreateInfoKHR sci = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    sci.hinstance = GetModuleHandleA(NULL); sci.hwnd = target;
    VkSurfaceKHR surface;
    if (pvkCreateWin32SurfaceKHR(in, &sci, NULL, &surface)) return 3;
    uint32_t n = 4; VkPhysicalDevice pd[4];
    if (pvkEnumeratePhysicalDevices(in, &n, pd) < 0 || !n) return 3;
    float prio = 1;
    VkDeviceQueueCreateInfo q = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO }; q.queueCount = 1; q.pQueuePriorities = &prio;
    const char *de[] = { "VK_KHR_swapchain" };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &q; dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = de;
    VkDevice dev; if (pvkCreateDevice(pd[0], &dci, NULL, &dev)) return 3;
    VkQueue queue; pvkGetDeviceQueue(dev, 0, 0, &queue);
    VkSwapchainCreateInfoKHR sw = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sw.surface = surface; sw.minImageCount = 2; sw.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    sw.imageExtent.width = cr.right; sw.imageExtent.height = cr.bottom; sw.imageArrayLayers = 1;
    sw.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sw.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; sw.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sw.presentMode = VK_PRESENT_MODE_FIFO_KHR; sw.clipped = VK_TRUE;
    VkSwapchainKHR sc; if (pvkCreateSwapchainKHR(dev, &sw, NULL, &sc)) return 3;
    uint32_t count = 8; VkImage images[8]; pvkGetSwapchainImagesKHR(dev, sc, &count, images);
    VkCommandPoolCreateInfo cpi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO }; VkCommandPool pool;
    pvkCreateCommandPool(dev, &cpi, NULL, &pool);
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO }; VkFence fence; pvkCreateFence(dev, &fci, NULL, &fence);
    for (int i = 0; i < 40; i++)
    {
        uint32_t index;
        if (pvkAcquireNextImageKHR(dev, sc, ~0ull, VK_NULL_HANDLE, fence, &index) < 0) return 3;
        pvkWaitForFences(dev, 1, &fence, VK_TRUE, ~0ull); pvkResetFences(dev, 1, &fence);
        VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        ai.commandPool = pool; ai.commandBufferCount = 1;
        VkCommandBuffer cb; pvkAllocateCommandBuffers(dev, &ai, &cb);
        VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }; pvkBeginCommandBuffer(cb, &bi);
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.image = images[index];
        b.subresourceRange = range; b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
        VkClearColorValue red = { { 1, 0, 0, 1 } };
        pvkCmdClearColorImage(cb, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &red, 1, &range);
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask = 0;
        pvkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &b);
        pvkEndCommandBuffer(cb);
        VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO }; si.commandBufferCount = 1; si.pCommandBuffers = &cb;
        pvkQueueSubmit(queue, 1, &si, fence); pvkWaitForFences(dev, 1, &fence, VK_TRUE, ~0ull); pvkResetFences(dev, 1, &fence);
        VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        pi.swapchainCount = 1; pi.pSwapchains = &sc; pi.pImageIndices = &index;
        pvkQueuePresentKHR(queue, &pi);
        Sleep(50);
    }
    /* the message naming another process than the sender is refused, even from the host's own child */
    {
        char text[64];
        BOOL sent = SendNotifyMessageW(target, SG_WM_CLIENT_SURFACE, GetCurrentProcessId() + 4, 1);
        snprintf(text, sizeof(text), "sent=%d\n", sent);
        write_file("C:\\forged.txt", text);
    }
    Sleep(60000);   /* a still page: nothing more is presented */
    return 0;
}

static unsigned int pixel_of(const DWORD *bits, int w, int x, int y) { return bits[y * w + x] & 0xffffff; }

static int print(HWND target, const char *file)
{
    RECT wr; GetWindowRect(target, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    HDC screen = GetDC(0), mem = CreateCompatibleDC(screen);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void *bits; HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, 0, 0);
    SelectObject(mem, bmp);
    BOOL ok = PrintWindow(target, mem, PW_RENDERFULLCONTENT);
    GdiFlush();
    FILE *f = fopen(file, "w");
    if (!f) return 1;
    /* the big child is at 50,50 200x150; a spot of the top-level outside its children at 20,280 */
    fprintf(f, "ok=%d child=%06x spot=%06x\n", ok, pixel_of((DWORD *)bits, w, 150, 125),
            pixel_of((DWORD *)bits, w, 20, 280));
    fclose(f);
    return 0;
}

/* CMD started at low integrity, as Chromium's sandbox runs its GPU process; FALSE if it could not be */
static BOOL spawn_low(char *cmd, PROCESS_INFORMATION *pi)
{
    STARTUPINFOA si = { sizeof(si) };
    HANDLE token = NULL, low = NULL;
    SID_IDENTIFIER_AUTHORITY mandatory = SECURITY_MANDATORY_LABEL_AUTHORITY;
    PSID sid = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY, &token) ||
        !DuplicateTokenEx(token, 0, NULL, SecurityImpersonation, TokenPrimary, &low) ||
        !AllocateAndInitializeSid(&mandatory, 1, SECURITY_MANDATORY_LOW_RID, 0, 0, 0, 0, 0, 0, 0, &sid))
        return FALSE;
    TOKEN_MANDATORY_LABEL label = { { sid, SE_GROUP_INTEGRITY } };
    return SetTokenInformation(low, TokenIntegrityLevel, &label, sizeof(label) + GetLengthSid(sid)) &&
           CreateProcessAsUserA(low, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, pi);
}

int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "render"))
        return render((HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16));
    if (argc > 3 && !strcmp(argv[1], "print"))
        return print((HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16), argv[3]);
    if (argc > 1 && !strcmp(argv[1], "spoof"))
    {
        /* not started by the host: a low-integrity process of its own tells the host's window
         * that it has a swap chain there, which it has not -- refused */
        char cmd[512], hwnd[64] = "";
        FILE *f = fopen("C:\\hwnd.txt", "r");
        if (f) { fgets(hwnd, sizeof(hwnd), f); fclose(f); }
        snprintf(cmd, sizeof(cmd), "\"%s\" spoof2 %s", argv[0], hwnd);
        PROCESS_INFORMATION pi = {};
        if (!spawn_low(cmd, &pi)) { write_file("C:\\spoof.txt", "sent=nolow\n"); return 1; }
        WaitForSingleObject(pi.hProcess, 20000);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "spoof2"))
    {
        char text[64];
        HWND target = (HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16);
        BOOL sent = SendNotifyMessageW(target, SG_WM_CLIENT_SURFACE, GetCurrentProcessId(), 1);
        snprintf(text, sizeof(text), "sent=%d err=%lu\n", sent, sent ? 0 : GetLastError());
        write_file("C:\\spoof.txt", text);
        return 0;
    }
    if (argc < 2 || strcmp(argv[1], "host")) return 4;
    BOOL into_top = argc > 2 && !strcmp(argv[2], "top");

    WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "XprocChild";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    RegisterClassA(&wc);
    HWND top = CreateWindowA("XprocChild", "xproc-child", WS_POPUP | WS_VISIBLE, 100, 100, 300, 300, 0, 0, 0, 0);
    /* the page's child window (as Chromium's), and a small one beside it */
    HWND child = CreateWindowA("XprocChild", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 50, 50, 200, 150, top, 0, 0, 0);
    CreateWindowA("XprocChild", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 10, top, 0, 0, 0);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%p", (void *)top);
    write_file("C:\\hwnd.txt", cmd);
    snprintf(cmd, sizeof(cmd), "\"%s\" render %p", argv[0], (void *)(into_top ? top : child));
    STARTUPINFOA si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    BOOL lowered = spawn_low(cmd, &pi);
    { FILE *f = fopen("C:\\low.txt", "w"); if (f) { fprintf(f, "low=%d\n", lowered); fclose(f); } }
    if (!lowered && !CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("child=FAIL spawn\n"); return 1; }
    pump(8000);
    phase("presented");
    pump(3000);
    RedrawWindow(top, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
    pump(1000);
    phase("repainted");
    pump(3000);
    /* another process prints the top-level: as it is, then covered by another window */
    for (int i = 0; i < 2; i++)
    {
        HWND cover = NULL;
        if (i)
        {
            WNDCLASSA cc = {}; cc.lpfnWndProc = DefWindowProcA; cc.lpszClassName = "XprocCover";
            cc.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
            RegisterClassA(&cc);
            cover = CreateWindowExA(WS_EX_TOPMOST, "XprocCover", "cover", WS_POPUP | WS_VISIBLE, 80, 80, 340, 340, 0, 0, 0, 0);
            pump(1000);
        }
        snprintf(cmd, sizeof(cmd), "\"%s\" print %p C:\\print%d.txt", argv[0], (void *)top, i);
        PROCESS_INFORMATION pp = {};
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pp))
        {
            while (WaitForSingleObject(pp.hProcess, 50) == WAIT_TIMEOUT) pump(50);
            CloseHandle(pp.hProcess); CloseHandle(pp.hThread);
        }
        if (cover) { DestroyWindow(cover); pump(500); }
    }
    phase("printed");
    pump(3000);
    if (!into_top)
    {
        phase("spoofwait");
        for (int i = 0; i < 400 && GetFileAttributesA("C:\\spoof.txt") == INVALID_FILE_ATTRIBUTES; i++) pump(50);
    }
    if (!into_top)
    {
        /* the child moves away: what is left where it was is the top-level's own white */
        SetWindowPos(child, NULL, 50, 210, 200, 80, SWP_NOZORDER | SWP_NOACTIVATE);
        pump(500);
        RedrawWindow(top, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
        pump(1000);
        phase("moved");
        pump(4000);
    }
    TerminateProcess(pi.hProcess, 0);
    printf("child=done\n");
    return 0;
}
