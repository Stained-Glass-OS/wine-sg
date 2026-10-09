/* JScript stubs (patches/sg/1653), run by test/jsstubs-gate.sh. A script
 * host runs JScript in the ES5 mode web pages and HTAs get and compares
 * what each expression gives: JSON.stringify with toJSON, an array
 * replacer and a circular value (TypeError); JSON.parse errors
 * (SyntaxError); decodeURI of bad sequences (URIError); apply with a
 * non-array (TypeError) or null; Function.prototype(); calling arguments;
 * String.localeCompare; ArrayBuffer.isView; GetObject("", class). These
 * were E_NOTIMPL stubs or E_FAIL. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <activscp.h>
#include <stdio.h>

static const CLSID CLSID_JScript = {0xf414c260,0x6ac0,0x11cf,{0xb6,0xd1,0x00,0xaa,0x00,0xbb,0xbb,0x58}};
static int failures;

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
    printf("  script error %#lx %ls\n", ei.scode, ei.bstrDescription ? ei.bstrDescription : L"");
    SysFreeString(ei.bstrSource); SysFreeString(ei.bstrDescription); SysFreeString(ei.bstrHelpFile);
    return S_OK;
}
static HRESULT WINAPI site_OnEnterScript(IActiveScriptSite *iface) { return S_OK; }
static HRESULT WINAPI site_OnLeaveScript(IActiveScriptSite *iface) { return S_OK; }
static IActiveScriptSiteVtbl site_vtbl = {
    site_QueryInterface, site_AddRef, site_Release, site_GetLCID, site_GetItemInfo, site_GetDocVersionString,
    site_OnScriptTerminate, site_OnStateChange, site_OnScriptError, site_OnEnterScript, site_OnLeaveScript
};
static IActiveScriptSite site = { &site_vtbl };
static IActiveScriptParse *parse;

static void expect(const WCHAR *expr, const WCHAR *want, const char *what)
{
    VARIANT v, s;
    HRESULT hr;
    int ok;

    VariantInit(&v); VariantInit(&s);
    hr = parse->lpVtbl->ParseScriptText(parse, expr, NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, &v, NULL);
    if (SUCCEEDED(hr)) hr = VariantChangeType(&s, &v, 0, VT_BSTR);
    ok = SUCCEEDED(hr) && !wcscmp(V_BSTR(&s), want);
    printf("%s  %s: %ls\n", ok ? "PASS" : "FAIL", what, SUCCEEDED(hr) ? V_BSTR(&s) : L"(script failed)");
    if (!ok) { printf("      wanted: %ls\n", want); failures++; }
    VariantClear(&v); VariantClear(&s);
}

/* the error a statement throws: "name number" */
#define CATCH(stmt) L"(function(){ try { " stmt L"; return 'no error'; } catch(e) { return e.name + ' ' + (e.number & 0xffff); } })()"

int main(void)
{
    IActiveScriptProperty *prop;
    IActiveScript *script;
    VARIANT ver;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_JScript, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void **)&script);
    if (FAILED(hr)) { printf("FAIL  no JScript engine: %#lx\nRESULT: FAIL\n", hr); return 1; }
    IActiveScript_QueryInterface(script, &IID_IActiveScriptProperty, (void **)&prop);
    V_VT(&ver) = VT_I4; V_I4(&ver) = 0x400 | 0x102;  /* HTML, ES5: what a standards-mode page gets */
    IActiveScriptProperty_SetProperty(prop, SCRIPTPROP_INVOKEVERSIONING, NULL, &ver);
    IActiveScriptProperty_Release(prop);
    IActiveScript_QueryInterface(script, &IID_IActiveScriptParse, (void **)&parse);
    parse->lpVtbl->InitNew(parse);
    IActiveScript_SetScriptSite(script, &site);
    IActiveScript_SetScriptState(script, SCRIPTSTATE_STARTED);

    expect(L"JSON.stringify({a: 1, d: {toJSON: function(k) { return 'key ' + k; }}})",
           L"{\"a\":1,\"d\":\"key d\"}", "JSON.stringify calls toJSON with the key");
    expect(L"JSON.stringify([new Date(Date.UTC(2020, 0, 2))])", L"[\"2020-01-02T00:00:00.000Z\"]",
           "JSON.stringify of a Date uses its toJSON");
    expect(L"JSON.stringify({b: 2, a: 1, c: {a: 3, z: 4}, 1: 'one'}, ['a', 'c', 1, 'a', 'missing'])",
           L"{\"a\":1,\"c\":{\"a\":3},\"1\":\"one\"}", "JSON.stringify with an array replacer: listed names in order, once");
    expect(CATCH(L"var o = {}; o.self = o; JSON.stringify(o)"), L"TypeError 5034",
           "JSON.stringify of a circular object: TypeError");
    expect(CATCH(L"var a = []; a.push(a); JSON.stringify(a)"), L"TypeError 5034",
           "JSON.stringify of a circular array: TypeError");
    expect(CATCH(L"JSON.parse('{\"a\": }')"), L"SyntaxError 1014", "JSON.parse of a bad value: SyntaxError");
    expect(CATCH(L"JSON.parse('[1] x')"), L"SyntaxError 1014", "JSON.parse with trailing text: SyntaxError");
    expect(CATCH(L"JSON.parse('\"abc')"), L"SyntaxError 1015", "JSON.parse of an unterminated string: SyntaxError");
    expect(CATCH(L"decodeURI('%E0%A4%A')"), L"URIError 5025", "decodeURI of a bad hex sequence: URIError");
    expect(CATCH(L"decodeURIComponent('%C0')"), L"URIError 5025", "decodeURIComponent of a cut UTF-8 sequence: URIError");
    expect(L"decodeURI('%E2%82%AC')", L"\x20ac", "decodeURI of a good sequence");
    expect(CATCH(L"Math.max.apply(null, 5)"), L"TypeError 5028", "apply with a non-array: TypeError");
    expect(L"(function() { return arguments.length; }).apply(null, null)", L"0", "apply with null: no arguments");
    expect(L"typeof Function.prototype()", L"undefined", "Function.prototype() returns undefined");
    expect(CATCH(L"(function() { arguments(); })()"), L"TypeError 5002", "calling the arguments object: TypeError");
    expect(L"['a'.localeCompare('b'), 'b'.localeCompare('a'), 'a'.localeCompare('a'), 'a'.localeCompare('B')].join()",
           L"-1,1,0,-1", "localeCompare compares in the user's locale");
    expect(L"[ArrayBuffer.isView(new DataView(new ArrayBuffer(4))), ArrayBuffer.isView(new ArrayBuffer(4)), ArrayBuffer.isView({})].join()",
           L"true,false,false", "ArrayBuffer.isView");
    expect(L"var d = GetObject('', 'Scripting.Dictionary'); d.Add('k', 'v'); d.Item('k') + d.Count",
           L"v1", "GetObject(\"\", class) creates an object");
    expect(CATCH(L"GetObject(undefined, 'Scripting.Dictionary')"), L"Error 429",
           "GetObject(, class) with nothing running: cannot create object");

    IActiveScript_Close(script);
    parse->lpVtbl->Release(parse);
    IActiveScript_Release(script);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
