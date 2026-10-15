#define COBJMACROS
#include <windows.h>
#include <msctf.h>
#include <textstor.h>
#include <olectl.h>
#include <stdio.h>
#include <initguid.h>

static int fails;
#define CHECK(c) do{ if(c) printf("PASS %s\n",#c); else {printf("FAIL line %d %s\n",__LINE__,#c); fails++;} }while(0)

/* interfaces new to msctf in this batch (identifiers from windows-rs 0.62.2, MIT) */
DEFINE_GUID(IID_ITfDisplayAttributeProvider_,0xfee47777,0x163c,0x4769,0x99,0x6a,0x6e,0x9c,0x50,0xad,0x8f,0x54);
DEFINE_GUID(IID_ITfDisplayAttributeNotifySink_,0xad56f402,0xe162,0x4f25,0x90,0x8f,0x7d,0x57,0x7c,0xf9,0xbd,0xa9);
DEFINE_GUID(IID_ITfPreservedKeyNotifySink_,0x6f77c993,0xd2b1,0x446e,0x85,0x3e,0x59,0x12,0xef,0xc8,0xa2,0x86);
DEFINE_GUID(IID_ITfCleanupContextDurationSink_,0x45c35144,0x154e,0x4797,0xbe,0xd8,0xd3,0x3a,0xe7,0xbf,0x87,0x94);
DEFINE_GUID(GUID_PROP_COMPOSING_,0xe12ac060,0xaf15,0x11d2,0xaf,0xc5,0x00,0x10,0x5a,0x27,0x99,0xb5);
DEFINE_GUID(GUID_TFCAT_DAP,0x046b8c80,0x1647,0x40f7,0x9b,0x21,0xb9,0x3b,0x81,0xaa,0xbc,0x1b);
DEFINE_GUID(CLSID_DAP,0x7a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0x01);
DEFINE_GUID(GUID_AttrA,0x7a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0x02);
DEFINE_GUID(GUID_AttrB,0x7a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0x03);
DEFINE_GUID(GUID_AttrX,0x7a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0x04);
DEFINE_GUID(GUID_PKey,0x7a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0x05);

typedef struct { void **vtbl; } Obj;

/* ITfDisplayAttributeInfo */
typedef struct { ITfDisplayAttributeInfo i; GUID g; } Info;
static HRESULT WINAPI in_qi(ITfDisplayAttributeInfo*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfDisplayAttributeInfo)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI in_ar(ITfDisplayAttributeInfo*f){return 2;}
static ULONG WINAPI in_rl(ITfDisplayAttributeInfo*f){return 1;}
static HRESULT WINAPI in_gg(ITfDisplayAttributeInfo*f,GUID*g){*g=((Info*)f)->g;return S_OK;}
static HRESULT WINAPI in_gd(ITfDisplayAttributeInfo*f,BSTR*b){*b=SysAllocString(L"attr");return S_OK;}
static HRESULT WINAPI in_ga(ITfDisplayAttributeInfo*f,TF_DISPLAYATTRIBUTE*d){memset(d,0,sizeof(*d));return S_OK;}
static HRESULT WINAPI in_sa(ITfDisplayAttributeInfo*f,const TF_DISPLAYATTRIBUTE*d){return S_OK;}
static HRESULT WINAPI in_rs(ITfDisplayAttributeInfo*f){return S_OK;}
static ITfDisplayAttributeInfoVtbl invt={in_qi,in_ar,in_rl,in_gg,in_gd,in_ga,in_sa,in_rs};
static Info infoA={{&invt}}, infoB={{&invt}};

/* IEnumTfDisplayAttributeInfo over the two */
typedef struct { IEnumTfDisplayAttributeInfo i; ULONG pos; } PEnum;
static HRESULT WINAPI pe_qi(IEnumTfDisplayAttributeInfo*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IEnumTfDisplayAttributeInfo)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI pe_ar(IEnumTfDisplayAttributeInfo*f){return 2;}
static ULONG WINAPI pe_rl(IEnumTfDisplayAttributeInfo*f){return 1;}
static HRESULT WINAPI pe_cl(IEnumTfDisplayAttributeInfo*f,IEnumTfDisplayAttributeInfo**o){return E_NOTIMPL;}
static HRESULT WINAPI pe_nx(IEnumTfDisplayAttributeInfo*f,ULONG c,ITfDisplayAttributeInfo**r,ULONG*n)
{
    PEnum*p=(PEnum*)f; ULONG k=0;
    while(k<c && p->pos<2){ r[k++]=p->pos==0?&infoA.i:&infoB.i; p->pos++; }
    if(n)*n=k; return k==c?S_OK:S_FALSE;
}
static HRESULT WINAPI pe_rs(IEnumTfDisplayAttributeInfo*f){((PEnum*)f)->pos=0;return S_OK;}
static HRESULT WINAPI pe_sk(IEnumTfDisplayAttributeInfo*f,ULONG c){return S_OK;}
static IEnumTfDisplayAttributeInfoVtbl pevt={pe_qi,pe_ar,pe_rl,pe_cl,pe_nx,pe_rs,pe_sk};
static PEnum penum={{&pevt},0};

/* ITfDisplayAttributeProvider (vtable by hand: not in mingw's headers) */
typedef struct DAPVtbl {
    HRESULT (WINAPI *QueryInterface)(void*,REFIID,void**);
    ULONG (WINAPI *AddRef)(void*);
    ULONG (WINAPI *Release)(void*);
    HRESULT (WINAPI *EnumDisplayAttributeInfo)(void*,IEnumTfDisplayAttributeInfo**);
    HRESULT (WINAPI *GetDisplayAttributeInfo)(void*,REFGUID,ITfDisplayAttributeInfo**);
} DAPVtbl;
typedef struct { const DAPVtbl *vtbl; } DAP;
static HRESULT WINAPI dp_qi(void*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfDisplayAttributeProvider_)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI dp_ar(void*f){return 2;}
static ULONG WINAPI dp_rl(void*f){return 1;}
static HRESULT WINAPI dp_en(void*f,IEnumTfDisplayAttributeInfo**o){penum.pos=0;*o=&penum.i;return S_OK;}
static HRESULT WINAPI dp_gi(void*f,REFGUID g,ITfDisplayAttributeInfo**o)
{
    if(IsEqualGUID(g,&GUID_AttrA)){*o=&infoA.i;return S_OK;}
    if(IsEqualGUID(g,&GUID_AttrB)){*o=&infoB.i;return S_OK;}
    *o=NULL; return E_INVALIDARG;
}
static const DAPVtbl dpvt={dp_qi,dp_ar,dp_rl,dp_en,dp_gi};
static DAP dap={&dpvt};
static HRESULT WINAPI cf_qi(IClassFactory*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_IClassFactory)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI cf_ar(IClassFactory*f){return 2;}
static ULONG WINAPI cf_rl(IClassFactory*f){return 1;}
static HRESULT WINAPI cf_ci(IClassFactory*f,IUnknown*o,REFIID r,void**ppv){ if(o)return CLASS_E_NOAGGREGATION; return dp_qi(&dap,r,ppv);}
static HRESULT WINAPI cf_ls(IClassFactory*f,BOOL l){return S_OK;}
static IClassFactoryVtbl cfvt={cf_qi,cf_ar,cf_rl,cf_ci,cf_ls};
static IClassFactory cfac={&cfvt};

/* sinks: IUnknown + one method (+ one more for cleanup) */
typedef struct { void **vtbl; int count; int count2; } Sink;
static HRESULT WINAPI sk_qi(void*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfDisplayAttributeNotifySink_)||IsEqualIID(r,&IID_ITfPreservedKeyNotifySink_)||IsEqualIID(r,&IID_ITfCleanupContextDurationSink_)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI sk_ar(void*f){return 2;}
static ULONG WINAPI sk_rl(void*f){return 1;}
static HRESULT WINAPI sk_m1(void*f){((Sink*)f)->count++;return S_OK;}
static HRESULT WINAPI sk_m2(void*f){((Sink*)f)->count2++;return S_OK;}
static HRESULT WINAPI sk_pk(void*f,const TF_PRESERVEDKEY*k){((Sink*)f)->count++;return S_OK;}
static void *vt_da[4]={sk_qi,sk_ar,sk_rl,sk_m1};
static void *vt_pk[4]={sk_qi,sk_ar,sk_rl,sk_pk};
static void *vt_cl[5]={sk_qi,sk_ar,sk_rl,sk_m1,sk_m2};

typedef HRESULT (*es_fn)(TfEditCookie ec, void *arg);
typedef struct { ITfEditSession i; es_fn fn; void *arg; } ES;
static HRESULT WINAPI es_qi(ITfEditSession*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfEditSession)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI es_ar(ITfEditSession*f){return 2;}
static ULONG WINAPI es_rl(ITfEditSession*f){return 1;}
static HRESULT WINAPI es_do(ITfEditSession*f,TfEditCookie ec){ES*e=(ES*)f;return e->fn(ec,e->arg);}
static ITfEditSessionVtbl esvt={es_qi,es_ar,es_rl,es_do};
static HRESULT run_session(ITfContext *ctx, TfClientId tid, es_fn fn, void *arg)
{
    ES e={{&esvt},fn,arg}; HRESULT hrs=E_FAIL, hr;
    hr=ITfContext_RequestEditSession(ctx,tid,&e.i,TF_ES_SYNC|TF_ES_READWRITE,&hrs);
    return FAILED(hr)?hr:hrs;
}

static ITfContext *g_ctx;
static HRESULT sess_comp(TfEditCookie ec, void *arg)
{
    ITfInsertAtSelection *ias; ITfRange *r; ITfContextComposition *cc; ITfComposition *comp=NULL; ITfProperty *prop; VARIANT v; HRESULT hr;
    ITfRange *r2; ITfRangeACP *w;
    ITfContext_QueryInterface(g_ctx,&IID_ITfInsertAtSelection,(void**)&ias);
    ITfInsertAtSelection_InsertTextAtSelection(ias,ec,0,L"abcdefgh",8,&r); ITfInsertAtSelection_Release(ias);
    ITfRange_QueryInterface(r,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,2,3); ITfRangeACP_Release(w);
    ITfContext_QueryInterface(g_ctx,&IID_ITfContextComposition,(void**)&cc);
    hr=ITfContextComposition_StartComposition(cc,ec,r,NULL,&comp); CHECK(hr==S_OK && comp);
    hr=ITfContext_GetProperty(g_ctx,&GUID_PROP_COMPOSING_,&prop); CHECK(hr==S_OK);
    VariantInit(&v); hr=ITfProperty_GetValue(prop,ec,r,&v); CHECK(hr==S_OK && V_VT(&v)==VT_I4 && V_I4(&v)!=0);
    /* outside the composition: empty */
    ITfContext_GetStart(g_ctx,ec,&r2); ITfRange_QueryInterface(r2,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,6,2); ITfRangeACP_Release(w);
    VariantInit(&v); hr=ITfProperty_GetValue(prop,ec,r2,&v); CHECK(hr==S_OK && V_VT(&v)==VT_EMPTY);
    /* grow the composition to the end */
    hr=ITfComposition_ShiftEnd(comp,ec,r2); CHECK(hr==S_OK);
    VariantInit(&v); hr=ITfProperty_GetValue(prop,ec,r2,&v); CHECK(hr==S_OK && V_VT(&v)==VT_I4);
    hr=ITfComposition_EndComposition(comp,ec); CHECK(hr==S_OK);
    VariantInit(&v); hr=ITfProperty_GetValue(prop,ec,r,&v); CHECK(hr==S_OK && V_VT(&v)==VT_EMPTY);
    ITfComposition_Release(comp); ITfProperty_Release(prop); ITfRange_Release(r); ITfRange_Release(r2); ITfContextComposition_Release(cc);
    return S_OK;
}

int main(void)
{
    ITfThreadMgrEx *tmx; ITfThreadMgr *tm; TfClientId tid; HRESULT hr; ITfInputProcessorProfiles *ipp; ITfCategoryMgr *cm; DWORD reg;
    ITfDisplayAttributeMgr *dam; ITfSource *src; DWORD ck1,ck2; Sink dsink={vt_da,0,0}, ksink={vt_pk,0,0}, csink={vt_cl,0,0};
    ITfDocumentMgr *dm; ITfContext *ctx; TfEditCookie ecs;
    CoInitialize(NULL);
    infoA.g=GUID_AttrA; infoB.g=GUID_AttrB;
    hr=CoCreateInstance(&CLSID_TF_ThreadMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfThreadMgrEx,(void**)&tmx); CHECK(hr==S_OK);
    if(FAILED(hr)){printf("RESULT: FAIL\n");return 1;}
    tm=(ITfThreadMgr*)tmx;
    hr=ITfThreadMgrEx_ActivateEx(tmx,&tid,TF_TMAE_NOACTIVATETIP); CHECK(hr==S_OK);
    hr=CoCreateInstance(&CLSID_TF_InputProcessorProfiles,NULL,CLSCTX_INPROC_SERVER,&IID_ITfInputProcessorProfiles,(void**)&ipp); CHECK(hr==S_OK);
    hr=CoCreateInstance(&CLSID_TF_CategoryMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfCategoryMgr,(void**)&cm); CHECK(hr==S_OK);
    hr=CoCreateInstance(&CLSID_TF_DisplayAttributeMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfDisplayAttributeMgr,(void**)&dam); CHECK(hr==S_OK);

    /* no provider registered yet */
    {
        IEnumTfDisplayAttributeInfo *en; ITfDisplayAttributeInfo *info; ULONG n=9;
        hr=ITfDisplayAttributeMgr_EnumDisplayAttributeInfo(dam,&en); CHECK(hr==S_OK);
        hr=IEnumTfDisplayAttributeInfo_Next(en,1,&info,&n); CHECK(hr==S_FALSE && n==0); IEnumTfDisplayAttributeInfo_Release(en);
    }
    /* register a provider: a class in the display attribute provider category */
    hr=CoRegisterClassObject(&CLSID_DAP,(IUnknown*)&cfac,CLSCTX_INPROC_SERVER,REGCLS_MULTIPLEUSE,&reg); CHECK(hr==S_OK);
    ITfInputProcessorProfiles_Register(ipp,&CLSID_DAP);
    hr=ITfCategoryMgr_RegisterCategory(cm,&CLSID_DAP,&GUID_TFCAT_DAP,&CLSID_DAP); CHECK(hr==S_OK);
    {
        IEnumTfDisplayAttributeInfo *en; ITfDisplayAttributeInfo *info[4]; ULONG n=0; GUID g; CLSID owner; ITfDisplayAttributeInfo *one=NULL;
        hr=ITfDisplayAttributeMgr_EnumDisplayAttributeInfo(dam,&en); CHECK(hr==S_OK);
        hr=IEnumTfDisplayAttributeInfo_Next(en,4,info,&n); CHECK(n==2 && hr==S_FALSE);
        if(n==2){ ITfDisplayAttributeInfo_GetGUID(info[0],&g); CHECK(IsEqualGUID(&g,&GUID_AttrA)); ITfDisplayAttributeInfo_GetGUID(info[1],&g); CHECK(IsEqualGUID(&g,&GUID_AttrB)); }
        IEnumTfDisplayAttributeInfo_Release(en);
        hr=ITfDisplayAttributeMgr_GetDisplayAttributeInfo(dam,&GUID_AttrB,&one,&owner); CHECK(hr==S_OK && one==&infoB.i && IsEqualGUID(&owner,&CLSID_DAP));
        hr=ITfDisplayAttributeMgr_GetDisplayAttributeInfo(dam,&GUID_AttrX,&one,&owner); CHECK(hr==E_FAIL && !one);
        hr=ITfDisplayAttributeMgr_GetDisplayAttributeInfo(dam,&GUID_AttrA,&one,NULL); CHECK(hr==S_OK);
    }
    /* update notification */
    hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfSource,(void**)&src); CHECK(hr==S_OK);
    hr=ITfSource_AdviseSink(src,&IID_ITfDisplayAttributeNotifySink_,(IUnknown*)&dsink,&ck1); CHECK(hr==S_OK);
    hr=ITfDisplayAttributeMgr_OnUpdateInfo(dam); CHECK(hr==S_OK && dsink.count==1);
    hr=ITfSource_UnadviseSink(src,ck1); CHECK(hr==S_OK);
    hr=ITfDisplayAttributeMgr_OnUpdateInfo(dam); CHECK(hr==S_OK && dsink.count==1);
    /* preserved key notifications */
    {
        ITfKeystrokeMgr *km; TF_PRESERVEDKEY pk={'Q',TF_MOD_CONTROL};
        hr=ITfSource_AdviseSink(src,&IID_ITfPreservedKeyNotifySink_,(IUnknown*)&ksink,&ck2); CHECK(hr==S_OK);
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfKeystrokeMgr,(void**)&km); CHECK(hr==S_OK);
        hr=ITfKeystrokeMgr_PreserveKey(km,tid,&GUID_PKey,&pk,L"k",1); CHECK(hr==S_OK && ksink.count==1);
        hr=ITfKeystrokeMgr_SetPreservedKeyDescription(km,&GUID_PKey,L"new",3); CHECK(hr==S_OK && ksink.count==2);
        hr=ITfKeystrokeMgr_UnpreserveKey(km,&GUID_PKey,&pk); CHECK(hr==S_OK && ksink.count==3);
        ITfSource_UnadviseSink(src,ck2);
        hr=ITfKeystrokeMgr_PreserveKey(km,tid,&GUID_PKey,&pk,L"k",1); CHECK(hr==S_OK && ksink.count==3);
        ITfKeystrokeMgr_UnpreserveKey(km,&GUID_PKey,&pk);
        ITfKeystrokeMgr_Release(km);
    }
    ITfSource_Release(src);

    /* composing property and cleanup notifications */
    hr=ITfThreadMgr_CreateDocumentMgr(tm,&dm); CHECK(hr==S_OK);
    hr=ITfDocumentMgr_CreateContext(dm,tid,0,NULL,&ctx,&ecs); CHECK(hr==S_OK);
    hr=ITfDocumentMgr_Push(dm,ctx); CHECK(hr==S_OK);
    g_ctx=ctx;
    hr=run_session(ctx,tid,sess_comp,NULL); CHECK(hr==S_OK);
    {
        ITfSourceSingle *ss;
        hr=ITfContext_QueryInterface(ctx,&IID_ITfSourceSingle,(void**)&ss); CHECK(hr==S_OK);
        hr=ITfSourceSingle_AdviseSingleSink(ss,tid,&IID_ITfCleanupContextDurationSink_,(IUnknown*)&csink); CHECK(hr==S_OK);
        hr=ITfSourceSingle_AdviseSingleSink(ss,tid,&IID_ITfCleanupContextDurationSink_,(IUnknown*)&csink); CHECK(hr==CONNECT_E_ADVISELIMIT);
        ITfSourceSingle_Release(ss);
    }
    CHECK(csink.count==0 && csink.count2==0);
    hr=ITfThreadMgrEx_Deactivate(tmx); CHECK(hr==S_OK);
    CHECK(csink.count==1 && csink.count2==1);

    ITfContext_Release(ctx); ITfDocumentMgr_Release(dm);
    ITfDisplayAttributeMgr_Release(dam);
    printf("RESULT: %s\n",fails?"FAIL":"PASS");
    return fails!=0;
}
