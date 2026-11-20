/* jshostacc-probe: JScript's __defineGetter__ / __defineSetter__ take a host's function object (an IDispatch
 * the host gives the script) as the accessor (wine-sg 8220; test/jshostacc-gate.sh). They failed with
 * E_NOTIMPL ("getter is not JS object"). The host's named item "host" has "fn" (returns 42) and "store"
 * (records its argument); the script defines o.x with fn and o.y with store, reads o.x and writes o.y = 7. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <activscp.h>
#include <stdio.h>

#ifndef DISPID_THIS
#define DISPID_THIS (-613)
#endif

static const IID IID_Property_ = {0x4954e0d0,0xfbc7,0x11d1,{0x84,0x10,0x00,0x60,0x08,0xc3,0xfb,0xfc}};
static const CLSID CLSID_JScript_ = {0xf414c260,0x6ac0,0x11cf,{0xb6,0xd1,0x00,0xaa,0x00,0xbb,0xbb,0x58}};
#ifdef _WIN64
static const IID IID_Parse_ = {0xc7ef7658,0xe1ee,0x480e,{0x97,0xea,0xd5,0x2c,0xb4,0xd7,0x6d,0x17}};
typedef IActiveScriptParse64 Parse;
#define Parse_InitNew IActiveScriptParse64_InitNew
#define Parse_ParseScriptText IActiveScriptParse64_ParseScriptText
#define Parse_Release IActiveScriptParse64_Release
#else
static const IID IID_Parse_ = {0xbb1a2ae2,0xa4f9,0x11cf,{0x8f,0x20,0x00,0x80,0x5f,0x2c,0xd0,0x64}};
typedef IActiveScriptParse32 Parse;
#define Parse_InitNew IActiveScriptParse32_InitNew
#define Parse_ParseScriptText IActiveScriptParse32_ParseScriptText
#define Parse_Release IActiveScriptParse32_Release
#endif

static int g_fn_calls, g_stored = -1, g_had_this;

/* a minimal IDispatch: kind 0 the host object, 1 fn, 2 store */
typedef struct { IDispatch iface; int kind; } Disp;
static Disp g_host, g_fn, g_store;

static HRESULT WINAPI D_QI(IDispatch *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IDispatch)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI D_AR(IDispatch *i) { return 2; }
static HRESULT WINAPI D_GTIC(IDispatch *i, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI D_GTI(IDispatch *i, UINT n, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI D_GIDs(IDispatch *i, REFIID r, LPOLESTR *names, UINT c, LCID l, DISPID *ids)
{
    Disp *d = (Disp *)i;
    if (d->kind == 0 && !lstrcmpW(names[0], L"fn")) { ids[0] = 1; return S_OK; }
    if (d->kind == 0 && !lstrcmpW(names[0], L"store")) { ids[0] = 2; return S_OK; }
    ids[0] = DISPID_UNKNOWN; return DISP_E_UNKNOWNNAME;
}
static HRESULT WINAPI D_Invoke(IDispatch *i, DISPID id, REFIID r, LCID l, WORD flags, DISPPARAMS *dp, VARIANT *res,
                               EXCEPINFO *ei, UINT *err)
{
    Disp *d = (Disp *)i;
    UINT k;

    if (d->kind == 0)
    {
        if (!(flags & DISPATCH_PROPERTYGET) || (id != 1 && id != 2)) return DISP_E_MEMBERNOTFOUND;
        V_VT(res) = VT_DISPATCH;
        V_DISPATCH(res) = id == 1 ? &g_fn.iface : &g_store.iface;
        return S_OK;
    }
    if (id != DISPID_VALUE) return DISP_E_MEMBERNOTFOUND;
    for (k = 0; k < dp->cNamedArgs; k++) if (dp->rgdispidNamedArgs[k] == DISPID_THIS) g_had_this = 1;
    if (d->kind == 1)
    {
        g_fn_calls++;
        if (res) { V_VT(res) = VT_I4; V_I4(res) = 42; }
        return S_OK;
    }
    /* store: the first positional argument (after the named ones) */
    if (dp->cArgs > dp->cNamedArgs)
    {
        VARIANT v;
        VariantInit(&v);
        if (SUCCEEDED(VariantChangeType(&v, &dp->rgvarg[dp->cArgs - 1], 0, VT_I4))) g_stored = V_I4(&v);
    }
    if (res) V_VT(res) = VT_EMPTY;
    return S_OK;
}
static IDispatchVtbl dvt = { D_QI, D_AR, D_AR, D_GTIC, D_GTI, D_GIDs, D_Invoke };

static IActiveScriptSite g_site;
static HRESULT WINAPI S_QI(IActiveScriptSite *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IActiveScriptSite)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI S_AR(IActiveScriptSite *i) { return 2; }
static HRESULT WINAPI S_GL(IActiveScriptSite *i, LCID *l) { return E_NOTIMPL; }
static HRESULT WINAPI S_GI(IActiveScriptSite *i, LPCOLESTR n, DWORD m, IUnknown **u, ITypeInfo **t)
{
    if (lstrcmpW(n, L"host")) return TYPE_E_ELEMENTNOTFOUND;
    if (m & SCRIPTINFO_IUNKNOWN) *u = (IUnknown *)&g_host.iface;
    if (m & SCRIPTINFO_ITYPEINFO) *t = NULL;
    return S_OK;
}
static HRESULT WINAPI S_GD(IActiveScriptSite *i, BSTR *b) { return E_NOTIMPL; }
static HRESULT WINAPI S_OST(IActiveScriptSite *i, const VARIANT *v, const EXCEPINFO *e) { return S_OK; }
static HRESULT WINAPI S_SC(IActiveScriptSite *i, SCRIPTSTATE s) { return S_OK; }
static HRESULT WINAPI S_OSE(IActiveScriptSite *i, IActiveScriptError *e) { return S_OK; }
static HRESULT WINAPI S_OE(IActiveScriptSite *i) { return S_OK; }
static IActiveScriptSiteVtbl svt = { S_QI, S_AR, S_AR, S_GL, S_GI, S_GD, S_OST, S_SC, S_OSE, S_OE, S_OE };

int main(void)
{
    IActiveScript *s; Parse *p; EXCEPINFO ei; HRESULT hr; VARIANT v;

    g_host.iface.lpVtbl = g_fn.iface.lpVtbl = g_store.iface.lpVtbl = &dvt;
    g_host.kind = 0; g_fn.kind = 1; g_store.kind = 2;
    g_site.lpVtbl = &svt;
    CoInitialize(NULL);
    setvbuf(stdout, NULL, _IONBF, 0);
    if (FAILED(CoCreateInstance(&CLSID_JScript_, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void **)&s)))
    {
        printf("nocreate\n");
        return 1;
    }
    IActiveScript_QueryInterface(s, &IID_Parse_, (void **)&p);
    {
        /* the accessors exist in the IE11-document-mode engine: HTML | ES6 (0x400 | 0x103), as mshtml asks */
        IActiveScriptProperty *prop = NULL;
        VARIANT ver;
        V_VT(&ver) = VT_I4;
        V_I4(&ver) = 0x400 | 0x103;
        if (SUCCEEDED(IActiveScript_QueryInterface(s, &IID_Property_, (void **)&prop)))
        {
            IActiveScriptProperty_SetProperty(prop, SCRIPTPROP_INVOKEVERSIONING, NULL, &ver);
            IActiveScriptProperty_Release(prop);
        }
    }
    Parse_InitNew(p);
    IActiveScript_SetScriptSite(s, &g_site);
    IActiveScript_AddNamedItem(s, L"host", SCRIPTITEM_ISVISIBLE);
    IActiveScript_SetScriptState(s, SCRIPTSTATE_STARTED);

    memset(&ei, 0, sizeof(ei));
    hr = Parse_ParseScriptText(p, L"var o = {}; o.__defineGetter__('x', host.fn); o.__defineSetter__('y', host.store);",
                               NULL, NULL, NULL, 0, 0, 0, NULL, &ei);
    printf("define=%d (%08lx)\n", SUCCEEDED(hr), hr);
    VariantInit(&v);
    hr = Parse_ParseScriptText(p, L"o.x", NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, &v, &ei);
    printf("getter=%d (%08lx vt %d %ld calls %d)\n", SUCCEEDED(hr) && V_VT(&v) == VT_I4 && V_I4(&v) == 42 && g_fn_calls == 1,
           hr, V_VT(&v), V_VT(&v) == VT_I4 ? V_I4(&v) : -1, g_fn_calls);
    printf("getter_this=%d\n", g_had_this);
    hr = Parse_ParseScriptText(p, L"o.y = 7;", NULL, NULL, NULL, 0, 0, 0, NULL, &ei);
    printf("setter=%d (%08lx stored %d)\n", SUCCEEDED(hr) && g_stored == 7, hr, g_stored);
    VariantClear(&v);
    hr = Parse_ParseScriptText(p, L"typeof Object.getOwnPropertyDescriptor(o, 'x').get", NULL, NULL, NULL, 0, 0,
                               SCRIPTTEXT_ISEXPRESSION, &v, &ei);
    printf("descriptor=%d (%ls)\n", SUCCEEDED(hr) && V_VT(&v) == VT_BSTR && !lstrcmpW(V_BSTR(&v), L"function"),
           V_VT(&v) == VT_BSTR ? V_BSTR(&v) : L"?");
    VariantClear(&v);
    IActiveScript_Close(s);
    Parse_Release(p);
    IActiveScript_Release(s);
    printf("DONE\n");
    return 0;
}
