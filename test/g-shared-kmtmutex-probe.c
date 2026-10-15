/* G-SHARED: D3DKMT keyed mutexes between processes (wine-sg 3322).
 * Run without arguments; it starts itself as the other side. */
#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#include <ddk/d3dkmthk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ST_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#define ST_TIMEOUT           ((NTSTATUS)0x00000102)
#define ST_ABANDONED         ((NTSTATUS)0x00000080)
#define ST_BUFFER_TOO_SMALL  ((NTSTATUS)0xC0000023)

#define WINE_KMT_ESCAPE_MAGIC 0x53474b4d
struct wine_kmt_alloc_data { UINT64 size; };
struct wine_kmt_escape { UINT magic, command; D3DKMT_HANDLE handle; UINT reserved; UINT64 section, offset, size; };

static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC *);
static NTSTATUS (WINAPI *pD3DKMTCreateDevice)(D3DKMT_CREATEDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTEscape)(const D3DKMT_ESCAPE *);
static NTSTATUS (WINAPI *pD3DKMTCreateAllocation)(D3DKMT_CREATEALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTShareObjects)(UINT, const D3DKMT_HANDLE *, OBJECT_ATTRIBUTES *, UINT, HANDLE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResourceFromNtHandle)(D3DKMT_OPENRESOURCEFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTCreateKeyedMutex)(D3DKMT_CREATEKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTCreateKeyedMutex2)(D3DKMT_CREATEKEYEDMUTEX2 *);
static NTSTATUS (WINAPI *pD3DKMTDestroyKeyedMutex)(const D3DKMT_DESTROYKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTOpenKeyedMutex)(D3DKMT_OPENKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTOpenKeyedMutex2)(D3DKMT_OPENKEYEDMUTEX2 *);
static NTSTATUS (WINAPI *pD3DKMTOpenKeyedMutexFromNtHandle)(D3DKMT_OPENKEYEDMUTEXFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTAcquireKeyedMutex)(D3DKMT_ACQUIREKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTAcquireKeyedMutex2)(D3DKMT_ACQUIREKEYEDMUTEX2 *);
static NTSTATUS (WINAPI *pD3DKMTReleaseKeyedMutex)(D3DKMT_RELEASEKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTReleaseKeyedMutex2)(D3DKMT_RELEASEKEYEDMUTEX2 *);

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_ST(status, expected, what) CHECK((NTSTATUS)(status) == (NTSTATUS)(expected), "%s: status %#lx, expected %#lx", what, (unsigned long)(status), (unsigned long)(expected))

static D3DKMT_HANDLE device, adapter_handle;

static void load(void)
{
    HMODULE gdi = LoadLibraryA("gdi32.dll");
#define L(n) p##n = (void *)GetProcAddress(gdi, #n); if (!p##n) { printf("FAIL  no %s\n", #n); exit(1); }
    L(D3DKMTOpenAdapterFromHdc) L(D3DKMTCreateDevice) L(D3DKMTEscape) L(D3DKMTCreateAllocation) L(D3DKMTShareObjects)
    L(D3DKMTOpenResourceFromNtHandle) L(D3DKMTCreateKeyedMutex) L(D3DKMTCreateKeyedMutex2) L(D3DKMTDestroyKeyedMutex)
    L(D3DKMTOpenKeyedMutex) L(D3DKMTOpenKeyedMutex2) L(D3DKMTOpenKeyedMutexFromNtHandle) L(D3DKMTAcquireKeyedMutex)
    L(D3DKMTAcquireKeyedMutex2) L(D3DKMTReleaseKeyedMutex) L(D3DKMTReleaseKeyedMutex2)
}

static void setup_device(void)
{
    D3DKMT_OPENADAPTERFROMHDC adapter = {0};
    D3DKMT_CREATEDEVICE dev = {0};

    adapter.hDc = GetDC(NULL);
    CHECK_ST(pD3DKMTOpenAdapterFromHdc(&adapter), 0, "OpenAdapterFromHdc");
    adapter_handle = adapter.hAdapter;
    dev.hAdapter = adapter.hAdapter;
    CHECK_ST(pD3DKMTCreateDevice(&dev), 0, "CreateDevice");
    device = dev.hDevice;
}

static NTSTATUS acquire(D3DKMT_HANDLE m, UINT64 key, LONGLONG timeout_100ns)  /* 0: forever? no: timeout_100ns<0 relative, 0 none, 1 forever */
{
    D3DKMT_ACQUIREKEYEDMUTEX a = {0};
    LARGE_INTEGER t;

    a.hKeyedMutex = m; a.Key = key;
    if (timeout_100ns != 1) { t.QuadPart = timeout_100ns; a.pTimeout = &t; }
    return pD3DKMTAcquireKeyedMutex(&a);
}

static NTSTATUS release(D3DKMT_HANDLE m, UINT64 key)
{
    D3DKMT_RELEASEKEYEDMUTEX r = {0};
    r.hKeyedMutex = m; r.Key = key;
    return pD3DKMTReleaseKeyedMutex(&r);
}

/* a resource with a content of 4096 bytes, for the shared counter; returns the mapped content */
static BYTE *make_resource(BOOL nt, D3DKMT_HANDLE *res, D3DKMT_HANDLE *alloc, void **base)
{
    D3DKMT_CREATEALLOCATION c = {0};
    D3DDDI_ALLOCATIONINFO info = {0};
    struct { struct wine_kmt_alloc_data hdr; } blob = {{4096}};
    struct wine_kmt_escape e = {WINE_KMT_ESCAPE_MAGIC, 1};
    D3DKMT_ESCAPE esc = {0};
    HANDLE section;
    BYTE *view;

    c.hDevice = device; c.NumAllocations = 1; c.pAllocationInfo = &info;
    info.pPrivateDriverData = &blob; info.PrivateDriverDataSize = sizeof(blob);
    c.Flags.CreateResource = 1; c.Flags.CreateShared = 1; c.Flags.NtSecuritySharing = nt;
    CHECK_ST(pD3DKMTCreateAllocation(&c), 0, "CreateAllocation");
    *res = c.hResource; *alloc = info.hAllocation;
    e.handle = info.hAllocation;
    esc.hAdapter = adapter_handle; esc.hDevice = device; esc.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    esc.pPrivateDriverData = &e; esc.PrivateDriverDataSize = sizeof(e);
    CHECK_ST(pD3DKMTEscape(&esc), 0, "escape");
    section = (HANDLE)(ULONG_PTR)e.section;
    view = MapViewOfFile(section, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CloseHandle(section);
    *base = view;
    return view + e.offset;
}

#define ROUNDS 40

/* the other process. 'g': a global mutex handle and a counter in a global resource? no: the counter is in a resource opened through
 * an NT handle, with its mutex ('n'); 'm': only a global mutex ('m'), 'd': a global mutex, takes it and exits. */
static int child_main(char kind, const char *arg)
{
    D3DKMT_HANDLE m = 0;
    NTSTATUS st;
    int i;
    volatile LONG *counter = NULL;
    void *base = NULL;

    load();
    setup_device();
    if (kind == 'n')
    {
        D3DDDI_OPENALLOCATIONINFO2 infos[1] = {{0}};
        D3DKMT_OPENRESOURCEFROMNTHANDLE o = {0};
        char runtime[64] = "", total[64], mrt[64] = "";
        struct wine_kmt_escape e = {WINE_KMT_ESCAPE_MAGIC, 1};
        D3DKMT_ESCAPE esc = {0};
        HANDLE section;
        BYTE *view;

        o.hDevice = device; o.hNtHandle = (HANDLE)(ULONG_PTR)strtoul(arg, NULL, 16);
        o.NumAllocations = 1; o.pOpenAllocationInfo2 = infos;
        o.pPrivateRuntimeData = runtime; o.PrivateRuntimeDataSize = sizeof(runtime);
        o.pTotalPrivateDriverDataBuffer = total; o.TotalPrivateDriverDataBufferSize = sizeof(total);
        o.pKeyedMutexPrivateRuntimeData = mrt; o.KeyedMutexPrivateRuntimeDataSize = sizeof(mrt);
        CHECK_ST(pD3DKMTOpenResourceFromNtHandle(&o), 0, "child OpenResourceFromNtHandle");
        m = o.hKeyedMutex;
        CHECK(m != 0, "the resource has its keyed mutex");
        CHECK(o.KeyedMutexPrivateRuntimeDataSize == 12 && !memcmp(mrt, "MUTEX-RT-01", 12), "the mutex's runtime data: %u", o.KeyedMutexPrivateRuntimeDataSize);
        e.handle = infos[0].hAllocation;
        esc.hAdapter = adapter_handle; esc.hDevice = device; esc.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
        esc.pPrivateDriverData = &e; esc.PrivateDriverDataSize = sizeof(e);
        CHECK_ST(pD3DKMTEscape(&esc), 0, "child escape");
        section = (HANDLE)(ULONG_PTR)e.section;
        view = MapViewOfFile(section, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        CloseHandle(section);
        base = view;
        counter = (volatile LONG *)(view + e.offset);
    }
    else
    {
        D3DKMT_OPENKEYEDMUTEX o = {0};
        o.hSharedHandle = strtoul(arg, NULL, 16);
        CHECK_ST(pD3DKMTOpenKeyedMutex(&o), 0, "child OpenKeyedMutex");
        m = o.hKeyedMutex;
    }
    if (kind == 'd')
    {
        st = acquire(m, 0, 1);
        CHECK_ST(st, 0, "child acquires key 0");
        printf("CHILD holding\n");
        fflush(stdout);
        ExitProcess(failures ? 1 : 0);   /* with the mutex held */
    }
    /* the child takes the odd keys: waits for key 2i+1, bumps the counter, hands over key 2i+2 */
    for (i = 0; i < ROUNDS; i++)
    {
        st = acquire(m, 2 * i + 1, 1);
        CHECK_ST(st, 0, "child acquire");
        if (st) break;
        if (counter)
        {
            CHECK(*counter == 2 * i, "child sees counter %ld at round %d", *counter, i);
            *counter += 1;
        }
        st = release(m, 2 * i + 2);
        CHECK_ST(st, 0, "child release");
    }
    if (base) UnmapViewOfFile(base);
    printf("CHILD %s checks %d failures %d\n", failures ? "FAIL" : "OK", checks, failures);
    return failures ? 1 : 0;
}

static int run_child(const char *args, char *out, size_t out_size, PROCESS_INFORMATION *pi_out)
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
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { printf("FAIL  CreateProcess %lu\n", GetLastError()); failures++; return 98; }
    CloseHandle(wr);
    while (ReadFile(rd, out + total, out_size - 1 - total, &n, NULL) && n) total += n;
    out[total] = 0;
    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    (void)pi_out;
    return code;
}

typedef struct { char args[300]; char out[2048]; int code; HANDLE thread; } child_t;
static DWORD WINAPI child_thread(void *p)
{
    child_t *c = p;
    c->code = run_child(c->args, c->out, sizeof(c->out), NULL);
    return 0;
}

static void test_single_process(void)
{
    D3DKMT_CREATEKEYEDMUTEX c = {0};
    D3DKMT_OPENKEYEDMUTEX o = {0};
    D3DKMT_DESTROYKEYEDMUTEX d = {0};
    D3DKMT_HANDLE m1, m2;
    DWORD t0, dt;
    NTSTATUS st;

    CHECK_ST(pD3DKMTCreateKeyedMutex(NULL), ST_INVALID_PARAMETER, "CreateKeyedMutex(NULL)");
    c.InitialValue = 5;
    CHECK_ST(pD3DKMTCreateKeyedMutex(&c), 0, "CreateKeyedMutex");
    CHECK(c.hKeyedMutex && c.hSharedHandle, "handles %#x %#x", c.hKeyedMutex, c.hSharedHandle);
    m1 = c.hKeyedMutex;
    CHECK_ST(acquire(m1, 4, 0), ST_TIMEOUT, "acquire the wrong key");
    CHECK_ST(acquire(0x1234, 5, 0), ST_INVALID_PARAMETER, "acquire an unknown mutex");
    CHECK_ST(release(m1, 6), ST_INVALID_PARAMETER, "release a mutex nobody holds");
    CHECK_ST(acquire(m1, 5, 0), 0, "acquire the initial key");
    CHECK_ST(acquire(m1, 5, 0), ST_INVALID_PARAMETER, "acquire again from the owner");
    CHECK_ST(release(m1, 6), 0, "release with key 6");
    CHECK_ST(acquire(m1, 5, 0), ST_TIMEOUT, "key 5 is gone");
    CHECK_ST(acquire(m1, 6, 0), 0, "key 6");

    /* a second object on the same mutex */
    o.hSharedHandle = c.hSharedHandle;
    CHECK_ST(pD3DKMTOpenKeyedMutex(&o), 0, "OpenKeyedMutex");
    m2 = o.hKeyedMutex;
    CHECK(m2 && m2 != m1, "second handle");
    t0 = GetTickCount();
    st = acquire(m2, 6, -500000);
    dt = GetTickCount() - t0;
    CHECK_ST(st, ST_TIMEOUT, "second object waits while the first holds");
    CHECK(dt >= 40 && dt < 2000, "the 50 ms wait took %lu ms", dt);
    CHECK_ST(release(m2, 7), ST_INVALID_PARAMETER, "release by a non-owner");
    /* the wait ends when the owner releases */
    CHECK_ST(release(m1, 7), 0, "release 7");
    CHECK_ST(acquire(m2, 7, -10000000), 0, "second object takes key 7");

    /* the owner's object is destroyed: the mutex is abandoned */
    d.hKeyedMutex = m2;
    CHECK_ST(pD3DKMTDestroyKeyedMutex(&d), 0, "DestroyKeyedMutex holding it");
    CHECK_ST(pD3DKMTDestroyKeyedMutex(&d), ST_INVALID_PARAMETER, "DestroyKeyedMutex twice");
    CHECK_ST(acquire(m1, 7, 0), ST_ABANDONED, "abandoned after its owner was destroyed");
    CHECK_ST(acquire(m1, 8, 0), ST_ABANDONED, "stays abandoned");
    d.hKeyedMutex = m1;
    CHECK_ST(pD3DKMTDestroyKeyedMutex(&d), 0, "Destroy last");
    o.hSharedHandle = c.hSharedHandle;
    CHECK_ST(pD3DKMTOpenKeyedMutex(&o), ST_INVALID_PARAMETER, "OpenKeyedMutex of a destroyed mutex");
    o.hSharedHandle = 0x4444;
    CHECK_ST(pD3DKMTOpenKeyedMutex(&o), ST_INVALID_PARAMETER, "OpenKeyedMutex of nothing");

    /* the variants with runtime data */
    {
        D3DKMT_CREATEKEYEDMUTEX2 c2 = {0};
        D3DKMT_OPENKEYEDMUTEX2 o2 = {0};
        D3DKMT_ACQUIREKEYEDMUTEX2 a2 = {0};
        D3DKMT_RELEASEKEYEDMUTEX2 r2 = {0};
        char buf[64] = "";
        LARGE_INTEGER zero = {{0}};

        c2.InitialValue = 0; c2.pPrivateRuntimeData = "RT-ONE"; c2.PrivateRuntimeDataSize = 7;
        CHECK_ST(pD3DKMTCreateKeyedMutex2(&c2), 0, "CreateKeyedMutex2");
        CHECK(c2.hSharedHandle != 0, "global handle");
        o2.hSharedHandle = c2.hSharedHandle; o2.pPrivateRuntimeData = buf; o2.PrivateRuntimeDataSize = 3;
        CHECK_ST(pD3DKMTOpenKeyedMutex2(&o2), ST_BUFFER_TOO_SMALL, "OpenKeyedMutex2 with a small buffer");
        o2.PrivateRuntimeDataSize = sizeof(buf);
        CHECK_ST(pD3DKMTOpenKeyedMutex2(&o2), 0, "OpenKeyedMutex2");
        CHECK(o2.PrivateRuntimeDataSize == 7 && !memcmp(buf, "RT-ONE", 7), "runtime data back: %u", o2.PrivateRuntimeDataSize);
        a2.hKeyedMutex = o2.hKeyedMutex; a2.Key = 0; a2.pTimeout = &zero;
        a2.pPrivateRuntimeData = buf; a2.PrivateRuntimeDataSize = sizeof(buf);
        CHECK_ST(pD3DKMTAcquireKeyedMutex2(&a2), 0, "AcquireKeyedMutex2");
        r2.hKeyedMutex = o2.hKeyedMutex; r2.Key = 9;
        CHECK_ST(pD3DKMTReleaseKeyedMutex2(&r2), 0, "ReleaseKeyedMutex2");
        CHECK_ST(acquire(c2.hKeyedMutex, 9, 0), 0, "the creator's object sees key 9");
        c2.Flags.NtSecuritySharing = 1; c2.hSharedHandle = 0x55;
        CHECK_ST(pD3DKMTCreateKeyedMutex2(&c2), 0, "CreateKeyedMutex2 for NT sharing");
        CHECK(c2.hSharedHandle == 0, "no global handle for NT sharing: %#x", c2.hSharedHandle);
        c2.PrivateRuntimeDataSize = 100000;
        CHECK_ST(pD3DKMTCreateKeyedMutex2(&c2), ST_INVALID_PARAMETER, "runtime data too big");
    }
}

int main(int argc, char **argv)
{
    child_t kid;
    D3DKMT_HANDLE m, res, alloc;
    D3DKMT_CREATEKEYEDMUTEX c = {0};
    NTSTATUS st;
    int i;

    if (argc >= 3 && argv[1][0] == '-') return child_main(argv[1][1], argv[2]);

    load();
    setup_device();
    test_single_process();

    /* ping-pong over a global mutex in two processes: the child takes the odd keys */
    c.InitialValue = 0;
    CHECK_ST(pD3DKMTCreateKeyedMutex(&c), 0, "create ping-pong mutex");
    m = c.hKeyedMutex;
    snprintf(kid.args, sizeof(kid.args), "-m %08x", c.hSharedHandle);
    kid.thread = CreateThread(NULL, 0, child_thread, &kid, 0, NULL);
    CHECK_ST(acquire(m, 0, 1), 0, "parent acquires key 0");
    CHECK_ST(release(m, 1), 0, "parent starts with key 1");
    for (i = 0; i < ROUNDS; i++)
    {
        st = acquire(m, 2 * i + 2, -300000000);   /* 30 s */
        CHECK_ST(st, 0, "parent acquire");
        if (st) break;
        if (i + 1 < ROUNDS) CHECK_ST(release(m, 2 * i + 3), 0, "parent release");
    }
    WaitForSingleObject(kid.thread, 60000);
    printf("%s", kid.out);
    CHECK(kid.code == 0, "ping-pong child exit %d", kid.code);

    /* a resource and its mutex shared as one NT handle: the child counts in the content under the mutex */
    {
        D3DKMT_CREATEKEYEDMUTEX2 c2 = {0};
        D3DKMT_HANDLE hs[2];
        OBJECT_ATTRIBUTES attr;
        HANDLE nt = NULL;
        void *base = NULL;
        BYTE *content;
        volatile LONG *counter;

        content = make_resource(TRUE, &res, &alloc, &base);
        counter = (volatile LONG *)content;
        c2.Flags.NtSecuritySharing = 1; c2.pPrivateRuntimeData = "MUTEX-RT-01"; c2.PrivateRuntimeDataSize = 12;
        CHECK_ST(pD3DKMTCreateKeyedMutex2(&c2), 0, "create the group's mutex");
        hs[0] = res; hs[1] = c2.hKeyedMutex;
        InitializeObjectAttributes(&attr, NULL, OBJ_INHERIT, 0, NULL);
        CHECK_ST(pD3DKMTShareObjects(2, hs, &attr, GENERIC_ALL, &nt), 0, "ShareObjects of a resource and its mutex");
        hs[0] = res; hs[1] = res;
        { HANDLE nt2 = NULL; CHECK_ST(pD3DKMTShareObjects(2, hs, &attr, GENERIC_ALL, &nt2), ST_INVALID_PARAMETER, "the same resource twice"); }
        m = c2.hKeyedMutex;
        CHECK_ST(acquire(m, 0, 1), 0, "parent acquires the group's mutex");
        *counter = 0;
        CHECK_ST(release(m, 1), 0, "parent releases key 1");
        snprintf(kid.args, sizeof(kid.args), "-n %lx", (unsigned long)(ULONG_PTR)nt);
        kid.thread = CreateThread(NULL, 0, child_thread, &kid, 0, NULL);
        for (i = 0; i < ROUNDS; i++)
        {
            st = acquire(m, 2 * i + 2, -300000000);
            CHECK_ST(st, 0, "parent acquire (group)");
            if (st) break;
            CHECK(*counter == 2 * i + 1, "parent sees counter %ld at round %d", *counter, i);
            *counter += 1;
            if (i + 1 < ROUNDS) CHECK_ST(release(m, 2 * i + 3), 0, "parent release (group)");
        }
        WaitForSingleObject(kid.thread, 60000);
        printf("%s", kid.out);
        CHECK(kid.code == 0, "group child exit %d", kid.code);
        CHECK(*counter == 2 * ROUNDS, "final counter %ld", *counter);
        CloseHandle(nt);
        UnmapViewOfFile(base);
    }

    /* the other process holds the mutex and exits */
    {
        char out[512];
        int code;

        c.InitialValue = 0;
        CHECK_ST(pD3DKMTCreateKeyedMutex(&c), 0, "create the mutex that is lost");
        snprintf(kid.args, sizeof(kid.args), "-d %08x", c.hSharedHandle);
        code = run_child(kid.args, out, sizeof(out), NULL);
        printf("%s", out);
        CHECK(code == 0, "dying child exit %d", code);
        CHECK_ST(acquire(c.hKeyedMutex, 0, -100000000), ST_ABANDONED, "a mutex whose owner process is gone");
        CHECK_ST(acquire(c.hKeyedMutex, 1, 0), ST_ABANDONED, "and stays so");
    }

    printf("checks %d failures %d\n", checks, failures);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
