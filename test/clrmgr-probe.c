/* clrmgr gate probe: a native program that hosts the runtime and customizes its default domain the way
 * Visual Studio's devenv does:
 *  - ICLRControl::GetCLRManager hands out ICLROnEventManager (an action can be registered) and
 *    ICLRDomainManager (the AppDomainManager type and the default domain's properties)
 *  - the runtime makes the AppDomainManager when the default domain starts, runs its InitializeNewDomain,
 *    tells the host (IHostControl::SetAppDomainManager), and the domain binds by the configuration file
 *    the manager chose (an assembly only that file's <codeBase> names loads) */
#include <windows.h>
#include <stdio.h>

static const GUID CLSID_CLRMetaHost_ = {0x9280188d,0x0e8e,0x4867,{0xb3,0x0c,0x7f,0xa8,0x38,0x84,0xe8,0xde}};
static const GUID IID_ICLRMetaHost_ = {0xd332db9e,0xb9b3,0x4125,{0x82,0x07,0xa1,0x48,0x84,0xf5,0x32,0x16}};
static const GUID IID_ICLRRuntimeInfo_ = {0xbd39d1d2,0xba2f,0x486a,{0x89,0xb0,0xb4,0xb0,0xcb,0x46,0x68,0x91}};
static const GUID CLSID_CLRRuntimeHost_ = {0x90f1a06e,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_ICLRRuntimeHost_ = {0x90f1a06c,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_ICLROnEventManager_ = {0x1d0e0132,0xe64f,0x493d,{0x92,0x60,0x02,0x5c,0x0e,0x32,0xc1,0x75}};
static const GUID IID_ICLRDomainManager_ = {0x270d00a2,0x8e15,0x4d0b,{0xad,0xeb,0x37,0xbc,0x3e,0x47,0xdf,0x77}};
static const GUID IID_IHostControl_ = {0x02ca073c,0x7079,0x4860,{0x88,0x0a,0xc2,0xf7,0xa4,0x49,0xc9,0x91}};
static const GUID IID_IActionOnCLREvent_ = {0x607be24b,0xd91b,0x4e28,{0xa2,0x42,0x61,0x87,0x1c,0xe5,0x6e,0x35}};

typedef struct { void *qi, *addref; ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *GetRuntime)(void *, const WCHAR *, const GUID *, void **); } metahost_vtbl;
typedef struct { metahost_vtbl *lpVtbl; } metahost;
typedef struct { void *qi, *addref, *release, *GetVersionString, *GetRuntimeDirectory, *IsLoaded, *LoadErrorString,
    *LoadLibrary_, *GetProcAddress_;
    HRESULT (WINAPI *GetInterface)(void *, const GUID *, const GUID *, void **); } info_vtbl;
typedef struct { info_vtbl *lpVtbl; } info;
typedef struct { void *qi, *addref, *release;
    HRESULT (WINAPI *GetCLRManager)(void *, const GUID *, void **);
    HRESULT (WINAPI *SetAppDomainManagerType)(void *, const WCHAR *, const WCHAR *); } control_vtbl;
typedef struct { control_vtbl *lpVtbl; } control;
typedef struct { void *qi, *addref, *release;
    HRESULT (WINAPI *Start)(void *); void *Stop;
    HRESULT (WINAPI *SetHostControl)(void *, void *);
    HRESULT (WINAPI *GetCLRControl)(void *, control **);
    void *UnloadAppDomain, *ExecuteInAppDomain, *GetCurrentAppDomainId, *ExecuteApplication;
    HRESULT (WINAPI *ExecuteInDefaultAppDomain)(void *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, DWORD *); } host_vtbl;
typedef struct { host_vtbl *lpVtbl; } host;
typedef struct { void *qi, *addref, *release;
    HRESULT (WINAPI *RegisterActionOnEvent)(void *, int, void *);
    HRESULT (WINAPI *UnregisterActionOnEvent)(void *, int, void *); } events_vtbl;
typedef struct { events_vtbl *lpVtbl; } events;
typedef struct { void *qi, *addref, *release;
    HRESULT (WINAPI *SetAppDomainManagerType)(void *, const WCHAR *, const WCHAR *, int);
    HRESULT (WINAPI *SetPropertiesForDefaultAppDomain)(void *, DWORD, const WCHAR **, const WCHAR **); } domains_vtbl;
typedef struct { domains_vtbl *lpVtbl; } domains;

/* the host's IHostControl and an IActionOnCLREvent */
static int manager_told; static DWORD manager_domain; static IUnknown *manager_unk;
static HRESULT WINAPI obj_qi(IUnknown *iface, REFIID riid, void **obj);
static ULONG WINAPI obj_addref(IUnknown *iface) { return 2; }
static ULONG WINAPI obj_release(IUnknown *iface) { return 1; }
static HRESULT WINAPI hc_GetHostManager(IUnknown *iface, REFIID riid, void **obj) { *obj = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI hc_SetAppDomainManager(IUnknown *iface, DWORD id, IUnknown *unk)
{
    manager_told++; manager_domain = id;
    if (unk) { manager_unk = unk; unk->lpVtbl->AddRef(unk); }
    return S_OK;
}
static void *hc_vtbl[] = { obj_qi, obj_addref, obj_release, hc_GetHostManager, hc_SetAppDomainManager };
static struct { void **lpVtbl; } host_control = { hc_vtbl };
static HRESULT WINAPI action_OnEvent(IUnknown *iface, int event, void *data) { return S_OK; }
static void *action_vtbl[] = { obj_qi, obj_addref, obj_release, action_OnEvent };
static struct { void **lpVtbl; } action = { action_vtbl };
static HRESULT WINAPI obj_qi(IUnknown *iface, REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || (iface == (IUnknown *)&host_control && IsEqualGUID(riid, &IID_IHostControl_)) ||
        (iface == (IUnknown *)&action && IsEqualGUID(riid, &IID_IActionOnCLREvent_)))
    { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}

int main(int argc, char **argv)
{
    HRESULT (WINAPI *create)(const GUID *, const GUID *, void **);
    HMODULE mscoree = LoadLibraryA("mscoree.dll");
    static const WCHAR *names[] = { L"SG_HOST_PROP" }, *values[] = { L"sg-value" };
    metahost *mh = NULL; info *rt = NULL; host *h = NULL; control *ctl = NULL; events *ev = NULL; domains *dm = NULL;
    int fails = 0; DWORD ret = 0; HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    create = (void *)GetProcAddress(mscoree, "CLRCreateInstance");
    if (!create || FAILED(create(&CLSID_CLRMetaHost_, &IID_ICLRMetaHost_, (void **)&mh))) { printf("FAIL  no CLRCreateInstance\n"); return 1; }
    if (FAILED(hr = mh->lpVtbl->GetRuntime(mh, L"v4.0.30319", &IID_ICLRRuntimeInfo_, (void **)&rt))) { printf("SKIP no v4 runtime (%#lx)\nRESULT: PASS\n", hr); return 0; }
    if (FAILED(hr = rt->lpVtbl->GetInterface(rt, &CLSID_CLRRuntimeHost_, &IID_ICLRRuntimeHost_, (void **)&h))) { printf("FAIL  GetInterface %#lx\n", hr); return 1; }
    if ((hr = h->lpVtbl->SetHostControl(h, &host_control)) != S_OK) { printf("FAIL  SetHostControl %#lx\n", hr); fails++; }
    if ((hr = h->lpVtbl->GetCLRControl(h, &ctl)) != S_OK || !ctl) { printf("FAIL  GetCLRControl %#lx\n", hr); return 1; }

    hr = ctl->lpVtbl->GetCLRManager(ctl, &IID_ICLROnEventManager_, (void **)&ev);
    if (hr == S_OK && ev && ev->lpVtbl->RegisterActionOnEvent(ev, 0 /* Event_DomainUnload */, &action) == S_OK)
        printf("PASS  ICLROnEventManager takes an action\n");
    else { printf("FAIL  ICLROnEventManager: %#lx\n", hr); fails++; }

    hr = ctl->lpVtbl->GetCLRManager(ctl, &IID_ICLRDomainManager_, (void **)&dm);
    if (hr != S_OK || !dm) { printf("FAIL  ICLRDomainManager: %#lx\n", hr); fails++; }
    else
    {
        if ((hr = dm->lpVtbl->SetAppDomainManagerType(dm, L"SgMgr, Version=0.0.0.0, Culture=neutral, PublicKeyToken=null", L"SgMgr.Manager", 2)) != S_OK)
        { printf("FAIL  SetAppDomainManagerType %#lx\n", hr); fails++; }
        if ((hr = dm->lpVtbl->SetPropertiesForDefaultAppDomain(dm, 1, names, values)) != S_OK)
        { printf("FAIL  SetPropertiesForDefaultAppDomain %#lx\n", hr); fails++; }
    }

    if (FAILED(hr = h->lpVtbl->Start(h))) { printf("FAIL  Start %#lx\n", hr); fails++; }
    hr = h->lpVtbl->ExecuteInDefaultAppDomain(h, L"C:\\clrmgr\\Entry2.dll", L"E2.E", L"Run", L"", &ret);
    if (hr == S_OK && ret == 31)
        printf("PASS  the default domain has the host's manager (initialized), its property and the manager's configuration\n");
    else
    {
        printf("FAIL  default domain: hr %#lx result %lu (want 31 = 1 manager + 2 initialized + 4 property + 8 config file + 16 codeBase from it)\n", hr, ret);
        fails++;
    }
    if (manager_told == 1 && manager_unk && manager_domain == 1)
        printf("PASS  the host was told of the manager (IHostControl::SetAppDomainManager)\n");
    else { printf("FAIL  IHostControl::SetAppDomainManager: %d calls, domain %lu, %p\n", manager_told, manager_domain, manager_unk); fails++; }

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(fails ? 1 : 0);
}
