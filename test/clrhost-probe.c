/* clrhost gate probe: a native program that hosts the runtime (as Visual Studio's devenv does)
 *  - a RequestRuntimeLoadedNotification callback that calls back into the runtime it is told about
 *    (GetInterface) is called once and that call works (it re-entered the runtime load for ever)
 *  - the default domain of a hosting program binds with the program's configuration: <probing
 *    privatePath>, <bindingRedirect> and <codeBase> */
#include <windows.h>
#include <stdio.h>

static const GUID CLSID_CLRMetaHost_ = {0x9280188d,0x0e8e,0x4867,{0xb3,0x0c,0x7f,0xa8,0x38,0x84,0xe8,0xde}};
static const GUID IID_ICLRMetaHost_ = {0xd332db9e,0xb9b3,0x4125,{0x82,0x07,0xa1,0x48,0x84,0xf5,0x32,0x16}};
static const GUID IID_ICLRRuntimeInfo_ = {0xbd39d1d2,0xba2f,0x486a,{0x89,0xb0,0xb4,0xb0,0xcb,0x46,0x68,0x91}};
static const GUID CLSID_CLRRuntimeHost_ = {0x90f1a06e,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_ICLRRuntimeHost_ = {0x90f1a06c,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};

typedef HRESULT (WINAPI *thread_fn)(void);
typedef struct info info;
typedef void (WINAPI *loaded_cb)(info *, thread_fn, thread_fn);
typedef struct { void *qi, *addref; ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *GetRuntime)(void *, const WCHAR *, const GUID *, void **);
    void *GetVersionFromFile, *EnumerateInstalledRuntimes, *EnumerateLoadedRuntimes;
    HRESULT (WINAPI *RequestRuntimeLoadedNotification)(void *, loaded_cb); } metahost_vtbl;
typedef struct { metahost_vtbl *lpVtbl; } metahost;
typedef struct { void *qi, *addref, *release, *GetVersionString, *GetRuntimeDirectory, *IsLoaded, *LoadErrorString,
    *LoadLibrary_, *GetProcAddress_;
    HRESULT (WINAPI *GetInterface)(info *, const GUID *, const GUID *, void **); } info_vtbl;
struct info { info_vtbl *lpVtbl; };
typedef struct { void *qi, *addref; ULONG (WINAPI *Release)(void *); HRESULT (WINAPI *Start)(void *);
    void *Stop, *SetHostControl, *GetCLRControl, *UnloadAppDomain, *ExecuteInAppDomain, *GetCurrentAppDomainId,
    *ExecuteApplication;
    HRESULT (WINAPI *ExecuteInDefaultAppDomain)(void *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, DWORD *); } host_vtbl;
typedef struct { host_vtbl *lpVtbl; } host;

static int calls, fails;
static HRESULT nested_hr = E_FAIL;

static void WINAPI on_loaded(info *runtime, thread_fn set, thread_fn unset)
{
    host *h = NULL;
    calls++;
    if (calls > 1) return;
    set();
    nested_hr = runtime->lpVtbl->GetInterface(runtime, &CLSID_CLRRuntimeHost_, &IID_ICLRRuntimeHost_, (void **)&h);
    unset();
}

int main(int argc, char **argv)
{
    HRESULT (WINAPI *create)(const GUID *, const GUID *, void **);
    HMODULE mscoree = LoadLibraryA("mscoree.dll");
    metahost *mh = NULL;
    info *rt = NULL;
    host *h = NULL;
    DWORD ret = 0;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    create = (void *)GetProcAddress(mscoree, "CLRCreateInstance");
    if (!create || FAILED(create(&CLSID_CLRMetaHost_, &IID_ICLRMetaHost_, (void **)&mh)))
    { printf("FAIL  no CLRCreateInstance\n"); return 1; }
    hr = mh->lpVtbl->RequestRuntimeLoadedNotification(mh, on_loaded);
    if (hr != S_OK) { printf("FAIL  RequestRuntimeLoadedNotification %#lx\n", hr); fails++; }
    hr = mh->lpVtbl->GetRuntime(mh, L"v4.0.30319", &IID_ICLRRuntimeInfo_, (void **)&rt);
    if (FAILED(hr)) { printf("SKIP no v4 runtime (%#lx)\nRESULT: PASS\n", hr); return 0; }
    hr = rt->lpVtbl->GetInterface(rt, &CLSID_CLRRuntimeHost_, &IID_ICLRRuntimeHost_, (void **)&h);
    if (FAILED(hr)) { printf("FAIL  GetInterface(CLRRuntimeHost) %#lx\n", hr); return 1; }

    if (calls == 1 && nested_hr == S_OK) printf("PASS  the runtime-loaded callback ran once and could get the runtime host\n");
    else { printf("FAIL  runtime-loaded callback: %d calls, nested GetInterface %#lx\n", calls, nested_hr); fails++; }

    hr = h->lpVtbl->Start(h);
    if (FAILED(hr)) { printf("FAIL  Start %#lx\n", hr); fails++; }
    hr = h->lpVtbl->ExecuteInDefaultAppDomain(h, L"C:\\clrhost\\Entry.dll", L"Entry.E", L"Run", L"", &ret);
    if (hr == S_OK && ret == 25)
        printf("PASS  the hosted default domain binds by the host program's configuration (codeBase 2.0 via redirect, privatePath)\n");
    else
    {
        printf("FAIL  ExecuteInDefaultAppDomain hr %#lx ret %lu (want 25: 2 = SgCb from its codeBase, 5 = SgPriv from the privatePath; 15 = the 1.0 decoy)\n", hr, ret);
        fails++;
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(fails ? 1 : 0);
}
