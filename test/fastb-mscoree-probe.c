/* mscoree hosting state calls (patch 3597): ICLRRuntimeInfo state, loaded runtime
 * enumeration, GetVersionFromProcess, ICLRRuntimeHost/ICorRuntimeHost domain calls.
 * Raw vtable slots (metahost.h is not in mingw). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { void **vtbl; } Obj;
#define SLOT(o, i) ((o)->vtbl[i])
#define CALL(o, i, ret, ...) ((ret (__stdcall *)(Obj*, ##__VA_ARGS__))SLOT(o, i))

static const GUID CLSID_MetaHost = {0x9280188d,0x0e8e,0x4867,{0xb3,0x0c,0x7f,0xa8,0x38,0x84,0xe8,0xde}};
static const GUID IID_MetaHost   = {0xd332db9e,0xb9b3,0x4125,{0x82,0x07,0xa1,0x48,0x84,0xf5,0x32,0x16}};
static const GUID IID_RTInfo     = {0xbd39d1d2,0xba2f,0x486a,{0x89,0xb0,0xb4,0xb0,0xcb,0x46,0x68,0x91}};
static const GUID CLSID_CLRHost  = {0x90f1a06e,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_CLRHost    = {0x90f1a06c,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID CLSID_CorHost  = {0xcb2f6723,0xab3a,0x11d2,{0x9c,0x40,0x00,0xc0,0x4f,0xa3,0x0a,0x3e}};
static const GUID IID_CorHost    = {0xcb2f6722,0xab3a,0x11d2,{0x9c,0x40,0x00,0xc0,0x4f,0xa3,0x0a,0x3e}};

#define E_SHIM_RUNTIMELOAD ((HRESULT)0x80131700)
#define E_SHIM_ALREADYBOUND ((HRESULT)0x80131704)
#define E_HOST_INVALIDOP ((HRESULT)0x80131022)
#define E_APPDOMAINUNLOADED ((HRESULT)0x80131014)

static int cb_calls;
static HRESULT __stdcall domain_cb(void *cookie)
{
    cb_calls++;
    return (HRESULT)(LONG_PTR)cookie;
}

static int count_enum(Obj *en)
{
    int n = 0;
    for (;;)
    {
        Obj *u = NULL; ULONG f = 0;
        HRESULT hr = CALL(en, 3, HRESULT, ULONG, Obj**, ULONG*)(en, 1, &u, &f);
        if (hr != S_OK) { CHECK(hr == S_FALSE && f == 0, "Next end hr=%lx f=%lu", hr, f); break; }
        n++;
        CALL(u, 2, ULONG)(u);
    }
    return n;
}


static void test_config_stream(HMODULE mscoree)
{
    HRESULT (WINAPI *pCreateConfigStream)(const WCHAR*, Obj**) = (void *)GetProcAddress(mscoree, "CreateConfigStream");
    WCHAR path[MAX_PATH];
    Obj *st = NULL, *mem = NULL;
    LARGE_INTEGER mv;
    ULARGE_INTEGER np, rd, wr, sz;
    STATSTG stat;
    char data[16];
    ULONG got;
    HRESULT hr;
    HANDLE f;
    HGLOBAL hg;
    char *p;

    if (!pCreateConfigStream) return;
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sgcfgstream.config");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "0123456789abcdefghij", 20, &got, NULL);
    CloseHandle(f);

    hr = pCreateConfigStream(path, &st);
    CHECK(hr == S_OK && st, "CreateConfigStream %lx", hr);
    if (!st) { DeleteFileW(path); return; }
    mv.QuadPart = 5; np.QuadPart = 99;
    hr = CALL(st, 5, HRESULT, LARGE_INTEGER, DWORD, ULARGE_INTEGER*)(st, mv, STREAM_SEEK_SET, &np);
    CHECK(hr == S_OK && np.QuadPart == 5, "Seek set %lx %I64u", hr, np.QuadPart);
    got = 0;
    hr = CALL(st, 3, HRESULT, void*, ULONG, ULONG*)(st, data, 4, &got);
    CHECK(hr == S_OK && got == 4 && !memcmp(data, "5678", 4), "Read after seek");
    mv.QuadPart = 2;
    hr = CALL(st, 5, HRESULT, LARGE_INTEGER, DWORD, ULARGE_INTEGER*)(st, mv, STREAM_SEEK_CUR, &np);
    CHECK(hr == S_OK && np.QuadPart == 11, "Seek cur %lx %I64u", hr, np.QuadPart);
    mv.QuadPart = -3;
    hr = CALL(st, 5, HRESULT, LARGE_INTEGER, DWORD, ULARGE_INTEGER*)(st, mv, STREAM_SEEK_END, &np);
    CHECK(hr == S_OK && np.QuadPart == 17, "Seek end %lx %I64u", hr, np.QuadPart);
    hr = CALL(st, 5, HRESULT, LARGE_INTEGER, DWORD, ULARGE_INTEGER*)(st, mv, 7, &np);
    CHECK(hr == 0x80030001, "Seek bad origin %lx", hr);
    memset(&stat, 0xcc, sizeof(stat));
    hr = CALL(st, 12, HRESULT, STATSTG*, DWORD)(st, &stat, STATFLAG_NONAME);
    CHECK(hr == S_OK && stat.type == STGTY_STREAM && stat.cbSize.QuadPart == 20, "Stat %lx type=%lu size=%I64u", hr, stat.type, stat.cbSize.QuadPart);
    mv.QuadPart = 10;
    CALL(st, 5, HRESULT, LARGE_INTEGER, DWORD, ULARGE_INTEGER*)(st, mv, STREAM_SEEK_SET, &np);
    hg = GlobalAlloc(GMEM_MOVEABLE, 0);
    CreateStreamOnHGlobal(hg, TRUE, (IStream **)&mem);
    sz.QuadPart = 6;
    hr = CALL(st, 7, HRESULT, void*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*)(st, mem, sz, &rd, &wr);
    CHECK(hr == S_OK && rd.QuadPart == 6 && wr.QuadPart == 6, "CopyTo %lx %I64u %I64u", hr, rd.QuadPart, wr.QuadPart);
    p = GlobalLock(hg);
    CHECK(p && !memcmp(p, "abcdef", 6), "CopyTo data");
    GlobalUnlock(hg);
    sz.QuadPart = 100;
    hr = CALL(st, 7, HRESULT, void*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*)(st, mem, sz, &rd, &wr);
    CHECK(hr == S_OK && rd.QuadPart == 4, "CopyTo to end read=%I64u", rd.QuadPart);
    CHECK(CALL(st, 8, HRESULT, DWORD)(st, 0) == S_OK, "Commit");
    {
        ULARGE_INTEGER z = {{0}}, one = {{1}};
        Obj *clone = NULL;
        char b1[2], b2[2];
        ULONG g1 = 0, g2 = 0;

        CHECK(CALL(st, 6, HRESULT, ULARGE_INTEGER)(st, z) == 0x80030005, "SetSize read-only");
        CHECK(CALL(st, 9, HRESULT)(st) == S_OK, "Revert");
        CHECK(CALL(st, 10, HRESULT, ULARGE_INTEGER, ULARGE_INTEGER, DWORD)(st, z, one, 1) == 0x80030001, "LockRegion");
        CHECK(CALL(st, 11, HRESULT, ULARGE_INTEGER, ULARGE_INTEGER, DWORD)(st, z, one, 1) == 0x80030001, "UnlockRegion");
        mv.QuadPart = 3;
        CALL(st, 5, HRESULT, LARGE_INTEGER, DWORD, ULARGE_INTEGER*)(st, mv, STREAM_SEEK_SET, &np);
        hr = CALL(st, 13, HRESULT, Obj**)(st, &clone);
        CHECK(hr == S_OK && clone, "Clone %lx", hr);
        if (clone)
        {
            /* independent seek pointers, both starting at 3 */
            CALL(clone, 3, HRESULT, void*, ULONG, ULONG*)(clone, b1, 2, &g1);
            CALL(clone, 3, HRESULT, void*, ULONG, ULONG*)(clone, b1, 2, &g1);
            CALL(st, 3, HRESULT, void*, ULONG, ULONG*)(st, b2, 2, &g2);
            CHECK(g1 == 2 && g2 == 2 && !memcmp(b2, "34", 2) && !memcmp(b1, "56", 2), "Clone reads %.2s %.2s", b1, b2);
            CALL(clone, 2, ULONG)(clone);
        }
    }
    CALL(mem, 2, ULONG)(mem);
    CALL(st, 2, ULONG)(st);
    DeleteFileW(path);
}

int main(void)
{
    HMODULE mscoree = LoadLibraryA("mscoree.dll");
    HRESULT (WINAPI *pCLRCreateInstance)(const GUID*, const GUID*, void**);
    HRESULT (WINAPI *pGetVersionFromProcess)(HANDLE, LPWSTR, DWORD, DWORD*);
    Obj *host = NULL, *v4 = NULL, *v2 = NULL, *clr = NULL, *cor = NULL, *en = NULL;
    HANDLE proc = GetCurrentProcess();
    WCHAR buf[64];
    DWORD len, flags, domid = 0;
    BOOL b;
    HRESULT hr;

    if (!mscoree) { printf("SKIP mscoree not loadable\nRESULT: PASS\n"); return 0; }
    pCLRCreateInstance = (void *)GetProcAddress(mscoree, "CLRCreateInstance");
    pGetVersionFromProcess = (void *)GetProcAddress(mscoree, "GetVersionFromProcess");
    test_config_stream(mscoree);
    hr = pCLRCreateInstance(&CLSID_MetaHost, &IID_MetaHost, (void **)&host);
    CHECK(hr == S_OK, "CLRCreateInstance %lx", hr);
    hr = CALL(host, 3, HRESULT, const WCHAR*, const GUID*, void**)(host, L"v4.0.30319", &IID_RTInfo, (void **)&v4);
    CHECK(hr == S_OK, "GetRuntime v4 %lx", hr);
    hr = CALL(host, 3, HRESULT, const WCHAR*, const GUID*, void**)(host, L"v2.0.50727", &IID_RTInfo, (void **)&v2);
    CHECK(hr == S_OK, "GetRuntime v2 %lx", hr);
    if (!v4 || !v2) { printf("RESULT: FAIL\n"); return 1; }

    /* nothing loaded yet */
    b = TRUE;
    hr = CALL(v4, 5, HRESULT, HANDLE, BOOL*)(v4, proc, &b);
    CHECK(hr == S_OK && !b, "IsLoaded before: %lx %d", hr, b);
    hr = CALL(v4, 5, HRESULT, HANDLE, BOOL*)(v4, proc, NULL);
    CHECK(hr == E_POINTER, "IsLoaded NULL out %lx", hr);
    hr = CALL(host, 6, HRESULT, HANDLE, Obj**)(host, proc, &en);
    CHECK(hr == S_OK && en && count_enum(en) == 0, "EnumerateLoadedRuntimes empty %lx", hr);
    if (en) CALL(en, 2, ULONG)(en);
    len = 64;
    hr = pGetVersionFromProcess(proc, buf, 64, &len);
    CHECK(hr == E_SHIM_RUNTIMELOAD, "GetVersionFromProcess none: %lx", hr);

    /* installed runtimes still list all four */
    en = NULL;
    hr = CALL(host, 5, HRESULT, Obj**)(host, &en);
    CHECK(hr == S_OK && count_enum(en) == 4, "installed runtimes");
    if (en) CALL(en, 2, ULONG)(en);

    b = FALSE;
    hr = CALL(v4, 10, HRESULT, BOOL*)(v4, &b);
    CHECK(hr == S_OK, "IsLoadable %lx", hr);
    if (hr == S_OK && !b) { printf("SKIP no Mono installed\nRESULT: PASS\n"); return 0; }
    CHECK(CALL(v4, 10, HRESULT, BOOL*)(v4, NULL) == E_POINTER, "IsLoadable NULL");

    /* startup flags */
    flags = 0xdead; len = 0;
    hr = CALL(v4, 12, HRESULT, DWORD*, WCHAR*, DWORD*)(v4, &flags, NULL, &len);
    CHECK(hr == S_OK && flags == 0 && len == 1, "Get flags default %lx f=%lx len=%lu", hr, flags, len);
    hr = CALL(v4, 11, HRESULT, DWORD, const WCHAR*)(v4, 0x5, L"C:\\host.config");
    CHECK(hr == S_OK, "Set flags %lx", hr);
    len = 3;
    hr = CALL(v4, 12, HRESULT, DWORD*, WCHAR*, DWORD*)(v4, &flags, buf, &len);
    CHECK(hr == E_NOT_SUFFICIENT_BUFFER && len == 15 && flags == 5, "Get flags small buffer %lx len=%lu f=%lx", hr, len, flags);
    len = 64;
    hr = CALL(v4, 12, HRESULT, DWORD*, WCHAR*, DWORD*)(v4, &flags, buf, &len);
    CHECK(hr == S_OK && len == 15 && !wcscmp(buf, L"C:\\host.config"), "Get flags %lx len=%lu", hr, len);
    hr = CALL(v4, 11, HRESULT, DWORD, const WCHAR*)(v4, 0x5, NULL);
    len = 64;
    CALL(v4, 12, HRESULT, DWORD*, WCHAR*, DWORD*)(v4, &flags, buf, &len);
    CHECK(len == 1 && buf[0] == 0, "config cleared len=%lu", len);
    CHECK(CALL(v4, 12, HRESULT, DWORD*, WCHAR*, DWORD*)(v4, NULL, buf, &len) == E_POINTER, "Get flags NULL");

    /* legacy binding */
    CHECK(CALL(v2, 13, HRESULT)(v2) == S_OK, "bind v2");
    CHECK(CALL(v2, 13, HRESULT)(v2) == S_OK, "bind v2 again");
    hr = CALL(v4, 13, HRESULT)(v4);
    CHECK(hr == E_SHIM_ALREADYBOUND, "bind v4 after v2: %lx", hr);

    /* load the runtime */
    b = TRUE; flags = 0xdead;
    hr = CALL(v4, 14, HRESULT, BOOL*, DWORD*)(v4, &b, &flags);
    CHECK(hr == S_OK && !b && flags == 0, "IsStarted before %lx", hr);
    hr = CALL(v4, 9, HRESULT, const GUID*, const GUID*, void**)(v4, &CLSID_CLRHost, &IID_CLRHost, (void **)&clr);
    if (hr != S_OK || !clr) { printf("SKIP runtime did not load (%lx)\n", hr); printf("RESULT: %s\n", fails ? "FAIL" : "PASS"); return !!fails; }
    b = FALSE;
    hr = CALL(v4, 5, HRESULT, HANDLE, BOOL*)(v4, proc, &b);
    CHECK(hr == S_OK && b, "IsLoaded after %lx %d", hr, b);
    b = TRUE;
    CALL(v2, 5, HRESULT, HANDLE, BOOL*)(v2, proc, &b);
    CHECK(!b, "v2 not loaded");
    en = NULL;
    hr = CALL(host, 6, HRESULT, HANDLE, Obj**)(host, proc, &en);
    CHECK(hr == S_OK && count_enum(en) == 1, "EnumerateLoadedRuntimes one");
    if (en) CALL(en, 2, ULONG)(en);
    len = 0;
    hr = pGetVersionFromProcess(proc, NULL, 0, &len);
    CHECK(hr == S_OK && len == 11, "GetVersionFromProcess size %lx %lu", hr, len);
    len = 64;
    hr = pGetVersionFromProcess(proc, buf, 64, &len);
    CHECK(hr == S_OK && !wcscmp(buf, L"v4.0.30319"), "GetVersionFromProcess %lx %ls", hr, buf);

    hr = CALL(clr, 3, HRESULT)(clr);
    CHECK(hr == S_OK, "Start %lx", hr);
    b = FALSE;
    hr = CALL(v4, 14, HRESULT, BOOL*, DWORD*)(v4, &b, &flags);
    CHECK(hr == S_OK && b, "IsStarted after %lx", hr);
    hr = CALL(v4, 11, HRESULT, DWORD, const WCHAR*)(v4, 1, NULL);
    CHECK(hr == E_HOST_INVALIDOP, "Set flags after start %lx", hr);

    /* ICLRRuntimeHost domain calls (ids are the runtime's own) */
    len = 0xffff;
    hr = CALL(clr, 9, HRESULT, DWORD*)(clr, &len);
    CHECK(hr == S_OK && len != 0xffff, "GetCurrentAppDomainId %lx %lu", hr, len);
    domid = len;
    CHECK(CALL(clr, 9, HRESULT, DWORD*)(clr, NULL) == E_POINTER, "GetCurrentAppDomainId NULL");
    cb_calls = 0;
    hr = CALL(clr, 8, HRESULT, DWORD, void*, void*)(clr, domid, (void *)domain_cb, (void *)(LONG_PTR)0x12345);
    CHECK(hr == 0x12345 && cb_calls == 1, "ExecuteInAppDomain %lx calls=%d", hr, cb_calls);
    hr = CALL(clr, 8, HRESULT, DWORD, void*, void*)(clr, 9999, (void *)domain_cb, NULL);
    CHECK(hr == E_APPDOMAINUNLOADED && cb_calls == 1, "ExecuteInAppDomain bad id %lx", hr);
    CHECK(CALL(clr, 8, HRESULT, DWORD, void*, void*)(clr, domid, NULL, NULL) == E_POINTER, "ExecuteInAppDomain NULL cb");

    /* ICorRuntimeHost */
    hr = CALL(v4, 9, HRESULT, const GUID*, const GUID*, void**)(v4, &CLSID_CorHost, &IID_CorHost, (void **)&cor);
    CHECK(hr == S_OK && cor, "GetInterface CorRuntimeHost %lx", hr);
    if (cor)
    {
        Obj *unk = NULL, *dom = NULL;
        void *henum = NULL;
        int n = 0;
        hr = CALL(cor, 21, HRESULT, Obj**)(cor, &unk);
        CHECK(hr == S_OK && unk, "CurrentDomain %lx", hr);
        if (unk) CALL(unk, 2, ULONG)(unk);
        unk = NULL;
        hr = CALL(cor, 19, HRESULT, Obj**)(cor, &unk);
        CHECK(hr == S_OK && unk, "CreateEvidence %lx", hr);
        if (unk) CALL(unk, 2, ULONG)(unk);
        hr = CALL(cor, 14, HRESULT, void**)(cor, &henum);
        CHECK(hr == S_OK && henum, "EnumDomains %lx", hr);
        for (;;)
        {
            dom = NULL;
            hr = CALL(cor, 15, HRESULT, void*, Obj**)(cor, henum, &dom);
            if (hr != S_OK) { CHECK(hr == S_FALSE && !dom, "NextDomain end %lx", hr); break; }
            n++;
            CALL(dom, 2, ULONG)(dom);
        }
        CHECK(n >= 1, "domains %d", n);
        CHECK(CALL(cor, 16, HRESULT, void*)(cor, henum) == S_OK, "CloseEnum");
        CHECK(CALL(cor, 16, HRESULT, void*)(cor, NULL) == E_POINTER, "CloseEnum NULL");
        CHECK(CALL(cor, 11, HRESULT)(cor) == S_OK, "ICorRuntimeHost::Stop");
    }
    CHECK(CALL(clr, 4, HRESULT)(clr) == S_OK, "ICLRRuntimeHost::Stop");

    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
