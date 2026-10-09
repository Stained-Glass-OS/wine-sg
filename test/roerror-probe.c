/* WinRT error information and COM process settings (patches/sg/1676), run
 * by test/roerror-gate.sh: RoOriginateError and its kin keep a restricted
 * error object for the thread (GetRestrictedErrorInfo, SetRestrictedErrorInfo,
 * RoGetMatchingRestrictedErrorInfo, RoResolveRestrictedErrorInfoReference,
 * RoTransformError's previous error, a language exception, the reporting
 * flags); RoRegisterActivationFactories and RoRevokeActivationFactories;
 * CoInitializeSecurity only once; REGCLS_SUSPENDED class objects reach
 * other processes after CoResumeClassObjects; CoAllowSetForegroundWindow
 * takes proxies only. These were stubs. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <roapi.h>
#include <winstring.h>
#include <activation.h>
#include <stdio.h>

/* restrictederrorinfo.h and roerrorapi.h, which mingw does not have */
DEFINE_GUID(IID_IRestrictedErrorInfo, 0x82ba7092, 0x4c88, 0x427d, 0xa7, 0xbc, 0x16, 0xdd, 0x93, 0xfe, 0xb6, 0x7e);
typedef struct IRestrictedErrorInfo IRestrictedErrorInfo;
struct restricted_vtbl
{
    HRESULT (WINAPI *QueryInterface)(IRestrictedErrorInfo *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IRestrictedErrorInfo *);
    ULONG (WINAPI *Release)(IRestrictedErrorInfo *);
    HRESULT (WINAPI *GetErrorDetails)(IRestrictedErrorInfo *, BSTR *, HRESULT *, BSTR *, BSTR *);
    HRESULT (WINAPI *GetReference)(IRestrictedErrorInfo *, BSTR *);
};
struct IRestrictedErrorInfo { const struct restricted_vtbl *lpVtbl; };
#define IRestrictedErrorInfo_QueryInterface(p, a, b) (p)->lpVtbl->QueryInterface(p, a, b)
#define IRestrictedErrorInfo_Release(p) (p)->lpVtbl->Release(p)
#define IRestrictedErrorInfo_GetErrorDetails(p, a, b, c, d) (p)->lpVtbl->GetErrorDetails(p, a, b, c, d)
#define IRestrictedErrorInfo_GetReference(p, a) (p)->lpVtbl->GetReference(p, a)
#define RO_ERROR_REPORTING_USESETERRORINFO 4
#define RO_ERROR_REPORTING_SUPPRESSSETERRORINFO 8

static HRESULT (WINAPI *pGetRestrictedErrorInfo)(IRestrictedErrorInfo **);
static HRESULT (WINAPI *pSetRestrictedErrorInfo)(IRestrictedErrorInfo *);
static BOOL (WINAPI *pRoOriginateError)(HRESULT, HSTRING);
static BOOL (WINAPI *pRoOriginateErrorW)(HRESULT, UINT, const WCHAR *);
static BOOL (WINAPI *pRoTransformErrorW)(HRESULT, HRESULT, UINT, const WCHAR *);
static BOOL (WINAPI *pRoOriginateLanguageException)(HRESULT, HSTRING, IUnknown *);
static HRESULT (WINAPI *pRoGetMatchingRestrictedErrorInfo)(HRESULT, IRestrictedErrorInfo **);
static HRESULT (WINAPI *pRoResolveRestrictedErrorInfoReference)(const WCHAR *, IRestrictedErrorInfo **);
static HRESULT (WINAPI *pRoSetErrorReportingFlags)(UINT32);
static HRESULT (WINAPI *pRoGetErrorReportingFlags)(UINT32 *);
static void (WINAPI *pRoClearError)(void);
static HRESULT (WINAPI *pRoRegisterActivationFactories)(HSTRING *, PFNGETACTIVATIONFACTORY *, UINT32, RO_REGISTRATION_COOKIE *);
static void (WINAPI *pRoRevokeActivationFactories)(RO_REGISTRATION_COOKIE);
#define GetRestrictedErrorInfo pGetRestrictedErrorInfo
#define SetRestrictedErrorInfo pSetRestrictedErrorInfo
#define RoOriginateError pRoOriginateError
#define RoOriginateErrorW pRoOriginateErrorW
#define RoTransformErrorW pRoTransformErrorW
#define RoOriginateLanguageException pRoOriginateLanguageException
#define RoGetMatchingRestrictedErrorInfo pRoGetMatchingRestrictedErrorInfo
#define RoResolveRestrictedErrorInfoReference pRoResolveRestrictedErrorInfoReference
#define RoSetErrorReportingFlags pRoSetErrorReportingFlags
#define RoGetErrorReportingFlags pRoGetErrorReportingFlags
#define RoClearError pRoClearError
#define RoRegisterActivationFactories pRoRegisterActivationFactories
#define RoRevokeActivationFactories pRoRevokeActivationFactories

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

DEFINE_GUID(CLSID_ProbeServer, 0x6c1f0a01, 0x3b2e, 0x4d5c, 0x8e, 0x7f, 0x90, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5);
DEFINE_GUID(IID_LanguageExceptionErrorInfo2, 0x5746e5c4, 0x5b97, 0x424c, 0xb6, 0x20, 0x28, 0x22, 0x91, 0x57, 0x34, 0xdd);

typedef struct lang_error lang_error;
struct lang_error_vtbl
{
    HRESULT (WINAPI *QueryInterface)(lang_error *, REFIID, void **);
    ULONG (WINAPI *AddRef)(lang_error *);
    ULONG (WINAPI *Release)(lang_error *);
    HRESULT (WINAPI *GetLanguageException)(lang_error *, IUnknown **);
    HRESULT (WINAPI *GetPreviousLanguageExceptionErrorInfo)(lang_error *, lang_error **);
};
struct lang_error { const struct lang_error_vtbl *lpVtbl; };

/* a class factory and an activation factory that count their use */
static LONG activations;
static HRESULT WINAPI cf_qi(IClassFactory *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_create(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out) { *out = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI cf_lock(IClassFactory *iface, BOOL lock) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static IClassFactory class_factory = { &cf_vtbl };

static HRESULT WINAPI af_qi(IActivationFactory *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IInspectable) ||
        IsEqualIID(riid, &IID_IActivationFactory)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI af_addref(IActivationFactory *iface) { return 2; }
static ULONG WINAPI af_release(IActivationFactory *iface) { return 1; }
static HRESULT WINAPI af_iids(IActivationFactory *iface, ULONG *count, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI af_name(IActivationFactory *iface, HSTRING *name) { return E_NOTIMPL; }
static HRESULT WINAPI af_trust(IActivationFactory *iface, TrustLevel *level) { return E_NOTIMPL; }
static HRESULT WINAPI af_activate(IActivationFactory *iface, IInspectable **out)
{
    activations++;
    *out = (IInspectable *)iface;
    return S_OK;
}
static IActivationFactoryVtbl af_vtbl = { af_qi, af_addref, af_release, af_iids, af_name, af_trust, af_activate };
static IActivationFactory activation_factory = { &af_vtbl };
static HRESULT WINAPI get_factory(HSTRING classid, IActivationFactory **factory)
{
    *factory = &activation_factory;
    return S_OK;
}

static IUnknown language_exception;
static HRESULT WINAPI le_qi(IUnknown *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI le_addref(IUnknown *iface) { return 2; }
static ULONG WINAPI le_release(IUnknown *iface) { return 1; }
static IUnknownVtbl le_vtbl = { le_qi, le_addref, le_release };

static HRESULT details(IRestrictedErrorInfo *info, BSTR *restricted)
{
    BSTR desc = NULL, sid = NULL;
    HRESULT hr = 0;

    *restricted = NULL;
    IRestrictedErrorInfo_GetErrorDetails(info, &desc, &hr, restricted, &sid);
    SysFreeString(desc);
    SysFreeString(sid);
    return hr;
}

/* the client process: the class object of the server */
static int child(void)
{
    IClassFactory *cf = NULL;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoGetClassObject(&CLSID_ProbeServer, CLSCTX_LOCAL_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    printf("child: CoGetClassObject %08lx\n", hr);
    if (FAILED(hr)) return 1;
    hr = CoAllowSetForegroundWindow((IUnknown *)cf, NULL);
    printf("child: CoAllowSetForegroundWindow %08lx\n", hr);
    IClassFactory_Release(cf);
    CoUninitialize();
    return hr == S_OK ? 0 : 2;
}

static DWORD run_child(const char *self)
{
    char cmd[MAX_PATH + 16];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 99;

    sprintf(cmd, "\"%s\" child", self);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 98;
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

int main(int argc, char **argv)
{
    IRestrictedErrorInfo *info = NULL, *info2 = NULL;
    IErrorInfo *error_info = NULL;
    lang_error *lang = NULL, *previous = NULL;
    IUnknown *unk = NULL;
    BSTR restricted = NULL, reference = NULL, desc = NULL;
    HSTRING name, message;
    HSTRING classes[1];
    PFNGETACTIVATIONFACTORY callbacks[1] = { get_factory };
    RO_REGISTRATION_COOKIE cookie = NULL;
    IActivationFactory *factory = NULL;
    IInspectable *instance = NULL;
    UINT32 flags = 0;
    DWORD reg = 0, code;
    HRESULT hr;
    BOOL ret;
    char self[MAX_PATH];

    if (argc > 1 && !strcmp(argv[1], "child")) return child();

    {
        HMODULE combase = LoadLibraryA("combase.dll");
#define LOAD(f) p##f = (void *)GetProcAddress(combase, #f); if (!p##f) { printf("FAIL  no %s\n", #f); return 1; }
        LOAD(GetRestrictedErrorInfo) LOAD(SetRestrictedErrorInfo) LOAD(RoOriginateError) LOAD(RoOriginateErrorW)
        LOAD(RoTransformErrorW) LOAD(RoOriginateLanguageException) LOAD(RoGetMatchingRestrictedErrorInfo)
        LOAD(RoResolveRestrictedErrorInfoReference) LOAD(RoSetErrorReportingFlags) LOAD(RoGetErrorReportingFlags)
        LOAD(RoClearError) LOAD(RoRegisterActivationFactories) LOAD(RoRevokeActivationFactories)
    }
    language_exception.lpVtbl = &le_vtbl;
    RoInitialize(RO_INIT_MULTITHREADED);

    /* originating */
    check(!RoOriginateErrorW(S_OK, 0, L"fine"), "RoOriginateErrorW of a success: FALSE");
    ret = RoOriginateErrorW(E_FAIL, 0, L"the probe failed");
    check(ret, "RoOriginateErrorW of a failure: TRUE");
    hr = GetRestrictedErrorInfo(&info);
    check(hr == S_OK && info, "GetRestrictedErrorInfo gives it");
    if (info)
    {
        check(details(info, &restricted) == E_FAIL && restricted && !lstrcmpW(restricted, L"the probe failed"),
              "its error and message");
        SysFreeString(restricted);
        check(IRestrictedErrorInfo_GetReference(info, &reference) == S_OK && reference && *reference,
              "and a reference");
    }
    check(GetRestrictedErrorInfo(&info2) == S_FALSE && !info2, "taken: the thread has none now");
    if (info)
    {
        check(SetRestrictedErrorInfo(info) == S_OK, "SetRestrictedErrorInfo");
        hr = GetErrorInfo(0, &error_info);
        check(hr == S_OK && error_info && IErrorInfo_GetDescription(error_info, &desc) == S_OK && desc &&
              !lstrcmpW(desc, L"the probe failed"), "it is the thread's error info too (GetErrorInfo)");
        SysFreeString(desc);
        if (error_info) IErrorInfo_Release(error_info);
        info2 = NULL;
        check(RoResolveRestrictedErrorInfoReference(reference, &info2) == S_OK && info2 == info,
              "RoResolveRestrictedErrorInfoReference finds it");
        if (info2) IRestrictedErrorInfo_Release(info2);
        IRestrictedErrorInfo_Release(info);
    }
    SysFreeString(reference);

    /* matching */
    RoOriginateErrorW(E_OUTOFMEMORY, 0, L"memory");
    info = NULL;
    hr = RoGetMatchingRestrictedErrorInfo(E_OUTOFMEMORY, &info);
    check(hr == S_OK && info && details(info, &restricted) == E_OUTOFMEMORY && restricted &&
          !lstrcmpW(restricted, L"memory"), "RoGetMatchingRestrictedErrorInfo: the thread's for its error");
    SysFreeString(restricted);
    if (info) IRestrictedErrorInfo_Release(info);
    info = NULL;
    hr = RoGetMatchingRestrictedErrorInfo(E_ABORT, &info);
    check(hr == S_OK && info && details(info, &restricted) == E_ABORT, "and a new one for another");
    SysFreeString(restricted);
    if (info) IRestrictedErrorInfo_Release(info);

    /* transforming */
    RoOriginateErrorW(E_FAIL, 0, L"first");
    ret = RoTransformErrorW(E_FAIL, E_ACCESSDENIED, 0, L"second");
    info = NULL;
    GetRestrictedErrorInfo(&info);
    check(ret && info && details(info, &restricted) == E_ACCESSDENIED, "RoTransformErrorW: the new error");
    SysFreeString(restricted);
    if (info && SUCCEEDED(IRestrictedErrorInfo_QueryInterface(info, &IID_LanguageExceptionErrorInfo2, (void **)&lang)))
    {
        lang->lpVtbl->GetPreviousLanguageExceptionErrorInfo(lang, &previous);
        check(previous != NULL, "after the old one");
        if (previous)
        {
            IRestrictedErrorInfo *prev_info = NULL;
            previous->lpVtbl->QueryInterface(previous, &IID_IRestrictedErrorInfo, (void **)&prev_info);
            check(prev_info && details(prev_info, &restricted) == E_FAIL, "which was E_FAIL");
            SysFreeString(restricted);
            if (prev_info) IRestrictedErrorInfo_Release(prev_info);
            previous->lpVtbl->Release(previous);
        }
        lang->lpVtbl->Release(lang);
    }
    else check(0, "the language exception interface");
    if (info) IRestrictedErrorInfo_Release(info);
    check(!RoTransformErrorW(E_FAIL, E_FAIL, 0, NULL), "the same error: FALSE");

    /* a language exception */
    WindowsCreateString(L"language", 8, &message);
    ret = RoOriginateLanguageException(E_INVALIDARG, message, &language_exception);
    info = NULL;
    GetRestrictedErrorInfo(&info);
    lang = NULL;
    if (info) IRestrictedErrorInfo_QueryInterface(info, &IID_LanguageExceptionErrorInfo2, (void **)&lang);
    if (lang) lang->lpVtbl->GetLanguageException(lang, &unk);
    check(ret && unk == &language_exception, "RoOriginateLanguageException keeps the exception");
    if (lang) lang->lpVtbl->Release(lang);
    if (info) IRestrictedErrorInfo_Release(info);

    /* reporting flags */
    check(RoSetErrorReportingFlags(RO_ERROR_REPORTING_SUPPRESSSETERRORINFO) == S_OK &&
          RoGetErrorReportingFlags(&flags) == S_OK && flags == RO_ERROR_REPORTING_SUPPRESSSETERRORINFO,
          "RoGetErrorReportingFlags gives the flags set");
    ret = RoOriginateError(E_FAIL, message);
    info = NULL;
    check(!ret && GetRestrictedErrorInfo(&info) == S_FALSE, "SUPPRESSSETERRORINFO: nothing kept");
    RoSetErrorReportingFlags(RO_ERROR_REPORTING_USESETERRORINFO);
    RoOriginateError(E_FAIL, message);
    RoClearError();
    info = NULL;
    check(GetRestrictedErrorInfo(&info) == S_FALSE, "RoClearError clears it");
    WindowsDeleteString(message);

    /* activation factories of the process */
    WindowsCreateString(L"Probe.Activatable", 17, &name);
    classes[0] = name;
    check(RoRegisterActivationFactories(classes, callbacks, 1, &cookie) == S_OK && cookie,
          "RoRegisterActivationFactories");
    hr = RoGetActivationFactory(name, &IID_IActivationFactory, (void **)&factory);
    check(hr == S_OK && factory == &activation_factory, "RoGetActivationFactory finds it");
    hr = RoActivateInstance(name, &instance);
    check(hr == S_OK && activations == 1, "RoActivateInstance activates through it");
    RoRevokeActivationFactories(cookie);
    factory = NULL;
    hr = RoGetActivationFactory(name, &IID_IActivationFactory, (void **)&factory);
    check(FAILED(hr) && !factory, "revoked: gone");
    WindowsDeleteString(name);

    /* CoInitializeSecurity once */
    hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IDENTIFY, NULL,
                              EOAC_NONE, NULL);
    check(hr == S_OK, "CoInitializeSecurity");
    hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IDENTIFY, NULL,
                              EOAC_NONE, NULL);
    check(hr == RPC_E_TOO_LATE, "a second time: RPC_E_TOO_LATE");

    /* a suspended class object */
    check(CoAllowSetForegroundWindow((IUnknown *)&class_factory, NULL) == E_NOINTERFACE,
          "CoAllowSetForegroundWindow of an object here: E_NOINTERFACE");
    hr = CoRegisterClassObject(&CLSID_ProbeServer, (IUnknown *)&class_factory, CLSCTX_LOCAL_SERVER,
                               REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED, &reg);
    check(hr == S_OK, "CoRegisterClassObject, suspended");
    GetModuleFileNameA(NULL, self, MAX_PATH);
    code = run_child(self);
    printf("      child exit %lu\n", code);
    check(code == 1, "another process cannot get it");
    check(CoResumeClassObjects() == S_OK, "CoResumeClassObjects");
    code = run_child(self);
    printf("      child exit %lu\n", code);
    check(code == 0, "now it can, and CoAllowSetForegroundWindow takes the proxy");
    CoSuspendClassObjects();
    code = run_child(self);
    check(code == 1, "CoSuspendClassObjects: not again");
    CoRevokeClassObject(reg);

    RoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
