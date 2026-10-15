/* netcfgx: components, classes and the system-wide write lock */
#define COBJMACROS
#include <windows.h>
#include <netcfgx.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const CLSID clsid_netcfg = {0x5b035261,0x40f9,0x11d1,{0xaa,0xec,0x00,0x80,0x5f,0xc1,0x27,0x0e}};
static const GUID guid_nettrans = {0x4d36e975,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
static const GUID guid_net = {0x4d36e972,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
static const GUID IID_INetCfg_ = {0xc0e8ae93,0x306e,0x11d1,{0xaa,0xcf,0x00,0x80,0x5f,0xc1,0x27,0x0e}};
static const GUID IID_INetCfgLock_ = {0xc0e8ae9f,0x306e,0x11d1,{0xaa,0xcf,0x00,0x80,0x5f,0xc1,0x27,0x0e}};
static const GUID IID_INetCfgClass_ = {0xc0e8ae97,0x306e,0x11d1,{0xaa,0xcf,0x00,0x80,0x5f,0xc1,0x27,0x0e}};

#define NOT_INITIALIZED   ((HRESULT)0x8004A021)
#define ALREADY_INIT      ((HRESULT)0x8004A020)
#define NO_WRITE_LOCK     ((HRESULT)0x8004A024)

int main(void)
{
    INetCfg *cfg = NULL, *cfg2 = NULL;
    INetCfgLock *lock = NULL, *lock2 = NULL;
    INetCfgComponent *comp = NULL;
    IEnumNetCfgComponent *en = NULL, *en2 = NULL;
    INetCfgClass *cls = NULL;
    WCHAR *s = NULL, *holder = NULL;
    GUID g;
    DWORD chars, i;
    ULONG fetched;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid_netcfg, NULL, CLSCTX_ALL, &IID_INetCfg_, (void **)&cfg);
    CHECK(hr == S_OK, "create %08lx", hr);
    if (hr != S_OK) goto end;
    INetCfg_QueryInterface(cfg, &IID_INetCfgLock_, (void **)&lock);

    /* state errors before Initialize */
    hr = INetCfg_Apply(cfg);
    CHECK(hr == NOT_INITIALIZED, "Apply before Initialize %08lx", hr);
    hr = INetCfg_Uninitialize(cfg);
    CHECK(hr == NOT_INITIALIZED, "Uninitialize before Initialize %08lx", hr);
    hr = INetCfg_FindComponent(cfg, L"ms_tcpip", &comp);
    CHECK(hr == NOT_INITIALIZED, "FindComponent before Initialize %08lx", hr);

    hr = INetCfgLock_IsWriteLocked(lock, &holder);
    CHECK(hr == S_FALSE && !holder, "not locked: %08lx", hr);

    /* the write lock, between two configuration objects */
    hr = INetCfgLock_AcquireWriteLock(lock, 1000, L"first client", &holder);
    CHECK(hr == S_OK, "AcquireWriteLock %08lx", hr);
    CHECK(INetCfgLock_IsWriteLocked(lock, &holder) == S_OK && holder && !wcscmp(holder, L"first client"), "holder %ls", holder ? holder : L"(null)");
    CoTaskMemFree(holder); holder = NULL;

    CoCreateInstance(&clsid_netcfg, NULL, CLSCTX_ALL, &IID_INetCfg_, (void **)&cfg2);
    INetCfg_QueryInterface(cfg2, &IID_INetCfgLock_, (void **)&lock2);
    hr = INetCfgLock_AcquireWriteLock(lock2, 100, L"second client", &holder);
    CHECK(hr == S_FALSE, "second client waits out: %08lx", hr);
    CHECK(holder && !wcscmp(holder, L"first client"), "second client is told who holds it: %ls", holder ? holder : L"(null)");
    CoTaskMemFree(holder); holder = NULL;
    CHECK(INetCfgLock_IsWriteLocked(lock2, &holder) == S_OK && holder && !wcscmp(holder, L"first client"), "IsWriteLocked from the other object");
    CoTaskMemFree(holder); holder = NULL;

    hr = INetCfg_Initialize(cfg, NULL);
    CHECK(hr == S_OK, "Initialize %08lx", hr);
    hr = INetCfg_Initialize(cfg, NULL);
    CHECK(hr == ALREADY_INIT, "Initialize twice %08lx", hr);
    hr = INetCfgLock_AcquireWriteLock(lock, 100, L"late", &holder);
    CHECK(hr == ALREADY_INIT, "AcquireWriteLock after Initialize %08lx", hr);
    hr = INetCfg_Apply(cfg);
    CHECK(hr == S_OK, "Apply with the lock %08lx", hr);
    hr = INetCfg_Cancel(cfg);
    CHECK(hr == S_OK, "Cancel with the lock %08lx", hr);

    /* the second object, which has no lock */
    INetCfg_Initialize(cfg2, NULL);
    CHECK(INetCfg_Apply(cfg2) == NO_WRITE_LOCK, "Apply without the lock");

    /* components */
    hr = INetCfg_FindComponent(cfg, L"MS_TCPIP", &comp);
    CHECK(hr == S_OK && comp, "FindComponent %08lx", hr);
    if (comp)
    {
        hr = INetCfgComponent_GetId(comp, &s);
        CHECK(hr == S_OK && s && !wcsicmp(s, L"ms_tcpip"), "id %ls", s ? s : L"(null)");
        CoTaskMemFree(s); s = NULL;
        hr = INetCfgComponent_GetDisplayName(comp, &s);
        CHECK(hr == S_OK && s && wcsstr(s, L"TCP/IPv4"), "name %ls", s ? s : L"(null)");
        CoTaskMemFree(s); s = NULL;
        hr = INetCfgComponent_GetBindName(comp, &s);
        CHECK(hr == S_OK && s && !wcscmp(s, L"Tcpip"), "bind name %ls", s ? s : L"(null)");
        CoTaskMemFree(s); s = NULL;
        hr = INetCfgComponent_GetClassGuid(comp, &g);
        CHECK(hr == S_OK && IsEqualGUID(&g, &guid_nettrans), "class guid");
        hr = INetCfgComponent_GetCharacteristics(comp, &chars);
        CHECK(hr == S_OK && (chars & NCF_NDIS_PROTOCOL) && !(chars & NCF_PHYSICAL), "characteristics %#lx", chars);
        s = (WCHAR *)1;
        hr = INetCfgComponent_GetPnpDevNodeId(comp, &s);
        CHECK(hr == S_FALSE && !s, "software component has no devnode: %08lx", hr);
        INetCfgComponent_Release(comp);
    }
    hr = INetCfg_FindComponent(cfg, L"ms_nonexistent", &comp);
    CHECK(hr == S_FALSE && !comp, "unknown component %08lx", hr);

    hr = INetCfg_EnumComponents(cfg, &guid_nettrans, &en);
    CHECK(hr == S_OK && en, "EnumComponents %08lx", hr);
    if (en)
    {
        INetCfgComponent *list[4] = {0};
        hr = IEnumNetCfgComponent_Next(en, 4, list, &fetched);
        CHECK(hr == S_FALSE && fetched == 2, "enum next: %08lx fetched %lu", hr, fetched);
        for (i = 0; i < fetched; i++) INetCfgComponent_Release(list[i]);
        hr = IEnumNetCfgComponent_Next(en, 1, list, &fetched);
        CHECK(hr == S_FALSE && fetched == 0, "enum exhausted");
        IEnumNetCfgComponent_Reset(en);
        IEnumNetCfgComponent_Skip(en, 1);
        IEnumNetCfgComponent_Clone(en, &en2);
        hr = IEnumNetCfgComponent_Next(en2, 1, list, &fetched);
        CHECK(hr == S_OK && fetched == 1, "clone continues after the skip");
        if (fetched)
        {
            INetCfgComponent_GetId(list[0], &s);
            CHECK(s && !wcsicmp(s, L"ms_tcpip6"), "second component %ls", s ? s : L"(null)");
            CoTaskMemFree(s); s = NULL;
            INetCfgComponent_Release(list[0]);
        }
        IEnumNetCfgComponent_Release(en2);
        IEnumNetCfgComponent_Release(en);
    }
    hr = INetCfg_EnumComponents(cfg, &guid_net, &en);
    CHECK(hr == S_OK && en, "EnumComponents NET");
    if (en)
    {
        INetCfgComponent *c = NULL;
        hr = IEnumNetCfgComponent_Next(en, 1, &c, &fetched);
        CHECK(hr == S_FALSE && fetched == 0, "no adapters listed");
        IEnumNetCfgComponent_Release(en);
    }

    hr = INetCfg_QueryNetCfgClass(cfg, &guid_nettrans, &IID_INetCfgClass_, (void **)&cls);
    CHECK(hr == S_OK && cls, "QueryNetCfgClass %08lx", hr);
    if (cls)
    {
        hr = INetCfgClass_FindComponent(cls, L"ms_tcpip", &comp);
        CHECK(hr == S_OK && comp, "class FindComponent");
        if (comp) INetCfgComponent_Release(comp);
        hr = INetCfgClass_FindComponent(cls, L"ms_msclient", &comp);
        CHECK(hr == S_FALSE, "class FindComponent unknown");
        INetCfgClass_Release(cls);
    }

    hr = INetCfg_Uninitialize(cfg);
    CHECK(hr == S_OK, "Uninitialize %08lx", hr);
    hr = INetCfgLock_ReleaseWriteLock(lock);
    CHECK(hr == S_OK, "ReleaseWriteLock %08lx", hr);
    CHECK(INetCfgLock_IsWriteLocked(lock2, &holder) == S_FALSE, "unlocked after release");
    INetCfg_Uninitialize(cfg2);
    hr = INetCfgLock_AcquireWriteLock(lock2, 100, L"second client", &holder);
    CHECK(hr == S_OK, "second client gets the lock now: %08lx", hr);

    INetCfgLock_Release(lock2);
    INetCfg_Release(cfg2);
    INetCfgLock_Release(lock);
    INetCfg_Release(cfg);
end:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
