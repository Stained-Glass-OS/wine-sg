/* G-SHARED: Direct3D 11 textures shared between processes (wine-sg 3324).
 * Run without arguments; it starts itself as the other side. */
#define COBJMACROS
#include <windows.h>
#include <d3d11_1.h>
#include <d3d10.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    checks++;
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static ID3D11Device *make_device(ID3D11DeviceContext **context)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    ID3D11Device *device = NULL;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, context);

    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, context);
    return SUCCEEDED(hr) ? device : NULL;
}

#define SIZE 8

static ID3D11Texture2D *make_texture(ID3D11Device *device, UINT misc)
{
    D3D11_TEXTURE2D_DESC desc = {0};
    ID3D11Texture2D *tex = NULL;

    desc.Width = SIZE; desc.Height = SIZE; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = misc;
    ID3D11Device_CreateTexture2D(device, &desc, NULL, &tex);
    return tex;
}

static UINT read_pixel(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *tex, UINT x, UINT y)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D *staging = NULL;
    D3D11_MAPPED_SUBRESOURCE mapped;
    UINT pixel = 0xdeadbeef;

    ID3D11Texture2D_GetDesc(tex, &desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    ID3D11Device_CreateTexture2D(device, &desc, NULL, &staging);
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)tex);
    if (SUCCEEDED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped)))
    {
        pixel = *(UINT *)((BYTE *)mapped.pData + y * mapped.RowPitch + x * 4);
        ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    }
    ID3D11Texture2D_Release(staging);
    return pixel;
}

static void fill(ID3D11DeviceContext *context, ID3D11Texture2D *tex, UINT colour)
{
    UINT pixels[SIZE * SIZE];
    int i;

    for (i = 0; i < SIZE * SIZE; ++i) pixels[i] = colour;
    ID3D11DeviceContext_UpdateSubresource(context, (ID3D11Resource *)tex, 0, NULL, pixels, SIZE * 4, 0);
}

static HANDLE legacy_handle(ID3D11Texture2D *tex)
{
    IDXGIResource *res = NULL;
    HANDLE h = NULL;

    ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res);
    IDXGIResource_GetSharedHandle(res, &h);
    IDXGIResource_Release(res);
    return h;
}

static HRESULT nt_handle(ID3D11Texture2D *tex, SECURITY_ATTRIBUTES *sa, const WCHAR *name, HANDLE *h)
{
    IDXGIResource1 *res = NULL;
    HRESULT hr;

    *h = NULL;
    hr = ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource1, (void **)&res);
    if (FAILED(hr)) return hr;
    hr = IDXGIResource1_CreateSharedHandle(res, sa, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, name, h);
    IDXGIResource1_Release(res);
    return hr;
}

#define COLOUR1 0xff112233
#define COLOUR2 0xff445566

/* the other process: kind k: keyed legacy, n: keyed NT handle, s: keyed by name, p: plain shared, d: takes the mutex and exits */
static int child_main(char kind, const char *arg, const char *arg2)
{
    ID3D11DeviceContext *context;
    ID3D11Device *device = make_device(&context);
    ID3D11Device1 *device1 = NULL;
    ID3D11Texture2D *tex = NULL;
    IDXGIKeyedMutex *km = NULL;
    HRESULT hr;
    UINT p;
    HANDLE h;

    if (!device) { printf("FAIL  child: no device\n"); return 2; }
    ID3D11Device_QueryInterface(device, &IID_ID3D11Device1, (void **)&device1);
    h = (HANDLE)(ULONG_PTR)strtoul(arg, NULL, 16);
    if (kind == 'k' || kind == 'p' || kind == 'd')
    {
        ID3D10Device *device10 = NULL;
        ID3D10Texture2D *tex10 = NULL;

        hr = ID3D11Device_OpenSharedResource(device, h, &IID_ID3D11Texture2D, (void **)&tex);
        check(hr == S_OK && tex, "child: OpenSharedResource (%#lx)", hr);
        /* the same resource through the Direct3D 10 interface of the device */
        if (SUCCEEDED(ID3D11Device_QueryInterface(device, &IID_ID3D10Device, (void **)&device10)))
        {
            hr = ID3D10Device_OpenSharedResource(device10, h, &IID_ID3D10Texture2D, (void **)&tex10);
            check(hr == S_OK && tex10, "child: the D3D10 OpenSharedResource (%#lx)", hr);
            if (tex10) ID3D10Texture2D_Release(tex10);
            ID3D10Device_Release(device10);
        }
    }
    else if (kind == 'n')
    {
        hr = ID3D11Device1_OpenSharedResource1(device1, h, &IID_ID3D11Texture2D, (void **)&tex);
        check(hr == S_OK && tex, "child: OpenSharedResource1 (%#lx)", hr);
    }
    else if (kind == 's')
    {
        WCHAR name[128];
        MultiByteToWideChar(CP_ACP, 0, arg, -1, name, 128);
        hr = ID3D11Device1_OpenSharedResourceByName(device1, name, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                &IID_ID3D11Texture2D, (void **)&tex);
        check(hr == S_OK && tex, "child: OpenSharedResourceByName (%#lx)", hr);
    }
    if (!tex) return 1;

    if (kind == 'p')
    {
        char name[64];
        HANDLE opened, pushed;

        snprintf(name, sizeof(name), "Local\\GSD3D_opened_%s", arg2);
        opened = OpenEventA(EVENT_ALL_ACCESS, FALSE, name);
        snprintf(name, sizeof(name), "Local\\GSD3D_pushed_%s", arg2);
        pushed = OpenEventA(EVENT_ALL_ACCESS, FALSE, name);
        check(opened && pushed, "child: the events");
        SetEvent(opened);
        check(WaitForSingleObject(pushed, 30000) == WAIT_OBJECT_0, "child: the creator flushed");
        ID3D11DeviceContext_Flush(context);
        p = read_pixel(device, context, tex, 3, 4);
        check(p == COLOUR1, "child: the creator's pixels without a mutex (%#x)", p);
    }
    else
    {
        hr = ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km);
        check(hr == S_OK && km, "child: keyed mutex (%#lx)", hr);
        if (km)
        {
            if (kind == 'd')
            {
                hr = IDXGIKeyedMutex_AcquireSync(km, 0, 20000);
                check(hr == S_OK, "child: takes the mutex (%#lx)", hr);
                printf("CHILD holding\n");
                fflush(stdout);
                ExitProcess(failures ? 1 : 0);
            }
            hr = IDXGIKeyedMutex_AcquireSync(km, 1, 20000);
            check(hr == S_OK, "child: key 1 (%#lx)", hr);
            p = read_pixel(device, context, tex, 5, 2);
            check(p == COLOUR1, "child: the pixels that came with key 1 (%#x)", p);
            fill(context, tex, COLOUR2);
            hr = IDXGIKeyedMutex_ReleaseSync(km, 2);
            check(hr == S_OK, "child: release with key 2 (%#lx)", hr);
            IDXGIKeyedMutex_Release(km);
        }
    }
    ID3D11Texture2D_Release(tex);
    printf("CHILD %s checks %d failures %d\n", failures ? "FAIL" : "OK", checks, failures);
    return failures ? 1 : 0;
}

static int run_child(const char *args, char *out, size_t out_size, BOOL inherit)
{
    char cmd[512], exe[MAX_PATH];
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    HANDLE rd, wr;
    DWORD code = 99, n, total = 0;

    GetModuleFileNameA(NULL, exe, sizeof(exe));
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe, args);
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return 98;
    CloseHandle(wr);
    while (ReadFile(rd, out + total, out_size - 1 - total, &n, NULL) && n) total += n;
    out[total] = 0;
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    (void)inherit;
    return code;
}

typedef struct { char args[300]; char out[4096]; int code; } child_t;
static DWORD WINAPI child_thread(void *p)
{
    child_t *c = p;
    c->code = run_child(c->args, c->out, sizeof(c->out), TRUE);
    return 0;
}

/* a keyed texture the child takes over with key 1, and gives back with key 2 */
static void exchange(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *tex, const char *args, const char *what)
{
    IDXGIKeyedMutex *km = NULL;
    child_t kid;
    HANDLE thread;
    HRESULT hr;
    UINT p;

    ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km);
    hr = IDXGIKeyedMutex_AcquireSync(km, 0, 0);
    check(hr == S_OK, "%s: key 0 (%#lx)", what, hr);
    fill(context, tex, COLOUR1);
    hr = IDXGIKeyedMutex_ReleaseSync(km, 1);
    check(hr == S_OK, "%s: release with key 1 (%#lx)", what, hr);

    snprintf(kid.args, sizeof(kid.args), "%s", args);
    thread = CreateThread(NULL, 0, child_thread, &kid, 0, NULL);
    hr = IDXGIKeyedMutex_AcquireSync(km, 2, 40000);
    check(hr == S_OK, "%s: key 2 comes back (%#lx)", what, hr);
    p = read_pixel(device, context, tex, 1, 6);
    check(p == COLOUR2, "%s: what the other process drew (%#x)", what, p);
    WaitForSingleObject(thread, 60000);
    CloseHandle(thread);
    printf("%s", kid.out);
    check(kid.code == 0, "%s: child exit %d", what, kid.code);
    hr = IDXGIKeyedMutex_ReleaseSync(km, 0);
    check(hr == S_OK, "%s: release with key 0, for the next one (%#lx)", what, hr);
    IDXGIKeyedMutex_Release(km);
}

static void test_luid(ID3D11Device *device, HANDLE handle, const char *what)
{
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory2 *factory = NULL;
    DXGI_ADAPTER_DESC desc;
    LUID luid = {0xdead, 0xbeef};
    HRESULT hr;

    ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    IDXGIAdapter_GetDesc(adapter, &desc);
    IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, (void **)&factory);
    if (factory)
    {
        hr = IDXGIFactory2_GetSharedResourceAdapterLuid(factory, handle, &luid);
        check(hr == S_OK && luid.LowPart == desc.AdapterLuid.LowPart && luid.HighPart == desc.AdapterLuid.HighPart,
                "%s: GetSharedResourceAdapterLuid (%#lx, %lx:%lx)", what, hr, luid.HighPart, luid.LowPart);
        hr = IDXGIFactory2_GetSharedResourceAdapterLuid(factory, (HANDLE)(ULONG_PTR)0x40998870, &luid);
        check(hr == DXGI_ERROR_INVALID_CALL, "%s: a handle of nothing (%#lx)", what, hr);
        IDXGIFactory2_Release(factory);
    }
    IDXGIAdapter_Release(adapter);
    IDXGIDevice_Release(dxgi_device);
}

int main(int argc, char **argv)
{
    ID3D11DeviceContext *context;
    ID3D11Device *device;
    ID3D11Device1 *device1 = NULL;
    ID3D11Texture2D *tex, *tex2;
    char args[300], pid[32];
    HRESULT hr;
    HANDLE h, h2;

    if (argc >= 3 && argv[1][0] == '-') return child_main(argv[1][1], argv[2], argc > 3 ? argv[3] : "");

    device = make_device(&context);
    if (!device) { printf("FAIL  no device\nRESULT: FAIL\n"); return 1; }
    ID3D11Device_QueryInterface(device, &IID_ID3D11Device1, (void **)&device1);
    snprintf(pid, sizeof(pid), "%lu", GetCurrentProcessId());

    /* a legacy handle with a keyed mutex */
    tex = make_texture(device, D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);
    h = legacy_handle(tex);
    check(h != NULL && ((ULONG_PTR)h & 0xc0000000) == 0x40000000, "a legacy handle (%p)", h);
    test_luid(device, h, "legacy");
    snprintf(args, sizeof(args), "-k %lx", (unsigned long)(ULONG_PTR)h);
    exchange(device, context, tex, args, "legacy");

    /* the process that holds the mutex exits */
    {
        IDXGIKeyedMutex *km = NULL;
        child_t kid;

        snprintf(args, sizeof(args), "-d %lx", (unsigned long)(ULONG_PTR)h);
        snprintf(kid.args, sizeof(kid.args), "%s", args);
        ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km);
        child_thread(&kid);
        printf("%s", kid.out);
        check(kid.code == 0, "dying child exit %d", kid.code);
        hr = IDXGIKeyedMutex_AcquireSync(km, 3, 20000);
        check(hr == WAIT_ABANDONED, "a mutex whose holder exited is abandoned (%#lx)", hr);
        IDXGIKeyedMutex_Release(km);
    }
    ID3D11Texture2D_Release(tex);

    /* an NT handle (inherited) with a keyed mutex */
    {
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};

        tex = make_texture(device, D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
        check(tex != NULL, "an NT shared texture with a keyed mutex");
        h = legacy_handle(tex);
        check(h == NULL, "no legacy handle for it (%p)", h);
        hr = nt_handle(tex, &sa, NULL, &h);
        check(hr == S_OK && h, "CreateSharedHandle (%#lx)", hr);
        test_luid(device, h, "NT");
        snprintf(args, sizeof(args), "-n %lx", (unsigned long)(ULONG_PTR)h);
        exchange(device, context, tex, args, "NT handle");
        if (h) CloseHandle(h);

        /* and by name */
        {
            WCHAR name[96];
            char name_a[96];

            snprintf(name_a, sizeof(name_a), "Local\\GSharedD3D%s", pid);
            MultiByteToWideChar(CP_ACP, 0, name_a, -1, name, 96);
            hr = nt_handle(tex, NULL, name, &h);
            check(hr == S_OK && h, "CreateSharedHandle with a name (%#lx)", hr);
            snprintf(args, sizeof(args), "-s %s", name_a);
            exchange(device, context, tex, args, "named");
            if (h) CloseHandle(h);
        }
        ID3D11Texture2D_Release(tex);
    }

    /* a texture without a keyed mutex: what the creator drew, once it is flushed */
    {
        char name[64];
        HANDLE opened, pushed;
        child_t kid;
        HANDLE thread;

        tex = make_texture(device, D3D11_RESOURCE_MISC_SHARED);
        h = legacy_handle(tex);
        snprintf(name, sizeof(name), "Local\\GSD3D_opened_%s", pid);
        opened = CreateEventA(NULL, FALSE, FALSE, name);
        snprintf(name, sizeof(name), "Local\\GSD3D_pushed_%s", pid);
        pushed = CreateEventA(NULL, FALSE, FALSE, name);
        fill(context, tex, COLOUR1);
        ID3D11DeviceContext_Flush(context);
        snprintf(kid.args, sizeof(kid.args), "-p %lx %s", (unsigned long)(ULONG_PTR)h, pid);
        thread = CreateThread(NULL, 0, child_thread, &kid, 0, NULL);
        check(WaitForSingleObject(opened, 40000) == WAIT_OBJECT_0, "the other process opened the texture");
        ID3D11DeviceContext_Flush(context);
        SetEvent(pushed);
        WaitForSingleObject(thread, 60000);
        CloseHandle(thread);
        printf("%s", kid.out);
        check(kid.code == 0, "plain child exit %d", kid.code);
        CloseHandle(opened); CloseHandle(pushed);
        ID3D11Texture2D_Release(tex);
    }

    /* handle kinds */
    {
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, FALSE};
        WCHAR none[] = L"Local\\GSharedD3DNothing";

        tex = make_texture(device, D3D11_RESOURCE_MISC_SHARED);
        tex2 = make_texture(device, D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
        h = legacy_handle(tex);
        hr = nt_handle(tex2, &sa, NULL, &h2);
        {
            ID3D11Texture2D *t = (void *)0xdeadbeef;
            hr = ID3D11Device_OpenSharedResource(device, h2, &IID_ID3D11Texture2D, (void **)&t);
            check(hr == E_INVALIDARG && !t, "OpenSharedResource of an NT handle (%#lx)", hr);
            t = (void *)0xdeadbeef;
            hr = ID3D11Device1_OpenSharedResource1(device1, h, &IID_ID3D11Texture2D, (void **)&t);
            check(hr == E_INVALIDARG && !t, "OpenSharedResource1 of a legacy handle (%#lx)", hr);
            t = (void *)0xdeadbeef;
            hr = ID3D11Device1_OpenSharedResourceByName(device1, none, DXGI_SHARED_RESOURCE_READ, &IID_ID3D11Texture2D, (void **)&t);
            check(FAILED(hr) && !t, "OpenSharedResourceByName of nothing (%#lx)", hr);
            t = NULL;
            hr = ID3D11Device1_OpenSharedResource1(device1, h2, &IID_ID3D11Texture2D, (void **)&t);
            check(hr == S_OK && t, "OpenSharedResource1 in the creating process (%#lx)", hr);
            if (t) ID3D11Texture2D_Release(t);
        }
        hr = nt_handle(tex, &sa, NULL, &h2);
        check(FAILED(hr), "CreateSharedHandle of a texture made for legacy sharing (%#lx)", hr);
        {
            ID3D11Texture2D *plain = make_texture(device, 0);

            hr = nt_handle(plain, &sa, NULL, &h2);
            check(hr == E_INVALIDARG, "CreateSharedHandle of a texture that is not shared (%#lx)", hr);
            ID3D11Texture2D_Release(plain);
        }
        ID3D11Texture2D_Release(tex);
        ID3D11Texture2D_Release(tex2);
    }

    /* in one process: a second device sees what the first drew, no mutex needed; release rules */
    {
        ID3D11DeviceContext *context2;
        ID3D11Device *device2 = make_device(&context2);
        ID3D11Texture2D *opened = NULL;
        IDXGIKeyedMutex *km = NULL;
        UINT p;

        tex = make_texture(device, D3D11_RESOURCE_MISC_SHARED);
        fill(context, tex, COLOUR1);
        ID3D11DeviceContext_Flush(context);
        hr = ID3D11Device_OpenSharedResource(device2, legacy_handle(tex), &IID_ID3D11Texture2D, (void **)&opened);
        check(hr == S_OK && opened, "second device opens it (%#lx)", hr);
        if (opened)
        {
            p = read_pixel(device2, context2, opened, 2, 2);
            check(p == COLOUR1, "and sees what the first drew (%#x)", p);
            ID3D11Texture2D_Release(opened);
        }
        ID3D11Texture2D_Release(tex);

        tex = make_texture(device, D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);
        ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km);
        hr = IDXGIKeyedMutex_ReleaseSync(km, 5);
        check(hr == DXGI_ERROR_INVALID_CALL, "releasing what was not acquired (%#lx)", hr);
        hr = IDXGIKeyedMutex_AcquireSync(km, 7, 0);
        check(hr == WAIT_TIMEOUT, "key 7 is not the first key (%#lx)", hr);
        IDXGIKeyedMutex_Release(km);
        ID3D11Texture2D_Release(tex);
        ID3D11DeviceContext_Release(context2);
        ID3D11Device_Release(device2);
    }

    printf("checks %d failures %d\n", checks, failures);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
