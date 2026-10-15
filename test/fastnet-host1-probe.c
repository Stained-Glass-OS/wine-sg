/* mscoree CLR hosting: ICLRRuntimeHost SetHostControl/GetCLRControl before and after Start, UnloadAppDomain and
 * ICorRuntimeHost::UnloadDomain on real domains (needs Wine Mono). */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>

static int failures, checks;
#define ok(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); } } while (0)

#define HOST_E_INVALIDOPERATION ((HRESULT)0x80131022)
#define COR_E_APPDOMAINUNLOADED ((HRESULT)0x80131014)
#define COR_E_CANNOTUNLOADAPPDOMAIN ((HRESULT)0x80131015)

static const GUID CLSID_CorRuntimeHost_ = {0xcb2f6723,0xab3a,0x11d2,{0x9c,0x40,0x00,0xc0,0x4f,0xa3,0x0a,0x3e}};
static const GUID CLSID_CLRRuntimeHost_ = {0x90f1a06e,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_ICLRRuntimeHost_ = {0x90f1a06c,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_ICorRuntimeHost_ = {0xcb2f6722,0xab3a,0x11d2,{0x9c,0x40,0x00,0xc0,0x4f,0xa3,0x0a,0x3e}};
static const GUID IID_IHostControl_ = {0x02ca073c,0x7079,0x4860,{0x88,0x0a,0xc2,0xf7,0xa4,0x49,0xc9,0x91}};
static const GUID IID_ICLRControl_ = {0x9065597e,0xd1a1,0x4fb2,{0xb6,0xba,0x7e,0x1f,0xce,0x23,0x0f,0x61}};

typedef struct ICLRRuntimeHost ICLRRuntimeHost;
typedef struct { HRESULT (WINAPI *QueryInterface)(ICLRRuntimeHost *, REFIID, void **); ULONG (WINAPI *AddRef)(ICLRRuntimeHost *);
    ULONG (WINAPI *Release)(ICLRRuntimeHost *); HRESULT (WINAPI *Start)(ICLRRuntimeHost *); HRESULT (WINAPI *Stop)(ICLRRuntimeHost *);
    HRESULT (WINAPI *SetHostControl)(ICLRRuntimeHost *, IUnknown *); HRESULT (WINAPI *GetCLRControl)(ICLRRuntimeHost *, IUnknown **);
    HRESULT (WINAPI *UnloadAppDomain)(ICLRRuntimeHost *, DWORD, BOOL); void *ExecuteInAppDomain; void *GetCurrentAppDomainId;
    void *ExecuteApplication; void *ExecuteInDefaultAppDomain; } ICLRRuntimeHostVtbl;
struct ICLRRuntimeHost { const ICLRRuntimeHostVtbl *lpVtbl; };

typedef struct ICorRuntimeHost ICorRuntimeHost;
typedef struct { HRESULT (WINAPI *QueryInterface)(ICorRuntimeHost *, REFIID, void **); ULONG (WINAPI *AddRef)(ICorRuntimeHost *);
    ULONG (WINAPI *Release)(ICorRuntimeHost *); void *slots[6]; void *GetConfiguration; HRESULT (WINAPI *Start)(ICorRuntimeHost *);
    void *Stop; HRESULT (WINAPI *CreateDomain)(ICorRuntimeHost *, LPCWSTR, IUnknown *, IUnknown **);
    HRESULT (WINAPI *GetDefaultDomain)(ICorRuntimeHost *, IUnknown **); void *EnumDomains, *NextDomain, *CloseEnum, *CreateDomainEx,
    *CreateDomainSetup, *CreateEvidence; HRESULT (WINAPI *UnloadDomain)(ICorRuntimeHost *, IUnknown *); } ICorRuntimeHostVtbl;
struct ICorRuntimeHost { const ICorRuntimeHostVtbl *lpVtbl; };

/* a host control that counts its references */
typedef struct { const void *vtbl; LONG ref; } host_control_t;
static HRESULT WINAPI hc_qi(IUnknown *This, REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IHostControl_)) { *obj = This; ((host_control_t *)This)->ref++; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI hc_addref(IUnknown *This) { return ++((host_control_t *)This)->ref; }
static ULONG WINAPI hc_release(IUnknown *This) { return --((host_control_t *)This)->ref; }
static HRESULT WINAPI hc_get_manager(IUnknown *This, REFIID riid, void **obj) { *obj = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI hc_set_manager(IUnknown *This, DWORD id, IUnknown *mgr) { return S_OK; }
static void *hc_vtbl[5] = { hc_qi, hc_addref, hc_release, hc_get_manager, hc_set_manager };

typedef struct { void *qi, *addref; ULONG (WINAPI *release)(IUnknown *); HRESULT (WINAPI *GetCLRManager)(IUnknown *, REFIID, void **);
    HRESULT (WINAPI *SetAppDomainManagerType)(IUnknown *, LPCWSTR, LPCWSTR); } ICLRControlVtbl;

static HRESULT domain_id(IUnknown *unk, LONG *id)
{
    IDispatch *disp;
    DISPID dispid;
    OLECHAR *name = (OLECHAR *)L"Id";
    DISPPARAMS dp = {0};
    VARIANT v;
    HRESULT hr;

    hr = unk->lpVtbl->QueryInterface(unk, &IID_IDispatch, (void **)&disp);
    if (FAILED(hr)) return hr;
    hr = disp->lpVtbl->GetIDsOfNames(disp, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (SUCCEEDED(hr))
    {
        VariantInit(&v);
        hr = disp->lpVtbl->Invoke(disp, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
        if (SUCCEEDED(hr)) { *id = V_I4(&v); VariantClear(&v); }
    }
    disp->lpVtbl->Release(disp);
    return hr;
}

int main(void)
{
    HMODULE mod = LoadLibraryA("mscoree.dll");
    HRESULT (WINAPI *pCorBindToRuntimeEx)(LPCWSTR, LPCWSTR, DWORD, REFCLSID, REFIID, void **) = (void *)GetProcAddress(mod, "CorBindToRuntimeEx");
    ICLRRuntimeHost *host = NULL;
    ICorRuntimeHost *cor = NULL;
    host_control_t hc1 = { &hc_vtbl, 1 }, hc2 = { &hc_vtbl, 1 };
    IUnknown *ctrl = NULL, *dom1 = NULL, *dom2 = NULL, *def = NULL;
    HRESULT hr;
    LONG id1 = -1, id2 = -1, defid = -1, tmp;

    CoInitialize(NULL);
    hr = pCorBindToRuntimeEx(L"v4.0.30319", NULL, 0, &CLSID_CLRRuntimeHost_, &IID_ICLRRuntimeHost_, (void **)&host);
    ok(hr == S_OK && host, "CorBindToRuntimeEx ICLRRuntimeHost %lx\n", hr);
    if (!host) goto done;

    hr = host->lpVtbl->SetHostControl(host, NULL);
    ok(hr == E_POINTER, "SetHostControl(NULL) %lx\n", hr);
    hr = host->lpVtbl->SetHostControl(host, (IUnknown *)&hc1);
    ok(hr == S_OK && hc1.ref == 2, "SetHostControl %lx ref %ld\n", hr, hc1.ref);
    hr = host->lpVtbl->SetHostControl(host, (IUnknown *)&hc2);
    ok(hr == S_OK && hc1.ref == 1 && hc2.ref == 2, "SetHostControl again %lx refs %ld %ld\n", hr, hc1.ref, hc2.ref);

    hr = host->lpVtbl->GetCLRControl(host, &ctrl);
    ok(hr == S_OK && ctrl, "GetCLRControl %lx\n", hr);
    if (ctrl)
    {
        const ICLRControlVtbl *cv = *(const ICLRControlVtbl **)ctrl;
        void *mgr = (void *)1;
        IUnknown *same = NULL;

        hr = ctrl->lpVtbl->QueryInterface(ctrl, &IID_ICLRControl_, (void **)&same);
        ok(hr == S_OK && same == ctrl, "QI ICLRControl %lx\n", hr);
        if (same) same->lpVtbl->Release(same);
        hr = cv->SetAppDomainManagerType(ctrl, L"Manager, Version=1.0.0.0", L"My.Manager");
        ok(hr == S_OK, "SetAppDomainManagerType %lx\n", hr);
        hr = cv->SetAppDomainManagerType(ctrl, NULL, L"My.Manager");
        ok(hr == E_POINTER, "SetAppDomainManagerType(NULL) %lx\n", hr);
        hr = cv->GetCLRManager(ctrl, &IID_IUnknown, &mgr);
        ok(FAILED(hr) && !mgr, "GetCLRManager %lx\n", hr);
        ctrl->lpVtbl->Release(ctrl);
    }

    hr = host->lpVtbl->Start(host);
    ok(hr == S_OK, "Start %lx\n", hr);
    hr = host->lpVtbl->SetHostControl(host, (IUnknown *)&hc1);
    ok(hr == HOST_E_INVALIDOPERATION, "SetHostControl after Start %lx\n", hr);
    ok(hc1.ref == 1 && hc2.ref == 2, "refs changed by a refused SetHostControl: %ld %ld\n", hc1.ref, hc2.ref);
    ctrl = (IUnknown *)1;
    hr = host->lpVtbl->GetCLRControl(host, &ctrl);
    ok(hr == HOST_E_INVALIDOPERATION && !ctrl, "GetCLRControl after Start %lx %p\n", hr, ctrl);

    /* real application domains */
    hr = pCorBindToRuntimeEx(L"v4.0.30319", NULL, 0, &CLSID_CorRuntimeHost_, &IID_ICorRuntimeHost_, (void **)&cor);
    ok(hr == S_OK && cor, "CorBindToRuntimeEx ICorRuntimeHost %lx\n", hr);
    if (!cor) goto done;
    hr = cor->lpVtbl->GetDefaultDomain(cor, &def);
    ok(hr == S_OK && def, "GetDefaultDomain %lx\n", hr);
    hr = def ? domain_id(def, &defid) : E_FAIL;
    ok(hr == S_OK, "default domain Id %lx\n", hr);
    hr = cor->lpVtbl->CreateDomain(cor, L"unload-one", NULL, &dom1);
    ok(hr == S_OK && dom1, "CreateDomain %lx\n", hr);
    hr = cor->lpVtbl->CreateDomain(cor, L"unload-two", NULL, &dom2);
    ok(hr == S_OK && dom2, "CreateDomain 2 %lx\n", hr);
    hr = dom1 ? domain_id(dom1, &id1) : E_FAIL;
    ok(hr == S_OK && id1 != defid && id1 > 0, "domain 1 Id %lx %ld (default %ld)\n", hr, id1, defid);
    hr = dom2 ? domain_id(dom2, &id2) : E_FAIL;
    ok(hr == S_OK && id2 != id1, "domain 2 Id %lx %ld\n", hr, id2);

    hr = host->lpVtbl->UnloadAppDomain(host, id1, TRUE);
    ok(hr == S_OK, "UnloadAppDomain %lx\n", hr);
    hr = host->lpVtbl->UnloadAppDomain(host, id1, TRUE);
    ok(hr == COR_E_APPDOMAINUNLOADED, "UnloadAppDomain twice %lx\n", hr);
    hr = host->lpVtbl->UnloadAppDomain(host, 99999, FALSE);
    ok(hr == COR_E_APPDOMAINUNLOADED, "UnloadAppDomain with an unknown id %lx\n", hr);
    hr = host->lpVtbl->UnloadAppDomain(host, defid, TRUE);
    ok(hr == COR_E_CANNOTUNLOADAPPDOMAIN, "UnloadAppDomain of the default domain %lx\n", hr);
    hr = domain_id(dom2, &tmp);
    ok(hr == S_OK && tmp == id2, "the other domain was affected: %lx\n", hr);

    hr = cor->lpVtbl->UnloadDomain(cor, NULL);
    ok(hr == E_POINTER, "UnloadDomain(NULL) %lx\n", hr);
    hr = cor->lpVtbl->UnloadDomain(cor, dom2);
    ok(hr == S_OK, "UnloadDomain %lx\n", hr);
    hr = host->lpVtbl->UnloadAppDomain(host, id2, TRUE);
    ok(hr == COR_E_APPDOMAINUNLOADED, "the domain unloaded through ICorRuntimeHost is still there: %lx\n", hr);
    hr = cor->lpVtbl->UnloadDomain(cor, def);
    ok(hr == COR_E_CANNOTUNLOADAPPDOMAIN, "UnloadDomain of the default domain %lx\n", hr);

done:
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
