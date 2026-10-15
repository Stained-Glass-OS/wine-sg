/* G-SHARED: Direct3D 11 shared fences between processes (wine-sg 3325). Run without arguments. */
#define COBJMACROS
#include <windows.h>
#include <d3d11_4.h>
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

static ID3D11Device5 *make_device(ID3D11DeviceContext4 **context4)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    ID3D11DeviceContext *context = NULL;
    ID3D11Device *device = NULL;
    ID3D11Device5 *device5 = NULL;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, &context);

    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, &context);
    if (FAILED(hr)) return NULL;
    ID3D11Device_QueryInterface(device, &IID_ID3D11Device5, (void **)&device5);
    ID3D11DeviceContext_QueryInterface(context, &IID_ID3D11DeviceContext4, (void **)context4);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    return device5;
}

static int child_main(char kind, const char *arg)
{
    ID3D11DeviceContext4 *context;
    ID3D11Device5 *device = make_device(&context);
    ID3D11Fence *fence = NULL;
    HANDLE h, ev;
    HRESULT hr;

    if (!device) { printf("FAIL  child: no device\n"); return 2; }
    if (kind == 'n') h = (HANDLE)(ULONG_PTR)strtoul(arg, NULL, 16);
    else
    {
        WCHAR name[128];
        MultiByteToWideChar(CP_ACP, 0, arg, -1, name, 128);
        h = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
        check(h != NULL, "child: the named fence (%lu)", GetLastError());
    }
    hr = ID3D11Device5_OpenSharedFence(device, h, &IID_ID3D11Fence, (void **)&fence);
    check(hr == S_OK && fence, "child: OpenSharedFence (%#lx)", hr);
    if (!fence) return 1;
    ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    hr = ID3D11Fence_SetEventOnCompletion(fence, 3, ev);
    check(hr == S_OK, "child: SetEventOnCompletion (%#lx)", hr);
    check(WaitForSingleObject(ev, 30000) == WAIT_OBJECT_0, "child: the parent signaled 3");
    check(ID3D11Fence_GetCompletedValue(fence) >= 3, "child: completed value %llu", (unsigned long long)ID3D11Fence_GetCompletedValue(fence));
    hr = ID3D11DeviceContext4_Signal(context, fence, 10);
    check(hr == S_OK, "child: Signal 10 (%#lx)", hr);
    ID3D11Fence_Release(fence);
    printf("CHILD %s checks %d failures %d\n", failures ? "FAIL" : "OK", checks, failures);
    return failures != 0;
}

typedef struct { char args[300]; char out[2048]; int code; } child_t;
static DWORD WINAPI child_thread(void *p)
{
    child_t *c = p;
    char cmd[512], exe[MAX_PATH];
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    HANDLE rd, wr;
    DWORD code = 99, n, total = 0;

    GetModuleFileNameA(NULL, exe, sizeof(exe));
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe, c->args);
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { c->code = 98; return 0; }
    CloseHandle(wr);
    while (ReadFile(rd, c->out + total, sizeof(c->out) - 1 - total, &n, NULL) && n) total += n;
    c->out[total] = 0;
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    c->code = code;
    return 0;
}

static void exchange(ID3D11DeviceContext4 *context, ID3D11Fence *fence, const char *args, const char *what)
{
    child_t kid;
    HANDLE thread, ev;
    HRESULT hr;
    UINT64 v;

    snprintf(kid.args, sizeof(kid.args), "%s", args);
    thread = CreateThread(NULL, 0, child_thread, &kid, 0, NULL);
    for (v = 1; v <= 3; v++)
    {
        Sleep(150);
        hr = ID3D11DeviceContext4_Signal(context, fence, v);
        check(hr == S_OK, "%s: Signal %llu (%#lx)", what, (unsigned long long)v, hr);
    }
    ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    hr = ID3D11Fence_SetEventOnCompletion(fence, 10, ev);
    check(hr == S_OK, "%s: SetEventOnCompletion 10 (%#lx)", what, hr);
    check(WaitForSingleObject(ev, 40000) == WAIT_OBJECT_0, "%s: the child signaled 10", what);
    check(ID3D11Fence_GetCompletedValue(fence) == 10, "%s: completed value %llu", what, (unsigned long long)ID3D11Fence_GetCompletedValue(fence));
    CloseHandle(ev);
    WaitForSingleObject(thread, 60000);
    CloseHandle(thread);
    printf("%s", kid.out);
    check(kid.code == 0, "%s: child exit %d", what, kid.code);
}

int main(int argc, char **argv)
{
    ID3D11DeviceContext4 *context;
    ID3D11Device5 *device;
    ID3D11Fence *fence;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE h = NULL;
    HRESULT hr;
    char args[300];

    if (argc >= 3 && argv[1][0] == '-') return child_main(argv[1][1], argv[2]);

    device = make_device(&context);
    if (!device) { printf("FAIL  no device\nRESULT: FAIL\n"); return 1; }

    hr = ID3D11Device5_CreateFence(device, 0, D3D11_FENCE_FLAG_SHARED, &IID_ID3D11Fence, (void **)&fence);
    check(hr == S_OK, "CreateFence (%#lx)", hr);
    hr = ID3D11Fence_CreateSharedHandle(fence, &sa, GENERIC_ALL, NULL, &h);
    check(hr == S_OK && h, "CreateSharedHandle (%#lx)", hr);
    snprintf(args, sizeof(args), "-n %lx", (unsigned long)(ULONG_PTR)h);
    exchange(context, fence, args, "inherited");
    CloseHandle(h);
    ID3D11Fence_Release(fence);

    {
        WCHAR name[96];
        char name_a[96];

        snprintf(name_a, sizeof(name_a), "Local\\GSharedFence%lu", GetCurrentProcessId());
        MultiByteToWideChar(CP_ACP, 0, name_a, -1, name, 96);
        hr = ID3D11Device5_CreateFence(device, 0, D3D11_FENCE_FLAG_SHARED, &IID_ID3D11Fence, (void **)&fence);
        ID3D11DeviceContext4_Signal(context, fence, 0);
        hr = ID3D11Fence_CreateSharedHandle(fence, NULL, GENERIC_ALL, name, &h);
        check(hr == S_OK && h, "CreateSharedHandle with a name (%#lx)", hr);
        snprintf(args, sizeof(args), "-s %s", name_a);
        exchange(context, fence, args, "named");
        hr = ID3D11Fence_CreateSharedHandle(fence, NULL, GENERIC_ALL, name, &h);
        check(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), "the same name again (%#lx)", hr);
        ID3D11Fence_Release(fence);
    }

    printf("checks %d failures %d\n", checks, failures);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
