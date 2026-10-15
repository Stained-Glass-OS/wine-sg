#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <olectl.h>
#include <oleauto.h>
#include <stdio.h>

static const CLSID CLSID_Locator = {0x76a64158,0xcb41,0x11d1,{0x8b,0x02,0x00,0x60,0x08,0x06,0xd9,0xb6}};
static const CLSID CLSID_Sink = {0x75718c9a,0xf029,0x11d1,{0xa1,0xac,0x00,0xc0,0x4f,0xb6,0xc2,0x23}};
static const IID DIID_SinkEvents = {0x75718ca0,0xf029,0x11d1,{0xa1,0xac,0x00,0xc0,0x4f,0xb6,0xc2,0x23}};
static const CLSID CLSID_NVSet = {0x9aed384e,0xce8b,0x11d1,{0x8b,0x05,0x00,0x60,0x08,0x06,0xd9,0xb6}};
#define P(...) printf(__VA_ARGS__)

static int ready, completed, puts_, last_hres, ctx_ok;
static WCHAR last_class[64];
static int events_seen;

static HRESULT inv(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *res, VARIANT *args, int nargs)
{
    DISPID id; DISPPARAMS dp = {args, NULL, nargs, 0}; OLECHAR *n = (OLECHAR *)name;
    HRESULT hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    if (res) VariantInit(res);
    return IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, res, NULL, NULL);
}

static HRESULT WINAPI ev_qi(IDispatch*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IDispatch)||IsEqualIID(r,&DIID_SinkEvents)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI ev_ar(IDispatch*f){return 2;}
static ULONG WINAPI ev_rl(IDispatch*f){return 1;}
static HRESULT WINAPI ev_c(IDispatch*f,UINT*n){*n=0;return S_OK;}
static HRESULT WINAPI ev_t(IDispatch*f,UINT i,LCID l,ITypeInfo**t){return E_NOTIMPL;}
static HRESULT WINAPI ev_n(IDispatch*f,REFIID r,LPOLESTR*n,UINT c,LCID l,DISPID*d){return E_NOTIMPL;}
static HRESULT WINAPI ev_i(IDispatch*f,DISPID d,REFIID r,LCID l,WORD fl,DISPPARAMS*p,VARIANT*v,EXCEPINFO*e,UINT*a)
{
    events_seen++;
    if (d == 1 && p->cArgs == 2)            /* OnObjectReady(obj, ctx) */
    {
        VARIANT *obj = &p->rgvarg[1], path, cls;
        ready++;
        if (V_VT(obj) == VT_DISPATCH && V_DISPATCH(obj))
        {
            IDispatch *pd = NULL;
            if (SUCCEEDED(inv(V_DISPATCH(obj), L"Path_", DISPATCH_PROPERTYGET, &path, NULL, 0)) && V_VT(&path) == VT_DISPATCH)
            {
                pd = V_DISPATCH(&path);
                if (SUCCEEDED(inv(pd, L"Class", DISPATCH_PROPERTYGET, &cls, NULL, 0)) && V_VT(&cls) == VT_BSTR)
                    lstrcpynW(last_class, V_BSTR(&cls), 64);
                VariantClear(&cls); IDispatch_Release(pd);
            }
        }
    }
    else if (d == 2 && p->cArgs == 3)       /* OnCompleted(hres, err, ctx) */
    {
        VARIANT *ctx = &p->rgvarg[0], item, val; VARIANT nm;
        completed++; last_hres = V_I4(&p->rgvarg[2]);
        if (V_VT(ctx) == VT_DISPATCH && V_DISPATCH(ctx))
        {
            V_VT(&nm) = VT_BSTR; V_BSTR(&nm) = SysAllocString(L"tag");
            if (SUCCEEDED(inv(V_DISPATCH(ctx), L"Item", DISPATCH_METHOD, &item, &nm, 1)) && V_VT(&item) == VT_DISPATCH &&
                SUCCEEDED(inv(V_DISPATCH(&item), L"Value", DISPATCH_PROPERTYGET, &val, NULL, 0)) && V_VT(&val) == VT_BSTR)
                ctx_ok = !lstrcmpW(V_BSTR(&val), L"abc");
            SysFreeString(V_BSTR(&nm));
        }
    }
    else if (d == 4) puts_++;
    return S_OK;
}
static IDispatchVtbl evt={ev_qi,ev_ar,ev_rl,ev_c,ev_t,ev_n,ev_i};
static IDispatch evsink={&evt};

static void arg_bstr(VARIANT *v, const WCHAR *s){ V_VT(v)=VT_BSTR; V_BSTR(v)=SysAllocString(s); }
static void arg_i4(VARIANT *v, LONG l){ V_VT(v)=VT_I4; V_I4(v)=l; }
static void arg_disp(VARIANT *v, IDispatch *d){ V_VT(v)=VT_DISPATCH; V_DISPATCH(v)=d; }
static void arg_null(VARIANT *v){ V_VT(v)=VT_DISPATCH; V_DISPATCH(v)=NULL; }

int main(void)
{
    IDispatch *loc, *svc, *sink, *ctx, *dict_like; HRESULT hr; VARIANT res, a[6];
    IConnectionPointContainer *cpc; IConnectionPoint *cp; DWORD cookie; int n, i;
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_Locator,NULL,CLSCTX_INPROC_SERVER,&IID_IDispatch,(void**)&loc); P("LOC %08lx\n", hr);
    arg_bstr(&a[1], L"."); arg_bstr(&a[0], L"root\\cimv2");
    /* ConnectServer(server, namespace): arguments in reverse order */
    hr = inv(loc, L"ConnectServer", DISPATCH_METHOD, &res, a, 2); P("CONNECT %08lx\n", hr);
    svc = V_DISPATCH(&res);
    hr = CoCreateInstance(&CLSID_Sink,NULL,CLSCTX_INPROC_SERVER,&IID_IDispatch,(void**)&sink); P("SINK %08lx\n", hr);
    hr = CoCreateInstance(&CLSID_NVSet,NULL,CLSCTX_INPROC_SERVER,&IID_IDispatch,(void**)&ctx); P("NVSET %08lx\n", hr);
    arg_bstr(&a[1], L"tag"); arg_bstr(&a[0], L"abc");
    inv(ctx, L"Add", DISPATCH_METHOD, &res, a, 2);

    hr = IDispatch_QueryInterface(sink,&IID_IConnectionPointContainer,(void**)&cpc); P("CPC %08lx\n", hr);
    hr = IConnectionPointContainer_FindConnectionPoint(cpc,&DIID_SinkEvents,&cp); P("CP %08lx\n", hr);
    hr = IConnectionPointContainer_FindConnectionPoint(cpc,&IID_IDispatch,&dict_like ? (IConnectionPoint**)&dict_like : NULL); P("CP_OTHER %d\n", hr == CONNECT_E_NOCONNECTION);
    { IProvideClassInfo2 *pci; GUID g; hr = IDispatch_QueryInterface(sink,&IID_IProvideClassInfo2,(void**)&pci);
      if (SUCCEEDED(hr)) { hr = IProvideClassInfo2_GetGUID(pci,GUIDKIND_DEFAULT_SOURCE_DISP_IID,&g); P("SRCIID %d\n", SUCCEEDED(hr) && IsEqualGUID(&g,&DIID_SinkEvents));
        { ITypeInfo *ti = NULL; hr = IProvideClassInfo2_GetClassInfo(pci,&ti); P("CLASSINFO %08lx\n", hr);
          if (ti) { TYPEATTR *ta; ITypeInfo_GetTypeAttr(ti,&ta); P("CLASSKIND %d impl %d\n", ta->typekind, ta->cImplTypes); ITypeInfo_ReleaseTypeAttr(ti,ta); ITypeInfo_Release(ti); } }
        IProvideClassInfo2_Release(pci); } else P("SRCIID 0\n"); }
    hr = IConnectionPoint_Advise(cp,(IUnknown*)&evsink,&cookie); P("ADVISE %08lx %lu\n", hr, cookie);

    /* GetAsync(sink, path, flags, valueset, ctx) */
    arg_null(&a[4]); /* placeholder to keep indexes simple below */
    ready = completed = 0;
    arg_disp(&a[4], sink); arg_bstr(&a[3], L"Win32_Process"); arg_i4(&a[2], 0); arg_null(&a[1]); arg_disp(&a[0], ctx);
    { VARIANT r[5] = { a[0], a[1], a[2], a[3], a[4] }; hr = inv(svc, L"GetAsync", DISPATCH_METHOD, &res, r, 5); }
    P("GETASYNC %08lx ready %d class %ls completed %d hres %d ctx %d\n", hr, ready, last_class, completed, last_hres, ctx_ok);

    /* the same set synchronously, for the count */
    arg_bstr(&a[0], L"SELECT * FROM Win32_Process");
    hr = inv(svc, L"ExecQuery", DISPATCH_METHOD, &res, a, 1);
    { VARIANT cnt; hr = inv(V_DISPATCH(&res), L"Count", DISPATCH_PROPERTYGET, &cnt, NULL, 0); n = V_I4(&cnt); }
    P("SYNC_COUNT %d\n", n > 0);
    ready = completed = 0;
    { VARIANT r[5]; arg_disp(&r[4], sink); arg_bstr(&r[3], L"SELECT * FROM Win32_Process"); arg_bstr(&r[2], L"WQL"); arg_i4(&r[1], 0); arg_null(&r[0]);
      VARIANT q[6]; q[5]=r[4]; q[4]=r[3]; q[3]=r[2]; q[2]=r[1]; q[1]=r[0]; arg_disp(&q[0], ctx);
      /* parameters in reverse: ctx, valueset, flags, language, query, sink */
      { VARIANT rr[6]; rr[0]=q[0]; rr[1]=q[1]; rr[2]=q[2]; rr[3]=q[3]; rr[4]=q[4]; rr[5]=q[5]; hr = inv(svc, L"ExecQueryAsync", DISPATCH_METHOD, &res, rr, 6); } }
    P("QUERYASYNC %08lx every %d completed %d\n", hr, ready == n, completed);
    ready = completed = 0;
    { VARIANT rr[5]; arg_disp(&rr[0], ctx); arg_null(&rr[1]); arg_i4(&rr[2], 0); arg_bstr(&rr[3], L"Win32_Process"); arg_disp(&rr[4], sink);
      hr = inv(svc, L"InstancesOfAsync", DISPATCH_METHOD, &res, rr, 5); }
    P("INSTANCESASYNC %08lx every %d completed %d\n", hr, ready == n, completed);
    ready = completed = 0;
    { VARIANT rr[5]; arg_disp(&rr[0], ctx); arg_null(&rr[1]); arg_i4(&rr[2], 0); arg_bstr(&rr[3], L"Win32_Process"); arg_disp(&rr[4], sink);
      hr = inv(svc, L"SubclassesOfAsync", DISPATCH_METHOD, &res, rr, 5); }
    P("SUBCLASSESASYNC %08lx completed %d\n", hr, completed);

    /* a bad query is refused at the call; a thing that is not a sink is refused */
    ready = completed = 0;
    { VARIANT rr[6]; arg_disp(&rr[0], ctx); arg_null(&rr[1]); arg_i4(&rr[2], 0); arg_bstr(&rr[3], L"WQL"); arg_bstr(&rr[4], L"SELECT * FROM"); arg_disp(&rr[5], sink);
      hr = inv(svc, L"ExecQueryAsync", DISPATCH_METHOD, &res, rr, 6); }
    P("BADQUERY %d completed %d\n", FAILED(hr), completed);
    ready = completed = 0;
    { VARIANT rr[2]; arg_bstr(&rr[0], L"Win32_Process"); arg_disp(&rr[1], ctx);   /* the name value set is not a sink */
      hr = inv(svc, L"GetAsync", DISPATCH_METHOD, &res, rr, 2); }
    P("NOTASINK %08lx ready %d\n", hr, ready);

    /* unadvised: no events; Cancel is fine */
    hr = IConnectionPoint_Unadvise(cp,cookie); P("UNADVISE %08lx\n", hr);
    hr = IConnectionPoint_Unadvise(cp,cookie); P("UNADVISE2 %d\n", hr == CONNECT_E_NOCONNECTION);
    ready = completed = 0;
    { VARIANT rr[5]; arg_null(&rr[0]); arg_null(&rr[1]); arg_i4(&rr[2], 0); arg_bstr(&rr[3], L"Win32_Process"); arg_disp(&rr[4], sink);
      hr = inv(svc, L"GetAsync", DISPATCH_METHOD, &res, rr, 5); }
    P("UNCONNECTED %08lx events %d\n", hr, ready + completed);
    hr = inv(sink, L"Cancel", DISPATCH_METHOD, &res, NULL, 0); P("CANCEL %08lx\n", hr);
    P("DONE\n");
    return 0;
}
