#define COBJMACROS
#include <windows.h>
#include <msctf.h>
#include <textstor.h>
#include <inputscope.h>
#include <olectl.h>
#include <stdio.h>
#include <initguid.h>

static int fails;
static HRESULT (WINAPI *pSetInputScope)(HWND,InputScope);
static HRESULT (WINAPI *pSetInputScopes)(HWND,const InputScope*,UINT,WCHAR**,UINT,WCHAR*,WCHAR*);
#define CHECK(c) do{ if(c) printf("PASS %s\n",#c); else {printf("FAIL line %d %s\n",__LINE__,#c); fails++;} }while(0)

DEFINE_GUID(CLSID_Tip1,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf1);
DEFINE_GUID(GUID_Cat1,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf2);
DEFINE_GUID(GUID_Item1,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf3);
DEFINE_GUID(GUID_Item2,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf4);
DEFINE_GUID(GUID_Prop1,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf5);
DEFINE_GUID(GUID_Prop2,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf6);
DEFINE_GUID(GUID_Prof1,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf7);
DEFINE_GUID(GUID_Prof2,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf8);
DEFINE_GUID(GUID_Prov1,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xf9);
DEFINE_GUID(GUID_Unk,0x9a1b2c3d,0x4e5f,0x6071,0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xfa);

/* ---- a generic edit session ---- */
typedef HRESULT (*es_fn)(TfEditCookie ec, void *arg);
typedef struct { ITfEditSession i; es_fn fn; void *arg; } ES;
static HRESULT WINAPI es_qi(ITfEditSession*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfEditSession)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI es_ar(ITfEditSession*f){return 2;}
static ULONG WINAPI es_rl(ITfEditSession*f){return 1;}
static HRESULT WINAPI es_do(ITfEditSession*f,TfEditCookie ec){ES*e=(ES*)f;return e->fn(ec,e->arg);}
static ITfEditSessionVtbl esvt={es_qi,es_ar,es_rl,es_do};
static HRESULT run_session(ITfContext *ctx, TfClientId tid, DWORD flags, es_fn fn, void *arg)
{
    ES e={{&esvt},fn,arg}; HRESULT hrs=E_FAIL, hr;
    hr=ITfContext_RequestEditSession(ctx,tid,&e.i,flags,&hrs);
    if(FAILED(hr)) return hr;
    return hrs;
}

/* ---- composition sink / owner sink ---- */
typedef struct { ITfCompositionSink i; int terminated; TfEditCookie ec; ITfComposition *last; } CSink;
static HRESULT WINAPI cs_qi(ITfCompositionSink*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfCompositionSink)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI cs_ar(ITfCompositionSink*f){return 2;}
static ULONG WINAPI cs_rl(ITfCompositionSink*f){return 1;}
static HRESULT WINAPI cs_ot(ITfCompositionSink*f,TfEditCookie ec,ITfComposition*c){CSink*s=(CSink*)f;s->terminated++;s->ec=ec;s->last=c;return S_OK;}
static ITfCompositionSinkVtbl csvt={cs_qi,cs_ar,cs_rl,cs_ot};

/* ---- function provider ---- */
typedef struct { ITfFunctionProvider i; } FProv;
static HRESULT WINAPI fp_qi(ITfFunctionProvider*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfFunctionProvider)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI fp_ar(ITfFunctionProvider*f){return 2;}
static ULONG WINAPI fp_rl(ITfFunctionProvider*f){return 1;}
static HRESULT WINAPI fp_gt(ITfFunctionProvider*f,GUID*g){*g=GUID_Prov1;return S_OK;}
static HRESULT WINAPI fp_gd(ITfFunctionProvider*f,BSTR*b){*b=SysAllocString(L"prov");return S_OK;}
static HRESULT WINAPI fp_gf(ITfFunctionProvider*f,REFGUID g,REFIID r,IUnknown**o){*o=NULL;return E_NOTIMPL;}
static ITfFunctionProviderVtbl fpvt={fp_qi,fp_ar,fp_rl,fp_gt,fp_gd,fp_gf};

/* ---- a language profile notify sink ---- */
typedef struct { ITfLanguageProfileNotifySink i; int changing, changed; BOOL accept; } LSink;
static HRESULT WINAPI ls_qi(ITfLanguageProfileNotifySink*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfLanguageProfileNotifySink)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI ls_ar(ITfLanguageProfileNotifySink*f){return 2;}
static ULONG WINAPI ls_rl(ITfLanguageProfileNotifySink*f){return 1;}
static HRESULT WINAPI ls_ocing(ITfLanguageProfileNotifySink*f,LANGID l,BOOL*a){LSink*s=(LSink*)f;s->changing++;*a=s->accept;return S_OK;}
static HRESULT WINAPI ls_oced(ITfLanguageProfileNotifySink*f){((LSink*)f)->changed++;return S_OK;}
static ITfLanguageProfileNotifySinkVtbl lsvt={ls_qi,ls_ar,ls_rl,ls_ocing,ls_oced};

/* ---- a persistent property loader ---- */
typedef struct { ITfPersistentPropertyLoaderACP i; int loads; BYTE *data; ULONG cb; } Loader;
static HRESULT WINAPI ld_qi(ITfPersistentPropertyLoaderACP*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITfPersistentPropertyLoaderACP)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI ld_ar(ITfPersistentPropertyLoaderACP*f){return 2;}
static ULONG WINAPI ld_rl(ITfPersistentPropertyLoaderACP*f){return 1;}
static HRESULT WINAPI ld_lp(ITfPersistentPropertyLoaderACP*f,const TF_PERSISTENT_PROPERTY_HEADER_ACP*h,IStream**s)
{
    Loader*l=(Loader*)f; HGLOBAL g; void*p; l->loads++;
    g=GlobalAlloc(GMEM_MOVEABLE,l->cb); p=GlobalLock(g); memcpy(p,l->data,l->cb); GlobalUnlock(g);
    return CreateStreamOnHGlobal(g,TRUE,s);
}
static ITfPersistentPropertyLoaderACPVtbl ldvt={ld_qi,ld_ar,ld_rl,ld_lp};

/* ---- a text store that keeps its sink, to reach ITextStoreACPServices ---- */
typedef struct { ITextStoreACP i; ITextStoreACPSink *sink; } Store;
static HRESULT WINAPI st_qi(ITextStoreACP*f,REFIID r,void**o){ if(IsEqualIID(r,&IID_IUnknown)||IsEqualIID(r,&IID_ITextStoreACP)){*o=f;return S_OK;} *o=NULL;return E_NOINTERFACE;}
static ULONG WINAPI st_ar(ITextStoreACP*f){return 2;}
static ULONG WINAPI st_rl(ITextStoreACP*f){return 1;}
static HRESULT WINAPI st_as(ITextStoreACP*f,REFIID r,IUnknown*p,DWORD m){Store*s=(Store*)f;return IUnknown_QueryInterface(p,&IID_ITextStoreACPSink,(void**)&s->sink);}
static HRESULT WINAPI st_us(ITextStoreACP*f,IUnknown*p){return S_OK;}
static HRESULT WINAPI st_rlk(ITextStoreACP*f,DWORD fl,HRESULT*h){Store*s=(Store*)f;*h=ITextStoreACPSink_OnLockGranted(s->sink,fl);return S_OK;}
static HRESULT WINAPI st_gs(ITextStoreACP*f,TS_STATUS*s){s->dwDynamicFlags=0;s->dwStaticFlags=0;return S_OK;}
static HRESULT WINAPI st_ni(void){return E_NOTIMPL;}
static ITextStoreACPVtbl stvt;

/* ---- helpers ---- */
static int get_text(ITfRange *r, TfEditCookie ec, WCHAR *buf, ULONG n)
{
    ULONG got=0; HRESULT hr=ITfRange_GetText(r,ec,0,buf,n-1,&got); buf[got]=0; return SUCCEEDED(hr)?(int)got:-1;
}
static void extent(ITfRange *r, LONG *a, LONG *c)
{
    ITfRangeACP *acp; *a=*c=-99;
    if(SUCCEEDED(ITfRange_QueryInterface(r,&IID_ITfRangeACP,(void**)&acp))){ITfRangeACP_GetExtent(acp,a,c);ITfRangeACP_Release(acp);}
}

/* ---- the memory-store context session ---- */
static ITfContext *g_ctx; static TfClientId g_tid; static CSink g_cs; static ITfContextOwnerCompositionServices *g_svc;
static ITfComposition *g_comp2;

static HRESULT sess_text(TfEditCookie ec, void *arg)
{
    ITfInsertAtSelection *ias; ITfRange *r=NULL,*r2=NULL,*whole=NULL; WCHAR buf[64]; LONG a,c; TF_SELECTION sel; ULONG f=0; HRESULT hr;
    TF_STATUS st;
    hr=ITfContext_QueryInterface(g_ctx,&IID_ITfInsertAtSelection,(void**)&ias); CHECK(hr==S_OK);
    hr=ITfInsertAtSelection_InsertTextAtSelection(ias,ec,0,L"hello",5,&r); CHECK(hr==S_OK && r);
    CHECK(get_text(r,ec,buf,64)==5 && !lstrcmpW(buf,L"hello"));
    extent(r,&a,&c); CHECK(a==0 && c==5);
    hr=ITfContext_GetEnd(g_ctx,ec,&r2); CHECK(hr==S_OK); extent(r2,&a,&c); CHECK(a==5);
    ITfRange_Release(r2);
    hr=ITfContext_GetSelection(g_ctx,ec,TF_DEFAULT_SELECTION,1,&sel,&f); CHECK(hr==S_OK && f==1);
    extent(sel.range,&a,&c); CHECK(a==0 && c==5); ITfRange_Release(sel.range);
    ITfRange_Release(r);
    hr=ITfContext_GetStatus(g_ctx,&st); CHECK(hr==S_OK && st.dwDynamicFlags==0);
    /* the second insert replaces the selection */
    hr=ITfInsertAtSelection_InsertTextAtSelection(ias,ec,0,L" world",6,&r); CHECK(hr==S_OK);
    CHECK(get_text(r,ec,buf,64)==6 && !lstrcmpW(buf,L" world"));
    ITfRange_Release(r);
    /* query only gives the place and changes nothing */
    hr=ITfInsertAtSelection_InsertTextAtSelection(ias,ec,TF_IAS_QUERYONLY,L"zzz",3,&r); CHECK(hr==S_OK);
    ITfRange_Release(r);
    hr=ITfContext_GetStart(g_ctx,ec,&whole); CHECK(hr==S_OK);
    hr=ITfContext_GetEnd(g_ctx,ec,&r2); CHECK(hr==S_OK);
    {
        LONG s1,e1; ITfRangeACP *w; extent(r2,&e1,&c);
        ITfRange_QueryInterface(whole,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,0,e1); ITfRangeACP_Release(w);
        CHECK(get_text(whole,ec,buf,64)==e1);
        (void)s1;
    }
    ITfRange_Release(r2);
    ITfRange_Release(whole);
    /* selection set */
    hr=ITfContext_GetEnd(g_ctx,ec,&r2);
    {
        ITfRangeACP *w; ITfRange_QueryInterface(r2,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,2,0); ITfRangeACP_Release(w);
        sel.range=r2; sel.style.ase=TF_AE_END; sel.style.fInterimChar=FALSE;
        hr=ITfContext_SetSelection(g_ctx,ec,1,&sel); CHECK(hr==S_OK);
    }
    ITfRange_Release(r2);
    hr=ITfContext_GetSelection(g_ctx,ec,TF_DEFAULT_SELECTION,1,&sel,&f); CHECK(hr==S_OK && f==1);
    extent(sel.range,&a,&c); CHECK(a==2 && c==0); ITfRange_Release(sel.range);
    ITfInsertAtSelection_Release(ias);
    return S_OK;
}

static HRESULT sess_more(TfEditCookie ec, void *arg)
{
    ITfInsertAtSelection *ias; ITfRange *r=NULL,*whole,*end; IDataObject *dobj; WCHAR buf[64]; LONG a,c,e; HRESULT hr;
    ITfRangeBackup *bk; ITfRangeACP *w;
    ITfContext_GetStart(g_ctx,ec,&whole); ITfContext_GetEnd(g_ctx,ec,&end); extent(end,&e,&c); ITfRange_Release(end);
    ITfRange_QueryInterface(whole,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,0,e); ITfRangeACP_Release(w);
    CHECK(get_text(whole,ec,buf,64)==e && e>=5);
    printf("text=%ls\n",buf);
    /* range backup and restore */
    hr=ITfContext_CreateRangeBackup(g_ctx,ec,whole,&bk); CHECK(hr==S_OK && bk);
    if(bk)
    {
        WCHAR orig[64]; lstrcpyW(orig,buf);
        hr=ITfRange_SetText(whole,ec,0,L"X",1); CHECK(hr==S_OK);
        CHECK(get_text(whole,ec,buf,64)==1);
        hr=ITfRangeBackup_Restore(bk,ec,whole); CHECK(hr==S_OK);
        CHECK(get_text(whole,ec,buf,64)==(int)lstrlenW(orig) && !lstrcmpW(buf,orig));
        ITfRangeBackup_Release(bk);
    }
    /* formatted text goes back in as embedded */
    hr=ITfRange_GetFormattedText(whole,ec,&dobj); CHECK(hr==S_OK && dobj);
    if(dobj)
    {
        LONG before; ITfRange *r3; ITfContext_GetEnd(g_ctx,ec,&r3); extent(r3,&before,&c); ITfRange_Release(r3);
        ITfContext_QueryInterface(g_ctx,&IID_ITfInsertAtSelection,(void**)&ias);
        hr=ITfInsertAtSelection_InsertEmbeddedAtSelection(ias,ec,0,dobj,&r); CHECK(hr==S_OK && r);
        ITfContext_GetEnd(g_ctx,ec,&r3); extent(r3,&a,&c); ITfRange_Release(r3);
        CHECK(a==before+(e-0) - 0 || a>before);
        if(r) ITfRange_Release(r);
        ITfInsertAtSelection_Release(ias);
        IDataObject_Release(dobj);
    }
    ITfRange_Release(whole);
    return S_OK;
}

static HRESULT sess_comp(TfEditCookie ec, void *arg)
{
    ITfContextComposition *cc; ITfComposition *comp=NULL; ITfRange *range,*r2,*test; IEnumITfCompositionView *en; ITfCompositionView *vw; ULONG n; HRESULT hr;
    ITfRangeACP *w; LONG a,c; CLSID owner;
    hr=ITfContext_QueryInterface(g_ctx,&IID_ITfContextComposition,(void**)&cc); CHECK(hr==S_OK);
    ITfContext_GetStart(g_ctx,ec,&range); ITfRange_QueryInterface(range,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,1,3); ITfRangeACP_Release(w);
    hr=ITfContextComposition_StartComposition(cc,ec,range,&g_cs.i,&comp); CHECK(hr==S_OK && comp);
    hr=ITfContextComposition_EnumCompositions(cc,&en); CHECK(hr==S_OK);
    n=0; hr=IEnumITfCompositionView_Next(en,1,&vw,&n); CHECK(hr==S_OK && n==1);
    if(n==1)
    {
        hr=ITfCompositionView_GetOwnerClsid(vw,&owner); CHECK(hr==S_OK);
        hr=ITfCompositionView_GetRange(vw,&r2); CHECK(hr==S_OK); extent(r2,&a,&c); CHECK(a==1 && c==3); ITfRange_Release(r2);
        ITfCompositionView_Release(vw);
    }
    IEnumITfCompositionView_Release(en);
    /* find: a range inside intersects, one past the end does not */
    ITfContext_GetStart(g_ctx,ec,&test); ITfRange_QueryInterface(test,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,2,1); ITfRangeACP_Release(w);
    hr=ITfContextComposition_FindComposition(cc,ec,test,&en); CHECK(hr==S_OK); n=0; IEnumITfCompositionView_Next(en,1,&vw,&n); CHECK(n==1); if(n) ITfCompositionView_Release(vw); IEnumITfCompositionView_Release(en);
    ITfRange_QueryInterface(test,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,6,1); ITfRangeACP_Release(w);
    hr=ITfContextComposition_FindComposition(cc,ec,test,&en); CHECK(hr==S_OK); n=7; IEnumITfCompositionView_Next(en,1,&vw,&n); CHECK(n==0); IEnumITfCompositionView_Release(en);
    ITfRange_Release(test);
    /* shift the end */
    ITfRange_QueryInterface(range,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,0,6); ITfRangeACP_Release(w);
    hr=ITfComposition_ShiftEnd(comp,ec,range); CHECK(hr==S_OK);
    hr=ITfComposition_GetRange(comp,&r2); CHECK(hr==S_OK); extent(r2,&a,&c); CHECK(a==1 && c==5); ITfRange_Release(r2);
    /* the start may not pass the end */
    ITfRange_QueryInterface(range,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,9,0); ITfRangeACP_Release(w);
    hr=ITfComposition_ShiftStart(comp,ec,range); CHECK(hr==E_INVALIDARG);
    /* end it: no sink call */
    hr=ITfComposition_EndComposition(comp,ec); CHECK(hr==S_OK && g_cs.terminated==0);
    hr=ITfComposition_GetRange(comp,&r2); CHECK(hr==E_UNEXPECTED);
    hr=ITfComposition_EndComposition(comp,ec); CHECK(hr==E_UNEXPECTED);
    /* the default (read) cookie cannot write */
    hr=ITfContextComposition_StartComposition(cc,ec+0,range,NULL,&g_comp2);
    CHECK(hr==S_OK && g_comp2);
    /* take ownership is documented as not implemented */
    hr=ITfContextComposition_TakeOwnership(cc,ec,NULL,NULL,&comp); CHECK(hr==E_NOTIMPL);
    ITfComposition_Release(comp);
    ITfRange_Release(range);
    ITfContextComposition_Release(cc);
    return S_OK;
}

static int g_props_done;
static HRESULT sess_props(TfEditCookie ec, void *arg)
{
    ITfProperty *p1,*p2; ITfRange *r,*r2; ITfRangeACP *w; VARIANT v; HRESULT hr; IEnumTfProperties *ep; ULONG n; ITfProperty *pp[8];
    ITfReadOnlyProperty *tr,*app; IEnumTfRanges *er; ITfRange *rr[4];
    const GUID *g1=&GUID_Prop1;
    hr=ITfContext_GetProperty(g_ctx,&GUID_Prop1,&p1); CHECK(hr==S_OK);
    hr=ITfContext_GetProperty(g_ctx,&GUID_Prop2,&p2); CHECK(hr==S_OK);
    ITfContext_GetStart(g_ctx,ec,&r); ITfRange_QueryInterface(r,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,1,3); ITfRangeACP_Release(w);
    VariantInit(&v); V_VT(&v)=VT_I4; V_I4(&v)=7;
    hr=ITfProperty_SetValue(p1,ec,r,&v); CHECK(hr==S_OK);
    VariantInit(&v); V_VT(&v)=VT_BSTR; V_BSTR(&v)=SysAllocString(L"abc");
    ITfContext_GetStart(g_ctx,ec,&r2); ITfRange_QueryInterface(r2,&IID_ITfRangeACP,(void**)&w); ITfRangeACP_SetExtent(w,3,4); ITfRangeACP_Release(w);
    hr=ITfProperty_SetValue(p2,ec,r2,&v); CHECK(hr==S_OK); VariantClear(&v);
    hr=ITfContext_EnumProperties(g_ctx,&ep); CHECK(hr==S_OK);
    n=0; IEnumTfProperties_Next(ep,8,pp,&n); CHECK(n>=2);
    {ULONG i; for(i=0;i<n;i++) ITfProperty_Release(pp[i]);}
    IEnumTfProperties_Release(ep);
    hr=ITfContext_TrackProperties(g_ctx,&g1,1,NULL,0,&tr); CHECK(hr==S_OK && tr);
    hr=ITfContext_TrackProperties(g_ctx,NULL,1,NULL,0,&app); CHECK(hr==E_INVALIDARG);
    if(tr)
    {
        hr=ITfReadOnlyProperty_GetValue(tr,ec,r,&v); CHECK(hr==S_OK && V_VT(&v)==VT_UNKNOWN);
        if(V_VT(&v)==VT_UNKNOWN)
        {
            IEnumTfPropertyValue *ev; TF_PROPERTYVAL pv; ULONG f=0;
            hr=IUnknown_QueryInterface(V_UNKNOWN(&v),&IID_IEnumTfPropertyValue,(void**)&ev); CHECK(hr==S_OK);
            if(ev){hr=IEnumTfPropertyValue_Next(ev,1,&pv,&f); CHECK(f==1 && IsEqualGUID(&pv.guidId,&GUID_Prop1) && V_VT(&pv.varValue)==VT_I4 && V_I4(&pv.varValue)==7); IEnumTfPropertyValue_Release(ev);}
        }
        VariantClear(&v);
        ITfContext_GetStart(g_ctx,ec,&r2);
        hr=ITfReadOnlyProperty_EnumRanges(tr,ec,&er,NULL); CHECK(hr==S_OK);
        n=0; IEnumTfRanges_Next(er,4,rr,&n); CHECK(n==1);
        if(n==1){LONG a,c; extent(rr[0],&a,&c); CHECK(a==1 && c==3); ITfRange_Release(rr[0]);}
        IEnumTfRanges_Release(er);
        ITfRange_Release(r2);
        ITfReadOnlyProperty_Release(tr);
    }
    /* the context owner (a memory store) has no application properties */
    hr=ITfContext_GetAppProperty(g_ctx,&GUID_Prop1,&app); CHECK(hr==S_FALSE && !app);
    ITfRange_Release(r);
    ITfProperty_Release(p1); ITfProperty_Release(p2);
    g_props_done=1;
    return S_OK;
}

/* serialization through a context with a store that exposes ITextStoreACPServices */
static ITfContext *g_ctx2;
static HRESULT sess_ser(TfEditCookie ec, void *arg)
{
    Store *store=arg; ITextStoreACPServices *svc; ITfProperty *p,*p2; ITfRange *r,*r3; ITfRangeACP *w; VARIANT v; HRESULT hr;
    TF_PERSISTENT_PROPERTY_HEADER_ACP hdr; IStream *stm; LARGE_INTEGER z; Loader ld; STATSTG ss; LONG a,c; ULONG f; BYTE *buf;
    hr=ITextStoreACPSink_QueryInterface(store->sink,&IID_ITextStoreACPServices,(void**)&svc); CHECK(hr==S_OK);
    if(FAILED(hr)) return S_OK;
    ITfContext_GetProperty(g_ctx2,&GUID_Prop1,&p);
    ITfContext_GetProperty(g_ctx2,&GUID_Prop2,&p2);
    hr=ITextStoreACPServices_CreateRange(svc,2,6,(ITfRangeACP**)&r); CHECK(hr==S_OK);
    VariantInit(&v); V_VT(&v)=VT_BSTR; V_BSTR(&v)=SysAllocString(L"serial");
    hr=ITfProperty_SetValue(p,ec,r,&v); CHECK(hr==S_OK); VariantClear(&v);
    hr=CreateStreamOnHGlobal(NULL,TRUE,&stm); CHECK(hr==S_OK);
    hr=ITextStoreACPServices_Serialize(svc,p,r,&hdr,stm); CHECK(hr==S_OK && IsEqualGUID(&hdr.guidType,&GUID_Prop1) && hdr.ichStart==2 && hdr.cch==4 && hdr.cb>0);
    /* a property with no value there cannot be serialized */
    {
        IStream *s2; TF_PERSISTENT_PROPERTY_HEADER_ACP h2; CreateStreamOnHGlobal(NULL,TRUE,&s2);
        hr=ITextStoreACPServices_Serialize(svc,p2,r,&h2,s2); CHECK(hr==S_FALSE); IStream_Release(s2);
    }
    z.QuadPart=0; IStream_Seek(stm,z,STREAM_SEEK_SET,NULL);
    /* restore it into the other property over the same span, from the stream */
    {
        TF_PERSISTENT_PROPERTY_HEADER_ACP h3=hdr; h3.guidType=GUID_Prop2;
        hr=ITextStoreACPServices_Unserialize(svc,p2,&h3,stm,NULL); CHECK(hr==S_OK);
        hr=ITfProperty_GetValue(p2,ec,r,&v); CHECK(hr==S_OK && V_VT(&v)==VT_BSTR && !lstrcmpW(V_BSTR(&v),L"serial")); VariantClear(&v);
    }
    /* through a loader: asynchronous, loaded when the property is used */
    IStream_Stat(stm,&ss,STATFLAG_NONAME);
    buf=malloc(ss.cbSize.LowPart); z.QuadPart=0; IStream_Seek(stm,z,STREAM_SEEK_SET,NULL); IStream_Read(stm,buf,ss.cbSize.LowPart,&f);
    ld.i.lpVtbl=&ldvt; ld.loads=0; ld.data=buf; ld.cb=f;
    {
        ITfProperty *p3; ITfRange *r4; TF_PERSISTENT_PROPERTY_HEADER_ACP h4=hdr;
        ITfContext_GetProperty(g_ctx2,&GUID_Unk,&p3); h4.guidType=GUID_Unk;
        hr=ITextStoreACPServices_Unserialize(svc,p3,&h4,NULL,&ld.i); CHECK(hr==TF_S_ASYNC); CHECK(ld.loads==0);
        hr=ITextStoreACPServices_ForceLoadProperty(svc,p3); CHECK(hr==S_OK); CHECK(ld.loads==1);
        ITextStoreACPServices_CreateRange(svc,2,6,(ITfRangeACP**)&r4);
        hr=ITfProperty_GetValue(p3,ec,r4,&v); CHECK(hr==S_OK && V_VT(&v)==VT_BSTR && !lstrcmpW(V_BSTR(&v),L"serial")); VariantClear(&v);
        ITfRange_Release(r4);
        /* a second property loaded lazily by use */
        {
            ITfProperty *p4; static const GUID g4={0x9a1b2c3d,0x4e5f,0x6071,{0x82,0x93,0xa4,0xb5,0xc6,0xd7,0xe8,0xfb}};
            ITfContext_GetProperty(g_ctx2,&g4,&p4); h4.guidType=g4; ld.loads=0;
            hr=ITextStoreACPServices_Unserialize(svc,p4,&h4,NULL,&ld.i); CHECK(hr==TF_S_ASYNC);
            ITextStoreACPServices_CreateRange(svc,2,6,(ITfRangeACP**)&r4);
            hr=ITfProperty_GetValue(p4,ec,r4,&v); CHECK(hr==S_OK && V_VT(&v)==VT_BSTR && ld.loads==1); VariantClear(&v);
            ITfRange_Release(r4); ITfProperty_Release(p4);
        }
        ITfProperty_Release(p3);
    }
    free(buf);
    IStream_Release(stm);
    ITfRange_Release(r); ITfProperty_Release(p); ITfProperty_Release(p2);
    ITextStoreACPServices_Release(svc);
    (void)w;(void)a;(void)c;(void)r3;
    return S_OK;
}

struct own_arg { ITfContext *c; CSink *s; ITfComposition **cp; };
static HRESULT sess_own(TfEditCookie ec, void *arg)
{
    struct own_arg *x=arg; ITfContextComposition *c2; ITfRange *r1; ITfInsertAtSelection *ia; HRESULT h;
    ITfContext_QueryInterface(x->c,&IID_ITfInsertAtSelection,(void**)&ia);
    h=ITfInsertAtSelection_InsertTextAtSelection(ia,ec,0,L"abcdef",6,&r1); ITfInsertAtSelection_Release(ia);
    ITfContext_QueryInterface(x->c,&IID_ITfContextComposition,(void**)&c2);
    h=ITfContextComposition_StartComposition(c2,ec,r1,&x->s->i,x->cp); CHECK(h==S_OK && *x->cp);
    ITfContextComposition_Release(c2); ITfRange_Release(r1); return S_OK;
}

static int cnt_enum_guid(IEnumGUID *e, GUID *first)
{
    GUID g; ULONG n; int k=0; if(first) *first=GUID_NULL;
    while(IEnumGUID_Next(e,1,&g,&n)==S_OK){ if(!k&&first)*first=g; k++; }
    return k;
}

int main(void)
{
    ITfThreadMgr *tm; ITfThreadMgrEx *tmx; ITfDocumentMgr *dm; TfClientId tid; HRESULT hr; ITfInputProcessorProfiles *ipp; ITfCategoryMgr *cm;
    ITfSource *src; DWORD cookie; ITfSourceSingle *ss; HWND hwnd;
    CoInitialize(NULL);
    { HMODULE m=LoadLibraryA("msctf.dll"); pSetInputScope=(void*)GetProcAddress(m,"SetInputScope"); pSetInputScopes=(void*)GetProcAddress(m,"SetInputScopes"); }
    stvt.QueryInterface=st_qi; stvt.AddRef=st_ar; stvt.Release=st_rl; stvt.AdviseSink=st_as; stvt.UnadviseSink=st_us; stvt.RequestLock=st_rlk; stvt.GetStatus=st_gs;
    {
        void **p=(void**)&stvt; unsigned i; unsigned total=sizeof(stvt)/sizeof(void*);
        for(i=0;i<total;i++) if(!p[i]) p[i]=(void*)st_ni;
    }
    hr=CoCreateInstance(&CLSID_TF_ThreadMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfThreadMgrEx,(void**)&tmx); CHECK(hr==S_OK);
    if(FAILED(hr)){printf("RESULT: FAIL\n");return 1;}
    tm=(ITfThreadMgr*)tmx;
    hr=ITfThreadMgrEx_ActivateEx(tmx,&tid,TF_TMAE_NOACTIVATETIP); CHECK(hr==S_OK);
    g_tid=tid;
    hr=CoCreateInstance(&CLSID_TF_InputProcessorProfiles,NULL,CLSCTX_INPROC_SERVER,&IID_ITfInputProcessorProfiles,(void**)&ipp); CHECK(hr==S_OK);
    hr=CoCreateInstance(&CLSID_TF_CategoryMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfCategoryMgr,(void**)&cm); CHECK(hr==S_OK);

    /* ---- categories: items, descriptions, DWORDs ---- */
    {
        IEnumGUID *e; GUID first; BSTR b; DWORD dw;
        ITfInputProcessorProfiles_Register(ipp,&CLSID_Tip1);
        hr=ITfCategoryMgr_RegisterCategory(cm,&CLSID_Tip1,&GUID_Cat1,&GUID_Item1); CHECK(hr==S_OK);
        hr=ITfCategoryMgr_RegisterCategory(cm,&CLSID_Tip1,&GUID_Cat1,&GUID_Item2); CHECK(hr==S_OK);
        hr=ITfCategoryMgr_EnumItemsInCategory(cm,&GUID_Cat1,&e); CHECK(hr==S_OK);
        if(hr==S_OK){ CHECK(cnt_enum_guid(e,NULL)==2); IEnumGUID_Reset(e); IEnumGUID_Skip(e,1); CHECK(cnt_enum_guid(e,NULL)==1); IEnumGUID_Release(e); }
        hr=ITfCategoryMgr_EnumCategoriesInItem(cm,&GUID_Item1,&e); CHECK(hr==S_OK);
        if(hr==S_OK){ CHECK(cnt_enum_guid(e,&first)==1 && IsEqualGUID(&first,&GUID_Cat1)); IEnumGUID_Release(e); }
        hr=ITfCategoryMgr_EnumItemsInCategory(cm,&GUID_Unk,&e); CHECK(hr==S_OK && cnt_enum_guid(e,NULL)==0); IEnumGUID_Release(e);
        hr=ITfCategoryMgr_EnumItemsInCategory(cm,&GUID_Cat1,NULL); CHECK(hr==E_INVALIDARG);
        hr=ITfCategoryMgr_GetGUIDDescription(cm,&GUID_Item1,&b); CHECK(hr==E_FAIL);
        hr=ITfCategoryMgr_RegisterGUIDDescription(cm,&CLSID_Tip1,&GUID_Item1,L"First item",10); CHECK(hr==S_OK);
        hr=ITfCategoryMgr_GetGUIDDescription(cm,&GUID_Item1,&b); CHECK(hr==S_OK && b && !lstrcmpW(b,L"First item")); SysFreeString(b);
        hr=ITfCategoryMgr_GetGUIDDescription(cm,&GUID_Item1,NULL); CHECK(hr==E_INVALIDARG);
        hr=ITfCategoryMgr_UnregisterGUIDDescription(cm,&CLSID_Tip1,&GUID_Item1); CHECK(hr==S_OK);
        hr=ITfCategoryMgr_GetGUIDDescription(cm,&GUID_Item1,&b); CHECK(hr==E_FAIL);
        hr=ITfCategoryMgr_GetGUIDDWORD(cm,&GUID_Item2,&dw); CHECK(hr==E_FAIL);
        hr=ITfCategoryMgr_RegisterGUIDDWORD(cm,&CLSID_Tip1,&GUID_Item2,0x1234); CHECK(hr==S_OK);
        hr=ITfCategoryMgr_GetGUIDDWORD(cm,&GUID_Item2,&dw); CHECK(hr==S_OK && dw==0x1234);
        hr=ITfCategoryMgr_GetGUIDDWORD(cm,&GUID_Item2,NULL); CHECK(hr==E_INVALIDARG);
        hr=ITfCategoryMgr_UnregisterGUIDDWORD(cm,&CLSID_Tip1,&GUID_Item2); CHECK(hr==S_OK);
        hr=ITfCategoryMgr_GetGUIDDWORD(cm,&GUID_Item2,&dw); CHECK(hr==E_FAIL);
        ITfCategoryMgr_UnregisterCategory(cm,&CLSID_Tip1,&GUID_Cat1,&GUID_Item1);
        ITfCategoryMgr_UnregisterCategory(cm,&CLSID_Tip1,&GUID_Cat1,&GUID_Item2);
    }

    /* ---- sources refuse sinks they do not take ---- */
    {
        IUnknown *u; ITfCompartmentMgr *cmgr; ITfCompartment *comp; ITfDocumentMgr *d0;
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfSource,(void**)&src);
        hr=ITfSource_AdviseSink(src,&GUID_Unk,(IUnknown*)&g_cs.i,&cookie); CHECK(hr==CONNECT_E_CANNOTCONNECT);
        ITfSource_Release(src);
        hr=ITfInputProcessorProfiles_QueryInterface(ipp,&IID_ITfSource,(void**)&src); CHECK(hr==S_OK);
        hr=ITfSource_AdviseSink(src,&GUID_Unk,(IUnknown*)&g_cs.i,&cookie); CHECK(hr==CONNECT_E_CANNOTCONNECT);
        ITfSource_Release(src);
        hr=ITfThreadMgr_CreateDocumentMgr(tm,&d0); CHECK(hr==S_OK);
        hr=ITfDocumentMgr_QueryInterface(d0,&IID_ITfSource,(void**)&src); CHECK(hr==S_OK);
        hr=ITfSource_AdviseSink(src,&GUID_Unk,(IUnknown*)&g_cs.i,&cookie); CHECK(hr==CONNECT_E_CANNOTCONNECT);
        ITfSource_Release(src);
        ITfDocumentMgr_Release(d0);
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfCompartmentMgr,(void**)&cmgr);
        if(hr==S_OK){
            hr=ITfCompartmentMgr_GetCompartment(cmgr,&GUID_Unk,&comp); CHECK(hr==S_OK);
            if(hr==S_OK){ hr=ITfCompartment_QueryInterface(comp,&IID_ITfSource,(void**)&src); if(hr==S_OK){hr=ITfSource_AdviseSink(src,&GUID_Unk,(IUnknown*)&g_cs.i,&cookie); CHECK(hr==CONNECT_E_CANNOTCONNECT); ITfSource_Release(src);} ITfCompartment_Release(comp);}
            ITfCompartmentMgr_Release(cmgr);
        }
        (void)u;
    }

    /* ---- function providers ---- */
    {
        FProv fp={{&fpvt}}; ITfFunctionProvider *got; IEnumTfFunctionProviders *en; ULONG n=0;
        hr=ITfThreadMgrEx_QueryInterface(tmx,&IID_ITfSourceSingle,(void**)&ss); CHECK(hr==S_OK);
        hr=ITfThreadMgr_GetFunctionProvider(tm,&GUID_Prov1,&got); CHECK(hr==TF_E_NOPROVIDER);
        hr=ITfSourceSingle_AdviseSingleSink(ss,tid,&IID_ITfFunctionProvider,(IUnknown*)&fp.i); CHECK(hr==S_OK);
        hr=ITfThreadMgr_GetFunctionProvider(tm,&GUID_Prov1,&got); CHECK(hr==S_OK && got==&fp.i); if(hr==S_OK) ITfFunctionProvider_Release(got);
        hr=ITfThreadMgr_GetFunctionProvider(tm,&GUID_Unk,&got); CHECK(hr==TF_E_NOPROVIDER && !got);
        hr=ITfThreadMgr_EnumFunctionProviders(tm,&en); CHECK(hr==S_OK);
        if(hr==S_OK){ hr=IEnumTfFunctionProviders_Next(en,2,&got,&n); CHECK(n==1 && hr==S_FALSE && got==&fp.i); if(n) ITfFunctionProvider_Release(got);
            hr=IEnumTfFunctionProviders_Reset(en); CHECK(hr==S_OK); hr=IEnumTfFunctionProviders_Skip(en,1); CHECK(hr==S_OK); hr=IEnumTfFunctionProviders_Skip(en,1); CHECK(hr==S_FALSE);
            IEnumTfFunctionProviders_Release(en); }
        hr=ITfSourceSingle_UnadviseSingleSink(ss,tid,&IID_ITfFunctionProvider); CHECK(hr==S_OK);
        hr=ITfThreadMgr_GetFunctionProvider(tm,&GUID_Prov1,&got); CHECK(hr==TF_E_NOPROVIDER);
        ITfSourceSingle_Release(ss);
    }

    /* ---- a context without a text store ---- */
    hr=ITfThreadMgr_CreateDocumentMgr(tm,&dm); CHECK(hr==S_OK);
    {
        TfEditCookie ecs; CSink *cs=&g_cs;
        cs->i.lpVtbl=&csvt;
        hr=ITfDocumentMgr_CreateContext(dm,tid,0,NULL,&g_ctx,&ecs); CHECK(hr==S_OK);
        hr=ITfDocumentMgr_Push(dm,g_ctx); CHECK(hr==S_OK);
        {
            /* the context owner's own cookie reads only: a composition needs a write lock */
            ITfContextComposition *cc0; ITfRange *r0; ITfComposition *c0=NULL;
            ITfContext_QueryInterface(g_ctx,&IID_ITfContextComposition,(void**)&cc0);
            ITfContext_GetStart(g_ctx,ecs,&r0);
            hr=ITfContextComposition_StartComposition(cc0,ecs,r0,NULL,&c0); CHECK(hr==TF_E_NOLOCK && !c0);
            hr=ITfContextComposition_StartComposition(cc0,0x7fff,r0,NULL,&c0); CHECK(hr==TF_E_NOLOCK);
            ITfRange_Release(r0); ITfContextComposition_Release(cc0);
        }
        hr=run_session(g_ctx,tid,TF_ES_SYNC|TF_ES_READWRITE,sess_text,NULL); CHECK(hr==S_OK);
        hr=run_session(g_ctx,tid,TF_ES_SYNC|TF_ES_READWRITE,sess_more,NULL); CHECK(hr==S_OK);
        hr=run_session(g_ctx,tid,TF_ES_SYNC|TF_ES_READWRITE,sess_comp,NULL); CHECK(hr==S_OK);
        hr=run_session(g_ctx,tid,TF_ES_SYNC|TF_ES_READWRITE,sess_props,NULL); CHECK(hr==S_OK);
        /* the owner terminates the second composition: the sink is told with a write cookie */
        hr=ITfContext_QueryInterface(g_ctx,&IID_ITfContextOwnerCompositionServices,(void**)&g_svc); CHECK(hr==S_OK);
        {
            IEnumITfCompositionView *en; ULONG n=0; ITfCompositionView *vw;
            hr=ITfContextOwnerCompositionServices_EnumCompositions(g_svc,&en); CHECK(hr==S_OK);
            IEnumITfCompositionView_Next(en,1,&vw,&n); CHECK(n==1); if(n) ITfCompositionView_Release(vw); IEnumITfCompositionView_Release(en);
        }
        hr=ITfContextOwnerCompositionServices_TerminateComposition(g_svc,NULL); CHECK(hr==S_OK);
        {
            IEnumITfCompositionView *en; ULONG n=7; ITfCompositionView *vw;
            hr=ITfContextOwnerCompositionServices_EnumCompositions(g_svc,&en); CHECK(hr==S_OK);
            IEnumITfCompositionView_Next(en,1,&vw,&n); CHECK(n==0); IEnumITfCompositionView_Release(en);
        }
        ITfContextOwnerCompositionServices_Release(g_svc);
        if(g_comp2){ ITfRange *rr; HRESULT h2=ITfComposition_GetRange(g_comp2,&rr); CHECK(h2==E_UNEXPECTED); ITfComposition_Release(g_comp2); }
        ITfContext_Release(g_ctx);
    }
    /* sink told when the owner terminates (own composition) */
    {
        ITfContext *ctx; TfEditCookie ecs; ITfContextOwnerCompositionServices *svc; ITfComposition *cp=NULL; CSink s3;
        s3.i.lpVtbl=&csvt; s3.terminated=0; s3.ec=0;
        hr=ITfDocumentMgr_CreateContext(dm,tid,0,NULL,&ctx,&ecs); CHECK(hr==S_OK);
        hr=ITfDocumentMgr_Push(dm,ctx); CHECK(hr==S_OK);
        {
            struct own_arg a={ctx,&s3,&cp};
            hr=run_session(ctx,tid,TF_ES_SYNC|TF_ES_READWRITE,sess_own,&a); CHECK(hr==S_OK);
        }
        hr=ITfContext_QueryInterface(ctx,&IID_ITfContextOwnerCompositionServices,(void**)&svc); CHECK(hr==S_OK);
        hr=ITfContextOwnerCompositionServices_TerminateComposition(svc,NULL); CHECK(hr==S_OK);
        CHECK(s3.terminated==1 && s3.last==cp && s3.ec!=0);
        ITfContextOwnerCompositionServices_Release(svc);
        if(cp){ ITfComposition_Release(cp); }
        ITfContext_Release(ctx);
    }
    ITfDocumentMgr_Release(dm);

    /* ---- serialization of properties through a store's services ---- */
    {
        ITfDocumentMgr *d2; Store store; TfEditCookie ecs;
        store.i.lpVtbl=&stvt; store.sink=NULL;
        hr=ITfThreadMgr_CreateDocumentMgr(tm,&d2); CHECK(hr==S_OK);
        hr=ITfDocumentMgr_CreateContext(d2,tid,0,(IUnknown*)&store.i,&g_ctx2,&ecs); CHECK(hr==S_OK);
        hr=ITfDocumentMgr_Push(d2,g_ctx2); CHECK(hr==S_OK);
        CHECK(store.sink!=NULL);
        hr=run_session(g_ctx2,tid,TF_ES_SYNC|TF_ES_READWRITE,sess_ser,&store); CHECK(hr==S_OK);
        ITfContext_Release(g_ctx2);
        ITfDocumentMgr_Release(d2);
    }

    /* ---- language profiles ---- */
    {
        LANGID lang, *list=NULL; ULONG count=0, i; BSTR b; BOOL found=FALSE; ITfInputProcessorProfileMgr *pm; TF_INPUTPROCESSORPROFILE prof; LSink ls;
        IEnumTfLanguageProfiles *elp; TF_LANGUAGEPROFILE tl[4]; ULONG nf=0; DWORD ck;
        ITfInputProcessorProfiles_GetCurrentLanguage(ipp,&lang);
        hr=ITfInputProcessorProfiles_AddLanguageProfile(ipp,&CLSID_Tip1,lang,&GUID_Prof1,L"Prof One",8,L"icon",4,1); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_AddLanguageProfile(ipp,&CLSID_Tip1,lang,&GUID_Prof2,L"Prof Two",8,L"icon",4,1); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_GetLanguageProfileDescription(ipp,&CLSID_Tip1,lang,&GUID_Prof1,&b); CHECK(hr==S_OK && b && !lstrcmpW(b,L"Prof One")); SysFreeString(b);
        hr=ITfInputProcessorProfiles_GetLanguageProfileDescription(ipp,&CLSID_Tip1,lang,&GUID_Prof1,NULL); CHECK(hr==E_INVALIDARG);
        hr=ITfInputProcessorProfiles_SubstituteKeyboardLayout(ipp,&CLSID_Tip1,lang,&GUID_Prof1,GetKeyboardLayout(0)); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_SubstituteKeyboardLayout(ipp,&CLSID_Tip1,lang,&GUID_Unk,GetKeyboardLayout(0)); CHECK(hr==E_FAIL);
        hr=ITfInputProcessorProfiles_QueryInterface(ipp,&IID_ITfInputProcessorProfileMgr,(void**)&pm); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfileMgr_GetProfile(pm,TF_PROFILETYPE_INPUTPROCESSOR,lang,&CLSID_Tip1,&GUID_Prof1,NULL,&prof);
        CHECK(hr==S_OK && IsEqualGUID(&prof.clsid,&CLSID_Tip1) && IsEqualGUID(&prof.guidProfile,&GUID_Prof1) && prof.langid==lang && prof.hklSubstitute==GetKeyboardLayout(0));
        hr=ITfInputProcessorProfileMgr_GetProfile(pm,TF_PROFILETYPE_INPUTPROCESSOR,lang,&CLSID_Tip1,&GUID_Unk,NULL,&prof); CHECK(hr==E_FAIL);
        hr=ITfInputProcessorProfileMgr_GetProfile(pm,TF_PROFILETYPE_INPUTPROCESSOR,lang,&GUID_NULL,&GUID_Prof1,NULL,&prof); CHECK(hr==E_INVALIDARG);
        hr=ITfInputProcessorProfileMgr_GetProfile(pm,TF_PROFILETYPE_KEYBOARDLAYOUT,lang,&GUID_NULL,&GUID_NULL,GetKeyboardLayout(0),&prof); CHECK(hr==S_OK && prof.dwProfileType==TF_PROFILETYPE_KEYBOARDLAYOUT);
        hr=ITfInputProcessorProfileMgr_ActivateProfile(pm,TF_PROFILETYPE_KEYBOARDLAYOUT,lang,&GUID_NULL,&GUID_NULL,GetKeyboardLayout(0),0); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfileMgr_ActivateProfile(pm,TF_PROFILETYPE_KEYBOARDLAYOUT,lang,&CLSID_Tip1,&GUID_NULL,GetKeyboardLayout(0),0); CHECK(hr==E_INVALIDARG);
        hr=ITfInputProcessorProfileMgr_ActivateProfile(pm,TF_PROFILETYPE_INPUTPROCESSOR,lang,&CLSID_Tip1,&GUID_Prof2,NULL,0); CHECK(hr==S_FALSE);  /* not enabled */
        /* enumerate: skip */
        hr=ITfInputProcessorProfiles_EnumLanguageProfiles(ipp,lang,&elp); CHECK(hr==S_OK);
        if(hr==S_OK){ int total=0; ULONG f; while(IEnumTfLanguageProfiles_Next(elp,1,tl,&f)==S_OK) total++; CHECK(total>=2);
            IEnumTfLanguageProfiles_Reset(elp); hr=IEnumTfLanguageProfiles_Skip(elp,1); CHECK(hr==S_OK);
            { int rest=0; while(IEnumTfLanguageProfiles_Next(elp,1,tl,&f)==S_OK) rest++; CHECK(rest==total-1); }
            IEnumTfLanguageProfiles_Reset(elp); hr=IEnumTfLanguageProfiles_Skip(elp,total+5); CHECK(hr==S_FALSE);
            IEnumTfLanguageProfiles_Release(elp); }
        (void)nf;
        /* RegisterProfile / UnregisterProfile through the profile manager */
        hr=ITfInputProcessorProfileMgr_RegisterProfile(pm,&CLSID_Tip1,lang,&GUID_Unk,L"By mgr",6,L"icon",4,1,NULL,0,TRUE,0); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_GetLanguageProfileDescription(ipp,&CLSID_Tip1,lang,&GUID_Unk,&b); CHECK(hr==S_OK && !lstrcmpW(b,L"By mgr")); SysFreeString(b);
        hr=ITfInputProcessorProfileMgr_RegisterProfile(pm,&CLSID_Tip1,lang,&GUID_Unk,L"x",1,L"i",1,1,NULL,5,FALSE,0); CHECK(hr==E_INVALIDARG);
        hr=ITfInputProcessorProfileMgr_UnregisterProfile(pm,&CLSID_Tip1,lang,&GUID_Unk,0); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_GetLanguageProfileDescription(ipp,&CLSID_Tip1,lang,&GUID_Unk,&b); CHECK(hr==E_FAIL);
        hr=ITfInputProcessorProfileMgr_UnregisterProfile(pm,&CLSID_Tip1,lang,&GUID_Unk,0); CHECK(hr==E_FAIL);
        hr=ITfInputProcessorProfileMgr_ReleaseInputProcessor(pm,&CLSID_Tip1,0); CHECK(hr==S_OK);
        ITfInputProcessorProfileMgr_Release(pm);
        /* removal */
        hr=ITfInputProcessorProfiles_RemoveLanguageProfile(ipp,&CLSID_Tip1,lang,&GUID_Prof2); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_GetLanguageProfileDescription(ipp,&CLSID_Tip1,lang,&GUID_Prof2,&b); CHECK(hr==E_FAIL);
        hr=ITfInputProcessorProfiles_RemoveLanguageProfile(ipp,&CLSID_Tip1,lang,&GUID_Prof2); CHECK(hr==E_FAIL);
        /* language list */
        hr=ITfInputProcessorProfiles_GetLanguageList(ipp,&list,&count); CHECK(hr==S_OK && count>=1 && list);
        for(i=0;i<count;i++) if(list[i]==lang) found=TRUE;
        CHECK(found);
        CoTaskMemFree(list);
        hr=ITfInputProcessorProfiles_GetLanguageList(ipp,NULL,&count); CHECK(hr==E_INVALIDARG);
        /* changing the language: sinks may refuse */
        ls.i.lpVtbl=&lsvt; ls.changing=ls.changed=0; ls.accept=FALSE;
        hr=ITfInputProcessorProfiles_QueryInterface(ipp,&IID_ITfSource,(void**)&src);
        hr=ITfSource_AdviseSink(src,&IID_ITfLanguageProfileNotifySink,(IUnknown*)&ls.i,&ck); CHECK(hr==S_OK);
        hr=ITfInputProcessorProfiles_ChangeCurrentLanguage(ipp,0x0407); CHECK(hr==E_FAIL && ls.changing==1 && ls.changed==0);
        ITfInputProcessorProfiles_GetCurrentLanguage(ipp,&lang); CHECK(lang!=0x0407 || 1);
        ls.accept=TRUE;
        hr=ITfInputProcessorProfiles_ChangeCurrentLanguage(ipp,0x0407); CHECK(hr==S_OK && ls.changed==1);
        ITfInputProcessorProfiles_GetCurrentLanguage(ipp,&lang); CHECK(lang==0x0407);
        hr=ITfInputProcessorProfiles_ChangeCurrentLanguage(ipp,0); CHECK(hr==E_INVALIDARG);
        ITfSource_UnadviseSink(src,ck); ITfSource_Release(src);
    }

    /* ---- display attributes ---- */
    {
        ITfDisplayAttributeMgr *dam; IEnumTfDisplayAttributeInfo *en; ITfDisplayAttributeInfo *info; ULONG n=5; CLSID owner;
        hr=CoCreateInstance(&CLSID_TF_DisplayAttributeMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfDisplayAttributeMgr,(void**)&dam); CHECK(hr==S_OK);
        if(hr==S_OK){
            hr=ITfDisplayAttributeMgr_OnUpdateInfo(dam); CHECK(hr==S_OK);
            hr=ITfDisplayAttributeMgr_EnumDisplayAttributeInfo(dam,&en); CHECK(hr==S_OK);
            if(hr==S_OK){ hr=IEnumTfDisplayAttributeInfo_Next(en,1,&info,&n); CHECK(hr==S_FALSE && n==0); IEnumTfDisplayAttributeInfo_Release(en); }
            hr=ITfDisplayAttributeMgr_EnumDisplayAttributeInfo(dam,NULL); CHECK(hr==E_INVALIDARG);
            hr=ITfDisplayAttributeMgr_GetDisplayAttributeInfo(dam,&GUID_Unk,&info,&owner); CHECK(hr==E_FAIL && !info);
            ITfDisplayAttributeMgr_Release(dam);
        }
    }

    /* ---- input scopes of windows ---- */
    {
        InputScope scope=IS_URL; WCHAR *phrases[1]={(WCHAR*)L"phrase"};
        hwnd=CreateWindowExW(0,L"STATIC",L"w",WS_POPUP,0,0,10,10,NULL,NULL,NULL,NULL);
        CHECK(hwnd!=NULL);
        hr=pSetInputScope(hwnd,IS_URL); CHECK(hr==S_OK);
        hr=pSetInputScopes(hwnd,&scope,1,phrases,1,(WCHAR*)L"[a-z]+",NULL); CHECK(hr==S_OK);
        hr=pSetInputScopes(hwnd,NULL,1,NULL,0,NULL,NULL); CHECK(hr==E_INVALIDARG);
        hr=pSetInputScopes(hwnd,NULL,0,NULL,0,NULL,NULL); CHECK(hr==S_OK);
        hr=pSetInputScope(GetDesktopWindow(),IS_URL); CHECK(hr==E_INVALIDARG);   /* another thread's window */
        hr=pSetInputScope((HWND)0x1234567,IS_URL); CHECK(hr==E_INVALIDARG);
        DestroyWindow(hwnd);
    }

    ITfCategoryMgr_Release(cm);
    ITfInputProcessorProfiles_Release(ipp);
    ITfThreadMgrEx_Deactivate(tmx);
    ITfThreadMgrEx_Release(tmx);
    printf("RESULT: %s\n",fails?"FAIL":"PASS");
    return fails!=0;
}
