/* test/msstimeout-gate.sh probe (wine-sg 7723): the Script Control's Timeout. A script that runs longer
 * than Timeout milliseconds is stopped: the call fails with E_FAIL, the Error object says "The script was
 * aborted because execution exceeded the specified timeout period.", the Timeout event fires (then the
 * Error event), and the control runs the next script. Timeout -1 lets a script run on; a script that ends
 * in time is not touched. VBScript and JScript; AllowUI False (no dialog). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)

static const CLSID CLSID_ScriptControl_ = {0x0e59f1d5, 0x1fbe, 0x11d0, {0x8f, 0xf2, 0x00, 0xa0, 0xd1, 0x00, 0x38, 0xbc}};
static const IID DIID_DScriptControlSource_ = {0x8b167d60, 0x8605, 0x11d0, {0xab, 0xcb, 0x00, 0xa0, 0xc9, 0x0f, 0xff, 0xc0}};

static HRESULT dcall(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *arg, VARIANT *res)
{
    DISPID id, put = DISPID_PROPERTYPUT;
    DISPPARAMS dp = { arg, NULL, arg ? 1 : 0, 0 };
    HRESULT hr;
    if (FAILED(hr = IDispatch_GetIDsOfNames(d, &IID_NULL, (WCHAR **)&name, 1, 0, &id))) return hr;
    if (flags == DISPATCH_PROPERTYPUT) { dp.cNamedArgs = 1; dp.rgdispidNamedArgs = &put; }
    return IDispatch_Invoke(d, id, &IID_NULL, 0, flags, &dp, res, NULL, NULL);
}

static HRESULT put_str(IDispatch *d, const WCHAR *name, const WCHAR *v)
{
    VARIANT a; HRESULT hr;
    V_VT(&a) = VT_BSTR; V_BSTR(&a) = SysAllocString(v);
    hr = dcall(d, name, DISPATCH_PROPERTYPUT, &a, NULL);
    VariantClear(&a);
    return hr;
}

static HRESULT put_i4(IDispatch *d, const WCHAR *name, LONG v)
{
    VARIANT a; V_VT(&a) = VT_I4; V_I4(&a) = v;
    return dcall(d, name, DISPATCH_PROPERTYPUT, &a, NULL);
}

static HRESULT put_bool(IDispatch *d, const WCHAR *name, BOOL v)
{
    VARIANT a; V_VT(&a) = VT_BOOL; V_BOOL(&a) = v ? VARIANT_TRUE : VARIANT_FALSE;
    return dcall(d, name, DISPATCH_PROPERTYPUT, &a, NULL);
}

/* ExecuteStatement through IDispatch: a failure comes back as DISP_E_EXCEPTION and its EXCEPINFO */
static HRESULT exec(IDispatch *d, const WCHAR *code)
{
    const WCHAR *name = L"ExecuteStatement";
    DISPPARAMS dp = { NULL, NULL, 1, 0 };
    EXCEPINFO ei;
    VARIANT a;
    DISPID id;
    HRESULT hr;

    V_VT(&a) = VT_BSTR; V_BSTR(&a) = SysAllocString(code);
    dp.rgvarg = &a;
    memset(&ei, 0, sizeof(ei));
    if (SUCCEEDED(hr = IDispatch_GetIDsOfNames(d, &IID_NULL, (WCHAR **)&name, 1, 0, &id)))
        hr = IDispatch_Invoke(d, id, &IID_NULL, 0, DISPATCH_METHOD, &dp, NULL, &ei, NULL);
    VariantClear(&a);
    if (hr == DISP_E_EXCEPTION)
    {
        if (ei.pfnDeferredFillIn) ei.pfnDeferredFillIn(&ei);
        hr = ei.scode ? ei.scode : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_DISPATCH, ei.wCode);
        SysFreeString(ei.bstrSource); SysFreeString(ei.bstrDescription); SysFreeString(ei.bstrHelpFile);
    }
    return hr;
}

static void error_text(IDispatch *d, WCHAR *buf, int len)
{
    VARIANT err, desc;
    buf[0] = 0;
    VariantInit(&err); VariantInit(&desc);
    if (SUCCEEDED(dcall(d, L"Error", DISPATCH_PROPERTYGET, NULL, &err)) && V_VT(&err) == VT_DISPATCH)
    {
        if (SUCCEEDED(dcall(V_DISPATCH(&err), L"Description", DISPATCH_PROPERTYGET, NULL, &desc)) && V_VT(&desc) == VT_BSTR)
            lstrcpynW(buf, V_BSTR(&desc), len);
        dcall(V_DISPATCH(&err), L"Clear", DISPATCH_METHOD, NULL, NULL);
        IDispatch_Release(V_DISPATCH(&err));
    }
    VariantClear(&desc);
}

struct sink { IDispatch IDispatch_iface; int timeouts, errors, order_ok; };
static HRESULT WINAPI s_qi(IDispatch *iface, REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch) || IsEqualGUID(riid, &DIID_DScriptControlSource_))
    { *obj = iface; return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI s_addref(IDispatch *iface) { return 2; }
static ULONG WINAPI s_release(IDispatch *iface) { return 1; }
static HRESULT WINAPI s_count(IDispatch *iface, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI s_ti(IDispatch *iface, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI s_ids(IDispatch *iface, REFIID r, WCHAR **n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI s_invoke(IDispatch *iface, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{
    struct sink *s = (struct sink *)iface;
    if (id == 0xbb9) s->timeouts++;
    else if (id == 0xbb8) { if (s->timeouts) s->order_ok = 1; s->errors++; }
    else return DISP_E_MEMBERNOTFOUND;
    return S_OK;
}
static IDispatchVtbl sink_vtbl = { s_qi, s_addref, s_release, s_count, s_ti, s_ids, s_invoke };

static void run_language(const WCHAR *lang, const WCHAR *forever, const WCHAR *short_loop)
{
    IDispatch *sc = NULL;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp = NULL;
    struct sink sink = { { &sink_vtbl } };
    DWORD cookie = 0, t0, ms;
    WCHAR text[256];
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_ScriptControl_, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&sc);
    if (FAILED(hr)) { CHECK(0, "%ls: ScriptControl %#lx", lang, hr); return; }
    put_str(sc, L"Language", lang);
    put_bool(sc, L"AllowUI", FALSE);
    if (SUCCEEDED(IDispatch_QueryInterface(sc, &IID_IConnectionPointContainer, (void **)&cpc)))
    {
        IConnectionPointContainer_FindConnectionPoint(cpc, &DIID_DScriptControlSource_, &cp);
        if (cp) IConnectionPoint_Advise(cp, (IUnknown *)&sink.IDispatch_iface, &cookie);
        IConnectionPointContainer_Release(cpc);
    }

    put_i4(sc, L"Timeout", 500);
    t0 = GetTickCount();
    hr = exec(sc, forever);
    ms = GetTickCount() - t0;
    error_text(sc, text, ARRAYSIZE(text));
    CHECK(hr == E_FAIL && ms >= 450 && ms < 8000, "%ls: an endless loop is stopped after Timeout 500 (hr %#lx after %lu ms)", lang, hr, ms);
    CHECK(wcsstr(text, L"timeout period") != NULL, "%ls: the error says so (%ls)", lang, text);
    CHECK(sink.timeouts == 1, "%ls: the Timeout event fired (%d)", lang, sink.timeouts);
    CHECK(sink.errors >= 1 && sink.order_ok, "%ls: then the Error event (%d)", lang, sink.errors);

    sink.timeouts = sink.errors = 0;
    put_i4(sc, L"Timeout", 5000);
    hr = exec(sc, short_loop);
    CHECK(hr == S_OK && !sink.timeouts, "%ls: the next script, which ends in time, runs (hr %#lx, %d timeouts)", lang, hr, sink.timeouts);

    put_i4(sc, L"Timeout", -1);
    t0 = GetTickCount();
    hr = exec(sc, short_loop);
    CHECK(hr == S_OK, "%ls: Timeout -1: it runs (hr %#lx, %lu ms)", lang, hr, GetTickCount() - t0);

    if (cp) { IConnectionPoint_Unadvise(cp, cookie); IConnectionPoint_Release(cp); }
    IDispatch_Release(sc);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    run_language(L"VBScript", L"Do: Loop", L"For i = 1 To 200000: Next");
    run_language(L"JScript", L"while (true) {}", L"for (var i = 0; i < 200000; i++) {}");
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return 0;
}
