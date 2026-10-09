/* A script host for test/vbsdyncode-gate.sh (patches/sg/1652): it runs
 * VBScript through IActiveScript as a web page or an HTA does, keeps the
 * object GetRef returns, and calls it later from outside the script, as an
 * event would; after the script is closed the object must refuse the call
 * instead of running freed code. Errors in the script reach the site. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <activscp.h>
#include <stdio.h>

static const CLSID CLSID_VBScript = {0xb54f3741,0x5b07,0x11cf,{0xa4,0xb0,0x00,0xaa,0x00,0x4a,0x55,0xe8}};
static int failures, script_errors;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static HRESULT WINAPI site_QueryInterface(IActiveScriptSite *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IActiveScriptSite)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI site_AddRef(IActiveScriptSite *iface) { return 2; }
static ULONG WINAPI site_Release(IActiveScriptSite *iface) { return 1; }
static HRESULT WINAPI site_GetLCID(IActiveScriptSite *iface, LCID *lcid) { *lcid = 1033; return S_OK; }
static HRESULT WINAPI site_GetItemInfo(IActiveScriptSite *iface, LPCOLESTR name, DWORD mask, IUnknown **unk, ITypeInfo **ti)
{ return TYPE_E_ELEMENTNOTFOUND; }
static HRESULT WINAPI site_GetDocVersionString(IActiveScriptSite *iface, BSTR *v) { return E_NOTIMPL; }
static HRESULT WINAPI site_OnScriptTerminate(IActiveScriptSite *iface, const VARIANT *r, const EXCEPINFO *e) { return S_OK; }
static HRESULT WINAPI site_OnStateChange(IActiveScriptSite *iface, SCRIPTSTATE s) { return S_OK; }
static HRESULT WINAPI site_OnScriptError(IActiveScriptSite *iface, IActiveScriptError *err)
{
    EXCEPINFO ei;
    memset(&ei, 0, sizeof(ei));
    IActiveScriptError_GetExceptionInfo(err, &ei);
    printf("script error %#lx %ls\n", ei.scode, ei.bstrDescription ? ei.bstrDescription : L"");
    SysFreeString(ei.bstrSource); SysFreeString(ei.bstrDescription); SysFreeString(ei.bstrHelpFile);
    script_errors++;
    return S_OK;
}
static HRESULT WINAPI site_OnEnterScript(IActiveScriptSite *iface) { return S_OK; }
static HRESULT WINAPI site_OnLeaveScript(IActiveScriptSite *iface) { return S_OK; }

static IActiveScriptSiteVtbl site_vtbl = {
    site_QueryInterface, site_AddRef, site_Release, site_GetLCID, site_GetItemInfo, site_GetDocVersionString,
    site_OnScriptTerminate, site_OnStateChange, site_OnScriptError, site_OnEnterScript, site_OnLeaveScript
};
static IActiveScriptSite site = { &site_vtbl };

int main(void)
{
    IActiveScript *script;
    IActiveScriptParse *parse;
    VARIANT v, arg, ret;
    DISPPARAMS dp;
    IDispatch *handler, *bad;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_VBScript, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void **)&script);
    if (FAILED(hr)) { printf("FAIL  no VBScript engine: %#lx\nRESULT: FAIL\n", hr); return 1; }
    IActiveScript_QueryInterface(script, &IID_IActiveScriptParse, (void **)&parse);
    parse->lpVtbl->InitNew(parse);
    IActiveScript_SetScriptSite(script, &site);
    IActiveScript_SetScriptState(script, SCRIPTSTATE_STARTED);

    hr = parse->lpVtbl->ParseScriptText(parse,
            L"Dim clicks : clicks = 0\n"
            L"Function OnClick(n) : clicks = clicks + n : OnClick = clicks * 10 : End Function\n"
            L"Sub Fails : Err.Raise 1234 : End Sub\n",
            NULL, NULL, NULL, 0, 0, 0, NULL, NULL);
    check(hr == S_OK, "the page script runs");

    VariantInit(&v);
    hr = parse->lpVtbl->ParseScriptText(parse, L"GetRef(\"OnClick\")", NULL, NULL, NULL, 0, 0,
                                            SCRIPTTEXT_ISEXPRESSION, &v, NULL);
    check(hr == S_OK && V_VT(&v) == VT_DISPATCH && V_DISPATCH(&v), "GetRef gives the host an object");
    handler = V_VT(&v) == VT_DISPATCH ? V_DISPATCH(&v) : NULL;

    if (handler)
    {
        /* the "event": called from outside the script */
        V_VT(&arg) = VT_I4; V_I4(&arg) = 4;
        dp.rgvarg = &arg; dp.cArgs = 1; dp.rgdispidNamedArgs = NULL; dp.cNamedArgs = 0;
        VariantInit(&ret);
        hr = IDispatch_Invoke(handler, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, &ret, NULL, NULL);
        check(hr == S_OK && V_VT(&ret) == VT_I4 && V_I4(&ret) == 40, "the host calls the procedure (4 -> 40)");
        VariantClear(&ret);
        hr = IDispatch_Invoke(handler, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, &ret, NULL, NULL);
        check(hr == S_OK && V_I4(&ret) == 80, "... and again; the script's state is kept (80)");
        dp.cArgs = 0;
        hr = IDispatch_Invoke(handler, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, &ret, NULL, NULL);
        check(FAILED(hr), "a call with the wrong number of arguments is refused");
    }

    VariantInit(&v);
    hr = parse->lpVtbl->ParseScriptText(parse, L"GetRef(\"Fails\")", NULL, NULL, NULL, 0, 0,
                                            SCRIPTTEXT_ISEXPRESSION, &v, NULL);
    bad = V_VT(&v) == VT_DISPATCH ? V_DISPATCH(&v) : NULL;
    if (bad)
    {
        script_errors = 0;
        dp.cArgs = 0;
        hr = IDispatch_Invoke(bad, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, NULL, NULL, NULL);
        printf("failing handler: %#lx, %d errors reported\n", hr, script_errors);
        check(FAILED(hr) && script_errors == 1, "an error in a called handler is reported to the site");
    }
    else check(0, "GetRef of a Sub");

    VariantInit(&v);
    hr = parse->lpVtbl->ParseScriptText(parse, L"Eval(\"clicks + 1\")", NULL, NULL, NULL, 0, 0,
                                            SCRIPTTEXT_ISEXPRESSION, &v, NULL);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 9, "Eval from a host expression sees the globals (9)");
    VariantClear(&v);

    /* a syntax error in Execute is the script's to handle, not the site's */
    script_errors = 0;
    hr = parse->lpVtbl->ParseScriptText(parse, L"On Error Resume Next\nExecute \"x = (1\"\nsynErr = Err.Number\n",
                                        NULL, NULL, NULL, 0, 0, 0, NULL, NULL);
    VariantInit(&v);
    parse->lpVtbl->ParseScriptText(parse, L"synErr", NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, &v, NULL);
    printf("syntax error in Execute: %#lx, Err.Number %ld, %d reported\n", hr, V_VT(&v) == VT_I4 ? V_I4(&v) : -1, script_errors);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 1002 && !script_errors,
          "a syntax error in Execute is raised in the script (1002), not reported to the site");
    VariantClear(&v);

    IActiveScript_Close(script);
    if (handler)
    {
        V_VT(&arg) = VT_I4; V_I4(&arg) = 1;
        dp.rgvarg = &arg; dp.cArgs = 1;
        hr = IDispatch_Invoke(handler, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, &ret, NULL, NULL);
        check(hr == E_UNEXPECTED, "after the script is closed the handler refuses the call");
        IDispatch_Release(handler);
    }
    if (bad) IDispatch_Release(bad);
    parse->lpVtbl->Release(parse);
    IActiveScript_Release(script);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
