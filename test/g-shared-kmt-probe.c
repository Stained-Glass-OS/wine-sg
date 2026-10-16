/* G-SHARED: D3DKMT shared resources between processes (wine-sg 3321).
 * Run without arguments; it starts itself as the opener. */
#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#include <ddk/d3dkmthk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0)
#endif
#define ST_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#define ST_BUFFER_TOO_SMALL  ((NTSTATUS)0xC0000023)
#define ST_NAME_COLLISION    ((NTSTATUS)0xC0000035)
#define ST_NOT_SUPPORTED     ((NTSTATUS)0xC00000BB)

/* copy of include/wine/kmtshare.h */
#define WINE_KMT_ESCAPE_MAGIC 0x53474b4d
struct wine_kmt_alloc_data { UINT64 size; };
struct wine_kmt_escape
{
    UINT magic, command;
    D3DKMT_HANDLE handle;
    UINT reserved;
    UINT64 section, offset, size;
};

static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC *);
static NTSTATUS (WINAPI *pD3DKMTCreateDevice)(D3DKMT_CREATEDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTDestroyDevice)(const D3DKMT_DESTROYDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTEscape)(const D3DKMT_ESCAPE *);
static NTSTATUS (WINAPI *pD3DKMTCreateAllocation)(D3DKMT_CREATEALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTCreateAllocation2)(D3DKMT_CREATEALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTDestroyAllocation)(const D3DKMT_DESTROYALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTDestroyAllocation2)(const D3DKMT_DESTROYALLOCATION2 *);
static NTSTATUS (WINAPI *pD3DKMTShareObjects)(UINT, const D3DKMT_HANDLE *, OBJECT_ATTRIBUTES *, UINT, HANDLE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResource)(D3DKMT_OPENRESOURCE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResource2)(D3DKMT_OPENRESOURCE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResourceFromNtHandle)(D3DKMT_OPENRESOURCEFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTQueryResourceInfo)(D3DKMT_QUERYRESOURCEINFO *);
static NTSTATUS (WINAPI *pD3DKMTQueryResourceInfoFromNtHandle)(D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *);
static NTSTATUS (WINAPI *pNtOpenSection)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *);
static NTSTATUS (WINAPI *pRtlInitUnicodeString)(UNICODE_STRING *, const WCHAR *);

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_ST(status, expected, what) CHECK((NTSTATUS)(status) == (NTSTATUS)(expected), "%s: status %#lx, expected %#lx", what, (unsigned long)(status), (unsigned long)(expected))

static D3DKMT_HANDLE device, adapter_handle;
static const char runtime_data[] = "RUNTIME-DATA-0123456789";
#define CONTENT 4096

static void load(void)
{
    HMODULE gdi = LoadLibraryA("gdi32.dll"), nt = GetModuleHandleA("ntdll.dll");
#define L(n) p##n = (void *)GetProcAddress(gdi, #n); if (!p##n) { printf("FAIL  no %s\n", #n); exit(1); }
    L(D3DKMTOpenAdapterFromHdc) L(D3DKMTCreateDevice) L(D3DKMTDestroyDevice) L(D3DKMTEscape)
    L(D3DKMTCreateAllocation) L(D3DKMTCreateAllocation2) L(D3DKMTDestroyAllocation) L(D3DKMTDestroyAllocation2)
    L(D3DKMTShareObjects) L(D3DKMTOpenResource) L(D3DKMTOpenResource2) L(D3DKMTOpenResourceFromNtHandle)
    L(D3DKMTQueryResourceInfo) L(D3DKMTQueryResourceInfoFromNtHandle)
    pNtOpenSection = (void *)GetProcAddress(nt, "NtOpenSection");
    pRtlInitUnicodeString = (void *)GetProcAddress(nt, "RtlInitUnicodeString");
}

static void setup_device(void)
{
    D3DKMT_OPENADAPTERFROMHDC adapter = {0};
    D3DKMT_CREATEDEVICE dev = {0};
    NTSTATUS st;

    adapter.hDc = GetDC(NULL);
    st = pD3DKMTOpenAdapterFromHdc(&adapter);
    CHECK_ST(st, STATUS_SUCCESS, "OpenAdapterFromHdc");
    adapter_handle = adapter.hAdapter;
    dev.hAdapter = adapter.hAdapter;
    st = pD3DKMTCreateDevice(&dev);
    CHECK_ST(st, STATUS_SUCCESS, "CreateDevice");
    device = dev.hDevice;
}

struct alloc_blob { struct wine_kmt_alloc_data hdr; char text[8]; };

/* creates a shared resource of one allocation; info2 selects the CreateAllocation2 structures */
static NTSTATUS create_resource(UINT shared_flags, BOOL info2, D3DKMT_HANDLE *res, D3DKMT_HANDLE *alloc, UINT *global)
{
    D3DKMT_CREATEALLOCATION c = {0};
    D3DDDI_ALLOCATIONINFO info = {0};
    D3DDDI_ALLOCATIONINFO2 info_2 = {{0}};
    struct alloc_blob blob = {{CONTENT}, "DRV-0"};
    NTSTATUS st;

    c.hDevice = device;
    c.pPrivateRuntimeData = runtime_data;
    c.PrivateRuntimeDataSize = sizeof(runtime_data);
    c.NumAllocations = 1;
    c.Flags.CreateResource = 1;
    c.Flags.CreateShared = (shared_flags & 1) != 0;
    c.Flags.NtSecuritySharing = (shared_flags & 2) != 0;
    if (info2)
    {
        info_2.pPrivateDriverData = &blob;
        info_2.PrivateDriverDataSize = sizeof(blob);
        c.pAllocationInfo2 = &info_2;
        st = pD3DKMTCreateAllocation2(&c);
        if (!st) *alloc = info_2.hAllocation;
    }
    else
    {
        info.pPrivateDriverData = &blob;
        info.PrivateDriverDataSize = sizeof(blob);
        c.pAllocationInfo = &info;
        st = pD3DKMTCreateAllocation(&c);
        if (!st) *alloc = info.hAllocation;
    }
    if (!st) { *res = c.hResource; *global = c.hGlobalShare; }
    return st;
}

/* the content of an allocation: a view of the section, and where the content is in it */
static BYTE *map_alloc(D3DKMT_HANDLE handle, UINT64 *size, void **base)
{
    struct wine_kmt_escape e = {WINE_KMT_ESCAPE_MAGIC, 1, handle};
    D3DKMT_ESCAPE esc = {0};
    NTSTATUS st;
    HANDLE section;
    BYTE *view;

    esc.hAdapter = adapter_handle;
    esc.hDevice = device;
    esc.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    esc.pPrivateDriverData = &e;
    esc.PrivateDriverDataSize = sizeof(e);
    st = pD3DKMTEscape(&esc);
    if (st) { printf("FAIL  escape status %#lx\n", (unsigned long)st); failures++; return NULL; }
    section = (HANDLE)(ULONG_PTR)e.section;
    view = MapViewOfFile(section, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CloseHandle(section);
    if (!view) { printf("FAIL  map %lu\n", GetLastError()); failures++; return NULL; }
    *base = view;
    *size = e.size;
    return view + e.offset;
}

static void fill(BYTE *p, UINT n)
{
    UINT i;
    for (i = 8; i < n; i++) p[i] = (BYTE)(i * 7 + 3);
}

static int check_fill(const BYTE *p, UINT n)
{
    UINT i;
    for (i = 8; i < n; i++) if (p[i] != (BYTE)(i * 7 + 3)) return 0;
    return 1;
}

/* the opener, in a child process. kind: g (global handle) n (NT handle value) s (name) */
static int child_main(char kind, const char *arg)
{
    D3DDDI_OPENALLOCATIONINFO2 infos2[1] = {{0}};
    D3DDDI_OPENALLOCATIONINFO infos[1] = {{0}};
    char runtime[64] = "", total[256];
    NTSTATUS st;
    UINT64 size;
    void *base;
    BYTE *content;
    UINT n_alloc, total_size, res_size, runtime_size = sizeof(runtime);
    D3DKMT_HANDLE res = 0;
    HANDLE nt = NULL;

    load();
    setup_device();
    if (kind == 'g')
    {
        D3DKMT_QUERYRESOURCEINFO q = {0};
        D3DKMT_OPENRESOURCE o = {0};

        q.hDevice = device;
        q.hGlobalShare = strtoul(arg, NULL, 16);
        q.pPrivateRuntimeData = runtime;
        q.PrivateRuntimeDataSize = runtime_size;
        st = pD3DKMTQueryResourceInfo(&q);
        CHECK_ST(st, STATUS_SUCCESS, "child QueryResourceInfo");
        n_alloc = q.NumAllocations; total_size = q.TotalPrivateDriverDataSize; res_size = q.ResourcePrivateDriverDataSize;
        CHECK(n_alloc == 1 && res_size == 0 && total_size == sizeof(struct alloc_blob), "query sizes %u %u %u", n_alloc, res_size, total_size);
        CHECK(q.PrivateRuntimeDataSize == sizeof(runtime_data) && !memcmp(runtime, runtime_data, sizeof(runtime_data)), "runtime data");
        o.hDevice = device;
        o.hGlobalShare = q.hGlobalShare;
        o.NumAllocations = 1;
        o.pOpenAllocationInfo = infos;
        o.pPrivateRuntimeData = runtime;
        o.PrivateRuntimeDataSize = runtime_size;
        o.pTotalPrivateDriverDataBuffer = total;
        o.TotalPrivateDriverDataBufferSize = sizeof(total);
        st = pD3DKMTOpenResource(&o);
        CHECK_ST(st, STATUS_SUCCESS, "child OpenResource");
        res = o.hResource;
        CHECK(infos[0].hAllocation && infos[0].PrivateDriverDataSize == sizeof(struct alloc_blob)
              && infos[0].pPrivateDriverData == (void *)total && !memcmp(total + 8, "DRV-0", 6), "alloc info");
    }
    else
    {
        D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE q = {0};
        D3DKMT_OPENRESOURCEFROMNTHANDLE o = {0};

        if (kind == 'n') nt = (HANDLE)(ULONG_PTR)strtoul(arg, NULL, 16);
        else
        {
            WCHAR name[128];
            UNICODE_STRING us;
            OBJECT_ATTRIBUTES attr;
            MultiByteToWideChar(CP_ACP, 0, arg, -1, name, 128);
            pRtlInitUnicodeString(&us, name);
            InitializeObjectAttributes(&attr, &us, 0, 0, NULL);
            st = pNtOpenSection(&nt, SECTION_ALL_ACCESS, &attr);
            CHECK_ST(st, STATUS_SUCCESS, "child NtOpenSection");
        }
        q.hDevice = device;
        q.hNtHandle = nt;
        q.pPrivateRuntimeData = runtime;
        q.PrivateRuntimeDataSize = runtime_size;
        st = pD3DKMTQueryResourceInfoFromNtHandle(&q);
        CHECK_ST(st, STATUS_SUCCESS, "child QueryResourceInfoFromNtHandle");
        CHECK(q.NumAllocations == 1 && q.TotalPrivateDriverDataSize == sizeof(struct alloc_blob), "nt query sizes");
        CHECK(!memcmp(runtime, runtime_data, sizeof(runtime_data)), "nt runtime data");
        o.hDevice = device;
        o.hNtHandle = nt;
        o.NumAllocations = 1;
        o.pOpenAllocationInfo2 = infos2;
        o.pPrivateRuntimeData = runtime;
        o.PrivateRuntimeDataSize = runtime_size;
        o.pTotalPrivateDriverDataBuffer = total;
        o.TotalPrivateDriverDataBufferSize = sizeof(total);
        st = pD3DKMTOpenResourceFromNtHandle(&o);
        CHECK_ST(st, STATUS_SUCCESS, "child OpenResourceFromNtHandle");
        res = o.hResource;
        CHECK(infos2[0].hAllocation && infos2[0].PrivateDriverDataSize == sizeof(struct alloc_blob), "nt alloc info");
    }
    content = res ? map_alloc(res, &size, &base) : NULL;
    if (content)
    {
        CHECK(size == CONTENT, "content size %llu", (unsigned long long)size);
        CHECK(!memcmp(content, "FROMPAR!", 8) && check_fill(content, CONTENT), "the parent's pixels are not there");
        memcpy(content, "FROMKID!", 8);
        UnmapViewOfFile(base);
    }
    printf("CHILD %s checks %d failures %d\n", failures ? "FAIL" : "OK", checks, failures);
    return failures ? 1 : 0;
}

static int run_child(const char *args, BOOL inherit, char *out, size_t out_size)
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
    return code;
}

static void test_validation(void)
{
    D3DKMT_HANDLE res = 0, alloc = 0, res2 = 0, alloc2 = 0;
    UINT global = 0;
    D3DKMT_CREATEALLOCATION c = {0};
    D3DDDI_ALLOCATIONINFO info = {0};
    D3DKMT_DESTROYALLOCATION d = {0};
    D3DKMT_DESTROYALLOCATION2 d2 = {0};
    struct alloc_blob blob = {{64}, "x"};
    D3DKMT_HANDLE both[2];
    HANDLE h = NULL;
    NTSTATUS st;

    st = pD3DKMTCreateAllocation(NULL);
    CHECK_ST(st, ST_INVALID_PARAMETER, "CreateAllocation(NULL)");
    c.hDevice = device; c.NumAllocations = 0;
    st = pD3DKMTCreateAllocation(&c);
    CHECK_ST(st, ST_INVALID_PARAMETER, "CreateAllocation no allocations");
    c.NumAllocations = 1; c.pAllocationInfo = &info; info.pPrivateDriverData = &blob; info.PrivateDriverDataSize = sizeof(blob);
    c.hDevice = 0x1234;
    st = pD3DKMTCreateAllocation(&c);
    CHECK_ST(st, ST_INVALID_PARAMETER, "CreateAllocation unknown device");
    c.hDevice = device; c.Flags.CreateShared = 1;
    st = pD3DKMTCreateAllocation(&c);
    CHECK_ST(st, ST_INVALID_PARAMETER, "CreateShared without CreateResource");
    c.Flags.CreateShared = 0; c.Flags.NtSecuritySharing = 1; c.Flags.CreateResource = 1;
    st = pD3DKMTCreateAllocation(&c);
    CHECK_ST(st, ST_INVALID_PARAMETER, "NtSecuritySharing without CreateShared");
    c.Flags.NtSecuritySharing = 0; c.Flags.StandardAllocation = 1;
    st = pD3DKMTCreateAllocation(&c);
    CHECK_ST(st, ST_NOT_SUPPORTED, "StandardAllocation");
    c.Flags.StandardAllocation = 0; info.PrivateDriverDataSize = 0; info.pPrivateDriverData = NULL;
    st = pD3DKMTCreateAllocation(&c);
    CHECK_ST(st, STATUS_SUCCESS, "CreateAllocation without private data");
    CHECK(c.hResource && info.hAllocation && !c.hGlobalShare, "handles %#x %#x %#x", c.hResource, info.hAllocation, c.hGlobalShare);
    d.hDevice = device; d.hResource = c.hResource;
    st = pD3DKMTDestroyAllocation(&d);
    CHECK_ST(st, STATUS_SUCCESS, "DestroyAllocation resource");
    st = pD3DKMTDestroyAllocation(&d);
    CHECK_ST(st, ST_INVALID_PARAMETER, "DestroyAllocation twice");

    /* allocations without a resource, destroyed one by one */
    memset(&c, 0, sizeof(c)); memset(&info, 0, sizeof(info));
    {
        D3DDDI_ALLOCATIONINFO infos[2] = {{0}};
        D3DKMT_HANDLE list[2];
        infos[0].pPrivateDriverData = &blob; infos[0].PrivateDriverDataSize = sizeof(blob);
        infos[1] = infos[0];
        c.hDevice = device; c.NumAllocations = 2; c.pAllocationInfo = infos;
        st = pD3DKMTCreateAllocation(&c);
        CHECK_ST(st, STATUS_SUCCESS, "two allocations without resource");
        CHECK(!c.hResource && infos[0].hAllocation && infos[1].hAllocation && infos[0].hAllocation != infos[1].hAllocation, "alloc handles");
        list[0] = infos[0].hAllocation; list[1] = infos[1].hAllocation;
        d.hResource = 0; d.phAllocationList = list; d.AllocationCount = 2;
        d.hDevice = 0x4321;
        st = pD3DKMTDestroyAllocation(&d);
        CHECK_ST(st, ST_INVALID_PARAMETER, "DestroyAllocation wrong device");
        d.hDevice = device; d.AllocationCount = 1;
        st = pD3DKMTDestroyAllocation(&d);
        CHECK_ST(st, STATUS_SUCCESS, "DestroyAllocation one");
        d2.hDevice = device; d2.phAllocationList = list; d2.AllocationCount = 2;
        st = pD3DKMTDestroyAllocation2(&d2);
        CHECK_ST(st, ST_INVALID_PARAMETER, "DestroyAllocation2 with one gone, nothing is destroyed");
        d2.phAllocationList = list + 1; d2.AllocationCount = 1;
        st = pD3DKMTDestroyAllocation2(&d2);
        CHECK_ST(st, STATUS_SUCCESS, "DestroyAllocation2 rest");
        d2.hResource = 0x777; d2.phAllocationList = NULL; d2.AllocationCount = 0;
        st = pD3DKMTDestroyAllocation2(&d2);
        CHECK_ST(st, ST_INVALID_PARAMETER, "DestroyAllocation2 unknown resource");
    }

    /* ShareObjects */
    st = create_resource(1, FALSE, &res, &alloc, &global);
    CHECK_ST(st, STATUS_SUCCESS, "create global resource");
    CHECK(global, "no global handle");
    both[0] = res;
    st = pD3DKMTShareObjects(1, both, NULL, GENERIC_ALL, &h);
    CHECK_ST(st, ST_INVALID_PARAMETER, "ShareObjects of a resource made for global sharing");
    st = pD3DKMTShareObjects(0, both, NULL, GENERIC_ALL, &h);
    CHECK_ST(st, ST_INVALID_PARAMETER, "ShareObjects no objects");
    both[0] = alloc;
    st = pD3DKMTShareObjects(1, both, NULL, GENERIC_ALL, &h);
    CHECK_ST(st, ST_INVALID_PARAMETER, "ShareObjects of an allocation");
    d.hDevice = device; d.hResource = res; d.phAllocationList = NULL; d.AllocationCount = 0;
    st = pD3DKMTDestroyAllocation(&d);
    CHECK_ST(st, STATUS_SUCCESS, "destroy global resource");
    {
        D3DKMT_OPENRESOURCE o = {0};
        D3DDDI_OPENALLOCATIONINFO oi[1] = {{0}};
        o.hDevice = device; o.hGlobalShare = global; o.NumAllocations = 1; o.pOpenAllocationInfo = oi;
        st = pD3DKMTOpenResource(&o);
        CHECK_ST(st, ST_INVALID_PARAMETER, "OpenResource of a destroyed resource");
    }
    (void)res2; (void)alloc2;
}

/* one process: both handles reach the same pixels */
static void test_same_process(void)
{
    D3DKMT_HANDLE res, alloc;
    UINT global;
    D3DDDI_OPENALLOCATIONINFO oi[1] = {{0}};
    D3DKMT_OPENRESOURCE o = {0};
    D3DKMT_DESTROYALLOCATION d = {0};
    char runtime[64], total[256];
    UINT64 size;
    void *b1, *b2;
    BYTE *c1, *c2;
    NTSTATUS st;

    st = create_resource(1, FALSE, &res, &alloc, &global);
    CHECK_ST(st, STATUS_SUCCESS, "create");
    c1 = map_alloc(alloc, &size, &b1);
    CHECK(c1 && size == CONTENT, "creator content size %llu", (unsigned long long)size);
    if (!c1) return;
    CHECK(c1[0] == 0 && c1[CONTENT - 1] == 0 && c1[777] == 0, "content starts zeroed");
    memcpy(c1, "FROMPAR!", 8); fill(c1, CONTENT);

    /* buffers that are too small */
    o.hDevice = device; o.hGlobalShare = global; o.NumAllocations = 1; o.pOpenAllocationInfo = oi;
    o.pPrivateRuntimeData = runtime; o.PrivateRuntimeDataSize = 4;
    st = pD3DKMTOpenResource(&o);
    CHECK_ST(st, ST_BUFFER_TOO_SMALL, "OpenResource runtime buffer too small");
    o.PrivateRuntimeDataSize = sizeof(runtime); o.pTotalPrivateDriverDataBuffer = total; o.TotalPrivateDriverDataBufferSize = 3;
    st = pD3DKMTOpenResource(&o);
    CHECK_ST(st, ST_BUFFER_TOO_SMALL, "OpenResource total buffer too small");
    o.NumAllocations = 2; o.TotalPrivateDriverDataBufferSize = sizeof(total);
    st = pD3DKMTOpenResource(&o);
    CHECK_ST(st, ST_INVALID_PARAMETER, "OpenResource wrong allocation count");
    o.NumAllocations = 1; o.hDevice = 0x999;
    st = pD3DKMTOpenResource(&o);
    CHECK_ST(st, ST_INVALID_PARAMETER, "OpenResource unknown device");
    o.hDevice = device;
    st = pD3DKMTOpenResource(&o);
    CHECK_ST(st, STATUS_SUCCESS, "OpenResource in the creating process");
    CHECK(o.hResource && o.hResource != res && oi[0].hAllocation && oi[0].hAllocation != alloc, "the opener has handles of its own");
    c2 = map_alloc(oi[0].hAllocation, &size, &b2);
    CHECK(c2 && size == CONTENT && c2 != c1, "opener content");
    if (c2)
    {
        CHECK(!memcmp(c2, "FROMPAR!", 8) && check_fill(c2, CONTENT), "the opener sees the pixels");
        c2[100] = 0x5a;
        CHECK(c1[100] == 0x5a, "a write through the opener reaches the creator");
        UnmapViewOfFile(b2);
    }
    /* the resource of the opener alone keeps the pixels, and the global handle */
    {
        D3DKMT_HANDLE first = o.hResource;

        d.hDevice = device; d.hResource = res;
        st = pD3DKMTDestroyAllocation(&d);
        CHECK_ST(st, STATUS_SUCCESS, "destroy the creator's resource");
        c2 = map_alloc(first, &size, &b2);
        CHECK(c2 && c2[100] == 0x5a, "the pixels live on for the opener");
        if (c2) UnmapViewOfFile(b2);
        UnmapViewOfFile(b1);
        st = pD3DKMTOpenResource(&o);
        CHECK_ST(st, STATUS_SUCCESS, "the global handle works while the opener holds the resource");
        d.hResource = o.hResource;
        st = pD3DKMTDestroyAllocation(&d);
        CHECK_ST(st, STATUS_SUCCESS, "destroy the second opened resource");
        d.hResource = first;
        st = pD3DKMTDestroyAllocation(&d);
        CHECK_ST(st, STATUS_SUCCESS, "destroy the opened resource");
        st = pD3DKMTOpenResource(&o);
        CHECK_ST(st, ST_INVALID_PARAMETER, "the global handle is gone with the last resource");
    }
}

int main(int argc, char **argv)
{
    char out[4096];
    D3DKMT_HANDLE res, alloc;
    UINT global;
    NTSTATUS st;
    UINT64 size;
    void *base;
    BYTE *content;
    char args[300];
    int code;

    if (argc >= 3 && argv[1][0] == '-') return child_main(argv[1][1], argv[2]);

    load();
    setup_device();
    test_validation();
    test_same_process();

    /* global share handle, across processes */
    st = create_resource(1, FALSE, &res, &alloc, &global);
    CHECK_ST(st, STATUS_SUCCESS, "create global");
    content = map_alloc(alloc, &size, &base);
    if (content)
    {
        memcpy(content, "FROMPAR!", 8); fill(content, CONTENT);
        snprintf(args, sizeof(args), "-g %08x", global);
        code = run_child(args, FALSE, out, sizeof(out));
        printf("%s", out);
        CHECK(code == 0, "global child exit %d", code);
        CHECK(!memcmp(content, "FROMKID!", 8) && check_fill(content, CONTENT), "the child's write is not seen");
        UnmapViewOfFile(base);
    }

    /* NT handle, inherited */
    {
        D3DKMT_HANDLE hs[1];
        OBJECT_ATTRIBUTES attr;
        HANDLE nt = NULL;

        st = create_resource(3, TRUE, &res, &alloc, &global);
        CHECK_ST(st, STATUS_SUCCESS, "create NT resource (info2)");
        CHECK(!global, "an NT resource has no global handle");
        content = map_alloc(alloc, &size, &base);
        if (content) { memcpy(content, "FROMPAR!", 8); fill(content, CONTENT); }
        hs[0] = res;
        InitializeObjectAttributes(&attr, NULL, OBJ_INHERIT, 0, NULL);
        st = pD3DKMTShareObjects(1, hs, &attr, GENERIC_ALL, &nt);
        CHECK_ST(st, STATUS_SUCCESS, "ShareObjects");
        CHECK(nt != NULL, "no handle");
        snprintf(args, sizeof(args), "-n %lx", (unsigned long)(ULONG_PTR)nt);
        code = run_child(args, TRUE, out, sizeof(out));
        printf("%s", out);
        CHECK(code == 0, "NT child exit %d", code);
        if (content)
        {
            CHECK(!memcmp(content, "FROMKID!", 8) && check_fill(content, CONTENT), "NT child's write is not seen");
            UnmapViewOfFile(base);
        }
        CloseHandle(nt);
    }

    /* NT handle with a name, opened by name */
    {
        D3DKMT_HANDLE hs[1];
        OBJECT_ATTRIBUTES attr;
        UNICODE_STRING us;
        HANDLE nt = NULL, nt2 = NULL;
        WCHAR name[128];
        char name_a[128];
        DWORD session = 0;

        ProcessIdToSessionId(GetCurrentProcessId(), &session);
        snprintf(name_a, sizeof(name_a), "\\Sessions\\%lu\\BaseNamedObjects\\GSharedKmt%lu", (unsigned long)session,
                 (unsigned long)GetCurrentProcessId());
        MultiByteToWideChar(CP_ACP, 0, name_a, -1, name, 128);
        pRtlInitUnicodeString(&us, name);
        st = create_resource(3, FALSE, &res, &alloc, &global);
        CHECK_ST(st, STATUS_SUCCESS, "create named NT resource");
        content = map_alloc(alloc, &size, &base);
        if (content) { memcpy(content, "FROMPAR!", 8); fill(content, CONTENT); }
        UnmapViewOfFile(base);   /* the name is given after the first mapping: the content moves with the block */
        hs[0] = res;
        InitializeObjectAttributes(&attr, &us, 0, 0, NULL);
        st = pD3DKMTShareObjects(1, hs, &attr, GENERIC_ALL, &nt);
        CHECK_ST(st, STATUS_SUCCESS, "ShareObjects with a name");
        st = pD3DKMTShareObjects(1, hs, &attr, GENERIC_ALL, &nt2);
        CHECK(st != STATUS_SUCCESS || nt2 != NULL, "second ShareObjects");
        if (nt2) CloseHandle(nt2);
        snprintf(args, sizeof(args), "-s %s", name_a);
        code = run_child(args, FALSE, out, sizeof(out));
        printf("%s", out);
        CHECK(code == 0, "named child exit %d", code);
        content = map_alloc(alloc, &size, &base);
        if (content)
        {
            CHECK(!memcmp(content, "FROMKID!", 8) && check_fill(content, CONTENT), "named child's write is not seen");
            UnmapViewOfFile(base);
        }
        CloseHandle(nt);
    }

    printf("checks %d failures %d\n", checks, failures);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
