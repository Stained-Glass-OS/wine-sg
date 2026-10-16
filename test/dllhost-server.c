/* dllhost-gate.sh's COM server: its only class is an IDispatch whose one method returns the path of the
 * process it runs in -- the surrogate's, when the class is registered with a DllSurrogate AppID. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <oaidl.h>
#include <stdio.h>


typedef struct { IDispatch iface; LONG ref; } obj;
static HRESULT WINAPI o_QI(IDispatch *i, REFIID r, void **p){ if (IsEqualGUID(r,&IID_IUnknown)||IsEqualGUID(r,&IID_IDispatch)){*p=i;IDispatch_AddRef(i);return S_OK;} *p=NULL; return E_NOINTERFACE;}
static ULONG WINAPI o_AddRef(IDispatch *i){ return InterlockedIncrement(&((obj*)i)->ref);}
static ULONG WINAPI o_Release(IDispatch *i){ LONG r=InterlockedDecrement(&((obj*)i)->ref); if(!r) HeapFree(GetProcessHeap(),0,i); return r;}
static HRESULT WINAPI o_GetTypeInfoCount(IDispatch *i, UINT *c){ *c=0; return S_OK;}
static HRESULT WINAPI o_GetTypeInfo(IDispatch *i, UINT n, LCID l, ITypeInfo **t){ *t=NULL; return DISP_E_BADINDEX;}
static HRESULT WINAPI o_GetIDsOfNames(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d){ d[0]=1; return S_OK;}
static HRESULT WINAPI o_Invoke(IDispatch *i, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{ WCHAR path[MAX_PATH]; GetModuleFileNameW(NULL, path, MAX_PATH); if (v){ V_VT(v)=VT_BSTR; V_BSTR(v)=SysAllocString(path);} return S_OK; }
static IDispatchVtbl vtbl = { o_QI,o_AddRef,o_Release,o_GetTypeInfoCount,o_GetTypeInfo,o_GetIDsOfNames,o_Invoke };

typedef struct { IClassFactory iface; } fac;
static HRESULT WINAPI f_QI(IClassFactory *i, REFIID r, void **p){ if (IsEqualGUID(r,&IID_IUnknown)||IsEqualGUID(r,&IID_IClassFactory)){*p=i;return S_OK;} *p=NULL; return E_NOINTERFACE;}
static ULONG WINAPI f_AddRef(IClassFactory *i){return 2;}
static ULONG WINAPI f_Release(IClassFactory *i){return 1;}
static HRESULT WINAPI f_Create(IClassFactory *i, IUnknown *o, REFIID r, void **p)
{ obj *x; HRESULT hr; if (o) return CLASS_E_NOAGGREGATION; x=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*x)); x->iface.lpVtbl=&vtbl; x->ref=1; hr=IDispatch_QueryInterface(&x->iface,r,p); IDispatch_Release(&x->iface); return hr;}
static HRESULT WINAPI f_Lock(IClassFactory *i, BOOL l){return S_OK;}
static IClassFactoryVtbl fvtbl = { f_QI,f_AddRef,f_Release,f_Create,f_Lock };
static IClassFactory the_factory = { &fvtbl };

HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID r, void **p){ return IClassFactory_QueryInterface(&the_factory,r,p);}
HRESULT WINAPI DllCanUnloadNow(void){return S_FALSE;}
