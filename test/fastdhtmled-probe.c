#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <stdio.h>
#include <olectl.h>
#include <wchar.h>
static int fails;
#define CHECK(c) do{ if(c) printf("PASS %s\n",#c); else {printf("FAIL line %d %s\n",__LINE__,#c); fails++;} }while(0)

static const CLSID CLSID_Edit={0x2d360200,0xfff5,0x11d1,{0x8d,0x03,0x00,0xa0,0xc9,0x59,0xbc,0x0a}};
static const IID DIID_Events={0x588d5040,0xcf28,0x11d1,{0x8c,0xd3,0x00,0xa0,0xc9,0x59,0xbc,0x0a}};

static IDispatch *ed;
static HRESULT inv(const WCHAR *name, WORD flags, VARIANT *res, VARIANT *args, int nargs)
{
    DISPID id, put = DISPID_PROPERTYPUT;
    DISPPARAMS dp = {args, NULL, nargs, 0};
    OLECHAR *n = (OLECHAR *)name;
    HRESULT hr = IDispatch_GetIDsOfNames(ed, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    if (flags & (DISPATCH_PROPERTYPUT)) { dp.cNamedArgs = 1; dp.rgdispidNamedArgs = &put; }
    if (res) VariantInit(res);
    return IDispatch_Invoke(ed, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, res, NULL, NULL);
}
static HRESULT getb(const WCHAR *n, VARIANT_BOOL *b){ VARIANT r; HRESULT hr=inv(n,DISPATCH_PROPERTYGET,&r,NULL,0); if(SUCCEEDED(hr)){*b=V_BOOL(&r);} return hr; }
static HRESULT putb(const WCHAR *n, VARIANT_BOOL b){ VARIANT a; V_VT(&a)=VT_BOOL; V_BOOL(&a)=b; return inv(n,DISPATCH_PROPERTYPUT,NULL,&a,1); }
static HRESULT geti(const WCHAR *n, LONG *v){ VARIANT r; HRESULT hr=inv(n,DISPATCH_PROPERTYGET,&r,NULL,0); if(SUCCEEDED(hr)){*v=V_I4(&r);} return hr; }
static HRESULT puti(const WCHAR *n, LONG v){ VARIANT a; V_VT(&a)=VT_I4; V_I4(&a)=v; return inv(n,DISPATCH_PROPERTYPUT,NULL,&a,1); }
static HRESULT getstr(const WCHAR *n, BSTR *s){ VARIANT r; HRESULT hr=inv(n,DISPATCH_PROPERTYGET,&r,NULL,0); if(SUCCEEDED(hr)){*s=V_BSTR(&r);} return hr; }
static HRESULT puts_(const WCHAR *n, const WCHAR *s){ VARIANT a; V_VT(&a)=VT_BSTR; V_BSTR(&a)=SysAllocString(s); HRESULT hr=inv(n,DISPATCH_PROPERTYPUT,NULL,&a,1); SysFreeString(V_BSTR(&a)); return hr; }

static int completes;
typedef struct { IDispatch i; } Ev;
static HRESULT WINAPI ev_qi(IDispatch*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IDispatch)||IsEqualIID(r,&DIID_Events)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI ev_ar(IDispatch*f){return 2;}
static ULONG WINAPI ev_rl(IDispatch*f){return 1;}
static HRESULT WINAPI ev_c(IDispatch*f,UINT*n){*n=0;return S_OK;}
static HRESULT WINAPI ev_t(IDispatch*f,UINT i,LCID l,ITypeInfo**t){return E_NOTIMPL;}
static HRESULT WINAPI ev_n(IDispatch*f,REFIID r,LPOLESTR*n,UINT c,LCID l,DISPID*d){return E_NOTIMPL;}
static HRESULT WINAPI ev_i(IDispatch*f,DISPID d,REFIID r,LCID l,WORD fl,DISPPARAMS*p,VARIANT*v,EXCEPINFO*e,UINT*a){ if(d==1) completes++; return S_OK;}
static IDispatchVtbl evt={ev_qi,ev_ar,ev_rl,ev_c,ev_t,ev_n,ev_i};

int main(void)
{
    VARIANT_BOOL b; LONG l; BSTR s; HRESULT hr; VARIANT args[2], res;
    WCHAR tmp[MAX_PATH], file[MAX_PATH];
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_Edit,NULL,CLSCTX_INPROC_SERVER,&IID_IDispatch,(void**)&ed);
    CHECK(hr==S_OK);
    if(FAILED(hr)){printf("RESULT: FAIL\n");return 1;}

    /* defaults */
    hr=getb(L"ActivateApplets",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    hr=getb(L"ActivateActiveXControls",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    hr=getb(L"ActivateDTCs",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    hr=getb(L"ShowDetails",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    hr=getb(L"ShowBorders",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    hr=getb(L"Scrollbars",&b); CHECK(hr==S_OK && b==VARIANT_TRUE);
    hr=getb(L"SourceCodePreservation",&b); CHECK(hr==S_OK && b==VARIANT_TRUE);
    hr=geti(L"SnapToGridX",&l); CHECK(hr==S_OK && l==50);
    hr=geti(L"SnapToGridY",&l); CHECK(hr==S_OK && l==50);
    hr=getb(L"IsDirty",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    hr=getb(L"Busy",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    /* set and get */
    hr=putb(L"ShowBorders",VARIANT_TRUE); CHECK(hr==S_OK);
    hr=getb(L"ShowBorders",&b); CHECK(hr==S_OK && b==VARIANT_TRUE);
    hr=putb(L"Scrollbars",VARIANT_FALSE); hr=getb(L"Scrollbars",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
    hr=puti(L"SnapToGridX",20); hr=geti(L"SnapToGridX",&l); CHECK(hr==S_OK && l==20);
    hr=puti(L"Appearance",0); hr=geti(L"Appearance",&l); CHECK(hr==S_OK && l==0);
    hr=puti(L"Appearance",7); CHECK(FAILED(hr));
    hr=puti(L"ScrollbarAppearance",1); hr=geti(L"ScrollbarAppearance",&l); CHECK(hr==S_OK && l==1);
    hr=puts_(L"BaseURL",L"http://example.test/x/"); CHECK(hr==S_OK);
    hr=getstr(L"BaseURL",&s); CHECK(hr==S_OK && !lstrcmpW(s,L"http://example.test/x/")); SysFreeString(s);

    /* events */
    {
        IConnectionPointContainer *cpc; IConnectionPoint *cp; Ev ev={&evt}; DWORD cookie; IID iid; IConnectionPointContainer *back;
        hr=IDispatch_QueryInterface(ed,&IID_IConnectionPointContainer,(void**)&cpc); CHECK(hr==S_OK);
        hr=IConnectionPointContainer_FindConnectionPoint(cpc,&DIID_Events,&cp); CHECK(hr==S_OK);
        hr=IConnectionPoint_GetConnectionInterface(cp,&iid); CHECK(hr==S_OK && IsEqualIID(&iid,&DIID_Events));
        hr=IConnectionPoint_GetConnectionPointContainer(cp,&back); CHECK(hr==S_OK && back==cpc); if(SUCCEEDED(hr)) IConnectionPointContainer_Release(back);
        hr=IConnectionPoint_Advise(cp,(IUnknown*)&ev.i,&cookie); CHECK(hr==S_OK && cookie);

        /* documents */
        hr=inv(L"NewDocument",DISPATCH_METHOD,NULL,NULL,0); CHECK(hr==S_OK && completes==1);
        hr=getstr(L"DocumentHTML",&s); CHECK(hr==S_OK && !lstrcmpW(s,L"<HTML></HTML>")); SysFreeString(s);
        hr=puts_(L"DocumentHTML",L"<html><head><title>My page</title></head><body>caf\u00e9</body></html>"); CHECK(hr==S_OK && completes==2);
        hr=getb(L"IsDirty",&b); CHECK(hr==S_OK && b==VARIANT_TRUE);
        hr=getstr(L"DocumentTitle",&s); CHECK(hr==S_OK && !lstrcmpW(s,L"My page")); SysFreeString(s);
        hr=getstr(L"CurrentDocumentPath",&s); CHECK(hr==S_OK && !*s); SysFreeString(s);
        GetTempPathW(MAX_PATH,tmp); lstrcpyW(file,tmp); lstrcatW(file,L"sgdhtmled-test.htm");
        V_VT(&args[1])=VT_BSTR; V_BSTR(&args[1])=SysAllocString(file);   /* args are in reverse order: prompt, path */
        V_VT(&args[0])=VT_BOOL; V_BOOL(&args[0])=VARIANT_FALSE;
        { VARIANT a[2]; a[0]=args[0]; a[1]=args[1]; hr=inv(L"SaveDocument",DISPATCH_METHOD,NULL,a,2); }
        CHECK(hr==S_OK);
        hr=getb(L"IsDirty",&b); CHECK(hr==S_OK && b==VARIANT_FALSE);
        hr=getstr(L"CurrentDocumentPath",&s); CHECK(hr==S_OK && s && !lstrcmpiW(s,file)); SysFreeString(s);
        hr=inv(L"NewDocument",DISPATCH_METHOD,NULL,NULL,0);
        { VARIANT a[2]; a[0]=args[0]; a[1]=args[1]; hr=inv(L"LoadDocument",DISPATCH_METHOD,NULL,a,2); }
        CHECK(hr==S_OK && completes==4);
        hr=getstr(L"DocumentTitle",&s); CHECK(hr==S_OK && !lstrcmpW(s,L"My page")); SysFreeString(s);
        hr=getstr(L"DocumentHTML",&s); CHECK(hr==S_OK && wcsstr(s,L"caf\u00e9")); SysFreeString(s);
        { VARIANT a; V_VT(&a)=VT_BSTR; V_BSTR(&a)=SysAllocString(L"C:\\no\\such\\file.htm"); hr=inv(L"LoadURL",DISPATCH_METHOD,NULL,&a,1); SysFreeString(V_BSTR(&a)); }
        CHECK(FAILED(hr));
        DeleteFileW(file);
        {
            VARIANT a; BSTR out;
            V_VT(&a)=VT_BSTR; V_BSTR(&a)=SysAllocString(L"<b>x</b>");
            hr=inv(L"FilterSourceCode",DISPATCH_METHOD,&res,&a,1); CHECK(hr==S_OK && V_VT(&res)==VT_BSTR && !lstrcmpW(V_BSTR(&res),L"<b>x</b>"));
            (void)out; SysFreeString(V_BSTR(&a));
        }
        hr=IConnectionPoint_Unadvise(cp,cookie); CHECK(hr==S_OK);
        hr=IConnectionPoint_Unadvise(cp,cookie); CHECK(hr==CONNECT_E_NOCONNECTION);
        hr=inv(L"NewDocument",DISPATCH_METHOD,NULL,NULL,0); CHECK(completes==4);
        IConnectionPoint_Release(cp); IConnectionPointContainer_Release(cpc);
    }
    /* OLE object and persistence */
    {
        IOleObject *oo; IPersistStreamInit *psi; IStream *st; CLSID clsid; LPOLESTR type; STATSTG stat; ULARGE_INTEGER max; DWORD conn; char buf[64]; ULONG rd;
        LARGE_INTEGER z={{0}};
        hr=IDispatch_QueryInterface(ed,&IID_IOleObject,(void**)&oo); CHECK(hr==S_OK);
        hr=IOleObject_GetUserClassID(oo,&clsid); CHECK(hr==S_OK && IsEqualGUID(&clsid,&CLSID_Edit));
        hr=IOleObject_SetHostNames(oo,L"app",L"obj"); CHECK(hr==S_OK);
        hr=IOleObject_IsUpToDate(oo); CHECK(hr==S_OK);
        hr=IOleObject_Unadvise(oo,12345); CHECK(FAILED(hr));
        hr=IOleObject_GetUserType(oo,USERCLASSTYPE_FULL,&type); printf("usertype hr=%lx\n",hr);
        IOleObject_Release(oo);

        hr=IDispatch_QueryInterface(ed,&IID_IPersistStreamInit,(void**)&psi); CHECK(hr==S_OK);
        hr=IPersistStreamInit_GetClassID(psi,&clsid); CHECK(hr==S_OK && IsEqualGUID(&clsid,&CLSID_Edit));
        hr=IPersistStreamInit_InitNew(psi); CHECK(hr==S_OK);
        hr=IPersistStreamInit_IsDirty(psi); CHECK(hr==S_FALSE);
        hr=CreateStreamOnHGlobal(NULL,TRUE,&st); CHECK(hr==S_OK);
        { const char *h="<html><title>T</title>x</html>"; ULONG w; IStream_Write(st,h,lstrlenA(h),&w); IStream_Seek(st,z,STREAM_SEEK_SET,NULL); }
        hr=IPersistStreamInit_Load(psi,st); CHECK(hr==S_OK);
        hr=getstr(L"DocumentTitle",&s); CHECK(hr==S_OK && !lstrcmpW(s,L"T")); SysFreeString(s);
        hr=puts_(L"DocumentHTML",L"<p>saved</p>");
        hr=getb(L"IsDirty",&b); CHECK(b==VARIANT_TRUE);
        IStream_Release(st); CreateStreamOnHGlobal(NULL,TRUE,&st);
        hr=IPersistStreamInit_Save(psi,st,TRUE); CHECK(hr==S_OK);
        hr=getb(L"IsDirty",&b); CHECK(b==VARIANT_FALSE);
        IStream_Stat(st,&stat,STATFLAG_NONAME); CHECK(stat.cbSize.QuadPart==12);
        IStream_Seek(st,z,STREAM_SEEK_SET,NULL); memset(buf,0,sizeof buf); IStream_Read(st,buf,63,&rd); CHECK(rd==12 && !strcmp(buf,"<p>saved</p>"));
        hr=IPersistStreamInit_GetSizeMax(psi,&max); CHECK(hr==S_OK && max.QuadPart>=12);
        IStream_Release(st); IPersistStreamInit_Release(psi);
    }
    IDispatch_Release(ed);
    printf("RESULT: %s\n",fails?"FAIL":"PASS");
    return fails!=0;
}
