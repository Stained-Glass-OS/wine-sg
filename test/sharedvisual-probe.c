/* test/sharedvisual-gate.sh's probe (7757): a dcomp visual shared with another process through the
 * desktop device's partner interface, as WebView2 composes itself into Office's task panes.
 *   (no argument)       the host: a window, a shared visual in its tree, a handle to it for the
 *                       child (a copy of this program), then checks
 *   child HANDLE X Y W H  opens the shared visual's target, shows a popup window of its own in it
 *                       (CreateSurfaceFromHwnd), reports where the popup went
 * The partner's methods are called by their slots (27-29 after the desktop device's).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { void **vt; } OBJ;
#define SLOT(o, n) (((OBJ *)(o))->vt[n])
typedef HRESULT (WINAPI *qi_t)(void *, const GUID *, void **);
typedef ULONG (WINAPI *rel_t)(void *);
typedef HRESULT (WINAPI *void_t)(void *);
typedef HRESULT (WINAPI *ptrout_t)(void *, void **);
typedef HRESULT (WINAPI *create2_t)(IUnknown *, const GUID *, void **);

static const GUID IID_Device = {0xc37ea93a,0xe7aa,0x450d,{0xb1,0x6f,0x97,0x46,0xcb,0x04,0x07,0xf3}};
static const GUID IID_DesktopDevice = {0x5f4633fe,0x1e08,0x4cb8,{0x8c,0x75,0xce,0x24,0x33,0x3f,0x56,0x02}};
static const GUID IID_Partner = {0xd14b6158,0xc3fa,0x4bce,{0x9c,0x1f,0xb6,0x1d,0x86,0x65,0xea,0xb0}};
static const GUID IID_Visual = {0x4d93059d,0x097b,0x4651,{0x9a,0x60,0xf0,0xf2,0x51,0x16,0xe2,0xf3}};
static const GUID IID_Target = {0xeacdd04c,0x117e,0x4e17,{0x88,0xf4,0xd1,0xb1,0x2b,0x0e,0x3d,0x89}};

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(20);
    }
}

static void *desktop_device(void)
{
    create2_t create2 = (create2_t)GetProcAddress(LoadLibraryW(L"dcomp.dll"), "DCompositionCreateDevice2");
    void *dev = NULL;
    if (create2) create2(NULL, &IID_DesktopDevice, &dev);
    return dev;
}

static int child(int argc, char **argv)
{
    HANDLE handle = (HANDLE)(ULONG_PTR)strtoull(argv[2], NULL, 16);
    int x = atoi(argv[3]), y = atoi(argv[4]), w = atoi(argv[5]), h = atoi(argv[6]);
    void *dev = desktop_device(), *partner = NULL, *target = NULL, *visual = NULL, *surface = NULL;
    HWND popup, container;
    RECT rect;
    HRESULT hr;

    if (!dev) { printf("ChildDevice=0\n"); return 0; }
    hr = ((qi_t)SLOT(dev, 0))(dev, &IID_Partner, &partner);
    printf("ChildPartner=0x%08lx\n", hr);
    if (FAILED(hr)) return 0;
    hr = ((HRESULT (WINAPI *)(void *, HANDLE, const GUID *, void **))SLOT(partner, 29))(partner, handle, &IID_Target, &target);
    printf("OpenTarget=0x%08lx\n", hr);
    if (FAILED(hr)) return 0;
    /* where the visual is on the screen: the popup is kept there by its process, as WebView2 does */
    popup = CreateWindowExW(0, L"Static", L"shared", WS_POPUP, x, y, w, h, NULL, NULL, NULL, NULL);
    hr = ((HRESULT (WINAPI *)(void *, HWND, void **))SLOT(dev, 26))(dev, popup, &surface);
    printf("SurfaceFromHwnd=0x%08lx\n", hr);
    ((ptrout_t)SLOT(dev, 6))(dev, &visual);
    ((HRESULT (WINAPI *)(void *, void *))SLOT(visual, 15))(visual, surface);
    ((HRESULT (WINAPI *)(void *, void *))SLOT(target, 3))(target, visual);
    ((void_t)SLOT(dev, 3))(dev);
    pump(2000);
    container = GetAncestor(popup, GA_PARENT);
    GetWindowRect(popup, &rect);
    printf("PopupChild=%d\n", !!(GetWindowLongW(popup, GWL_STYLE) & WS_CHILD));
    printf("PopupVisible=%d\n", IsWindowVisible(popup));
    printf("PopupRect=%ld,%ld %ldx%ld\n", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    printf("ContainerParent=%p\n", GetAncestor(container, GA_PARENT));
    printf("Container=%p\n", container);
    fflush(stdout);
    /* the host moves its window: the container follows (the popup's process moves the popup) */
    pump(1500);
    GetWindowRect(container, &rect);
    printf("ContainerOrigin=%ld,%ld\n", rect.left, rect.top);
    ((rel_t)SLOT(target, 2))(target);
    pump(1000);
    printf("ContainerGone=%d\n", !IsWindow(container));
    return 0;
}

int main(int argc, char **argv)
{
    void *dev, *partner = NULL, *v1 = NULL, *x = NULL, *shared = NULL, *root = NULL, *target = NULL;
    create2_t create1;
    HANDLE handle = NULL, inherit;
    WCHAR self[MAX_PATH], cmd[MAX_PATH + 128];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HWND window, parent;
    HRESULT hr;
    RECT rect;
    char line[256];
    HANDLE rd, wr;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    DWORD n;

    if (argc > 6 && !strcmp(argv[1], "child")) return child(argc, argv);

    create1 = (create2_t)GetProcAddress(LoadLibraryW(L"dcomp.dll"), "DCompositionCreateDevice");
    if (create1 && SUCCEEDED(create1(NULL, &IID_Device, &v1)))
    {
        hr = ((qi_t)SLOT(v1, 0))(v1, &IID_Partner, &x);
        printf("V1Partner=0x%08lx\n", hr);
    }
    if (!(dev = desktop_device())) { printf("Device=0\n"); return 0; }
    hr = ((qi_t)SLOT(dev, 0))(dev, &IID_Partner, &partner);
    printf("Partner=0x%08lx\n", hr);
    printf("PartnerSame=%d\n", partner == dev);
    if (FAILED(hr)) return 0;
    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(partner, 27))(partner, &IID_Visual, &shared);
    printf("CreateShared=0x%08lx\n", hr);
    hr = ((HRESULT (WINAPI *)(void *, void *, HANDLE *))SLOT(partner, 28))(partner, dev, &handle);
    printf("HandleOfNonVisual=0x%08lx\n", hr);
    hr = ((HRESULT (WINAPI *)(void *, void *, HANDLE *))SLOT(partner, 28))(partner, shared, &handle);
    printf("Handle=0x%08lx\n", hr);
    if (FAILED(hr)) return 0;

    window = CreateWindowExW(0, L"Static", L"host", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 500, 400, NULL, NULL, NULL, NULL);
    ((HRESULT (WINAPI *)(void *, HWND, BOOL, void **))SLOT(dev, 24))(dev, window, FALSE, &target);
    ((ptrout_t)SLOT(dev, 6))(dev, &root);
    ((HRESULT (WINAPI *)(void *, void *, BOOL, void *))SLOT(root, 16))(root, shared, FALSE, NULL);
    ((HRESULT (WINAPI *)(void *, void *))SLOT(target, 3))(target, root);
    ((void_t)SLOT(dev, 3))(dev);
    GetWindowRect(window, &rect);

    DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &inherit, 0, TRUE, DUPLICATE_SAME_ACCESS);
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    GetModuleFileNameW(NULL, self, MAX_PATH);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%s\" child %p %ld %ld %d %d", self, inherit, rect.left + 10, rect.top + 40, 300, 200);
    if (!CreateProcessW(self, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { printf("Child=%lu\n", GetLastError()); return 0; }
    CloseHandle(wr);
    /* let the child show its popup, then move the host window: the shared visual's new place
     * reaches the child and its container follows */
    pump(2600);
    SetWindowPos(window, 0, 150, 120, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ((void_t)SLOT(dev, 3))(dev);
    while (WaitForSingleObject(pi.hProcess, 20) == WAIT_TIMEOUT) pump(20);
    for (;;)
    {
        if (!ReadFile(rd, line, sizeof(line) - 1, &n, NULL) || !n) break;
        line[n] = 0;
        fputs(line, stdout);
    }
    printf("Host=%p\n", window);
    GetWindowRect(window, &rect);
    printf("HostRect=%ld,%ld\n", rect.left, rect.top);
    parent = window;
    (void)parent;
    return 0;
}
