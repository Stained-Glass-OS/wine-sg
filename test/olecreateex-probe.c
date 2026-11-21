/* olecreateex-probe: ole32 OleCreateEx, which was a spec stub (Stained Glass OS wine-sg 8275;
 * test/olecreateex-gate.sh): an object (a data cache class registered in-process) created with two formats to
 * render caches both;
 * with OLERENDER_DRAW an advise sink is the view's.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static ULONG WINAPI s_AddRef( IAdviseSink *s ) { return 2; }
static ULONG WINAPI s_Release( IAdviseSink *s ) { return 1; }
static HRESULT WINAPI s_QI( IAdviseSink *s, REFIID riid, void **ppv )
{
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IAdviseSink )) { *ppv = s; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static void WINAPI s_OnDataChange( IAdviseSink *s, FORMATETC *f, STGMEDIUM *m ) { }
static void WINAPI s_OnViewChange( IAdviseSink *s, DWORD a, LONG i ) { }
static void WINAPI s_OnRename( IAdviseSink *s, IMoniker *m ) { }
static void WINAPI s_OnSave( IAdviseSink *s ) { }
static void WINAPI s_OnClose( IAdviseSink *s ) { }
static IAdviseSinkVtbl sink_vtbl = { s_QI, s_AddRef, s_Release, s_OnDataChange, s_OnViewChange, s_OnRename, s_OnSave, s_OnClose };
static IAdviseSink sink = { &sink_vtbl };

/* an in-process class whose objects are data caches (IOleCache, IPersistStorage, IDataObject) */
static const CLSID CLSID_SgCache = { 0x5d6d73fc, 0xa524, 0x4d8f, { 0xa5, 0x22, 0x53, 0x42, 0x3c, 0x3b, 0xfa, 0x60 } };
static HRESULT WINAPI cf_QI( IClassFactory *f, REFIID riid, void **ppv )
{
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IClassFactory )) { *ppv = f; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef( IClassFactory *f ) { return 2; }
static ULONG WINAPI cf_Release( IClassFactory *f ) { return 1; }
static HRESULT WINAPI cf_Create( IClassFactory *f, IUnknown *outer, REFIID riid, void **ppv )
{
    return CreateDataCache( outer, &CLSID_SgCache, riid, ppv );
}
static HRESULT WINAPI cf_Lock( IClassFactory *f, BOOL lock ) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_Create, cf_Lock };
static IClassFactory factory = { &cf_vtbl };

static int failures;
static void check( int ok, const char *what, long v ) { printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, v ); if (!ok) failures++; }

int main( void )
{
    HRESULT (WINAPI *pCreateEx)(REFCLSID, REFIID, DWORD, DWORD, ULONG, DWORD *, FORMATETC *, IAdviseSink *, DWORD *,
                                IOleClientSite *, IStorage *, void **) = (void *)GetProcAddress( LoadLibraryA( "ole32.dll" ), "OleCreateEx" );
    static const CLSID CLSID_Picture_Dib_ = { 0x00000316, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
    FORMATETC fmt[2] = { { CF_DIB, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }, { CF_BITMAP, NULL, DVASPECT_CONTENT, -1, TYMED_GDI } };
    DWORD advf[2] = { ADVF_PRIMEFIRST, ADVF_PRIMEFIRST }, conn[2] = { 0, 0 };
    IStorage *stg;
    IUnknown *obj = NULL;
    IOleCache *cache;
    IEnumSTATDATA *en;
    STATDATA sd;
    ULONG n = 0;
    HRESULT hr;

    if (!pCreateEx) { printf( "FAIL  export\nRESULT: FAIL\n" ); return 1; }
    CoInitialize( NULL );
    {
        DWORD cookie;
        CoRegisterClassObject( &CLSID_SgCache, (IUnknown *)&factory, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie );
    }
    StgCreateDocfile( NULL, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg );
    hr = pCreateEx( &CLSID_SgCache, &IID_IUnknown, 0, OLERENDER_FORMAT, 2, advf, fmt, NULL, NULL, NULL, stg, (void **)&obj );
    check( hr == S_OK && obj, "OleCreateEx, two formats to render", hr );
    if (obj && SUCCEEDED(IUnknown_QueryInterface( obj, &IID_IOleCache, (void **)&cache )))
    {
        if (SUCCEEDED(IOleCache_EnumCache( cache, &en )) && en)
        {
            while (IEnumSTATDATA_Next( en, 1, &sd, NULL ) == S_OK) n++;
            IEnumSTATDATA_Release( en );
        }
        IOleCache_Release( cache );
    }
    check( n >= 2, "both formats are in the object's cache", n );
    if (obj) IUnknown_Release( obj );
    obj = NULL;
    IStorage_Release( stg );

    StgCreateDocfile( NULL, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg );
    hr = pCreateEx( &CLSID_SgCache, &IID_IUnknown, 0, OLERENDER_DRAW, 0, NULL, NULL, &sink, NULL, NULL, stg, (void **)&obj );
    {
        IViewObject *view;
        IAdviseSink *got = NULL;
        DWORD aspects = 0, flags = 0;
        if (obj && SUCCEEDED(IUnknown_QueryInterface( obj, &IID_IViewObject, (void **)&view )))
        {
            IViewObject_GetAdvise( view, &aspects, &flags, &got );
            IViewObject_Release( view );
        }
        check( hr == S_OK && got == &sink, "OLERENDER_DRAW with an advise sink: the view advises it", hr );
    }
    if (obj) IUnknown_Release( obj );
    check( pCreateEx( &CLSID_SgCache, &IID_IUnknown, 0, OLERENDER_FORMAT, 0, NULL, NULL, NULL, NULL, NULL, stg, (void **)&obj ) == E_INVALIDARG,
           "OLERENDER_FORMAT without formats: E_INVALIDARG", 0 );
    IStorage_Release( stg );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
