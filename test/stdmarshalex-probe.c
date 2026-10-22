/* stdmarshalex-probe: CoGetStdMarshalEx, the standard marshaler aggregated into
 * an object (Stained Glass OS wine-sg 6012). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

#ifndef SMEXF_SERVER
#define SMEXF_SERVER  1
#define SMEXF_HANDLER 2
#endif
static const CLSID CLSID_StdMarshal_ = {0x00000017,0,0,{0xc0,0,0,0,0,0,0,0x46}};

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

/* an outer object that aggregates the standard marshaler */
static IUnknown outer;
static IUnknown *inner;
static LONG outer_refs = 1;

static HRESULT WINAPI outer_QI( IUnknown *iface, REFIID riid, void **obj )
{
    if (IsEqualIID( riid, &IID_IUnknown )) { *obj = iface; IUnknown_AddRef( iface ); return S_OK; }
    if (IsEqualIID( riid, &IID_IMarshal ) && inner) return IUnknown_QueryInterface( inner, riid, obj );
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI outer_AddRef( IUnknown *iface ) { return InterlockedIncrement( &outer_refs ); }
static ULONG WINAPI outer_Release( IUnknown *iface ) { return InterlockedDecrement( &outer_refs ); }
static const IUnknownVtbl outer_vtbl = { outer_QI, outer_AddRef, outer_Release };

int main( void )
{
    IMarshal *marshal = NULL;
    IUnknown *unk = NULL, *inner2 = (void *)1;
    CLSID clsid;
    IStream *stream;
    HRESULT hr;
    LONG before;

    CoInitialize( NULL );
    outer.lpVtbl = (IUnknownVtbl *)&outer_vtbl;

    hr = CoGetStdMarshalEx( &outer, SMEXF_SERVER, &inner );
    check( hr == S_OK && inner, "CoGetStdMarshalEx(SMEXF_SERVER)", hr );
    if (!inner) { printf( "RESULT: FAIL\n" ); return 1; }

    hr = IUnknown_QueryInterface( &outer, &IID_IMarshal, (void **)&marshal );
    check( hr == S_OK && marshal, "the outer object's IMarshal is the aggregated one", hr );
    before = outer_refs;
    IMarshal_QueryInterface( marshal, &IID_IUnknown, (void **)&unk );
    check( unk == &outer, "IMarshal's IUnknown is the outer object's (delegation)", (long)(ULONG_PTR)unk );
    check( outer_refs == before + 1, "and its references are the outer object's", outer_refs - before );
    if (unk) IUnknown_Release( unk );
    hr = IMarshal_GetUnmarshalClass( marshal, &IID_IUnknown, &outer, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &clsid );
    check( hr == S_OK && IsEqualCLSID( &clsid, &CLSID_StdMarshal_ ), "GetUnmarshalClass is the standard marshaler's", hr );
    CreateStreamOnHGlobal( NULL, TRUE, &stream );
    hr = IMarshal_MarshalInterface( marshal, stream, &IID_IUnknown, &outer, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL );
    check( hr == S_OK, "MarshalInterface of the outer object", hr );
    IStream_Release( stream );
    IMarshal_Release( marshal );
    check( IUnknown_Release( inner ) == 0, "the inner object goes with its last reference", 0 );

    hr = CoGetStdMarshalEx( &outer, SMEXF_HANDLER, &inner2 );
    check( hr == S_OK && inner2, "CoGetStdMarshalEx(SMEXF_HANDLER)", hr );
    if (hr == S_OK) IUnknown_Release( inner2 );
    inner2 = (void *)1;
    check( CoGetStdMarshalEx( &outer, 7, &inner2 ) == E_INVALIDARG && !inner2, "a bad flag: E_INVALIDARG, NULL", 0 );
    check( CoGetStdMarshalEx( NULL, SMEXF_SERVER, &inner2 ) == E_INVALIDARG, "no outer object: E_INVALIDARG", 0 );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
