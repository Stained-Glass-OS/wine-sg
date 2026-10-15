#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <activscp.h>
#include <activdbg.h>
#include <dispex.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)

static const CLSID CLSID_VBScript_ = {0xb54f3741,0x5b07,0x11cf,{0xa4,0xb0,0x00,0xaa,0x00,0x4a,0x55,0xe8}};
static const IID IID_IDispatchEx_ = {0xa6ef9860,0xc720,0x11d0,{0x93,0x37,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
#define IID_IDispatchEx IID_IDispatchEx_
static const IID IID_IBindEventHandler_ = {0x63cdbcb0,0xc1b1,0x11d0,{0x93,0x36,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
#ifdef _WIN64
static const IID IID_Parse_ = {0xc7ef7658,0xe1ee,0x480e,{0x97,0xea,0xd5,0x2c,0xb4,0xd7,0x6d,0x17}};
typedef IActiveScriptParse64 Parse;
#define Parse_InitNew IActiveScriptParse64_InitNew
#define Parse_ParseScriptText IActiveScriptParse64_ParseScriptText
#define Parse_AddScriptlet IActiveScriptParse64_AddScriptlet
#define Parse_Release IActiveScriptParse64_Release
static const IID IID_Debug_ = {0xbc437e23,0xf5b8,0x47f4,{0xbb,0x79,0x7d,0x1c,0xe5,0x48,0x3b,0x86}};
typedef IActiveScriptDebug64 DbgI;
#define Dbg_Text IActiveScriptDebug64_GetScriptTextAttributes
#define Dbg_Slet IActiveScriptDebug64_GetScriptletTextAttributes
#define Dbg_Release IActiveScriptDebug64_Release
#else
static const IID IID_Parse_ = {0xbb1a2ae2,0xa4f9,0x11cf,{0x8f,0x20,0x00,0x80,0x5f,0x2c,0xd0,0x64}};
typedef IActiveScriptParse32 Parse;
#define Parse_InitNew IActiveScriptParse32_InitNew
#define Parse_ParseScriptText IActiveScriptParse32_ParseScriptText
#define Parse_AddScriptlet IActiveScriptParse32_AddScriptlet
#define Parse_Release IActiveScriptParse32_Release
static const IID IID_Debug_ = {0x51973c10,0xcb0c,0x11d0,{0xb5,0xc9,0x00,0xa0,0x24,0x4a,0x0e,0x7a}};
typedef IActiveScriptDebug32 DbgI;
#define Dbg_Text IActiveScriptDebug32_GetScriptTextAttributes
#define Dbg_Slet IActiveScriptDebug32_GetScriptletTextAttributes
#define Dbg_Release IActiveScriptDebug32_Release
#endif

static const GUID guid_scrrun = {0x420B2830,0xE718,0x11CF,{0x89,0x3D,0x00,0xA0,0xC9,0x05,0x42,0x28}};
static const GUID guid_bogus = {0x11111111,0x2222,0x3333,{0,1,2,3,4,5,6,7}};

static IActiveScript *g_script;
static IDispatch *g_handler;
static WCHAR g_handler_name[64];
static WCHAR g_linetext[256];
static ULONG g_line = 999;
static int g_errors;

/* ---- the host item "hostex": IDispatch with state() and an IBindEventHandler ---- */
typedef struct { const IDispatchVtbl *d; const void *bh; } Host;
static HRESULT WINAPI H_QI(IDispatch *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IDispatch)) { *o = i; return S_OK; }
    if (IsEqualGUID(r, &IID_IBindEventHandler_)) { *o = (char*)i + sizeof(void*); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI H_AR(IDispatch *i) { return 2; }
static HRESULT WINAPI H_GTIC(IDispatch *i, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI H_GTI(IDispatch *i, UINT n, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI H_GIDS(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d)
{
    if (!wcsicmp(n[0], L"state")) { *d = 1; return S_OK; }
    return DISP_E_UNKNOWNNAME;
}
static HRESULT WINAPI H_Invoke(IDispatch *i, DISPID d, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{
    if (d == 1 && v) {
        SCRIPTTHREADSTATE st = 99; HRESULT hr = IActiveScript_GetScriptThreadState(g_script, SCRIPTTHREADID_CURRENT, &st);
        V_VT(v) = VT_I4; V_I4(v) = hr ? -1 : (int)st; return S_OK;
    }
    return DISP_E_MEMBERNOTFOUND;
}
static IDispatchVtbl hvt = { H_QI, H_AR, H_AR, H_GTIC, H_GTI, H_GIDS, H_Invoke };
static HRESULT WINAPI B_QI(IUnknown *i, REFIID r, void **o) { return H_QI((IDispatch*)((char*)i - sizeof(void*)), r, o); }
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
static HRESULT WINAPI S_QI(IActiveScriptSite *i, REFIID r, void **o) { if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IActiveScriptSite)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI S_AR(IActiveScriptSite *i) { return 2; }
static HRESULT WINAPI S_GL(IActiveScriptSite *i, LCID *l) { return E_NOTIMPL; }
static HRESULT WINAPI S_GI(IActiveScriptSite *i, LPCOLESTR n, DWORD m, IUnknown **u, ITypeInfo **t)
{
    if (!wcsicmp(n, L"hostex") && (m & SCRIPTINFO_IUNKNOWN)) { *u = (IUnknown*)&host; return S_OK; }
    return TYPE_E_ELEMENTNOTFOUND;
}
static HRESULT WINAPI S_GD(IActiveScriptSite *i, BSTR *b) { return E_NOTIMPL; }
static HRESULT WINAPI S_OST(IActiveScriptSite *i, const VARIANT *v, const EXCEPINFO *e) { return S_OK; }
static HRESULT WINAPI S_SC(IActiveScriptSite *i, SCRIPTSTATE s) { return S_OK; }
static HRESULT WINAPI S_OSE(IActiveScriptSite *i, IActiveScriptError *e)
{
    BSTR b = NULL; DWORD cookie; LONG ch; HRESULT hr;
    g_errors++;
    hr = IActiveScriptError_GetSourceLineText(e, &b);
    wcscpy(g_linetext, hr ? L"(fail)" : b ? b : L"(null)");
    if (hr) swprintf(g_linetext, 256, L"(fail %08lx)", hr);
    SysFreeString(b);
    IActiveScriptError_GetSourcePosition(e, &cookie, &g_line, &ch);
    return S_OK;
}
static HRESULT WINAPI S_E(IActiveScriptSite *i) { return S_OK; }
static IActiveScriptSiteVtbl svt = { S_QI, S_AR, S_AR, S_GL, S_GI, S_GD, S_OST, S_SC, S_OSE, S_E, S_E };
static struct { IActiveScriptSite iface; } site = { { &svt } };

typedef struct { IActiveScript *as; Parse *ap; } Eng;

static HRESULT eng_new(Eng *e, int with_host)
{
    HRESULT hr = CoCreateInstance(&CLSID_VBScript_, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void**)&e->as);
    if (hr) return hr;
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
static HRESULT run(Eng *e, const WCHAR *code, DWORD flags)
{
    EXCEPINFO ei; memset(&ei, 0, sizeof(ei));
    return Parse_ParseScriptText(e->ap, code, NULL, NULL, NULL, 0, 0, flags, NULL, &ei);
}
static long vlong(VARIANT *v)
{
    VARIANT t; long r = 0; VariantInit(&t);
    if (SUCCEEDED(VariantChangeType(&t, v, 0, VT_I4))) r = V_I4(&t);
    return r;
}
static IDispatch *get_obj(Eng *e, const WCHAR *expr)
{
    VARIANT res; IDispatch *d = NULL;
    if (eval(e, expr, &res) == S_OK && V_VT(&res) == VT_DISPATCH && V_DISPATCH(&res)) { d = V_DISPATCH(&res); IDispatch_AddRef(d); }
    VariantClear(&res);
    return d;
}

/* ---- interrupt: a thread interrupts the script running in main ---- */
static DWORD g_int_flags;
static DWORD WINAPI interrupter(void *arg)
{
    EXCEPINFO ei; Sleep(400);
    memset(&ei, 0, sizeof(ei)); ei.scode = 0x80041234;
    IActiveScript_InterruptScriptThread(g_script, SCRIPTTHREADID_BASE, &ei, g_int_flags);
    return 0;
}
static void interrupt_test(const char *name, const WCHAR *code, const WCHAR *after_expr, DWORD flags)
{
    Eng e; HANDLE th; HRESULT hr;
    eng_new(&e, 0);
    g_int_flags = flags;
    th = CreateThread(NULL, 0, interrupter, NULL, 0, NULL);
    hr = run(&e, code, 0);
    WaitForSingleObject(th, 20000); CloseHandle(th);
    P("%s_hr=%d\n", name, hr != S_OK);
    { char nm[64]; sprintf(nm, "%s_after", name); ev(&e, nm, after_expr); }
    eng_free(&e);
}

static const char *kind_name(TYPEKIND k) { return k == TKIND_DISPATCH ? "dispatch" : k == TKIND_INTERFACE ? "interface" : "other"; }
static void typeinfo_of(Eng *e, const char *name, const WCHAR *expr)
{
    IDispatch *d = get_obj(e, expr); ITypeInfo *ti = NULL; TYPEATTR *ta; HRESULT hr;
    if (!d) { P("%s=noobj\n", name); return; }
    hr = IDispatch_GetTypeInfo(d, 0, 0, &ti);
    if (hr || !ti) { P("%s=ERR %08lx\n", name, hr); IDispatch_Release(d); return; }
    hr = ITypeInfo_GetTypeAttr(ti, &ta);
    if (!hr) { P("%s=%s funcs>0:%d\n", name, kind_name(ta->typekind), ta->cFuncs > 0); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    else P("%s=attr ERR %08lx\n", name, hr);
    ITypeInfo_Release(ti);
    hr = IDispatch_GetTypeInfo(d, 1, 0, &ti);
    P("%s_bad=%08lx\n", name, hr);
    IDispatch_Release(d);
}

static void attrs_to_str(const SOURCE_TEXT_ATTR *a, int n, char *out)
{
    int i;
    for (i = 0; i < n; i++) {
        SOURCE_TEXT_ATTR v = a[i];
        char c = '.';
        if (v & SOURCETEXT_ATTR_FUNCTION_START) c = 'F';
        else if (v & SOURCETEXT_ATTR_KEYWORD) c = 'K';
        else if (v & SOURCETEXT_ATTR_COMMENT) c = 'C';
        else if (v & SOURCETEXT_ATTR_NONSOURCE) c = 'X';
        else if (v & SOURCETEXT_ATTR_OPERATOR) c = 'O';
        else if (v & SOURCETEXT_ATTR_NUMBER) c = 'N';
        else if (v & SOURCETEXT_ATTR_STRING) c = 'S';
        out[i] = c;
    }
    out[n] = 0;
}

int main(void)
{
    Eng e; HRESULT hr; BSTR name = NULL; EXCEPINFO ei; DWORD tid; SCRIPTTHREADSTATE st; SCRIPTSTATE ss;
    IDispatch *gd, *obj; IDispatchEx *dex; DISPID id;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    /* error line text */
    eng_new(&e, 0);
    g_errors = 0; g_line = 999; g_linetext[0] = 0;
    run(&e, L"x = 1\r\ny = 2 +* 3\r\nz = 4", 0);
    P("LT_SYNTAX=[%ls] line=%lu errors=%d\n", g_linetext, g_line, g_errors);
    g_errors = 0; g_line = 999; g_linetext[0] = 0;
    run(&e, L"Dim a\r\n\r\n  b = 1 / 0  \r\n", 0);
    P("LT_RUNTIME=[%ls] line=%lu errors=%d\n", g_linetext, g_line, g_errors);
    eng_free(&e);

    /* statements, operators and builtins that had no behaviour */
    eng_new(&e, 0);
    run(&e, L"Class Dc\r\n Public cnt\r\n Public Default Function D()\r\n  cnt = cnt + 1\r\n  D = 0\r\n End Function\r\nEnd Class\r\n"
            L"Function Ff()\r\n Ff = 1\r\nEnd Function\r\nConst KK = 3\r\nDim od, n5\r\nSet od = New Dc\r\nn5 = 5\r\n"
            L"On Error Resume Next\r\n"
            L"Err.Clear : x = (5 Is Nothing) : r_is = Err.Number\r\n"
            L"Err.Clear : x = (Nothing Is 5) : r_is2 = Err.Number\r\n"
            L"Err.Clear : x = (od Is od) : r_is3 = Err.Number & \",\" & x\r\n"
            L"Err.Clear : y = n5(1) : r_call = Err.Number\r\n"
            L"Err.Clear : KK = 4 : r_const = Err.Number\r\n"
            L"Err.Clear : Ff = 2 : r_func = Err.Number\r\n"
            L"Err.Clear : od : r_stmt = Err.Number & \",\" & od.cnt\r\n"
            L"Err.Clear : n5 : r_stmt2 = Err.Number\r\n"
            L"Err.Clear : Err = 7 : r_errset = Err.Number\r\n"
            L"Err.Clear : Set o1 = GetObject() : r_go0 = Err.Number\r\n"
            L"Err.Clear : Set o2 = GetObject(\"\", \"Nonexistent.Prog\") : r_go1 = Err.Number\r\n"
            L"Err.Clear : Set o3 = GetObject(\"c:\\nofile\", \"Scripting.Dictionary\") : r_go2 = Err.Number\r\n"
            L"Err.Clear : Set o4 = GetObject(, \"Nonexistent.Prog\") : r_go3 = Err.Number\r\n"
            L"Err.Clear : Set o5 = GetObject(\"\", \"Scripting.Dictionary\") : r_go4 = Err.Number\r\n", 0);
    ev(&e, "IS_NONOBJ", L"r_is & \",\" & r_is2");
    ev(&e, "IS_SAME", L"r_is3");
    ev(&e, "CALL_NONARRAY", L"r_call");
    ev(&e, "ASSIGN_CONST", L"r_const");
    ev(&e, "ASSIGN_FUNC", L"r_func");
    ev(&e, "STMT_OBJ", L"r_stmt");
    ev(&e, "STMT_VAL", L"r_stmt2");
    ev(&e, "ERR_SET", L"r_errset");
    ev(&e, "GETOBJ_NOARGS", L"r_go0");
    ev(&e, "GETOBJ_NOCLASS", L"r_go1");
    ev(&e, "GETOBJ_NOPERSIST", L"r_go2");
    ev(&e, "GETOBJ_OMITTED", L"r_go3");
    ev(&e, "GETOBJ_NOTRUNNING", L"r_go4");
    eng_free(&e);

    /* a class instance as IDispatchEx */
    eng_new(&e, 0);
    run(&e, L"Class Cls\r\n Public pubVar\r\n Private privVar\r\n"
            L" Public Default Function Def()\r\n  Def = 99\r\n End Function\r\n"
            L" Public Function Add(a, b)\r\n  Add = a + b\r\n End Function\r\n"
            L" Public Property Get Prop()\r\n  Prop = 5\r\n End Property\r\n"
            L" Public Property Let Prop(v)\r\n  privVar = v\r\n End Property\r\n"
            L" Private Sub Hidden()\r\n End Sub\r\nEnd Class\r\nSet inst = New Cls\r\n", 0);
    obj = get_obj(&e, L"inst");
    if (!obj) P("CLASS=noobj\n");
    else {
        DISPID ids[3]; LPOLESTR names[3]; DWORD fl; BSTR nm; char list[256] = ""; DISPID add_id = -2, prop_id = -2, var_id = -2, cur;
        names[0] = L"add"; names[1] = L"a"; names[2] = L"b";
        hr = IDispatch_GetIDsOfNames(obj, &IID_NULL, names, 1, 0, ids); add_id = ids[0];
        P("CL_GIDS=%08lx %s\n", hr, add_id >= 0 ? "id" : "noid");
        hr = IDispatch_GetIDsOfNames(obj, &IID_NULL, names, 3, 0, ids);
        P("CL_GIDS3=%08lx %ld %ld\n", hr, ids[1], ids[2]);
        names[0] = L"hidden";
        hr = IDispatch_GetIDsOfNames(obj, &IID_NULL, names, 1, 0, ids); P("CL_GIDS_PRIV=%08lx %ld\n", hr, ids[0]);
        names[0] = L"PROP"; IDispatch_GetIDsOfNames(obj, &IID_NULL, names, 1, 0, ids); prop_id = ids[0];
        names[0] = L"pubvar"; IDispatch_GetIDsOfNames(obj, &IID_NULL, names, 1, 0, ids); var_id = ids[0];
        hr = IDispatch_GetIDsOfNames(obj, &IID_NULL, names, 0, 0, ids); P("CL_GIDS0=%08lx\n", hr);
        IDispatch_QueryInterface(obj, &IID_IDispatchEx, (void**)&dex);
        /* enumeration in id order: the default member, the procedures, the fields; private ones are left out */
        cur = DISPID_STARTENUM;
        while ((hr = IDispatchEx_GetNextDispID(dex, fdexEnumAll, cur, &cur)) == S_OK) {
            nm = NULL; if (IDispatchEx_GetMemberName(dex, cur, &nm) == S_OK) { if (list[0]) strcat(list, ","); sprintf(list + strlen(list), "%ls", nm); SysFreeString(nm); }
        }
        P("CL_ENUM=%s end=%08lx next=%ld\n", list, hr, cur);
        hr = IDispatchEx_GetNextDispID(dex, fdexEnumAll, 777, &cur); P("CL_ENUM_BAD=%08lx\n", hr);
        hr = IDispatchEx_GetMemberName(dex, 777, &nm); P("CL_NAME_BAD=%08lx\n", hr);
        hr = IDispatchEx_GetMemberProperties(dex, add_id, grfdexPropAll, &fl); P("CL_PROPS_ADD=%08lx %lx\n", hr, fl);
        hr = IDispatchEx_GetMemberProperties(dex, prop_id, grfdexPropAll, &fl); P("CL_PROPS_PROP=%08lx %lx\n", hr, fl);
        hr = IDispatchEx_GetMemberProperties(dex, var_id, grfdexPropAll, &fl); P("CL_PROPS_VAR=%08lx %lx\n", hr, fl);
        hr = IDispatchEx_GetMemberProperties(dex, add_id, fdexPropCanCall | fdexPropCannotCall, &fl); P("CL_PROPS_ONLY=%08lx %lx\n", hr, fl);
        hr = IDispatchEx_GetMemberProperties(dex, 777, grfdexPropAll, &fl); P("CL_PROPS_BAD=%08lx\n", hr);
        hr = IDispatchEx_GetMemberProperties(dex, add_id, 0x40000000, &fl); P("CL_PROPS_BADFETCH=%08lx\n", hr);
        {
            BSTR b1 = SysAllocString(L"add"), b2 = SysAllocString(L"nosuch");
            hr = IDispatchEx_DeleteMemberByName(dex, b1, fdexNameCaseInsensitive); P("CL_DEL_NAME=%08lx\n", hr);
            hr = IDispatchEx_DeleteMemberByName(dex, b2, fdexNameCaseInsensitive); P("CL_DEL_UNKNOWN=%08lx\n", hr);
            SysFreeString(b1); SysFreeString(b2);
        }
        hr = IDispatchEx_DeleteMemberByDispID(dex, add_id); P("CL_DEL_ID=%08lx\n", hr);
        hr = IDispatchEx_DeleteMemberByDispID(dex, 777); P("CL_DEL_ID_BAD=%08lx\n", hr);
        {   /* flags that describe the lookup */
            BSTR b = SysAllocString(L"ADD");
            hr = IDispatchEx_GetDispID(dex, b, fdexNameImplicit | fdexNameCaseInsensitive, &id); P("CL_GETDISPID_IMPL=%08lx\n", hr);
            hr = IDispatchEx_GetDispID(dex, b, fdexNameInternal | fdexNameCaseInsensitive, &id); P("CL_GETDISPID_INT=%08lx\n", hr);
            SysFreeString(b);
        }
        {   /* the type information */
            ITypeInfo *ti; TYPEATTR *ta;
            hr = IDispatch_GetTypeInfo(obj, 0, 0, &ti);
            P("CL_TI=%08lx\n", hr);
            if (!hr) {
                UINT k, cnt; char buf[256] = "";
                hr = ITypeInfo_GetTypeAttr(ti, &ta);
                P("CL_TI_ATTR=%s funcs=%u vars=%u\n", kind_name(ta->typekind), ta->cFuncs, ta->cVars);
                for (k = 0; k < ta->cFuncs; k++) {
                    FUNCDESC *fd; BSTR fn[4]; UINT nn = 0;
                    ITypeInfo_GetFuncDesc(ti, k, &fd);
                    ITypeInfo_GetNames(ti, fd->memid, fn, 4, &nn);
                    if (buf[0]) strcat(buf, ";");
                    sprintf(buf + strlen(buf), "%ls/%d/%d/%u", nn ? fn[0] : L"?", fd->invkind, fd->memid == add_id, fd->cParams);
                    for (cnt = 0; cnt < nn; cnt++) SysFreeString(fn[cnt]);
                    ITypeInfo_ReleaseFuncDesc(ti, fd);
                }
                P("CL_TI_FUNCS=%s\n", buf);
                { VARDESC *vd; hr = ITypeInfo_GetVarDesc(ti, 0, &vd); P("CL_TI_VAR=%08lx %d\n", hr, hr ? 0 : vd->memid == var_id); if (!hr) ITypeInfo_ReleaseVarDesc(ti, vd); }
                { BSTR docname = NULL; hr = ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &docname, NULL, NULL, NULL); P("CL_TI_NAME=%ls\n", hr ? L"ERR" : docname); SysFreeString(docname); }
                { BSTR docname = NULL; hr = ITypeInfo_GetDocumentation(ti, var_id, &docname, NULL, NULL, NULL); P("CL_TI_VARNAME=%ls\n", hr ? L"ERR" : docname); SysFreeString(docname); }
                {
                    LPOLESTR vn[1] = { L"pubVar" }; MEMBERID mid = -2;
                    hr = ITypeInfo_GetIDsOfNames(ti, vn, 1, &mid); P("CL_TI_GIDS=%08lx %d\n", hr, mid == var_id);
                }
                {
                    VARIANT args[2], r; DISPPARAMS dp; UINT ae = 0;
                    VariantInit(&args[0]); VariantInit(&args[1]); VariantInit(&r);
                    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 4; V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 3;
                    dp.rgvarg = args; dp.cArgs = 2; dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL;
                    hr = ITypeInfo_Invoke(ti, obj, add_id, DISPATCH_METHOD, &dp, &r, NULL, &ae);
                    P("CL_TI_INVOKE=%08lx %ld\n", hr, hr ? 0 : V_I4(&r));
                }
                ITypeInfo_ReleaseTypeAttr(ti, ta);
                ITypeInfo_Release(ti);
            }
        }
        IDispatchEx_Release(dex);
        IDispatch_Release(obj);
    }
    eng_free(&e);

    /* the script's global scope as IDispatchEx */
    eng_new(&e, 0);
    run(&e, L"Const K = 5\r\nDim gv, garr(3)\r\ngv = 1\r\nFunction Foo(x)\r\n Foo = x * 2\r\nEnd Function\r\n", 0);
    IActiveScript_GetScriptDispatch(e.as, NULL, &gd);
    IDispatch_QueryInterface(gd, &IID_IDispatchEx, (void**)&dex);
    {
        DISPID cur = DISPID_STARTENUM, kid, fid, gid, aid; char list[256] = ""; BSTR nm; DWORD fl; BSTR b;
        while ((hr = IDispatchEx_GetNextDispID(dex, fdexEnumAll, cur, &cur)) == S_OK) {
            nm = NULL; if (IDispatchEx_GetMemberName(dex, cur, &nm) == S_OK) { if (list[0]) strcat(list, ","); sprintf(list + strlen(list), "%ls", nm); SysFreeString(nm); }
        }
        P("SD_ENUM=%s end=%08lx next=%ld\n", list, hr, cur);
        b = SysAllocString(L"k"); IDispatchEx_GetDispID(dex, b, fdexNameCaseInsensitive, &kid); SysFreeString(b);
        b = SysAllocString(L"foo"); IDispatchEx_GetDispID(dex, b, fdexNameCaseInsensitive, &fid); SysFreeString(b);
        b = SysAllocString(L"gv"); IDispatchEx_GetDispID(dex, b, fdexNameCaseInsensitive, &gid); SysFreeString(b);
        b = SysAllocString(L"garr"); IDispatchEx_GetDispID(dex, b, fdexNameCaseInsensitive, &aid); SysFreeString(b);
        hr = IDispatchEx_GetMemberProperties(dex, kid, grfdexPropAll, &fl); P("SD_PROPS_CONST=%08lx %lx\n", hr, fl);
        hr = IDispatchEx_GetMemberProperties(dex, fid, grfdexPropAll, &fl); P("SD_PROPS_FUNC=%08lx %lx\n", hr, fl);
        hr = IDispatchEx_GetMemberProperties(dex, gid, grfdexPropAll, &fl); P("SD_PROPS_VAR=%08lx %lx\n", hr, fl);
        hr = IDispatchEx_GetMemberProperties(dex, 0x5555, grfdexPropAll, &fl); P("SD_PROPS_BAD=%08lx\n", hr);
        hr = IDispatchEx_GetMemberName(dex, fid, &nm); P("SD_NAME_FUNC=%ls\n", hr ? L"ERR" : nm); if (!hr) SysFreeString(nm);
        hr = IDispatchEx_GetMemberName(dex, 0x5555, &nm); P("SD_NAME_BAD=%08lx\n", hr);
        hr = IDispatchEx_GetNextDispID(dex, fdexEnumAll, 0x5555, &cur); P("SD_NEXT_BAD=%08lx\n", hr);
        {
            VARIANT arg, r; DISPPARAMS dp; DISPID named = DISPID_PROPERTYPUT;
            VariantInit(&arg); VariantInit(&r);
            V_VT(&arg) = VT_I4; V_I4(&arg) = 21; dp.rgvarg = &arg; dp.cArgs = 1; dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL;
            hr = IDispatchEx_InvokeEx(dex, fid, 0, DISPATCH_PROPERTYGET, &dp, &r, NULL, NULL); P("SD_FUNC_GET=%08lx %ld\n", hr, hr ? 0 : vlong(&r));
            hr = IDispatchEx_InvokeEx(dex, fid, 0, DISPATCH_PROPERTYPUT, &dp, &r, NULL, NULL); P("SD_FUNC_PUT=%08lx\n", hr);
            dp.cNamedArgs = 1; dp.rgdispidNamedArgs = &named;
            hr = IDispatchEx_InvokeEx(dex, kid, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL); P("SD_CONST_PUT=%08lx\n", hr);
            VariantInit(&r);
            hr = IDispatchEx_InvokeEx(dex, kid, 0, DISPATCH_PROPERTYGET, &(DISPPARAMS){0}, &r, NULL, NULL); P("SD_CONST_GET=%08lx %ld\n", hr, hr ? 0 : vlong(&r));
            {   /* garr(2) = 77 */
                VARIANT pa[2]; DISPPARAMS dp2;
                V_VT(&pa[0]) = VT_I4; V_I4(&pa[0]) = 77; V_VT(&pa[1]) = VT_I4; V_I4(&pa[1]) = 2;
                dp2.rgvarg = pa; dp2.cArgs = 2; dp2.cNamedArgs = 1; dp2.rgdispidNamedArgs = &named;
                hr = IDispatchEx_InvokeEx(dex, aid, 0, DISPATCH_PROPERTYPUT, &dp2, NULL, NULL, NULL); P("SD_ARR_PUT=%08lx\n", hr);
                hr = IDispatchEx_InvokeEx(dex, gid, 0, DISPATCH_PROPERTYPUT, &dp2, NULL, NULL, NULL); P("SD_NONARR_PUT=%08lx\n", hr);
            }
            {   /* garr(2) read back */
                VARIANT idx; DISPPARAMS dp3;
                V_VT(&idx) = VT_I4; V_I4(&idx) = 2; dp3.rgvarg = &idx; dp3.cArgs = 1; dp3.cNamedArgs = 0; dp3.rgdispidNamedArgs = NULL;
                VariantInit(&r);
                hr = IDispatchEx_InvokeEx(dex, aid, 0, DISPATCH_PROPERTYGET, &dp3, &r, NULL, NULL); P("SD_ARR_GET=%08lx %ld\n", hr, hr ? 0 : V_I4(&r));
            }
        }
        ev(&e, "SD_ARR_SCRIPT", L"garr(2)");
        b = SysAllocString(L"gv"); hr = IDispatchEx_DeleteMemberByName(dex, b, fdexNameCaseInsensitive); P("SD_DEL=%08lx\n", hr); SysFreeString(b);
        b = SysAllocString(L"nosuch"); hr = IDispatchEx_DeleteMemberByName(dex, b, fdexNameCaseInsensitive); P("SD_DEL_UNKNOWN=%08lx\n", hr); SysFreeString(b);
        hr = IDispatchEx_DeleteMemberByDispID(dex, fid); P("SD_DEL_ID=%08lx\n", hr);
        hr = IDispatchEx_DeleteMemberByDispID(dex, 0x5555); P("SD_DEL_ID_BAD=%08lx\n", hr);
    }
    IDispatchEx_Release(dex);
    IDispatch_Release(gd);
    eng_free(&e);

    /* threads and interrupt arguments */
    eng_new(&e, 1);
    hr = IActiveScript_GetCurrentScriptThreadID(e.as, &tid); P("TID=%08lx %d\n", hr, tid == GetCurrentThreadId());
    hr = IActiveScript_GetCurrentScriptThreadID(e.as, NULL); P("TID_NULL=%08lx\n", hr);
    hr = IActiveScript_GetScriptThreadID(e.as, GetCurrentThreadId(), &tid); P("TID2=%08lx %d\n", hr, tid == GetCurrentThreadId());
    hr = IActiveScript_GetScriptThreadID(e.as, GetCurrentThreadId() + 4, &tid); P("TID2_BAD=%08lx\n", hr);
    hr = IActiveScript_GetScriptThreadState(e.as, SCRIPTTHREADID_BASE, &st); P("TS_OUT=%08lx %d\n", hr, (int)st);
    hr = IActiveScript_GetScriptThreadState(e.as, 0x1234567, &st); P("TS_BAD=%08lx\n", hr);
    ev(&e, "TS_IN", L"hostex.state()");
    memset(&ei, 0, sizeof(ei));
    hr = IActiveScript_InterruptScriptThread(e.as, SCRIPTTHREADID_BASE, NULL, 0); P("INT_NULL=%08lx\n", hr);
    hr = IActiveScript_InterruptScriptThread(e.as, SCRIPTTHREADID_BASE, &ei, 0x100); P("INT_BADFLAGS=%08lx\n", hr);
    hr = IActiveScript_InterruptScriptThread(e.as, 0x7654321, &ei, 0); P("INT_BADTID=%08lx\n", hr);

    /* state transitions */
    IActiveScript_GetScriptState(e.as, &ss); P("ST0=%d\n", ss);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_DISCONNECTED); IActiveScript_GetScriptState(e.as, &ss); P("ST_DISC=%08lx %d\n", hr, ss);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_INITIALIZED); IActiveScript_GetScriptState(e.as, &ss); P("ST_INIT=%08lx %d\n", hr, ss);
    hr = run(&e, L"pend = 5", 0);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_DISCONNECTED); IActiveScript_GetScriptState(e.as, &ss); P("ST_DISC2=%08lx %d\n", hr, ss);
    ev(&e, "ST_PEND", L"pend");
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_CLOSED); P("ST_CLOSED=%08lx\n", hr);
    hr = IActiveScript_SetScriptState(e.as, SCRIPTSTATE_DISCONNECTED); P("ST_DISC_CLOSED=%08lx\n", hr);
    eng_free(&e);

    /* type libraries: the Scripting library (scrrun) defines the IOMode enumeration */
    eng_new(&e, 0);
    ev(&e, "TL_BEFORE", L"IsEmpty(TristateMixed)");
    hr = IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, 0);
    P("TL_ADD=%08lx\n", hr);
    ev(&e, "TL_AFTER", L"ForReading & \",\" & ForAppending & \",\" & TristateTrue & \",\" & TristateUseDefault");
    hr = IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, 0); P("TL_ADD2=%08lx\n", hr);
    run(&e, L"On Error Resume Next\r\nForReading = 3\r\nr_tlconst = Err.Number\r\nFor i = 1 To 2\r\nNext\r\nDim ForWriting\r\nForWriting = 77\r\n", 0);
    ev(&e, "TL_CONST_ASSIGN", L"r_tlconst");
    ev(&e, "TL_SHADOW", L"ForWriting");
    hr = IActiveScript_AddTypeLib(e.as, &guid_bogus, 1, 0, 0);
    P("TL_BAD=%d\n", hr != S_OK);
    hr = IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, 0x4);
    P("TL_BADFLAGS=%08lx\n", hr);
    eng_free(&e);
    /* a persistent type library survives a reset, another does not */
    eng_new(&e, 0);
    IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, SCRIPTTYPELIB_ISPERSISTENT);
    IActiveScript_SetScriptState(e.as, SCRIPTSTATE_UNINITIALIZED);
    IActiveScript_SetScriptSite(e.as, &site.iface);
    IActiveScript_SetScriptState(e.as, SCRIPTSTATE_STARTED);
    ev(&e, "TL_PERSIST", L"ForAppending");
    eng_free(&e);
    eng_new(&e, 0);
    IActiveScript_AddTypeLib(e.as, &guid_scrrun, 1, 0, 0);
    IActiveScript_SetScriptState(e.as, SCRIPTSTATE_UNINITIALIZED);
    IActiveScript_SetScriptSite(e.as, &site.iface);
    IActiveScript_SetScriptState(e.as, SCRIPTSTATE_STARTED);
    ev(&e, "TL_NONPERSIST", L"IsEmpty(ForAppending)");
    eng_free(&e);

    /* AddScriptlet */
    eng_new(&e, 1);
    run(&e, L"glob_hook = 0\r\nevcount = 0\r\n", 0);
    hr = Parse_AddScriptlet(e.ap, L"h1", L"glob_hook = 7", NULL, NULL, NULL, NULL, 0, 0, 0, &name, &ei);
    P("SL_FN=%08lx %ls\n", hr, hr || !name ? L"" : name); SysFreeString(name); name = NULL;
    run(&e, L"h1", 0);
    ev(&e, "SL_FN_RUN", L"glob_hook");
    hr = Parse_AddScriptlet(e.ap, NULL, L"evcount = evcount + 1", L"hostex", NULL, L"onclick", NULL, 0, 0, 0, &name, &ei);
    P("SL_EVT=%08lx %ls handler=%ls\n", hr, hr || !name ? L"" : name, g_handler ? g_handler_name : L"none"); SysFreeString(name); name = NULL;
    if (g_handler) {
        VARIANT hv[2]; DISPPARAMS dp = {0}; HRESULT h2; VARIANT r;
        VariantInit(&r); V_VT(&hv[0]) = VT_I4; V_I4(&hv[0]) = 1; V_VT(&hv[1]) = VT_I4; V_I4(&hv[1]) = 2;
        dp.rgvarg = hv; dp.cArgs = 2;
        h2 = IDispatch_Invoke(g_handler, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, NULL); P("SL_CALL=%08lx\n", h2);
        dp.cArgs = 0;
        h2 = IDispatch_Invoke(g_handler, DISPID_VALUE, &IID_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, NULL); P("SL_CALL0=%08lx\n", h2);
        ev(&e, "SL_EVT_COUNT", L"evcount");
    }
    hr = Parse_AddScriptlet(e.ap, L"h4", L"glob_hook = 8 @@ junk", NULL, NULL, NULL, L"@@", 0, 0, 0, &name, &ei);
    P("SL_DELIM=%08lx\n", hr); SysFreeString(name); name = NULL;
    run(&e, L"h4", 0);
    ev(&e, "SL_DELIM_RUN", L"glob_hook");
    hr = Parse_AddScriptlet(e.ap, NULL, L"x=1", NULL, NULL, NULL, NULL, 0, 0, 0, &name, &ei); P("SL_NONAME=%08lx\n", hr);
    hr = Parse_AddScriptlet(e.ap, L"h2", L"x=1", L"nosuchitem", NULL, L"onx", NULL, 0, 0, 0, &name, &ei); P("SL_BADITEM=%08lx\n", hr);
    hr = Parse_AddScriptlet(e.ap, L"h3", L"x = + +", NULL, NULL, NULL, NULL, 0, 0, 0, &name, &ei); P("SL_SYNTAX=%d\n", hr != S_OK);
    hr = Parse_AddScriptlet(e.ap, L"h5", L"x=1", NULL, NULL, NULL, NULL, 0, 0, 0, NULL, &ei); P("SL_NULLOUT=%08lx\n", hr);
    eng_free(&e);

    /* Clone: named items, type libraries and persistent code are carried over */
    {
        IActiveScript *clone = NULL; Eng c;
        eng_new(&e, 1);
        run(&e, L"pv = 42", SCRIPTTEXT_ISPERSISTENT);
        run(&e, L"npv = 7", 0);
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
            ev(&c, "CL_ITEM", L"hostex.state()");
            ev(&c, "CL_CODE", L"pv");
            ev(&c, "CL_NONPERSISTENT", L"IsEmpty(npv)");
            ev(&c, "CL_TYPELIB", L"ForAppending");
            eng_free(&c);
        }
        g_script = e.as;
        eng_free(&e);
        /* a script that is not ready cannot be cloned */
        hr = CoCreateInstance(&CLSID_VBScript_, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void**)&clone);
        hr = IActiveScript_Clone(clone, &c.as); P("CL_UNINIT=%08lx\n", hr);
        IActiveScript_Release(clone);
    }

    /* interrupt: an abort is not resumed by On Error Resume Next; RAISEEXCEPTION raises a normal error */
#define BUSY L"On Error Resume Next\r\nDim n\r\nn = 0\r\ncaught = 0\r\nDo While n < 20000000\r\n n = n + 1\r\n If Err.Number <> 0 Then\r\n  caught = Err.Number\r\n  Exit Do\r\n End If\r\nLoop\r\nafter = 1\r\n"
    interrupt_test("INTR_ABORT", BUSY, L"IsEmpty(caught) & \",\" & IsEmpty(after)", 0);
    interrupt_test("INTR_RAISE", BUSY, L"caught & \",\" & after", SCRIPTINTERRUPT_RAISEEXCEPTION);
    interrupt_test("INTR_NESTED", L"Function Spin()\r\n Dim n\r\n Do While n < 20000000\r\n  n = n + 1\r\n Loop\r\n Spin = 1\r\nEnd Function\r\n"
                   L"Function Outer()\r\n On Error Resume Next\r\n Outer = Spin()\r\n got = 1\r\nEnd Function\r\nCall Outer()\r\nfin = 1\r\n",
                   L"IsEmpty(got) & \",\" & IsEmpty(fin)", 0);
    {   /* a stale interrupt does not hit the next script run */
        Eng e2; eng_new(&e2, 0); memset(&ei, 0, sizeof(ei)); ei.scode = E_FAIL;
        IActiveScript_InterruptScriptThread(e2.as, SCRIPTTHREADID_BASE, &ei, 0);
        ev(&e2, "INTR_STALE", L"1 + 1");
        eng_free(&e2);
    }
    {   /* after an abort a later run on the same engine works, On Error Resume Next included */
        Eng e3; eng_new(&e3, 0);
        g_int_flags = 0;
        { HANDLE th = CreateThread(NULL, 0, interrupter, NULL, 0, NULL); run(&e3, BUSY, 0); WaitForSingleObject(th, 20000); CloseHandle(th); }
        run(&e3, L"On Error Resume Next\r\nx = 1 / 0\r\nafter_abort = Err.Number\r\ny = 2\r\n", 0);
        ev(&e3, "INTR_RESUME_AFTER", L"after_abort & \",\" & y");
        eng_free(&e3);
    }

    /* debug interface: text attributes */
    eng_new(&e, 0);
    {
        DbgI *dbg = NULL;
        hr = IActiveScript_QueryInterface(e.as, &IID_Debug_, (void**)&dbg);
        P("DBG_QI=%08lx\n", hr);
        if (!hr) {
            static const WCHAR code[] = L"Dim x 'c\r\nx = \"a\"\"b\" + 12.5e3 & &HFF\r\nRem hi\r\nFunction f()\r\nEnd Function\r\nEnd Sub : If x = 1 Then";
            SOURCE_TEXT_ATTR a[128]; char out[130];
            memset(a, 0xff, sizeof(a));
            hr = Dbg_Text(dbg, code, ARRAYSIZE(code) - 1, NULL, 0, a);
            attrs_to_str(a, ARRAYSIZE(code) - 1, out);
            P("DBG_ATTR=%08lx %s\n", hr, out);
            memset(a, 0xff, sizeof(a));
            hr = Dbg_Slet(dbg, L"x = 1 '', ''END tail", 20, L"''END", 0, a);
            attrs_to_str(a, 20, out);
            P("DBG_SLET=%08lx %s\n", hr, out);
            hr = Dbg_Text(dbg, code, 4, NULL, 0, NULL); P("DBG_NULL=%08lx\n", hr);
            Dbg_Release(dbg);
        }
    }
    eng_free(&e);

    /* regular expression objects: type information, SubMatches enumeration, enumerator clone */
    eng_new(&e, 0);
    run(&e, L"Set re = New RegExp\r\nre.Pattern = \"(a)(b)\"\r\nSet mm = re.Execute(\"ab\")\r\nSet one = mm(0)\r\n"
            L"re.Pattern = \"\\w\"\r\nre.Global = True\r\nSet mc = re.Execute(\"x y\")\r\n", 0);
    typeinfo_of(&e, "RX_SUBMATCHES_TI", L"one.SubMatches");
    typeinfo_of(&e, "RX_MATCH_TI", L"one");
    typeinfo_of(&e, "RX_MATCHES_TI", L"mm");
    run(&e, L"subs = \"\"\r\nFor Each s In one.SubMatches\r\n subs = subs & s & \";\"\r\nNext\r\nms = \"\"\r\nFor Each m In mc\r\n ms = ms & m & \";\"\r\nNext\r\n", 0);
    ev(&e, "RX_SUBMATCHES_ENUM", L"subs");
    ev(&e, "RX_MATCHES_ENUM", L"ms");
    {
        IDispatch *d = get_obj(&e, L"mc");
        if (d) {
            DISPPARAMS dp = {0}; VARIANT r, v1, v2; IEnumVARIANT *en = NULL, *cl = NULL; ULONG got = 0;
            VariantInit(&r);
            hr = IDispatch_Invoke(d, DISPID_NEWENUM, &IID_NULL, 0, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp, &r, NULL, NULL);
            if (!hr && V_VT(&r) == VT_UNKNOWN) IUnknown_QueryInterface(V_UNKNOWN(&r), &IID_IEnumVARIANT, (void**)&en);
            else if (!hr && V_VT(&r) == VT_DISPATCH) IDispatch_QueryInterface(V_DISPATCH(&r), &IID_IEnumVARIANT, (void**)&en);
            if (en) {
                VariantInit(&v1); VariantInit(&v2);
                IEnumVARIANT_Next(en, 1, &v1, &got);
                hr = IEnumVARIANT_Clone(en, &cl);
                P("RX_ENUM_CLONE=%08lx\n", hr);
                if (!hr) {
                    VARIANT s; got = 0; VariantInit(&s);
                    IEnumVARIANT_Next(cl, 1, &v2, &got);
                    if (got && SUCCEEDED(VariantChangeType(&s, &v2, 0, VT_BSTR))) P("RX_ENUM_CLONE_NEXT=%ls\n", V_BSTR(&s)); else P("RX_ENUM_CLONE_NEXT=none\n");
                    VariantClear(&s);
                    IEnumVARIANT_Release(cl);
                }
                VariantClear(&v1); VariantClear(&v2);
                IEnumVARIANT_Release(en);
            }else P("RX_ENUM_CLONE=noenum %08lx\n", hr);
            VariantClear(&r);
            IDispatch_Release(d);
        }
    }
    eng_free(&e);

    P("DONE\n");
    return 0;
}
