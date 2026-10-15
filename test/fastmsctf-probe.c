#define COBJMACROS
#include <windows.h>
#include <msctf.h>
#include <stdio.h>
#include <olectl.h>
#include <ctfutb.h>
static int fails;
#define CHECK(c) do{ if(c) printf("PASS %s\n",#c); else {printf("FAIL line %d %s\n",__LINE__,#c); fails++;} }while(0)

typedef struct { ITfUIElement i; LONG ref; } Elem;
static HRESULT WINAPI e_qi(ITfUIElement*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfUIElement)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI e_ar(ITfUIElement*f){return InterlockedIncrement(&((Elem*)f)->ref);}
static ULONG WINAPI e_rl(ITfUIElement*f){return InterlockedDecrement(&((Elem*)f)->ref);}
static HRESULT WINAPI e_d(ITfUIElement*f,BSTR*b){*b=NULL;return S_OK;}
static HRESULT WINAPI e_g(ITfUIElement*f,GUID*g){*g=GUID_NULL;return S_OK;}
static HRESULT WINAPI e_s(ITfUIElement*f,BOOL b){return S_OK;}
static HRESULT WINAPI e_sh(ITfUIElement*f,BOOL*b){*b=TRUE;return S_OK;}
static ITfUIElementVtbl evt={e_qi,e_ar,e_rl,e_d,e_g,e_s,e_sh};

typedef struct { ITfUIElementSink i; int begin,update,end; BOOL hide; } USink;
static HRESULT WINAPI s_qi(ITfUIElementSink*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfUIElementSink)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI s_ar(ITfUIElementSink*f){return 2;}
static ULONG WINAPI s_rl(ITfUIElementSink*f){return 1;}
static HRESULT WINAPI s_b(ITfUIElementSink*f,DWORD id,BOOL*show){USink*s=(USink*)f;s->begin++; if(s->hide)*show=FALSE;return S_OK;}
static HRESULT WINAPI s_u(ITfUIElementSink*f,DWORD id){((USink*)f)->update++;return S_OK;}
static HRESULT WINAPI s_e(ITfUIElementSink*f,DWORD id){((USink*)f)->end++;return S_OK;}
static ITfUIElementSinkVtbl svt={s_qi,s_ar,s_rl,s_b,s_u,s_e};

typedef struct { ITfKeyEventSink i; int tdown,down,tup,up,pres; GUID pg; WPARAM last; BOOL eat; } KSink;
static HRESULT WINAPI k_qi(ITfKeyEventSink*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfKeyEventSink)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI k_ar(ITfKeyEventSink*f){return 2;}
static ULONG WINAPI k_rl(ITfKeyEventSink*f){return 1;}
static HRESULT WINAPI k_sf(ITfKeyEventSink*f,BOOL b){return S_OK;}
static HRESULT WINAPI k_td(ITfKeyEventSink*f,ITfContext*c,WPARAM w,LPARAM l,BOOL*e){KSink*k=(KSink*)f;k->tdown++;k->last=w;*e=k->eat;return S_OK;}
static HRESULT WINAPI k_tu(ITfKeyEventSink*f,ITfContext*c,WPARAM w,LPARAM l,BOOL*e){KSink*k=(KSink*)f;k->tup++;*e=k->eat;return S_OK;}
static HRESULT WINAPI k_d(ITfKeyEventSink*f,ITfContext*c,WPARAM w,LPARAM l,BOOL*e){KSink*k=(KSink*)f;k->down++;*e=k->eat;return S_OK;}
static HRESULT WINAPI k_u(ITfKeyEventSink*f,ITfContext*c,WPARAM w,LPARAM l,BOOL*e){KSink*k=(KSink*)f;k->up++;*e=k->eat;return S_OK;}
static HRESULT WINAPI k_p(ITfKeyEventSink*f,ITfContext*c,REFGUID g,BOOL*e){KSink*k=(KSink*)f;k->pres++;k->pg=*g;*e=TRUE;return S_OK;}
static ITfKeyEventSinkVtbl kvt={k_qi,k_ar,k_rl,k_sf,k_td,k_tu,k_d,k_u,k_p};

typedef struct { ITfLangBarItem i; GUID g; DWORD status; } LItem;
static HRESULT WINAPI li_qi(ITfLangBarItem*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfLangBarItem)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI li_ar(ITfLangBarItem*f){return 2;}
static ULONG WINAPI li_rl(ITfLangBarItem*f){return 1;}
static HRESULT WINAPI li_gi(ITfLangBarItem*f,TF_LANGBARITEMINFO*i){memset(i,0,sizeof(*i));i->guidItem=((LItem*)f)->g;lstrcpyW(i->szDescription,L"item");return S_OK;}
static HRESULT WINAPI li_gs(ITfLangBarItem*f,DWORD*s){*s=((LItem*)f)->status;return S_OK;}
static HRESULT WINAPI li_sh(ITfLangBarItem*f,BOOL b){return S_OK;}
static HRESULT WINAPI li_tt(ITfLangBarItem*f,BSTR*b){*b=NULL;return S_OK;}
static ITfLangBarItemVtbl livt={li_qi,li_ar,li_rl,li_gi,li_gs,li_sh,li_tt};
typedef struct { ITfLangBarEventSink i; } LEv;
static HRESULT WINAPI le_qi(ITfLangBarEventSink*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfLangBarEventSink)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI le_ar(ITfLangBarEventSink*f){return 2;}
static ULONG WINAPI le_rl(ITfLangBarEventSink*f){return 1;}
static HRESULT WINAPI le_a(ITfLangBarEventSink*f,DWORD t){return S_OK;}
static HRESULT WINAPI le_m(ITfLangBarEventSink*f,DWORD t,UINT m,WPARAM w,LPARAM l){return S_OK;}
static HRESULT WINAPI le_r(ITfLangBarEventSink*f,DWORD t,REFGUID g,RECT*r){return E_FAIL;}
static ITfLangBarEventSinkVtbl levt={le_qi,le_ar,le_rl,le_a,le_a,le_a,le_m,le_a,le_r};

static const GUID pkguid={0x12345678,1,2,{3,4,5,6,7,8,9,10}};

static const GUID CLSID_Fake={0x9a1b2c3d,0x4e5f,0x6071,{0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf9}};
static TfClientId tip_tid; static int tip_act;
static HRESULT WINAPI t_qi(ITfTextInputProcessor*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfTextInputProcessor)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI t_ar(ITfTextInputProcessor*f){return 2;}
static ULONG WINAPI t_rl(ITfTextInputProcessor*f){return 1;}
static HRESULT WINAPI t_a(ITfTextInputProcessor*f,ITfThreadMgr*m,TfClientId id){tip_tid=id;tip_act++;return S_OK;}
static HRESULT WINAPI t_d(ITfTextInputProcessor*f){return S_OK;}
static ITfTextInputProcessorVtbl tvt={t_qi,t_ar,t_rl,t_a,t_d};
static ITfTextInputProcessor tip={&tvt};
static HRESULT WINAPI cf_qi(IClassFactory*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IClassFactory)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI cf_ar(IClassFactory*f){return 2;}
static ULONG WINAPI cf_rl(IClassFactory*f){return 1;}
static HRESULT WINAPI cf_ci(IClassFactory*f,IUnknown*o,REFIID r,void**ppv){ if(o)return CLASS_E_NOAGGREGATION; return ITfTextInputProcessor_QueryInterface(&tip,r,ppv);}
static HRESULT WINAPI cf_ls(IClassFactory*f,BOOL l){return S_OK;}
static IClassFactoryVtbl cfvt={cf_qi,cf_ar,cf_rl,cf_ci,cf_ls};
static IClassFactory cfac={&cfvt};

int main(void)
{
    ITfInputProcessorProfiles *ipp;
    ITfThreadMgrEx *tmx; ITfThreadMgr *tm; TfClientId tid; DWORD fl; HRESULT hr;
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_TF_ThreadMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfThreadMgrEx,(void**)&tmx);
    CHECK(hr==S_OK);
    if(FAILED(hr)){printf("RESULT: FAIL\n");return 1;}
    tm=(ITfThreadMgr*)tmx;
    ITfThreadMgrEx_GetActiveFlags(tmx,&fl); CHECK(fl==0);
    hr=ITfThreadMgrEx_ActivateEx(tmx,&tid,TF_TMAE_NOACTIVATETIP); CHECK(hr==S_OK);
    ITfThreadMgrEx_GetActiveFlags(tmx,&fl); CHECK(fl==(TF_TMF_ACTIVATED|TF_TMAE_NOACTIVATETIP));
    hr=ITfThreadMgrEx_ActivateEx(tmx,&tid,0x40000000); CHECK(hr==E_INVALIDARG);
    ITfThreadMgrEx_Deactivate(tmx);
    ITfThreadMgrEx_GetActiveFlags(tmx,&fl); CHECK(fl==0);
    hr=ITfThreadMgrEx_ActivateEx(tmx,&tid,0); CHECK(hr==S_OK);

    /* UI elements */
    {
        ITfUIElementMgr *um; ITfSource *src; DWORD cookie,id1,id2,n; BOOL show; USink us={{&svt},0,0,0,FALSE};
        Elem e1={{&evt},1}, e2={{&evt},1}; ITfUIElement *got; IEnumTfUIElements *en,*en2; ITfUIElement *arr[4];
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfUIElementMgr,(void**)&um); CHECK(hr==S_OK);
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfSource,(void**)&src); CHECK(hr==S_OK);
        hr=ITfSource_AdviseSink(src,&IID_ITfUIElementSink,(IUnknown*)&us.i,&cookie); CHECK(hr==S_OK);
        hr=ITfUIElementMgr_BeginUIElement(um,&e1.i,&show,&id1); CHECK(hr==S_OK && show && id1==1 && us.begin==1);
        us.hide=TRUE;
        hr=ITfUIElementMgr_BeginUIElement(um,&e2.i,&show,&id2); CHECK(hr==S_OK && !show && id2==2 && us.begin==2);
        hr=ITfUIElementMgr_UpdateUIElement(um,id1); CHECK(hr==S_OK && us.update==1);
        hr=ITfUIElementMgr_UpdateUIElement(um,99); CHECK(hr==E_INVALIDARG);
        hr=ITfUIElementMgr_GetUIElement(um,id2,&got); CHECK(hr==S_OK && got==&e2.i);
        if(SUCCEEDED(hr)) ITfUIElement_Release(got);
        hr=ITfUIElementMgr_GetUIElement(um,77,&got); CHECK(hr==E_INVALIDARG);
        hr=ITfUIElementMgr_EnumUIElements(um,&en); CHECK(hr==S_OK);
        hr=IEnumTfUIElements_Next(en,4,arr,&n); CHECK(hr==S_FALSE && n==2 && arr[0]==&e1.i);
        ITfUIElement_Release(arr[0]);ITfUIElement_Release(arr[1]);
        IEnumTfUIElements_Reset(en); IEnumTfUIElements_Skip(en,1);
        IEnumTfUIElements_Clone(en,&en2);
        hr=IEnumTfUIElements_Next(en2,1,arr,&n); CHECK(hr==S_OK && arr[0]==&e2.i);
        ITfUIElement_Release(arr[0]);
        IEnumTfUIElements_Release(en); IEnumTfUIElements_Release(en2);
        hr=ITfUIElementMgr_EndUIElement(um,id1); CHECK(hr==S_OK && us.end==1);
        hr=ITfUIElementMgr_EndUIElement(um,id1); CHECK(hr==E_INVALIDARG);
        ITfUIElementMgr_EndUIElement(um,id2);
        CHECK(e1.ref==1 && e2.ref==1);
        ITfSource_UnadviseSink(src,cookie);
        ITfSource_Release(src); ITfUIElementMgr_Release(um);
    }
    /* single sinks */
    {
        ITfSourceSingle *ss; KSink ks={{&kvt}}; KSink ks2={{&kvt}};
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfSourceSingle,(void**)&ss); CHECK(hr==S_OK);
        hr=ITfSourceSingle_UnadviseSingleSink(ss,tid,&IID_ITfKeyEventSink); CHECK(hr==CONNECT_E_NOCONNECTION);
        hr=ITfSourceSingle_AdviseSingleSink(ss,tid,&IID_ITfKeyEventSink,(IUnknown*)&ks.i); CHECK(hr==S_OK);
        hr=ITfSourceSingle_AdviseSingleSink(ss,tid,&IID_ITfKeyEventSink,(IUnknown*)&ks2.i); CHECK(hr==CONNECT_E_ADVISELIMIT);
        hr=ITfSourceSingle_UnadviseSingleSink(ss,tid,&IID_ITfKeyEventSink); CHECK(hr==S_OK);
        hr=ITfSourceSingle_UnadviseSingleSink(ss,tid,&IID_ITfKeyEventSink); CHECK(hr==CONNECT_E_NOCONNECTION);
        hr=ITfSourceSingle_AdviseSingleSink(ss,tid,&IID_ITfKeyEventSink,NULL); CHECK(hr==E_INVALIDARG);
        ITfSourceSingle_Release(ss);
    }
    /* keystroke */
    {
        DWORD reg; LANGID lang;
        hr=CoCreateInstance(&CLSID_TF_InputProcessorProfiles,NULL,CLSCTX_INPROC_SERVER,&IID_ITfInputProcessorProfiles,(void**)&ipp); CHECK(hr==S_OK);
        ITfInputProcessorProfiles_GetCurrentLanguage(ipp,&lang);
        hr=CoRegisterClassObject(&CLSID_Fake,(IUnknown*)&cfac,CLSCTX_INPROC_SERVER,REGCLS_MULTIPLEUSE,&reg); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_Register(ipp,&CLSID_Fake); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_AddLanguageProfile(ipp,&CLSID_Fake,lang,&CLSID_Fake,L"Fake",4,L"Fake",4,1); CHECK(hr==S_OK);
        ITfInputProcessorProfiles_EnableLanguageProfile(ipp,&CLSID_Fake,lang,&CLSID_Fake,TRUE);
        hr=ITfInputProcessorProfiles_ActivateLanguageProfile(ipp,&CLSID_Fake,lang,&CLSID_Fake); CHECK(hr==S_OK);
        CHECK(tip_act>=1 && tip_tid);
        tid=tip_tid;
    }
    {
        ITfKeystrokeMgr *km; KSink ks={{&kvt}}; BOOL eaten=TRUE; TF_PRESERVEDKEY pk={'K',TF_MOD_CONTROL}; GUID g; BSTR b;
        static const WCHAR desc[]=L"My key";
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfKeystrokeMgr,(void**)&km); CHECK(hr==S_OK);
        hr=ITfKeystrokeMgr_AdviseKeyEventSink(km,tid,&ks.i,TRUE);
        printf("advise hr=%lx\n",hr);
        if(hr==S_OK){
            ks.eat=TRUE;
            hr=ITfKeystrokeMgr_TestKeyDown(km,'A',0,&eaten); CHECK(hr==S_OK && eaten && ks.tdown==1 && ks.last=='A');
            ks.eat=FALSE;
            hr=ITfKeystrokeMgr_KeyDown(km,'A',0,&eaten); CHECK(hr==S_OK && !eaten && ks.down==1);
            hr=ITfKeystrokeMgr_TestKeyUp(km,'A',0,&eaten); CHECK(hr==S_OK && !eaten && ks.tup==1);
            hr=ITfKeystrokeMgr_KeyUp(km,'A',0,&eaten); CHECK(hr==S_OK && ks.up==1);
            hr=ITfKeystrokeMgr_TestKeyDown(km,'A',0,NULL); CHECK(hr==E_INVALIDARG);
            hr=ITfKeystrokeMgr_PreserveKey(km,tid,&pkguid,&pk,desc,6); CHECK(hr==S_OK);
            hr=ITfKeystrokeMgr_GetPreservedKey(km,NULL,&pk,&g); CHECK(hr==S_OK && IsEqualGUID(&g,&pkguid));
            pk.uVKey='L'; hr=ITfKeystrokeMgr_GetPreservedKey(km,NULL,&pk,&g); CHECK(hr==S_FALSE); pk.uVKey='K';
            hr=ITfKeystrokeMgr_GetPreservedKeyDescription(km,&pkguid,&b); CHECK(hr==S_OK && b && !lstrcmpW(b,desc)); SysFreeString(b);
            hr=ITfKeystrokeMgr_SetPreservedKeyDescription(km,&pkguid,L"Other",5); CHECK(hr==S_OK);
            hr=ITfKeystrokeMgr_GetPreservedKeyDescription(km,&pkguid,&b); CHECK(hr==S_OK && !lstrcmpW(b,L"Other")); SysFreeString(b);
            hr=ITfKeystrokeMgr_GetPreservedKeyDescription(km,&g,&b); CHECK(hr==CONNECT_E_NOCONNECTION);
            hr=ITfKeystrokeMgr_SetPreservedKeyDescription(km,&g,L"x",1); CHECK(hr==CONNECT_E_NOCONNECTION);
            {
                TF_PRESERVEDKEY pm={'M',TF_MOD_IGNORE_ALL_MODIFIER}; GUID mg={0x22222222,3,4,{5,6,7,8,9,10,11,12}}; int td=ks.tdown, dn=ks.down;
                hr=ITfKeystrokeMgr_PreserveKey(km,tid,&mg,&pm,L"M",1); CHECK(hr==S_OK);
                eaten=FALSE; ks.eat=FALSE;
                hr=ITfKeystrokeMgr_TestKeyDown(km,'M',0,&eaten); CHECK(hr==S_OK && eaten && ks.tdown==td);
                eaten=FALSE;
                hr=ITfKeystrokeMgr_KeyDown(km,'M',0,&eaten); CHECK(hr==S_OK && eaten && ks.pres==1 && IsEqualGUID(&ks.pg,&mg) && ks.down==dn);
                eaten=TRUE;
                hr=ITfKeystrokeMgr_KeyUp(km,'M',0,&eaten); CHECK(hr==S_OK && !eaten && ks.pres==1);
                hr=ITfKeystrokeMgr_UnpreserveKey(km,&mg,&pm); CHECK(hr==S_OK);
                eaten=FALSE; hr=ITfKeystrokeMgr_TestKeyDown(km,'M',0,&eaten); CHECK(hr==S_OK && !eaten && ks.tdown==td+1);
            }
            {
                BYTE ks_[256]; GetKeyboardState(ks_);
                ITfKeystrokeMgr_SimulatePreservedKey(km,NULL,&pkguid,&eaten);
            }
            hr=ITfKeystrokeMgr_UnpreserveKey(km,&pkguid,&pk); CHECK(hr==S_OK);
        }
        ITfKeystrokeMgr_UnadviseKeyEventSink(km,tid);
        ITfKeystrokeMgr_Release(km);
        ITfInputProcessorProfiles_Unregister(ipp,&CLSID_Fake);
    }
    /* language bar */
    {
        ITfLangBarMgr *lbm; ITfLangBarItemMgr *im,*im2; ITfInputProcessorProfiles *pp; DWORD cookie,t,st,n; ULONG num;
        LItem a={{&livt},{1,2,3,{4,5,6,7,8,9,10,11}},0}, b={{&livt},{9,2,3,{4,5,6,7,8,9,10,12}},2};
        LEv ev={{&levt}}; ITfLangBarItem *got,*arr[4]; IEnumTfLangBarItems *en; TF_LANGBARITEMINFO info[4]; DWORD sts[4]; GUID gs[2];
        hr=CoCreateInstance(&CLSID_TF_LangBarMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfLangBarMgr,(void**)&lbm); CHECK(hr==S_OK);
        hr=ITfLangBarMgr_GetShowFloatingStatus(lbm,&st); CHECK(hr==S_OK && st==8);
        hr=ITfLangBarMgr_ShowFloating(lbm,1); CHECK(hr==S_OK);
        hr=ITfLangBarMgr_GetShowFloatingStatus(lbm,&st); CHECK(hr==S_OK && st==1);
        hr=ITfLangBarMgr_AdviseEventSink(lbm,&ev.i,NULL,0,&cookie); CHECK(hr==S_OK && cookie);
        hr=ITfLangBarMgr_UnAdviseEventSink(lbm,cookie); CHECK(hr==S_OK);
        hr=ITfLangBarMgr_UnAdviseEventSink(lbm,cookie); CHECK(hr==CONNECT_E_NOCONNECTION);
        hr=ITfLangBarMgr_AdviseEventSink(lbm,NULL,NULL,0,&cookie); CHECK(hr==E_INVALIDARG);
        hr=ITfLangBarMgr_SetModalInput(lbm,&ev.i,GetCurrentThreadId(),0); CHECK(hr==S_OK);
        hr=ITfLangBarMgr_SetModalInput(lbm,NULL,GetCurrentThreadId(),0); CHECK(hr==S_OK);
        hr=ITfLangBarMgr_GetThreadLangBarItemMgr(lbm,0,&im,&t); CHECK(hr==S_OK && t==GetCurrentThreadId());
        hr=ITfLangBarMgr_GetInputProcessorProfiles(lbm,0,&pp,&t); CHECK(hr==S_OK && pp);
        if(pp) ITfInputProcessorProfiles_Release(pp);
        hr=((HRESULT(WINAPI*)(ITfLangBarItemMgr**))GetProcAddress(LoadLibraryA("msctf.dll"),"TF_CreateLangBarItemMgr"))(&im2); CHECK(hr==S_OK && im2==im);
        hr=ITfLangBarItemMgr_GetItemNum(im,&num); CHECK(hr==S_OK && num==0);
        hr=ITfLangBarItemMgr_AddItem(im,&a.i); CHECK(hr==S_OK);
        hr=ITfLangBarItemMgr_AddItem(im,&b.i); CHECK(hr==S_OK);
        hr=ITfLangBarItemMgr_AddItem(im,&a.i); CHECK(hr==E_INVALIDARG);
        hr=ITfLangBarItemMgr_GetItemNum(im,&num); CHECK(hr==S_OK && num==2);
        hr=ITfLangBarItemMgr_GetItem(im,&b.g,&got); CHECK(hr==S_OK && got==&b.i);
        hr=ITfLangBarItemMgr_GetItem(im,&pkguid,&got); CHECK(hr==E_FAIL && !got);
        gs[0]=b.g; gs[1]=a.g;
        hr=ITfLangBarItemMgr_GetItemsStatus(im,2,gs,sts); CHECK(hr==S_OK && sts[0]==2 && sts[1]==0);
        hr=ITfLangBarItemMgr_GetItems(im,4,arr,info,sts,&n); CHECK(hr==S_FALSE && n==2 && arr[0]==&a.i && IsEqualGUID(&info[1].guidItem,&b.g) && sts[1]==2);
        hr=ITfLangBarItemMgr_EnumItems(im,&en); CHECK(hr==S_OK);
        hr=IEnumTfLangBarItems_Next(en,1,arr,NULL); CHECK(hr==S_OK && arr[0]==&a.i);
        hr=IEnumTfLangBarItems_Skip(en,1); CHECK(hr==S_OK);
        hr=IEnumTfLangBarItems_Next(en,1,arr,&n); CHECK(hr==S_FALSE && n==0);
        IEnumTfLangBarItems_Release(en);
        hr=ITfLangBarItemMgr_RemoveItem(im,&a.i); CHECK(hr==S_OK);
        hr=ITfLangBarItemMgr_RemoveItem(im,&a.i); CHECK(hr==E_FAIL);
        hr=ITfLangBarItemMgr_GetItemNum(im,&num); CHECK(hr==S_OK && num==1);
        ITfLangBarItemMgr_RemoveItem(im,&b.i);
        ITfLangBarItemMgr_Release(im); ITfLangBarItemMgr_Release(im2);
        ITfLangBarMgr_Release(lbm);
    }
    ITfThreadMgrEx_Deactivate(tmx);
    ITfThreadMgrEx_Release(tmx);
    printf("RESULT: %s\n",fails?"FAIL":"PASS");
    return fails!=0;
}
