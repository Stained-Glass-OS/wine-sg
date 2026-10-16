/* G-SHARED: D3DKMT synchronization objects between processes (wine-sg 3323).
 * Run without arguments; it starts itself as the other side. */
#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#include <ddk/d3dkmthk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ST_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#define ST_NOT_SUPPORTED     ((NTSTATUS)0xC00000BB)
#define ST_SEMAPHORE_LIMIT   ((NTSTATUS)0xC0000047)

#define WINE_KMT_ESCAPE_MAGIC 0x53474b4d
struct wine_kmt_alloc_data { UINT64 size; };

static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC *);
static NTSTATUS (WINAPI *pD3DKMTCreateDevice)(D3DKMT_CREATEDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTCreateAllocation)(D3DKMT_CREATEALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTShareObjects)(UINT, const D3DKMT_HANDLE *, OBJECT_ATTRIBUTES *, UINT, HANDLE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResourceFromNtHandle)(D3DKMT_OPENRESOURCEFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTCreateSynchronizationObject)(D3DKMT_CREATESYNCHRONIZATIONOBJECT *);
static NTSTATUS (WINAPI *pD3DKMTCreateSynchronizationObject2)(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *);
static NTSTATUS (WINAPI *pD3DKMTDestroySynchronizationObject)(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *);
static NTSTATUS (WINAPI *pD3DKMTOpenSynchronizationObject)(D3DKMT_OPENSYNCHRONIZATIONOBJECT *);
static NTSTATUS (WINAPI *pD3DKMTOpenSyncObjectFromNtHandle)(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTOpenSyncObjectFromNtHandle2)(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *);
static NTSTATUS (WINAPI *pD3DKMTOpenSyncObjectNtHandleFromName)(D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *);
static NTSTATUS (WINAPI *pD3DKMTSignalSynchronizationObjectFromCpu)(const D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *);
static NTSTATUS (WINAPI *pD3DKMTWaitForSynchronizationObjectFromCpu)(const D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *);
static void (WINAPI *pRtlInitUnicodeString)(UNICODE_STRING *, const WCHAR *);

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_ST(status, expected, what) CHECK((NTSTATUS)(status) == (NTSTATUS)(expected), "%s: status %#lx, expected %#lx", what, (unsigned long)(status), (unsigned long)(expected))

static D3DKMT_HANDLE device;

static void load(void)
{
    HMODULE gdi = LoadLibraryA("gdi32.dll");
#define L(n) p##n = (void *)GetProcAddress(gdi, #n); if (!p##n) { printf("FAIL  no %s\n", #n); exit(1); }
    L(D3DKMTOpenAdapterFromHdc) L(D3DKMTCreateDevice) L(D3DKMTCreateAllocation) L(D3DKMTShareObjects)
    L(D3DKMTOpenResourceFromNtHandle) L(D3DKMTCreateSynchronizationObject) L(D3DKMTCreateSynchronizationObject2)
    L(D3DKMTDestroySynchronizationObject) L(D3DKMTOpenSynchronizationObject) L(D3DKMTOpenSyncObjectFromNtHandle)
    L(D3DKMTOpenSyncObjectFromNtHandle2) L(D3DKMTOpenSyncObjectNtHandleFromName)
    L(D3DKMTSignalSynchronizationObjectFromCpu) L(D3DKMTWaitForSynchronizationObjectFromCpu)
    pRtlInitUnicodeString = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlInitUnicodeString");
}

static void setup_device(void)
{
    D3DKMT_OPENADAPTERFROMHDC adapter = {0};
    D3DKMT_CREATEDEVICE dev = {0};

    adapter.hDc = GetDC(NULL);
    CHECK_ST(pD3DKMTOpenAdapterFromHdc(&adapter), 0, "OpenAdapterFromHdc");
    dev.hAdapter = adapter.hAdapter;
    CHECK_ST(pD3DKMTCreateDevice(&dev), 0, "CreateDevice");
    device = dev.hDevice;
}

static NTSTATUS signal_obj(D3DKMT_HANDLE h, UINT64 value, UINT rewind)
{
    D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU s = {0};
    s.hDevice = device; s.ObjectCount = 1; s.ObjectHandleArray = &h; s.FenceValueArray = &value;
    s.Flags.AllowFenceRewind = rewind;
    return pD3DKMTSignalSynchronizationObjectFromCpu(&s);
}

static NTSTATUS wait_objs(UINT n, const D3DKMT_HANDLE *h, const UINT64 *values, BOOL any, HANDLE event)
{
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU w = {0};
    w.hDevice = device; w.ObjectCount = n; w.ObjectHandleArray = h; w.FenceValueArray = values;
    w.hAsyncEvent = event; w.Flags.WaitAny = any;
    return pD3DKMTWaitForSynchronizationObjectFromCpu(&w);
}

static NTSTATUS wait_one(D3DKMT_HANDLE h, UINT64 value)
{
    return wait_objs(1, &h, &value, FALSE, NULL);
}

static D3DKMT_HANDLE create2(UINT type, UINT flags, UINT a, UINT b, UINT64 fence, HANDLE event, NTSTATUS *status, UINT *shared, void **cpu)
{
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 c = {0};

    c.hDevice = device;
    c.Info.Type = type;
    c.Info.Flags.Value = flags;
    switch (type)
    {
    case D3DDDI_SYNCHRONIZATION_MUTEX: c.Info.SynchronizationMutex.InitialState = a; break;
    case D3DDDI_SEMAPHORE: c.Info.Semaphore.MaxCount = a; c.Info.Semaphore.InitialCount = b; break;
    case D3DDDI_FENCE: c.Info.Fence.FenceValue = fence; break;
    case D3DDDI_CPU_NOTIFICATION: c.Info.CPUNotification.Event = event; break;
    case D3DDDI_MONITORED_FENCE: c.Info.MonitoredFence.InitialFenceValue = fence; break;
    }
    *status = pD3DKMTCreateSynchronizationObject2(&c);
    if (shared) *shared = c.Info.SharedHandle;
    if (cpu) *cpu = c.Info.MonitoredFence.FenceValueCPUVirtualAddress;
    return *status ? 0 : c.hSyncObject;
}

static DWORD WINAPI signal_later(void *p)
{
    D3DKMT_HANDLE h = (D3DKMT_HANDLE)(ULONG_PTR)p;
    Sleep(150);
    signal_obj(h, 1, 0);
    return 0;
}

static void test_single_process(void)
{
    NTSTATUS st;
    D3DKMT_HANDLE mutex, sem, fence, mon, cpu, h[3];
    void *cpu_addr = NULL;
    UINT64 v[3];
    HANDLE ev;
    DWORD t0, dt;

    /* creation */
    create2(D3DDDI_SEMAPHORE, 0, 2, 3, 0, NULL, &st, NULL, NULL);
    CHECK_ST(st, ST_INVALID_PARAMETER, "semaphore with more than its maximum");
    create2(D3DDDI_SEMAPHORE, 0, 0, 0, 0, NULL, &st, NULL, NULL);
    CHECK_ST(st, ST_INVALID_PARAMETER, "semaphore without a maximum");
    create2(D3DDDI_CPU_NOTIFICATION, 0, 0, 0, 0, NULL, &st, NULL, NULL);
    CHECK_ST(st, ST_INVALID_PARAMETER, "CPU notification without an event");
    create2(D3DDDI_PERIODIC_MONITORED_FENCE, 0, 0, 0, 0, NULL, &st, NULL, NULL);
    CHECK_ST(st, ST_NOT_SUPPORTED, "periodic monitored fence");
    create2(99, 0, 0, 0, 0, NULL, &st, NULL, NULL);
    CHECK_ST(st, ST_INVALID_PARAMETER, "unknown type");
    {
        D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS f = {{0}};
        f.NtSecuritySharing = 1;
        create2(D3DDDI_FENCE, f.Value, 0, 0, 0, NULL, &st, NULL, NULL);
        CHECK_ST(st, ST_INVALID_PARAMETER, "NtSecuritySharing without Shared");
    }
    {
        D3DKMT_CREATESYNCHRONIZATIONOBJECT c = {0};
        c.hDevice = device; c.Info.Type = D3DDDI_FENCE;
        CHECK_ST(pD3DKMTCreateSynchronizationObject(&c), ST_INVALID_PARAMETER, "a fence in the first version");
        c.Info.Type = D3DDDI_SEMAPHORE; c.Info.Semaphore.MaxCount = 1;
        CHECK_ST(pD3DKMTCreateSynchronizationObject(&c), 0, "a semaphore in the first version");
        CHECK(c.hSyncObject != 0, "its handle");
        c.hDevice = 0x777;
        CHECK_ST(pD3DKMTCreateSynchronizationObject(&c), ST_INVALID_PARAMETER, "unknown device");
    }

    mutex = create2(D3DDDI_SYNCHRONIZATION_MUTEX, 0, 1, 0, 0, NULL, &st, NULL, NULL);
    CHECK_ST(st, 0, "mutex signaled at the start");
    sem = create2(D3DDDI_SEMAPHORE, 0, 2, 1, 0, NULL, &st, NULL, NULL);
    CHECK_ST(st, 0, "semaphore");
    fence = create2(D3DDDI_FENCE, 0, 0, 0, 4, NULL, &st, NULL, NULL);
    CHECK_ST(st, 0, "fence at 4");
    mon = create2(D3DDDI_MONITORED_FENCE, 0, 0, 0, 10, NULL, &st, NULL, &cpu_addr);
    CHECK_ST(st, 0, "monitored fence at 10");
    CHECK(cpu_addr && *(volatile UINT64 *)cpu_addr == 10, "CPU address of the monitored fence: %p", cpu_addr);

    /* mutex: taken by a wait, signaled again */
    CHECK_ST(wait_one(mutex, 0), 0, "wait on the signaled mutex takes it");
    t0 = GetTickCount();
    CloseHandle(CreateThread(NULL, 0, signal_later, (void *)(ULONG_PTR)mutex, 0, NULL));
    CHECK_ST(wait_one(mutex, 0), 0, "wait ends when another thread signals");
    dt = GetTickCount() - t0;
    CHECK(dt >= 100 && dt < 3000, "the wait took %lu ms", dt);

    /* semaphore: counts */
    CHECK_ST(signal_obj(sem, 0, 0), 0, "signal the semaphore (2)");
    CHECK_ST(signal_obj(sem, 0, 0), ST_SEMAPHORE_LIMIT, "signal the semaphore past its maximum");
    CHECK_ST(wait_one(sem, 0), 0, "wait 1"); CHECK_ST(wait_one(sem, 0), 0, "wait 2");

    /* fence: values */
    CHECK_ST(wait_one(fence, 3), 0, "fence already past 3");
    CHECK_ST(wait_one(fence, 4), 0, "fence at 4");
    CHECK_ST(signal_obj(fence, 4, 0), ST_INVALID_PARAMETER, "signal the same value");
    CHECK_ST(signal_obj(fence, 2, 0), ST_INVALID_PARAMETER, "signal a lower value");
    CHECK_ST(signal_obj(fence, 2, 1), 0, "signal a lower value with AllowFenceRewind");
    CHECK_ST(signal_obj(fence, 8, 0), 0, "signal 8");
    CHECK_ST(signal_obj(mon, 11, 0), 0, "signal the monitored fence");
    CHECK(*(volatile UINT64 *)cpu_addr == 11, "the CPU address follows: %llu", (unsigned long long)*(volatile UINT64 *)cpu_addr);

    /* several at once */
    h[0] = fence; h[1] = mon; v[0] = 8; v[1] = 12;
    ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK_ST(wait_objs(2, h, v, TRUE, NULL), 0, "wait any: the fence is there");
    CHECK_ST(wait_objs(2, h, v, FALSE, ev), ST_NOT_SUPPORTED, "wait all with an event, one is not there");
    v[1] = 11;
    CHECK_ST(wait_objs(2, h, v, FALSE, ev), 0, "wait all: both are there");
    CHECK(WaitForSingleObject(ev, 0) == 0, "the event of an ended wait is set");
    h[2] = 0x1234;
    CHECK_ST(wait_objs(3, h, v, FALSE, NULL), ST_INVALID_PARAMETER, "an unknown object");
    CHECK_ST(wait_objs(0, h, v, FALSE, NULL), ST_INVALID_PARAMETER, "no object");

    /* CPU notification */
    cpu = create2(D3DDDI_CPU_NOTIFICATION, 0, 0, 0, 0, ev, &st, NULL, NULL);
    CHECK_ST(st, 0, "CPU notification");
    ResetEvent(ev);
    CHECK_ST(signal_obj(cpu, 0, 0), 0, "signal the CPU notification");
    CHECK(WaitForSingleObject(ev, 0) == 0, "the event is set");
    CHECK_ST(wait_one(cpu, 0), ST_INVALID_PARAMETER, "waiting on a CPU notification");
    CloseHandle(ev);

    {
        D3DKMT_DESTROYSYNCHRONIZATIONOBJECT d = {0};
        d.hSyncObject = fence;
        CHECK_ST(pD3DKMTDestroySynchronizationObject(&d), 0, "destroy");
        CHECK_ST(pD3DKMTDestroySynchronizationObject(&d), ST_INVALID_PARAMETER, "destroy twice");
        CHECK_ST(signal_obj(fence, 100, 0), ST_INVALID_PARAMETER, "signal a destroyed object");
    }
}

static int child_main(char kind, const char *arg)
{
    D3DKMT_HANDLE h = 0;
    NTSTATUS st;
    UINT64 v;

    load();
    setup_device();
    if (kind == 'g')
    {
        D3DKMT_OPENSYNCHRONIZATIONOBJECT o = {0};
        o.hSharedHandle = strtoul(arg, NULL, 16);
        CHECK_ST(pD3DKMTOpenSynchronizationObject(&o), 0, "child OpenSynchronizationObject");
        h = o.hSyncObject;
    }
    else if (kind == 'n')
    {
        D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 o = {0};
        o.hNtHandle = (HANDLE)(ULONG_PTR)strtoul(arg, NULL, 16);
        o.hDevice = device;
        CHECK_ST(pD3DKMTOpenSyncObjectFromNtHandle2(&o), 0, "child OpenSyncObjectFromNtHandle2");
        h = o.hSyncObject;
        CHECK(o.MonitoredFence.FenceValueCPUVirtualAddress != NULL, "monitored fence address in the child");
    }
    else if (kind == 's')
    {
        D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME on = {0};
        D3DKMT_OPENSYNCOBJECTFROMNTHANDLE o = {0};
        OBJECT_ATTRIBUTES attr;
        UNICODE_STRING us;
        WCHAR name[128];

        MultiByteToWideChar(CP_ACP, 0, arg, -1, name, 128);
        pRtlInitUnicodeString(&us, name);
        InitializeObjectAttributes(&attr, &us, 0, 0, NULL);
        on.pObjAttrib = &attr;
        CHECK_ST(pD3DKMTOpenSyncObjectNtHandleFromName(&on), 0, "child OpenSyncObjectNtHandleFromName");
        o.hNtHandle = on.hNtHandle;
        CHECK_ST(pD3DKMTOpenSyncObjectFromNtHandle(&o), 0, "child OpenSyncObjectFromNtHandle");
        h = o.hSyncObject;
    }
    else if (kind == 'r')
    {
        D3DDDI_OPENALLOCATIONINFO2 infos[1] = {{0}};
        D3DKMT_OPENRESOURCEFROMNTHANDLE o = {0};
        char total[64], runtime[16];

        o.hDevice = device; o.hNtHandle = (HANDLE)(ULONG_PTR)strtoul(arg, NULL, 16);
        o.NumAllocations = 1; o.pOpenAllocationInfo2 = infos;
        o.pTotalPrivateDriverDataBuffer = total; o.TotalPrivateDriverDataBufferSize = sizeof(total);
        o.pPrivateRuntimeData = runtime; o.PrivateRuntimeDataSize = sizeof(runtime);
        CHECK_ST(pD3DKMTOpenResourceFromNtHandle(&o), 0, "child OpenResourceFromNtHandle");
        h = o.hSyncObject;
        CHECK(h != 0 && o.hKeyedMutex == 0, "the resource's sync object, and no mutex");
    }
    /* the parent signals 1 .. 3 with pauses; the child waits for them in order, then signals 10 */
    for (v = 1; v <= 3 && h; v++)
    {
        st = wait_one(h, v);
        CHECK_ST(st, 0, "child wait");
    }
    CHECK_ST(signal_obj(h, 10, 0), 0, "child signal 10");
    printf("CHILD %s checks %d failures %d\n", failures ? "FAIL" : "OK", checks, failures);
    return failures ? 1 : 0;
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

/* runs the child, signals 1..3 with pauses, waits for the child's 10 */
static void run_exchange(const char *args, D3DKMT_HANDLE mine, const char *what)
{
    child_t kid;
    HANDLE thread;
    UINT64 v;
    NTSTATUS st;

    snprintf(kid.args, sizeof(kid.args), "%s", args);
    thread = CreateThread(NULL, 0, child_thread, &kid, 0, NULL);
    for (v = 1; v <= 3; v++)
    {
        Sleep(120);
        CHECK_ST(signal_obj(mine, v, 0), 0, what);
    }
    st = wait_one(mine, 10);
    CHECK_ST(st, 0, "the child's signal arrives");
    WaitForSingleObject(thread, 60000);
    CloseHandle(thread);
    printf("%s", kid.out);
    CHECK(kid.code == 0, "%s: child exit %d", what, kid.code);
}

int main(int argc, char **argv)
{
    D3DKMT_HANDLE fence;
    NTSTATUS st;
    UINT shared = 0;
    char args[300];

    if (argc >= 3 && argv[1][0] == '-') return child_main(argv[1][1], argv[2]);

    load();
    setup_device();
    test_single_process();

    /* a global shared fence */
    {
        D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS f = {{0}};
        f.Shared = 1;
        fence = create2(D3DDDI_FENCE, f.Value, 0, 0, 0, NULL, &st, &shared, NULL);
        CHECK_ST(st, 0, "create a shared fence");
        CHECK(shared != 0, "its global handle");
        snprintf(args, sizeof(args), "-g %08x", shared);
        run_exchange(args, fence, "signal the global fence");
    }

    /* an NT handle (inherited), and a named one */
    {
        D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS f = {{0}};
        OBJECT_ATTRIBUTES attr;
        UNICODE_STRING us;
        HANDLE nt = NULL;
        WCHAR name[128];
        char name_a[128];
        DWORD session = 0;
        void *cpu = NULL;

        f.Shared = 1; f.NtSecuritySharing = 1;
        fence = create2(D3DDDI_MONITORED_FENCE, f.Value, 0, 0, 0, NULL, &st, &shared, &cpu);
        CHECK_ST(st, 0, "create an NT shared monitored fence");
        CHECK(shared == 0, "no global handle for an NT shared fence");
        InitializeObjectAttributes(&attr, NULL, OBJ_INHERIT, 0, NULL);
        CHECK_ST(pD3DKMTShareObjects(1, &fence, &attr, GENERIC_ALL, &nt), 0, "ShareObjects of a fence");
        snprintf(args, sizeof(args), "-n %lx", (unsigned long)(ULONG_PTR)nt);
        run_exchange(args, fence, "signal the NT shared fence");
        CHECK(cpu && *(volatile UINT64 *)cpu == 10, "the CPU address shows the child's value: %llu", cpu ? (unsigned long long)*(volatile UINT64 *)cpu : 0);
        CloseHandle(nt);

        fence = create2(D3DDDI_FENCE, f.Value, 0, 0, 0, NULL, &st, NULL, NULL);
        CHECK_ST(st, 0, "create another NT shared fence");
        ProcessIdToSessionId(GetCurrentProcessId(), &session);
        snprintf(name_a, sizeof(name_a), "\\Sessions\\%lu\\BaseNamedObjects\\GSharedSync%lu", (unsigned long)session, (unsigned long)GetCurrentProcessId());
        MultiByteToWideChar(CP_ACP, 0, name_a, -1, name, 128);
        pRtlInitUnicodeString(&us, name);
        InitializeObjectAttributes(&attr, &us, 0, 0, NULL);
        CHECK_ST(pD3DKMTShareObjects(1, &fence, &attr, GENERIC_ALL, &nt), 0, "ShareObjects with a name");
        snprintf(args, sizeof(args), "-s %s", name_a);
        run_exchange(args, fence, "signal the named fence");
        CloseHandle(nt);
    }

    /* a resource with its sync object, one NT handle */
    {
        D3DDDI_SYNCHRONIZATIONOBJECT_FLAGS f = {{0}};
        D3DKMT_CREATEALLOCATION c = {0};
        D3DDDI_ALLOCATIONINFO info = {0};
        struct wine_kmt_alloc_data blob = {64};
        D3DKMT_HANDLE hs[2];
        OBJECT_ATTRIBUTES attr;
        HANDLE nt = NULL;

        f.Shared = 1; f.NtSecuritySharing = 1;
        fence = create2(D3DDDI_FENCE, f.Value, 0, 0, 0, NULL, &st, NULL, NULL);
        c.hDevice = device; c.NumAllocations = 1; c.pAllocationInfo = &info;
        info.pPrivateDriverData = &blob; info.PrivateDriverDataSize = sizeof(blob);
        c.Flags.CreateResource = 1; c.Flags.CreateShared = 1; c.Flags.NtSecuritySharing = 1;
        CHECK_ST(pD3DKMTCreateAllocation(&c), 0, "create the resource");
        hs[0] = c.hResource; hs[1] = fence;
        InitializeObjectAttributes(&attr, NULL, OBJ_INHERIT, 0, NULL);
        CHECK_ST(pD3DKMTShareObjects(2, hs, &attr, GENERIC_ALL, &nt), 0, "ShareObjects of a resource and a sync object");
        snprintf(args, sizeof(args), "-r %lx", (unsigned long)(ULONG_PTR)nt);
        run_exchange(args, fence, "signal the resource's fence");
        CloseHandle(nt);
    }

    printf("checks %d failures %d\n", checks, failures);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
