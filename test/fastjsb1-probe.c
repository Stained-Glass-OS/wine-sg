#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <activscp.h>
#include <dispex.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)

static const CLSID CLSID_JScript_ = {0xf414c260,0x6ac0,0x11cf,{0xb6,0xd1,0x00,0xaa,0x00,0xbb,0xbb,0x58}};
static const IID IID_IActiveScriptProperty_ = {0x4954e0d0,0xfbc7,0x11d1,{0x84,0x10,0x00,0x60,0x08,0xc3,0xfb,0xfc}};
static const IID IID_IDispatchEx_ = {0xa6ef9860,0xc720,0x11d0,{0x93,0x37,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
#define IID_IDispatchEx IID_IDispatchEx_
static const IID IID_IBindEventHandler_ = {0x63cdbcb0,0xc1b1,0x11d0,{0x93,0x36,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
static const IID IID_IVariantChangeType_ = {0xa6ef9862,0xc720,0x11d0,{0x93,0x37,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
#ifdef _WIN64
static const IID IID_Parse_ = {0xc7ef7658,0xe1ee,0x480e,{0x97,0xea,0xd5,0x2c,0xb4,0xd7,0x6d,0x17}};
typedef IActiveScriptParse64 Parse;
#define Parse_InitNew IActiveScriptParse64_InitNew
#define Parse_ParseScriptText IActiveScriptParse64_ParseScriptText
#define Parse_AddScriptlet IActiveScriptParse64_AddScriptlet
#define Parse_Release IActiveScriptParse64_Release
#else
static const IID IID_Parse_ = {0xbb1a2ae2,0xa4f9,0x11cf,{0x8f,0x20,0x00,0x80,0x5f,0x2c,0xd0,0x64}};
typedef IActiveScriptParse32 Parse;
#define Parse_InitNew IActiveScriptParse32_InitNew
#define Parse_ParseScriptText IActiveScriptParse32_ParseScriptText
#define Parse_AddScriptlet IActiveScriptParse32_AddScriptlet
#define Parse_Release IActiveScriptParse32_Release
#endif

static const GUID guid_scrrun = {0x420B2830,0xE718,0x11CF,{0x89,0x3D,0x00,0xA0,0xC9,0x05,0x42,0x28}};
static const GUID guid_bogus = {0x11111111,0x2222,0x3333,{0,1,2,3,4,5,6,7}};
typedef struct IVCT IVCT;
typedef struct { HRESULT (WINAPI *QueryInterface)(IVCT*,REFIID,void**); ULONG (WINAPI *AddRef)(IVCT*); ULONG (WINAPI *Release)(IVCT*);
    HRESULT (WINAPI *ChangeType)(IVCT*, VARIANT*, VARIANT*, LCID, VARTYPE); } IVCTVtbl;
struct IVCT { const IVCTVtbl *lpVtbl; };

/* ---- the host object: an IDispatchEx with two enumerable data members and a few methods ---- */
static IActiveScript *g_script;
static IDispatch *g_handler;
static WCHAR g_handler_name[64];
static struct { WCHAR name[16]; int val; } props[8] = { { L"alpha", 11 }, { L"beta", 22 } };
static int nprops = 2;

static HRESULT WINAPI H_QI(IDispatchEx *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IDispatch) || IsEqualGUID(r, &IID_IDispatchEx)) { *o = i; return S_OK; }
    if (IsEqualGUID(r, &IID_IBindEventHandler_)) { *o = (char*)i + sizeof(void*); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI H_AR(IDispatchEx *i) { return 2; }
static HRESULT WINAPI H_GTIC(IDispatchEx *i, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI H_GTI(IDispatchEx *i, UINT n, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI H_GIDS(IDispatchEx *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return IDispatchEx_GetDispID(i, n[0], fdexNameCaseSensitive, d); }
static HRESULT WINAPI H_Invoke(IDispatchEx *i, DISPID d, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{ return IDispatchEx_InvokeEx(i, d, l, f, p, v, e, NULL); }
static HRESULT WINAPI H_GetDispID(IDispatchEx *i, BSTR name, DWORD grfdex, DISPID *id)
{
    static const WCHAR * const meth[] = { L"vdate", L"vi8", L"vdec", L"verr", L"varr", L"state" };
    int k;
    for (k = 0; k < nprops; k++) if (!wcscmp(props[k].name, name)) { *id = 100 + k; return S_OK; }
    for (k = 0; k < 6; k++) if (!wcscmp(meth[k], name)) { *id = 10 + k; return S_OK; }
    if ((grfdex & fdexNameEnsure) && nprops < 8) {
        wcscpy(props[nprops].name, name); props[nprops].val = 0; *id = 100 + nprops++; return S_OK;
    }
    return DISP_E_UNKNOWNNAME;
}
static HRESULT WINAPI H_InvokeEx(IDispatchEx *i, DISPID id, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, IServiceProvider *sp)
{
    if (id >= 100 && id < 100 + nprops) {
        if (f & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) {
            VARIANT t; VariantInit(&t);
            if (SUCCEEDED(VariantChangeType(&t, &p->rgvarg[0], 0, VT_I4))) props[id - 100].val = V_I4(&t);
            return S_OK;
        }
        if (v) { V_VT(v) = VT_I4; V_I4(v) = props[id - 100].val; }
        return S_OK;
    }
    if (!v) return S_OK;
    switch (id) {
    case 10: V_VT(v) = VT_DATE; V_DATE(v) = 2.5; return S_OK;
    case 11: V_VT(v) = VT_I8; V_I8(v) = 12345678901LL; return S_OK;
    case 12: V_VT(v) = VT_DECIMAL; VarDecFromR8(1.5, &V_DECIMAL(v)); V_VT(v) = VT_DECIMAL; return S_OK;
    case 13: V_VT(v) = VT_ERROR; V_ERROR(v) = DISP_E_PARAMNOTFOUND; return S_OK;
    case 14: {
        SAFEARRAYBOUND b = { 2, 0 }; SAFEARRAY *sa = SafeArrayCreate(VT_VARIANT, 1, &b); LONG ix; VARIANT e1;
        VariantInit(&e1); V_VT(&e1) = VT_I4;
        for (ix = 0; ix < 2; ix++) { V_I4(&e1) = 7 + ix; SafeArrayPutElement(sa, &ix, &e1); }
        V_VT(v) = VT_ARRAY | VT_VARIANT; V_ARRAY(v) = sa; return S_OK;
    }
    case 15: { SCRIPTTHREADSTATE st = 99; HRESULT hr = IActiveScript_GetScriptThreadState(g_script, SCRIPTTHREADID_CURRENT, &st);
               V_VT(v) = VT_I4; V_I4(v) = hr ? -1 : (int)st; return S_OK; }
    }
    return DISP_E_MEMBERNOTFOUND;
}
static HRESULT WINAPI H_Del(IDispatchEx *i, BSTR n, DWORD g) { return S_OK; }
static HRESULT WINAPI H_DelId(IDispatchEx *i, DISPID d) { return S_OK; }
static HRESULT WINAPI H_GMP(IDispatchEx *i, DISPID d, DWORD g, DWORD *o) { return E_NOTIMPL; }
static HRESULT WINAPI H_GMN(IDispatchEx *i, DISPID d, BSTR *n)
{
    if (d < 100 || d >= 100 + nprops) return DISP_E_UNKNOWNNAME;
    *n = SysAllocString(props[d - 100].name); return S_OK;
}
static HRESULT WINAPI H_GNDI(IDispatchEx *i, DWORD g, DISPID d, DISPID *n)
{
    int next = (d == DISPID_STARTENUM) ? 0 : (d - 100 + 1);
    if (next >= nprops) { *n = DISPID_STARTENUM; return S_FALSE; }
    *n = 100 + next; return S_OK;
}
static HRESULT WINAPI H_GNSP(IDispatchEx *i, IUnknown **p) { *p = NULL; return E_NOTIMPL; }
static IDispatchExVtbl hvt = { H_QI, H_AR, H_AR, H_GTIC, H_GTI, H_GIDS, H_Invoke, H_GetDispID, H_InvokeEx, H_Del, H_DelId, H_GMP, H_GMN, H_GNDI, H_GNSP };

/* IBindEventHandler lives in the second pointer slot of the host */
typedef struct { const IDispatchExVtbl *ex; const void *bh; } Host;
static HRESULT WINAPI B_QI(IUnknown *i, REFIID r, void **o) { return H_QI((IDispatchEx*)((char*)i - sizeof(void*)), r, o); }
static HRESULT WINAPI B_BindHandler(IUnknown *i, LPCOLESTR name, IDispatch *h)
{
    wcsncpy(g_handler_name, name, 63);
    if (g_handler) IDispatch_Release(g_handler);
    g_handler = h; IDispatch_AddRef(h);
    return S_OK;
}
static struct { HRESULT (WINAPI *qi)(IUnknown*,REFIID,void**); ULONG (WINAPI *ar)(IUnknown*); ULONG (WINAPI *rl)(IUnknown*);
    HRESULT (WINAPI *bind)(IUnknown*, LPCOLESTR, IDispatch*); } bvt = { B_QI, (void*)H_AR, (void*)H_AR, B_BindHandler };
static Host host = { &hvt, &bvt };

/* ---- the site ---- */
static int g_site_items;
static HRESULT WINAPI S_QI(IActiveScriptSite *i, REFIID r, void **o) { if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IActiveScriptSite)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI S_AR(IActiveScriptSite *i) { return 2; }
static HRESULT WINAPI S_GL(IActiveScriptSite *i, LCID *l) { return E_NOTIMPL; }
static HRESULT WINAPI S_GI(IActiveScriptSite *i, LPCOLESTR n, DWORD m, IUnknown **u, ITypeInfo **t)
{
    if (!wcscmp(n, L"hostex") && (m & SCRIPTINFO_IUNKNOWN)) { g_site_items++; *u = (IUnknown*)&host; return S_OK; }
    return TYPE_E_ELEMENTNOTFOUND;
}
static HRESULT WINAPI S_GD(IActiveScriptSite *i, BSTR *b) { return E_NOTIMPL; }
static HRESULT WINAPI S_OST(IActiveScriptSite *i, const VARIANT *v, const EXCEPINFO *e) { return S_OK; }
static HRESULT WINAPI S_SC(IActiveScriptSite *i, SCRIPTSTATE s) { return S_OK; }
static HRESULT WINAPI S_OSE(IActiveScriptSite *i, IActiveScriptError *e) { return S_OK; }
static HRESULT WINAPI S_E(IActiveScriptSite *i) { return S_OK; }
static IActiveScriptSiteVtbl svt = { S_QI, S_AR, S_AR, S_GL, S_GI, S_GD, S_OST, S_SC, S_OSE, S_E, S_E };
static struct { IActiveScriptSite iface; } site = { { &svt } };

typedef struct { IActiveScript *as; Parse *ap; } Eng;

static HRESULT eng_new(Eng *e, int with_host)
{
    HRESULT hr = CoCreateInstance(&CLSID_JScript_, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void**)&e->as);
    if (hr) return hr;
    {
        IActiveScriptProperty *pr; VARIANT pv;
        IActiveScript_QueryInterface(e->as, &IID_IActiveScriptProperty_, (void**)&pr);
        V_VT(&pv) = VT_I4; V_I4(&pv) = 0x503;
        IActiveScriptProperty_SetProperty(pr, SCRIPTPROP_INVOKEVERSIONING, NULL, &pv);
        IActiveScriptProperty_Release(pr);
    }
    IActiveScript_QueryInterface(e->as, &IID_Parse_, (void**)&e->ap);
    Parse_InitNew(e->ap);
    IActiveScript_SetScriptSite(e->as, &site.iface);
    if (with_host) IActiveScript_AddNamedItem(e->as, L"hostex", SCRIPTITEM_ISVISIBLE);
    IActiveScript_SetScriptState(e->as, SCRIPTSTATE_CONNECTED);
    g_script = e->as;
    return S_OK;
}
static void eng_free(Eng *e) { Parse_Release(e->ap); IActiveScript_Close(e->as); IActiveScript_Release(e->as); }

static HRESULT eval(Eng *e, const WCHAR *expr, VARIANT *res)
{
    EXCEPINFO ei; memset(&ei, 0, sizeof(ei)); VariantInit(res);
    return Parse_ParseScriptText(e->ap, expr, NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, res, &ei);
}
static void ev(Eng *e, const char *name, const WCHAR *expr)
{
    VARIANT res, s; HRESULT hr = eval(e, expr, &res);
    if (hr) { P("%s=ERR %08lx\n", name, hr); return; }
    VariantInit(&s);
    if (SUCCEEDED(VariantChangeType(&s, &res, 0, VT_BSTR))) P("%s=%ls\n", name, V_BSTR(&s)); else P("%s=vt%d\n", name, V_VT(&res));
    VariantClear(&s); VariantClear(&res);
}
static void run(Eng *e, const WCHAR *code, DWORD flags)
{
    EXCEPINFO ei; memset(&ei, 0, sizeof(ei));
    HRESULT hr = Parse_ParseScriptText(e->ap, code, NULL, NULL, NULL, 0, 0, flags, NULL, &ei);
    if (hr) P("RUN ERR %08lx\n", hr);
}

/* ---- interrupt: a thread interrupts the script running in main ---- */
static DWORD g_int_flags; static HRESULT g_int_hr;
static DWORD WINAPI interrupter(void *arg)
{
    EXCEPINFO ei; Sleep(400);
    memset(&ei, 0, sizeof(ei)); ei.scode = 0x80041234;
    g_int_hr = IActiveScript_InterruptScriptThread(g_script, SCRIPTTHREADID_BASE, &ei, g_int_flags);
    return 0;
}
static void interrupt_test(const char *name, const WCHAR *code, const WCHAR *after_expr, DWORD flags)
{
    Eng e; HANDLE th; EXCEPINFO ei; HRESULT hr; VARIANT res; DWORD t0;
    eng_new(&e, 0);
    g_int_flags = flags;
    th = CreateThread(NULL, 0, interrupter, NULL, 0, NULL);
    memset(&ei, 0, sizeof(ei)); t0 = GetTickCount();
    hr = Parse_ParseScriptText(e.ap, code, NULL, NULL, NULL, 0, 0, 0, NULL, &ei);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    P("%s=%08lx int=%08lx took=%s\n", name, hr, g_int_hr, (GetTickCount() - t0 < 4000) ? "short" : "LONG");
    if (after_expr) {
        VARIANT s; hr = eval(&e, after_expr, &res); VariantInit(&s);
        if (!hr && SUCCEEDED(VariantChangeType(&s, &res, 0, VT_BSTR))) P("%s_after=%ls\n", name, V_BSTR(&s)); else P("%s_after=ERR %08lx\n", name, hr);
    }
    eng_free(&e);
}

static void change_type(Eng *e, const char *name, VARTYPE from, VARTYPE to)
{
    IVCT *ct = NULL; VARIANT src, dst; HRESULT hr;
    IActiveScript_QueryInterface(e->as, &IID_IVariantChangeType_, (void**)&ct);
    VariantInit(&src); VariantInit(&dst);
    V_VT(&src) = from;
    if (from == VT_I8) V_I8(&src) = 200;
    else if (from == VT_R8) V_R8(&src) = 200.0;
    else V_I4(&src) = 200;
    hr = ct->lpVtbl->ChangeType(ct, &dst, &src, 0, to);
    if (hr) { P("%s=ERR %08lx\n", name, hr); }
    else switch (to) {
    case VT_UI1: P("%s=%u\n", name, V_UI1(&dst)); break;
    case VT_I1: P("%s=%d\n", name, V_I1(&dst)); break;
    case VT_UI4: P("%s=%lu\n", name, V_UI4(&dst)); break;
    case VT_INT: P("%s=%d\n", name, V_INT(&dst)); break;
    case VT_I8: P("%s=%lld\n", name, (long long)V_I8(&dst)); break;
    case VT_CY: P("%s=%lld\n", name, (long long)V_CY(&dst).int64); break;
    case VT_DATE: P("%s=%g\n", name, V_DATE(&dst)); break;
    default: P("%s=vt%d\n", name, V_VT(&dst));
    }
    ct->lpVtbl->Release(ct);
}

int main(void)
{
    Eng e; HRESULT hr; BSTR name = NULL; EXCEPINFO ei; DWORD tid; SCRIPTTHREADSTATE st; SCRIPTSTATE ss;
    IActiveScriptProperty *prop; IDispatch *gd; DISPID id; DWORD flags; VARIANT v;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    /* calling or constructing from an instance is "Function expected" (5002) */
    eng_new(&e, 0);
    ev(&e, "CALL_BOOL", L"var r; try{ new (new Boolean(true))(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "CALL_BOOL2", L"var r; try{ (new Boolean(true))(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "CALL_RE", L"var r; try{ new (/a/)(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "CALL_ERR", L"var r; try{ new (new Error())(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "ENUM_CALL", L"typeof Enumerator()");
    ev(&e, "ENUM_BAD", L"var r; try{ new Enumerator(5); }catch(x){ r = x.number & 0xffff; } r");
    /* ActiveXObject without new; with no argument */
    ev(&e, "AX_CALL", L"ActiveXObject('Scripting.Dictionary').Count");
    ev(&e, "AX_NOARG", L"var r; try{ new ActiveXObject(); }catch(x){ r = x.number & 0xffff; } r");
    eng_free(&e);

    /* host objects (IDispatchEx) and plain IDispatch objects (Scripting.Dictionary) */
    eng_new(&e, 1);
    ev(&e, "H_KEYS", L"Object.keys(hostex).join(',')");
    ev(&e, "H_NAMES", L"Object.getOwnPropertyNames(hostex).join(',')");
    ev(&e, "H_PIE", L"[Object.prototype.propertyIsEnumerable.call(hostex,'alpha'), Object.prototype.propertyIsEnumerable.call(hostex,'vdate'), Object.prototype.propertyIsEnumerable.call(hostex,'nosuch'), Object.prototype.propertyIsEnumerable.call(hostex)].join(',')");
    ev(&e, "H_GOPD", L"var d = Object.getOwnPropertyDescriptor(hostex, 'beta'); [d.value, d.writable, d.enumerable, d.configurable, Object.getOwnPropertyDescriptor(hostex, 'nosuch')].join(',')");
    ev(&e, "H_DEFPROP", L"var o = Object.defineProperty(hostex, 'gamma', {value: 33}); [o === hostex, hostex.gamma].join(',')");
    ev(&e, "H_DEFPROPS", L"Object.defineProperties(hostex, {delta: {value: 44}, eps: {value: 55}}); [hostex.delta, hostex.eps].join(',')");
    ev(&e, "H_DEFACC", L"var r; try{ Object.defineProperty(hostex, 'acc', {get: function(){return 1;}}); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "H_DESC_HOST", L"var d = {}; Object.defineProperty(hostex, 'zeta', {value: 66}); hostex.zeta");
    ev(&e, "H_EXT", L"[Object.isExtensible(hostex), Object.isFrozen(hostex), Object.isSealed(hostex), Object.freeze(hostex) === hostex, Object.seal(hostex) === hostex, Object.preventExtensions(hostex) === hostex, Object.isExtensible(hostex)].join(',')");
    ev(&e, "D_KEYS", L"var d = new ActiveXObject('Scripting.Dictionary'); d.Add('a', 1); Object.keys(d).length");
    ev(&e, "D_GOPD", L"var d = new ActiveXObject('Scripting.Dictionary'); d.Add('a', 1); Object.getOwnPropertyDescriptor(d, 'Count').value");
    ev(&e, "D_DEF", L"var d = new ActiveXObject('Scripting.Dictionary'); var r; try{ Object.defineProperty(d, 'nope', {value: 1}); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "D_PIE", L"var d = new ActiveXObject('Scripting.Dictionary'); [Object.prototype.propertyIsEnumerable.call(d, 'Count'), Object.prototype.propertyIsEnumerable.call(d, 'zz')].join(',')");
    ev(&e, "O_CREATE_BAD", L"var r; try{ Object.create(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "O_CREATE_BAD2", L"var r; try{ Object.create(5); }catch(x){ r = x.number & 0xffff; } r");

    /* variants the engine keeps as variants: date, 64 bit integer, decimal, error, array */
    ev(&e, "V_TYPEOF", L"[typeof hostex.vdate(), typeof hostex.vi8(), typeof hostex.vdec(), typeof hostex.verr(), typeof hostex.varr()].join(',')");
    ev(&e, "V_DATE_NUM", L"(+hostex.vdate()) === Date.parse('Mon Jan 1 12:00:00 1900')");
    ev(&e, "V_I8_NUM", L"(+hostex.vi8())");
    ev(&e, "V_I8_STR", L"'' + hostex.vi8()");
    ev(&e, "V_DEC", L"(+hostex.vdec()) + ',' + ('' + hostex.vdec())");
    ev(&e, "V_I8_BOOL", L"[!!hostex.vi8(), !!hostex.vdate(), !!hostex.varr()].join(',')");
    ev(&e, "V_OBJ", L"[typeof Object(hostex.vi8()), Object(hostex.vi8()) + 1, typeof Object(hostex.vdate()), Object(hostex.vdate()).getFullYear()].join(',')");
    ev(&e, "V_ERR", L"var r; try{ r = +hostex.verr(); }catch(x){ r = 'err ' + (x.number & 0xffff); } r");
    ev(&e, "V_JSON", L"var j = JSON.stringify([hostex.vi8(), hostex.vdate()]); j.length + ',' + j.substr(0, 13)");
    ev(&e, "V_DELETE", L"[delete hostex.alpha, delete hostex].join(',')");
    ev(&e, "STATE_IN", L"hostex.state()");
    ev(&e, "CALL_VBA", L"var r; try{ new (new VBArray(hostex.varr()))(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "CALL_MAP", L"var r; try{ new (new Map())(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "CALL_SET", L"var r; try{ (new Set())(); }catch(x){ r = x.number & 0xffff; } r");
    ev(&e, "CALL_WMAP", L"var r; try{ (new WeakMap())(); }catch(x){ r = x.number & 0xffff; } r");
    eng_free(&e);

    /* IVariantChangeType */
    eng_new(&e, 0);
    change_type(&e, "CT_UI1", VT_I4, VT_UI1); change_type(&e, "CT_I1", VT_I4, VT_I1); change_type(&e, "CT_UI4", VT_I4, VT_UI4);
    change_type(&e, "CT_INT", VT_I4, VT_INT); change_type(&e, "CT_I8", VT_R8, VT_I8); change_type(&e, "CT_CY", VT_R8, VT_CY);
    change_type(&e, "CT_I8FROM", VT_I8, VT_I4);
    eng_free(&e);

    /* script dispatch: property flags, internal names, thread ids, properties, state transitions */
    eng_new(&e, 0);
    run(&e, L"var fn = function(){}; var num = 5; var acc = {}; Object.defineProperty(this, 'ro', {value: 1, writable: false});", 0);
    IActiveScript_GetScriptDispatch(e.as, NULL, &gd);
#define FLAGS(nm, fnm) do { DISPID i2; BSTR b = SysAllocString(nm); flags = 0xdead; hr = IDispatchEx_GetDispID((IDispatchEx*)gd, b, fdexNameCaseSensitive, &i2); \
        if (!hr) hr = IDispatchEx_GetMemberProperties((IDispatchEx*)gd, i2, grfdexPropAll, &flags); \
        P("%s=%08lx %lx\n", fnm, hr, flags); SysFreeString(b); } while (0)
    FLAGS(L"fn", "MP_FN"); FLAGS(L"num", "MP_NUM"); FLAGS(L"ro", "MP_RO");
    {
        DISPID i2; BSTR b = SysAllocString(L"fn");
        IDispatchEx_GetDispID((IDispatchEx*)gd, b, fdexNameCaseSensitive, &i2);
        flags = 0; hr = IDispatchEx_GetMemberProperties((IDispatchEx*)gd, i2, fdexPropCanCall | fdexPropCannotCall, &flags);
        P("MP_ONLYCALL=%08lx %lx\n", hr, flags);
        hr = IDispatchEx_GetMemberProperties((IDispatchEx*)gd, i2, 0x40000000, &flags);
        P("MP_BADFLAG=%08lx\n", hr);
        SysFreeString(b);
    }
    {
        BSTR b1 = SysAllocString(L"hiddenInternal"), b2 = SysAllocString(L"shownNormal");
        hr = IDispatchEx_GetDispID((IDispatchEx*)gd, b1, fdexNameCaseSensitive | fdexNameEnsure | fdexNameInternal, &id);
        P("INT_ENSURE=%08lx\n", hr);
        hr = IDispatchEx_GetDispID((IDispatchEx*)gd, b2, fdexNameCaseSensitive | fdexNameEnsure, &id);
        P("NORM_ENSURE=%08lx\n", hr);
        hr = IDispatchEx_GetDispID((IDispatchEx*)gd, b2, 0x100, &id);
        P("BAD_GRFDEX=%08lx\n", hr);
        SysFreeString(b1); SysFreeString(b2);
    }
    ev(&e, "INT_ENUM", L"var c = []; for(var k in this) if(k == 'hiddenInternal' || k == 'shownNormal') c.push(k); c.join(',')");
    IDispatch_Release(gd);

    hr = IActiveScript_GetCurrentScriptThreadID(e.as, &tid); P("TID=%08lx %d\n", hr, tid == GetCurrentThreadId());
    hr = IActiveScript_GetCurrentScriptThreadID(e.as, NULL); P("TID_NULL=%08lx\n", hr);
    hr = IActiveScript_GetScriptThreadID(e.as, GetCurrentThreadId(), &tid); P("TID2=%08lx %d\n", hr, tid == GetCurrentThreadId());
    hr = IActiveScript_GetScriptThreadID(e.as, GetCurrentThreadId() + 4, &tid); P("TID2_BAD=%08lx\n", hr);
    hr = IActiveScript_GetScriptThreadState(e.as, SCRIPTTHREADID_BASE, &st); P("TS_OUT=%08lx %d\n", hr, (int)st);
    hr = IActiveScript_GetScriptThreadState(e.as, 0x1234567, &st); P("TS_BAD=%08lx\n", hr);
    memset(&ei, 0, sizeof(ei));
    hr = IActiveScript_InterruptScriptThread(e.as, SCRIPTTHREADID_BASE, NULL, 0); P("INT_NULL=%08lx\n", hr);
    hr = IActiveScript_InterruptScriptThread(e.as, SCRIPTTHREADID_BASE, &ei, 0x100); P("INT_BADFLAGS=%08lx\n", hr);
    hr = IActiveScript_InterruptScriptThread(e.as, 0x7654321, &ei, 0); P("INT_BADTID=%08lx\n", hr);

    IActiveScript_QueryInterface(e.as, &IID_IActiveScriptProperty_, (void**)&prop);
    VariantInit(&v);
    hr = IActiveScriptProperty_GetProperty(prop, SCRIPTPROP_NAME, NULL, &v); P("PR_NAME=%08lx %ls\n", hr, hr ? L"" : V_BSTR(&v));
    VariantClear(&v);
    hr = IActiveScriptProperty_GetProperty(prop, SCRIPTPROP_MAJORVERSION, NULL, &v); P("PR_MAJOR=%08lx %ld\n", hr, hr ? 0 : V_I4(&v));
    hr = IActiveScriptProperty_GetProperty(prop, SCRIPTPROP_MINORVERSION, NULL, &v); P("PR_MINOR=%08lx %ld\n", hr, hr ? 0 : V_I4(&v));
    V_VT(&v) = VT_I4; V_I4(&v) = 0x503;
    IActiveScriptProperty_SetProperty(prop, SCRIPTPROP_INVOKEVERSIONING, NULL, &v);
    VariantInit(&v);
    hr = IActiveScriptProperty_GetProperty(prop, SCRIPTPROP_INVOKEVERSIONING, NULL, &v); P("PR_VER=%08lx %lx\n", hr, hr ? 0 : V_I4(&v));
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    hr = IActiveScriptProperty_SetProperty(prop, SCRIPTPROP_ABBREVIATE_GLOBALNAME_RESOLUTION, NULL, &v); P("PR_ABBR_SET=%08lx\n", hr);
    VariantInit(&v);
    hr = IActiveScriptProperty_GetProperty(prop, SCRIPTPROP_ABBREVIATE_GLOBALNAME_RESOLUTION, NULL, &v); P("PR_ABBR=%08lx %d\n", hr, hr ? 0 : V_BOOL(&v) == VARIANT_TRUE);
    VariantInit(&v);
    hr = IActiveScriptProperty_GetProperty(prop, 0x77777777, NULL, &v); P("PR_UNKNOWN=%08lx\n", hr);
    IActiveScriptProperty_Release(prop);

    /* state transitions */
    IActiveScript_GetScriptState(e.as, &ss); P("ST0=%d\n", ss);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_DISCONNECTED); IActiveScript_GetScriptState(e.as, &ss); P("ST_DISC=%08lx %d\n", hr, ss);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_INITIALIZED); IActiveScript_GetScriptState(e.as, &ss); P("ST_INIT=%08lx %d\n", hr, ss);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_STARTED); IActiveScript_GetScriptState(e.as, &ss); P("ST_START=%08lx %d\n", hr, ss);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_CLOSED); P("ST_CLOSED=%08lx\n", hr);
    eng_free(&e);

    /* type libraries: the Scripting library (scrrun) defines the IOMode enumeration */
    eng_new(&e, 0);
    ev(&e, "TL_BEFORE", L"typeof ForReading");
    hr = IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, 0);
    P("TL_ADD=%08lx\n", hr);
    ev(&e, "TL_AFTER", L"[typeof ForReading, ForReading, ForAppending, TristateTrue].join(',')");
    ev(&e, "TL_DELETE", L"delete ForReading");
    hr = IActiveScript_AddTypeLib(e.as, &guid_bogus, 1, 0, 0);
    P("TL_BAD=%d\n", hr != S_OK);
    hr = IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, 0x1);
    P("TL_BADFLAGS=%08lx\n", hr);
    eng_free(&e);

    /* AddScriptlet */
    eng_new(&e, 1);
    hr = Parse_AddScriptlet(e.ap, L"h1", L"glob_hook = 7;", NULL, NULL, NULL, NULL, 0, 0, 0, &name, &ei);
    P("SL_FN=%08lx %ls\n", hr, hr || !name ? L"" : name); SysFreeString(name); name = NULL;
    ev(&e, "SL_FN_RUN", L"h1(); glob_hook");
    hr = Parse_AddScriptlet(e.ap, NULL, L"return 9;", L"hostex", NULL, L"onclick", NULL, 0, 0, 0, &name, &ei);
    P("SL_EVT=%08lx %ls handler=%ls\n", hr, hr || !name ? L"" : name, g_handler ? g_handler_name : L"none"); SysFreeString(name); name = NULL;
    if (g_handler) { DISPPARAMS dp = {0}; VARIANT hv; HRESULT h2; VariantInit(&hv); h2 = IDispatch_Invoke(g_handler, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, &hv, NULL, NULL); P("SL_CALL=%08lx %ld\n", h2, V_I4(&hv)); }

    hr = Parse_AddScriptlet(e.ap, NULL, L"x=1;", NULL, NULL, NULL, NULL, 0, 0, 0, &name, &ei); P("SL_NONAME=%08lx\n", hr);
    hr = Parse_AddScriptlet(e.ap, L"h2", L"x=1;", L"nosuchitem", NULL, L"onx", NULL, 0, 0, 0, &name, &ei); P("SL_BADITEM=%08lx\n", hr);
    hr = Parse_AddScriptlet(e.ap, L"h3", L"x=;", NULL, NULL, NULL, NULL, 0, 0, 0, &name, &ei); P("SL_SYNTAX=%d\n", hr != S_OK);
    eng_free(&e);

    /* Clone: named items, type libraries and persistent code are carried over */
    {
        IActiveScript *clone = NULL; Eng c;
        eng_new(&e, 1);
        run(&e, L"pv = 42;", SCRIPTTEXT_ISPERSISTENT);
        IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, 0);
        hr = IActiveScript_Clone(e.as, NULL); P("CL_NULL=%08lx\n", hr);
        hr = IActiveScript_Clone(e.as, &clone); P("CL=%08lx\n", hr);
        if (!hr) {
            c.as = clone; IActiveScript_QueryInterface(clone, &IID_Parse_, (void**)&c.ap);
            IActiveScript_GetScriptState(clone, &ss); P("CL_STATE0=%d\n", ss);
            hr = IActiveScript_SetScriptSite(clone, &site.iface); P("CL_SITE=%08lx\n", hr);
            IActiveScript_GetScriptState(clone, &ss); P("CL_STATE1=%d\n", ss);
            IActiveScript_SetScriptState(clone, SCRIPTSTATE_CONNECTED);
            g_script = clone;
            ev(&c, "CL_ITEM", L"hostex.alpha");
            ev(&c, "CL_CODE", L"pv");
            ev(&c, "CL_TYPELIB", L"ForAppending");
            eng_free(&c);
        }
        g_script = e.as;
        eng_free(&e);
    }

    /* interrupt: an abort skips catch and finally; with RAISEEXCEPTION the script can catch it */
    interrupt_test("INTR_ABORT", L"var caught = 0, fin = 0; try{ while(true){} }catch(x){ caught = 1; } finally{ fin = 1; }",
                   L"caught + ',' + fin", 0);
    interrupt_test("INTR_RAISE", L"var caught = 0, num = 0; try{ while(true){} }catch(x){ caught = 1; num = x.number >>> 0; }",
                   L"caught + ',' + num.toString(16)", SCRIPTINTERRUPT_RAISEEXCEPTION);
    interrupt_test("INTR_NESTED", L"var f = function(){ while(true){} }; function g(){ try{ f(); }catch(x){ got = 1; } } g();",
                   L"typeof got", 0);
    {   /* a stale interrupt does not hit the next script run */
        Eng e2; eng_new(&e2, 0); memset(&ei, 0, sizeof(ei)); ei.scode = E_FAIL;
        IActiveScript_InterruptScriptThread(e2.as, SCRIPTTHREADID_BASE, &ei, 0);
        ev(&e2, "INTR_STALE", L"1 + 1");
        eng_free(&e2);
    }
    P("DONE\n");
    return 0;
}
