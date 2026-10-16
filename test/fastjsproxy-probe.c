/* fastjsproxy-probe: the global object and the script site of jsproxy.dll (cut out of main.c by the gate),
 * and the whole DLL on local proxy auto-config scripts */
#define COBJMACROS
#include <initguid.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <dispex.h>
#include <activscp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DEFINE_GUID(GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

#define TRACE(...) ((void)0)
#define WARN(...) ((void)0)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
static const char *debugstr_w(const WCHAR *w) { return ""; }
static char *strdupWA(const WCHAR *src)
{
    char *dst = NULL;
    if (src)
    {
        int len = WideCharToMultiByte(CP_ACP, 0, src, -1, NULL, 0, NULL, NULL);
        if ((dst = malloc(len))) WideCharToMultiByte(CP_ACP, 0, src, -1, dst, len, NULL, NULL);
    }
    return dst;
}

typedef struct { DWORD dwStructSize; LPSTR lpszScriptBuffer; DWORD dwScriptBufferSize; } AUTO_PROXY_SCRIPT_BUFFER;

#include "jsproxy-slice.inc"

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* an error object for OnScriptError */
static int err_info_calls, err_pos_calls;
static HRESULT WINAPI err_QI(IActiveScriptError *iface, REFIID riid, void **obj) { *obj = NULL; return E_NOINTERFACE; }
static ULONG WINAPI err_AddRef(IActiveScriptError *iface) { return 2; }
static ULONG WINAPI err_Release(IActiveScriptError *iface) { return 1; }
static HRESULT WINAPI err_GetExceptionInfo(IActiveScriptError *iface, EXCEPINFO *info)
{
    err_info_calls++;
    memset(info, 0, sizeof(*info));
    info->scode = 0x800a1234;
    info->bstrDescription = SysAllocString(L"boom");
    info->bstrSource = SysAllocString(L"pac");
    return S_OK;
}
static HRESULT WINAPI err_GetSourcePosition(IActiveScriptError *iface, DWORD *ctx, ULONG *line, LONG *pos)
{
    err_pos_calls++;
    *ctx = 0; *line = 3; *pos = 7;
    return S_OK;
}
static HRESULT WINAPI err_GetSourceLineText(IActiveScriptError *iface, BSTR *text) { return E_NOTIMPL; }
static const IActiveScriptErrorVtbl err_vtbl = { err_QI, err_AddRef, err_Release, err_GetExceptionInfo, err_GetSourcePosition, err_GetSourceLineText };
static IActiveScriptError fake_error = { &err_vtbl };

static void test_global_object(void)
{
    IDispatchEx *dx = &global_dispex;
    IDispatch *disp;
    ITypeInfo *ti;
    DISPID id, ids[2];
    DISPPARAMS dp;
    VARIANT arg, res;
    EXCEPINFO ei;
    UINT n, argerr;
    DWORD props;
    BSTR name;
    IUnknown *unk;
    LPOLESTR names[2];
    HRESULT hr;
    void *obj;

    hr = IDispatchEx_QueryInterface(dx, &IID_IDispatch, (void **)&disp);
    CHECK(hr == S_OK && disp, "QI IDispatch %08lx", hr);
    hr = IDispatchEx_QueryInterface(dx, &IID_IActiveScript, &obj);
    CHECK(hr == E_NOINTERFACE, "QI other %08lx", hr);

    n = 7;
    hr = IDispatch_GetTypeInfoCount(disp, &n);
    CHECK(hr == S_OK && n == 0, "typeinfo count %08lx %u", hr, n);
    ti = (ITypeInfo *)1;
    hr = IDispatch_GetTypeInfo(disp, 0, 0, &ti);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND && ti == NULL, "typeinfo 0 %08lx", hr);
    ti = (ITypeInfo *)1;
    hr = IDispatch_GetTypeInfo(disp, 1, 0, &ti);
    CHECK(hr == DISP_E_BADINDEX && ti == NULL, "typeinfo 1 %08lx", hr);

    names[0] = (LPOLESTR)L"dns_resolve";
    names[1] = (LPOLESTR)L"host";
    ids[0] = ids[1] = 99;
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, 0, ids);
    CHECK(hr == S_OK && ids[0] == 0x1000, "names %08lx %lx", hr, ids[0]);
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 2, 0, ids);
    CHECK(hr == DISP_E_UNKNOWNNAME && ids[0] == 0x1000 && ids[1] == DISPID_UNKNOWN, "names with an argument %08lx %lx %lx", hr, ids[0], ids[1]);
    names[0] = (LPOLESTR)L"nothing";
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, 0, ids);
    CHECK(hr == DISP_E_UNKNOWNNAME && ids[0] == DISPID_UNKNOWN, "unknown name %08lx %lx", hr, ids[0]);
    names[0] = (LPOLESTR)L"dns_resolve";
    hr = IDispatch_GetIDsOfNames(disp, &IID_IUnknown, names, 1, 0, ids);
    CHECK(hr == DISP_E_UNKNOWNINTERFACE, "names riid %08lx", hr);
    names[0] = (LPOLESTR)L"DNS_RESOLVE";
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, 0, ids);
    CHECK(hr == DISP_E_UNKNOWNNAME, "names are case sensitive %08lx", hr);
    name = SysAllocString(L"DNS_resolve");
    hr = IDispatchEx_GetDispID(dx, name, fdexNameCaseInsensitive, &id);
    CHECK(hr == S_OK && id == 0x1000, "case insensitive %08lx", hr);
    SysFreeString(name);

    /* Invoke through IDispatch */
    VariantInit(&arg);
    V_VT(&arg) = VT_BSTR;
    V_BSTR(&arg) = SysAllocString(L"localhost");
    memset(&dp, 0, sizeof(dp));
    dp.rgvarg = &arg;
    dp.cArgs = 1;
    VariantInit(&res);
    memset(&ei, 0, sizeof(ei));
    argerr = 5;
    hr = IDispatch_Invoke(disp, 0x1000, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, &ei, &argerr);
    CHECK(hr == S_OK && V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"127.0.0.1"),
          "invoke %08lx vt %d", hr, V_VT(&res));
    VariantClear(&res);
    hr = IDispatch_Invoke(disp, 0x1000, &IID_IUnknown, 0, DISPATCH_METHOD, &dp, &res, &ei, &argerr);
    CHECK(hr == DISP_E_UNKNOWNINTERFACE, "invoke riid %08lx", hr);
    hr = IDispatch_Invoke(disp, 0x1000, &IID_NULL, 0, DISPATCH_METHOD, &dp, NULL, &ei, &argerr);
    CHECK(hr == S_OK, "invoke without a result %08lx", hr);
    hr = IDispatch_Invoke(disp, 0x2000, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, &ei, &argerr);
    CHECK(hr == DISP_E_MEMBERNOTFOUND, "invoke unknown %08lx", hr);
    dp.cArgs = 0;
    hr = IDispatch_Invoke(disp, 0x1000, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, &ei, &argerr);
    CHECK(hr == DISP_E_BADPARAMCOUNT, "invoke no args %08lx", hr);
    dp.cArgs = 1;
    dp.cNamedArgs = 1;
    hr = IDispatch_Invoke(disp, 0x1000, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, &ei, &argerr);
    CHECK(hr == DISP_E_NONAMEDARGS, "invoke named %08lx", hr);
    VariantClear(&arg);
    V_VT(&arg) = VT_I4;
    dp.cNamedArgs = 0;
    hr = IDispatch_Invoke(disp, 0x1000, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, &ei, &argerr);
    CHECK(hr == DISP_E_BADVARTYPE, "invoke wrong type %08lx", hr);
    IDispatch_Release(disp);

    /* nothing is deletable */
    name = SysAllocString(L"dns_resolve");
    hr = IDispatchEx_DeleteMemberByName(dx, name, fdexNameCaseSensitive);
    CHECK(hr == S_FALSE, "delete builtin %08lx", hr);
    SysFreeString(name);
    name = SysAllocString(L"nothing");
    hr = IDispatchEx_DeleteMemberByName(dx, name, fdexNameCaseSensitive);
    CHECK(hr == S_OK, "delete unknown %08lx", hr);
    SysFreeString(name);
    hr = IDispatchEx_DeleteMemberByDispID(dx, 0x1000);
    CHECK(hr == S_FALSE, "delete id %08lx", hr);
    hr = IDispatchEx_DeleteMemberByDispID(dx, 0x2000);
    CHECK(hr == DISP_E_MEMBERNOTFOUND, "delete unknown id %08lx", hr);

    props = 0xdead;
    hr = IDispatchEx_GetMemberProperties(dx, 0x1000, grfdexPropCanAll | grfdexPropCannotAll, &props);
    CHECK(hr == S_OK && props == (fdexPropCanCall | fdexPropCannotGet | fdexPropCannotPut | fdexPropCannotPutRef |
                                   fdexPropCannotConstruct | fdexPropCannotSourceEvents), "props %08lx %lx", hr, props);
    hr = IDispatchEx_GetMemberProperties(dx, 0x1000, fdexPropCanGet, &props);
    CHECK(hr == S_OK && props == 0, "props asked for get %08lx %lx", hr, props);
    hr = IDispatchEx_GetMemberProperties(dx, 0x2000, grfdexPropCanAll, &props);
    CHECK(hr == DISP_E_MEMBERNOTFOUND && props == 0, "props unknown %08lx", hr);

    name = (BSTR)1;
    hr = IDispatchEx_GetMemberName(dx, 0x1000, &name);
    CHECK(hr == S_OK && name && !lstrcmpW(name, L"dns_resolve"), "member name %08lx", hr);
    SysFreeString(name);
    hr = IDispatchEx_GetMemberName(dx, 0x2000, &name);
    CHECK(hr == DISP_E_MEMBERNOTFOUND && name == NULL, "member name unknown %08lx", hr);

    hr = IDispatchEx_GetNextDispID(dx, fdexEnumAll, DISPID_STARTENUM, &id);
    CHECK(hr == S_OK && id == 0x1000, "enum start %08lx %lx", hr, id);
    hr = IDispatchEx_GetNextDispID(dx, fdexEnumAll, 0x1000, &id);
    CHECK(hr == S_FALSE && id == DISPID_UNKNOWN, "enum end %08lx %lx", hr, id);
    hr = IDispatchEx_GetNextDispID(dx, fdexEnumAll, 0x3000, &id);
    CHECK(hr == DISP_E_MEMBERNOTFOUND, "enum unknown %08lx", hr);

    unk = (IUnknown *)1;
    hr = IDispatchEx_GetNameSpaceParent(dx, &unk);
    CHECK(hr == S_OK && unk == NULL, "parent %08lx", hr);
}

static void test_site(void)
{
    IActiveScriptSite *site = &script_site;
    IUnknown *item;
    ITypeInfo *ti;
    LCID lcid = 0;
    BSTR ver;
    VARIANT v;
    EXCEPINFO ei;
    HRESULT hr;

    hr = IActiveScriptSite_GetLCID(site, &lcid);
    CHECK(hr == S_OK && lcid == LOCALE_SYSTEM_DEFAULT, "lcid %08lx %lx", hr, lcid);

    item = (IUnknown *)1; ti = (ITypeInfo *)1;
    hr = IActiveScriptSite_GetItemInfo(site, L"global_funcs", SCRIPTINFO_IUNKNOWN, &item, &ti);
    CHECK(hr == S_OK && item == (IUnknown *)&global_dispex && ti == NULL, "item %08lx %p %p", hr, item, ti);
    hr = IActiveScriptSite_GetItemInfo(site, L"global_funcs", SCRIPTINFO_ITYPEINFO, &item, &ti);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND && ti == NULL, "item type info %08lx", hr);
    item = (IUnknown *)1;
    hr = IActiveScriptSite_GetItemInfo(site, L"other_item", SCRIPTINFO_IUNKNOWN, &item, &ti);
    CHECK(hr == TYPE_E_ELEMENTNOTFOUND && item == NULL, "unknown item %08lx %p", hr, item);

    ver = NULL;
    hr = IActiveScriptSite_GetDocVersionString(site, &ver);
    CHECK(hr == S_OK && ver && ver[0], "doc version %08lx", hr);
    SysFreeString(ver);

    VariantInit(&v);
    memset(&ei, 0, sizeof(ei));
    hr = IActiveScriptSite_OnScriptTerminate(site, &v, &ei);
    CHECK(hr == S_OK, "terminate %08lx", hr);
    hr = IActiveScriptSite_OnStateChange(site, SCRIPTSTATE_STARTED);
    CHECK(hr == S_OK, "state %08lx", hr);
    hr = IActiveScriptSite_OnEnterScript(site);
    CHECK(hr == S_OK, "enter %08lx", hr);
    hr = IActiveScriptSite_OnLeaveScript(site);
    CHECK(hr == S_OK, "leave %08lx", hr);
    hr = IActiveScriptSite_OnScriptError(site, &fake_error);
    CHECK(hr == S_OK && err_info_calls == 1 && err_pos_calls == 1, "error %08lx %d %d", hr, err_info_calls, err_pos_calls);
    hr = IActiveScriptSite_OnScriptError(site, NULL);
    CHECK(hr == S_OK, "error null %08lx", hr);
}

/* the whole DLL on local scripts */
static BOOL (WINAPI *pInit)(DWORD, LPSTR, LPSTR, void *, AUTO_PROXY_SCRIPT_BUFFER *);
static BOOL (WINAPI *pDeinit)(LPSTR, DWORD);
static BOOL (WINAPI *pGet)(LPCSTR, DWORD, LPCSTR, DWORD, LPSTR *, LPDWORD);

static BOOL load_script(const char *text)
{
    AUTO_PROXY_SCRIPT_BUFFER buf;
    char *copy = strdup(text);
    BOOL ret;

    buf.dwStructSize = sizeof(buf);
    buf.lpszScriptBuffer = copy;
    buf.dwScriptBufferSize = strlen(copy) + 1;
    ret = pInit(0, NULL, NULL, NULL, &buf);
    free(copy);
    return ret;
}

static void expect(const char *url, const char *host, const char *want)
{
    char *proxy = NULL;
    DWORD len = 0;
    BOOL ret = pGet(url, strlen(url), host, strlen(host), &proxy, &len);

    CHECK(ret && proxy && !strcmp(proxy, want), "%s -> %s (wanted %s)", url, ret && proxy ? proxy : "(failed)", want);
    if (proxy) GlobalFree(proxy);
}

static void test_dll(void)
{
    static const char pac[] =
        "function FindProxyForURL(url, host) {\n"
        "  if (isPlainHostName(host)) return 'DIRECT';\n"
        "  if (dnsDomainIs(host, '.example.org')) return 'PROXY proxy1:8080';\n"
        "  if (shExpMatch(url, '*.gif')) return 'PROXY gifproxy:3128; DIRECT';\n"
        "  if (isInNet(dnsResolve('localhost'), '127.0.0.0', '255.0.0.0')) return 'PROXY loopback:1';\n"
        "  return 'PROXY other:80';\n"
        "}\n";
    HMODULE mod = LoadLibraryA("jsproxy.dll");
    char *proxy;
    DWORD len;
    BOOL ret;

    pInit = (void *)GetProcAddress(mod, "InternetInitializeAutoProxyDll");
    pDeinit = (void *)GetProcAddress(mod, "InternetDeInitializeAutoProxyDll");
    pGet = (void *)GetProcAddress(mod, "InternetGetProxyInfo");
    CHECK(pInit && pDeinit && pGet, "exports");
    if (!pInit || !pDeinit || !pGet) return;

    CHECK(load_script(pac), "script loaded");
    expect("http://intranet/", "intranet", "DIRECT");
    expect("http://www.example.org/", "www.example.org", "PROXY proxy1:8080");
    expect("http://a.net/x.gif", "a.net", "PROXY gifproxy:3128; DIRECT");
    expect("http://b.net/", "b.net", "PROXY loopback:1");

    /* a script that fails in the engine: the call fails, the engine reported the error to the site */
    CHECK(load_script("function FindProxyForURL(url, host) { return nosuchfunction(host); }"), "failing script loaded");
    proxy = NULL; len = 0;
    ret = pGet("http://x/", 9, "x", 1, &proxy, &len);
    CHECK(!ret, "a script error fails the call");

    CHECK(load_script("function FindProxyForURL(url, host) { return dnsResolve('no-such-host.invalid') == null ? 'NULL' : 'FOUND'; }"),
          "dns script");
    expect("http://x/", "x", "NULL");
    pDeinit(NULL, 0);
}

int main(void)
{
    WSADATA wsa;

    setvbuf(stdout, NULL, _IONBF, 0);
    WSAStartup(MAKEWORD(2, 2), &wsa);
    CoInitialize(NULL);
    printf("-- test_global_object\n");
    test_global_object();
    printf("-- test_site\n");
    test_site();
    printf("-- test_dll\n");
    test_dll();
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
