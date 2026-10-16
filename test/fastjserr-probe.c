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
static EXCEPINFO last_ei; static BOOL got_err;
static HRESULT WINAPI S_OSE(IActiveScriptSite *i, IActiveScriptError *e){ memset(&last_ei, 0, sizeof(last_ei)); IActiveScriptError_GetExceptionInfo(e, &last_ei); got_err = TRUE; return S_OK; }
static HRESULT WINAPI S_E(IActiveScriptSite *i){ return S_OK; }
static IActiveScriptSiteVtbl vt = { S_QI, S_AR, S_AR, S_GL, S_GI, S_GD, S_OST, S_SC, S_OSE, S_E, S_E };
static Site site = { { &vt } };


static void run(const char *name, int ver, const WCHAR *script, const WCHAR *expr)
{
    IActiveScript *as; IActiveScriptParse64 *ap; IActiveScriptProperty *prop; VARIANT v, res; EXCEPINFO ei; HRESULT hr, hr2 = 0;
    hr = CoCreateInstance(&CLSID_JScript_, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void**)&as); if (hr) { P("%s=CREATE %08lx\n", name, hr); return; }
    IActiveScript_QueryInterface(as, &IID_IActiveScriptProperty, (void**)&prop);
    V_VT(&v) = VT_I4; V_I4(&v) = ver;
    IActiveScriptProperty_SetProperty(prop, SCRIPTPROP_INVOKEVERSIONING, NULL, &v);
    IActiveScript_QueryInterface(as, &IID_IActiveScriptParse64, (void**)&ap);
    IActiveScriptParse64_InitNew(ap);
    IActiveScript_SetScriptSite(as, &site.iface);
    IActiveScript_SetScriptState(as, SCRIPTSTATE_CONNECTED);
    VariantInit(&res); memset(&ei, 0, sizeof(ei)); got_err = FALSE;
    hr = IActiveScriptParse64_ParseScriptText(ap, script, NULL, NULL, NULL, 0, 0, 0, NULL, &ei);
    if (got_err) {
        P("%s=err %08lx %d %ls\n", name, (DWORD)last_ei.scode, last_ei.wReserved == (WORD)GetUserDefaultLCID() && last_ei.wReserved != 0, last_ei.bstrDescription ? last_ei.bstrDescription : L"-");
        SysFreeString(last_ei.bstrSource); SysFreeString(last_ei.bstrDescription);
    } else if (expr) {
        hr2 = IActiveScriptParse64_ParseScriptText(ap, expr, NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, &res, &ei);
        if (SUCCEEDED(hr2) && V_VT(&res) == VT_BSTR) P("%s=ok %ls\n", name, V_BSTR(&res));
        else P("%s=EXPR %08lx\n", name, hr2);
    } else P("%s=ok %08lx\n", name, hr);
    VariantClear(&res);
    IActiveScriptProperty_Release(prop); IActiveScriptParse64_Release(ap);
    IActiveScript_Close(as); IActiveScript_Release(as);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    run("E1", 0x502, L"var a=1;\nif(a\n-->0) a=5;\n", NULL);
    run("E2", 0x502, L"v\xfeff" L"ar a = 1;", NULL);
    run("E3", 0x502, L"var a=1 b=2;", NULL);
    run("E4", 0x502, L"var a=1,b=1;\na\n++b\n", L"a+','+b");
    run("E5", 0x502, L"var a=1,b=5; a++ ; a\n--b;", L"a+','+b");
    run("E6", 0x502, L"var e=new Error(); e.number=-2146828279; e.description='test'; throw e;", NULL);
    run("E7", 0x502, L"var e={number:-2146828279, description:'test'}; throw e;", NULL);
    run("E8", 0x502, L"throw new Error();", NULL);
    P("DONE\n");
    return 0;
}
