#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <activscp.h>
#include <mshtml.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_JScript_ = {0xf414c260,0x6ac0,0x11cf,{0xb6,0xd1,0x00,0xaa,0x00,0xbb,0xbb,0x58}};
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};

static const IID IID_IActiveScriptProperty_ = {0x4954e0d0,0xfbc7,0x11d1,{0x84,0x10,0x00,0x60,0x08,0xc3,0xfb,0xfc}};
static const IID IID_IActiveScriptParse64_ = {0xc7ef7658,0xe1ee,0x480e,{0x97,0xea,0xd5,0x2c,0xb4,0xd7,0x6d,0x17}};
#define IID_IActiveScriptProperty IID_IActiveScriptProperty_
#define IID_IActiveScriptParse64 IID_IActiveScriptParse64_
typedef struct { IActiveScriptSite iface; } Site;
static HRESULT WINAPI S_QI(IActiveScriptSite *i, REFIID r, void **o){ if(IsEqualGUID(r,&IID_IUnknown)||IsEqualGUID(r,&IID_IActiveScriptSite)){*o=i;return S_OK;} *o=NULL; return E_NOINTERFACE; }
static ULONG WINAPI S_AR(IActiveScriptSite *i){ return 2; }
static HRESULT WINAPI S_GL(IActiveScriptSite *i, LCID *l){ return E_NOTIMPL; }
static HRESULT WINAPI S_GI(IActiveScriptSite *i, LPCOLESTR n, DWORD m, IUnknown **u, ITypeInfo **t){ return TYPE_E_ELEMENTNOTFOUND; }
static HRESULT WINAPI S_GD(IActiveScriptSite *i, BSTR *b){ return E_NOTIMPL; }
static HRESULT WINAPI S_OST(IActiveScriptSite *i, const VARIANT *v, const EXCEPINFO *e){ return S_OK; }
static HRESULT WINAPI S_SC(IActiveScriptSite *i, SCRIPTSTATE s){ return S_OK; }
static HRESULT WINAPI S_OSE(IActiveScriptSite *i, IActiveScriptError *e){ return S_OK; }
static HRESULT WINAPI S_E(IActiveScriptSite *i){ return S_OK; }
static IActiveScriptSiteVtbl vt = { S_QI, S_AR, S_AR, S_GL, S_GI, S_GD, S_OST, S_SC, S_OSE, S_E, S_E };
static Site site = { { &vt } };

static void engine(const char *name, int ver, const WCHAR *expr)
{
    IActiveScript *as; IActiveScriptParse64 *ap; IActiveScriptProperty *prop; VARIANT v, res; EXCEPINFO ei; HRESULT hr;
    hr = CoCreateInstance(&CLSID_JScript_, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void**)&as); if (hr) { P("%s=CREATE %08lx\n", name, hr); return; }
    IActiveScript_QueryInterface(as, &IID_IActiveScriptProperty, (void**)&prop);
    V_VT(&v) = VT_I4; V_I4(&v) = ver;
    hr = IActiveScriptProperty_SetProperty(prop, SCRIPTPROP_INVOKEVERSIONING, NULL, &v);
    if (hr) P("%s=SETPROP %08lx\n", name, hr);
    IActiveScript_QueryInterface(as, &IID_IActiveScriptParse64, (void**)&ap);
    IActiveScriptParse64_InitNew(ap);
    IActiveScript_SetScriptSite(as, &site.iface);
    IActiveScript_SetScriptState(as, SCRIPTSTATE_CONNECTED);
    VariantInit(&res); memset(&ei, 0, sizeof(ei));
    hr = IActiveScriptParse64_ParseScriptText(ap, expr, NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, &res, &ei);
    if (SUCCEEDED(hr) && V_VT(&res) == VT_BSTR) P("%s=%ls\n", name, V_BSTR(&res));
    else P("%s=ERR %08lx vt %d\n", name, hr, V_VT(&res));
    VariantClear(&res);
    IActiveScriptProperty_Release(prop); IActiveScriptParse64_Release(ap);
    IActiveScript_Close(as); IActiveScript_Release(as);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static void html(const char *name, int mode)
{
    IHTMLDocument2 *doc; IHTMLWindow2 *win; IPersistStreamInit *psi; IStream *stream; HGLOBAL mem; char *a; BSTR b, lang; VARIANT res; char page[600];
    sprintf(page, "<!DOCTYPE html><html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=%d\"></head><body></body></html>", mode);
    CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void**)&doc);
    mem = GlobalAlloc(0, strlen(page)); a = GlobalLock(mem); memcpy(a, page, strlen(page)); GlobalUnlock(mem);
    CreateStreamOnHGlobal(mem, TRUE, &stream);
    IHTMLDocument2_QueryInterface(doc, &IID_IPersistStreamInit, (void**)&psi);
    IPersistStreamInit_InitNew(psi); IPersistStreamInit_Load(psi, stream);
    IPersistStreamInit_Release(psi); IStream_Release(stream);
    pump(1000);
    IHTMLDocument2_get_parentWindow(doc, &win);
    b = SysAllocString(L"document.title=typeof ArrayBuffer+','+typeof Int8Array+','+typeof Float64Array+','+typeof DataView+','+typeof Math.trunc+','+typeof Object.setPrototypeOf;");
    lang = SysAllocString(L"jscript"); VariantInit(&res);
    IHTMLWindow2_execScript(win, b, lang, &res);
    pump(100);
    b = NULL; IHTMLDocument2_get_title(doc, &b);
    P("%s=%ls\n", name, b ? b : L"");
    IHTMLWindow2_Release(win); IHTMLDocument2_Release(doc);
}

int main(void)
{
    static const WCHAR t[] = L"typeof ArrayBuffer+','+typeof Int8Array+','+typeof DataView";
    static const WCHAR m[] = L"[Math.trunc(-4.7),Math.sign(-3),Math.cbrt(27),Math.log2(8),Math.hypot(3,4),Math.imul(65536,65536),Math.clz32(1),Math.fround(5.5),Math.expm1(0),Math.cosh(0)].join(',')";
    static const WCHAR mt[] = L"typeof Math.trunc+','+typeof Math.imul";
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    engine("E_ES5", 0x502, t);
    engine("E_ES5_NOTYPED", 0x502 | 0x800, t);
    engine("E_ES6", 0x503, t);
    engine("E_ES6_MATH", 0x503, mt);
    engine("E_NEXT_MATH", 0x504, m);
    engine("E_SETPROTO6", 0x503, L"typeof Object.setPrototypeOf");
    engine("E_SETPROTO5", 0x502, L"typeof Object.setPrototypeOf");
    engine("E_SETPROTO_USE", 0x503, L"var p={a:1}; var o={}; var r=Object.setPrototypeOf(o,p); var e1='no'; try{Object.setPrototypeOf(undefined,p);}catch(x){e1='thrown';} var e2='no'; try{Object.setPrototypeOf(o,5);}catch(x){e2='thrown';} (r===o)+','+o.a+','+(Object.getPrototypeOf(o)===p)+','+(Object.setPrototypeOf(5,null)===5)+','+e1+','+e2");
    html("H9", 9); html("H10", 10); html("H11", 11);
    P("DONE\n");
    return 0;
}
