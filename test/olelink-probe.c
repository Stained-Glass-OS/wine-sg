/* olelink-probe: OLE linked objects (Stained Glass OS wine-sg 6005):
 * OleCreateLink / OleCreateLinkToFile / OleCreateLinkFromData, IOleLink
 * (source moniker and display name, binding, update, update options) and a
 * link's storage (the source moniker kept across OleSave / OleLoad). */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

/* ---- a link source: IOleObject + IDataObject, in this process ---- */
struct source
{
    IOleObject IOleObject_iface;
    IDataObject IDataObject_iface;
    LONG ref;
    int updates, getdata, advised;
};
static struct source src;

static HRESULT WINAPI so_QI( IOleObject *i, REFIID riid, void **out )
{
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IOleObject )) *out = &src.IOleObject_iface;
    else if (IsEqualIID( riid, &IID_IDataObject )) *out = &src.IDataObject_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement( &src.ref );
    return S_OK;
}
static ULONG WINAPI so_AddRef( IOleObject *i ) { return InterlockedIncrement( &src.ref ); }
static ULONG WINAPI so_Release( IOleObject *i ) { return InterlockedDecrement( &src.ref ); }
static HRESULT WINAPI so_SetClientSite( IOleObject *i, IOleClientSite *s ) { return S_OK; }
static HRESULT WINAPI so_GetClientSite( IOleObject *i, IOleClientSite **s ) { *s = NULL; return E_NOTIMPL; }
static HRESULT WINAPI so_SetHostNames( IOleObject *i, LPCOLESTR a, LPCOLESTR o ) { return S_OK; }
static HRESULT WINAPI so_Close( IOleObject *i, DWORD opt ) { return S_OK; }
static HRESULT WINAPI so_SetMoniker( IOleObject *i, DWORD w, IMoniker *m ) { return S_OK; }
static HRESULT WINAPI so_GetMoniker( IOleObject *i, DWORD a, DWORD w, IMoniker **m ) { *m = NULL; return E_NOTIMPL; }
static HRESULT WINAPI so_InitFromData( IOleObject *i, IDataObject *d, BOOL c, DWORD r ) { return E_NOTIMPL; }
static HRESULT WINAPI so_GetClipboardData( IOleObject *i, DWORD r, IDataObject **d ) { *d = NULL; return E_NOTIMPL; }
static HRESULT WINAPI so_DoVerb( IOleObject *i, LONG v, MSG *m, IOleClientSite *s, LONG l, HWND h, LPCRECT r ) { return S_OK; }
static HRESULT WINAPI so_EnumVerbs( IOleObject *i, IEnumOLEVERB **e ) { return E_NOTIMPL; }
static HRESULT WINAPI so_Update( IOleObject *i ) { src.updates++; return S_OK; }
static HRESULT WINAPI so_IsUpToDate( IOleObject *i ) { return S_OK; }
static HRESULT WINAPI so_GetUserClassID( IOleObject *i, CLSID *c ) { *c = CLSID_NULL; return S_OK; }
static HRESULT WINAPI so_GetUserType( IOleObject *i, DWORD f, LPOLESTR *t ) { return OLE_S_USEREG; }
static HRESULT WINAPI so_SetExtent( IOleObject *i, DWORD a, SIZEL *s ) { return S_OK; }
static HRESULT WINAPI so_GetExtent( IOleObject *i, DWORD a, SIZEL *s ) { s->cx = 1000; s->cy = 500; return S_OK; }
static HRESULT WINAPI so_Advise( IOleObject *i, IAdviseSink *a, DWORD *c ) { src.advised++; *c = 7; return S_OK; }
static HRESULT WINAPI so_Unadvise( IOleObject *i, DWORD c ) { return S_OK; }
static HRESULT WINAPI so_EnumAdvise( IOleObject *i, IEnumSTATDATA **e ) { return E_NOTIMPL; }
static HRESULT WINAPI so_GetMiscStatus( IOleObject *i, DWORD a, DWORD *s ) { *s = 0; return S_OK; }
static HRESULT WINAPI so_SetColorScheme( IOleObject *i, LOGPALETTE *p ) { return E_NOTIMPL; }
static const IOleObjectVtbl so_vtbl = { so_QI, so_AddRef, so_Release, so_SetClientSite, so_GetClientSite,
    so_SetHostNames, so_Close, so_SetMoniker, so_GetMoniker, so_InitFromData, so_GetClipboardData, so_DoVerb,
    so_EnumVerbs, so_Update, so_IsUpToDate, so_GetUserClassID, so_GetUserType, so_SetExtent, so_GetExtent,
    so_Advise, so_Unadvise, so_EnumAdvise, so_GetMiscStatus, so_SetColorScheme };

static HRESULT WINAPI sd_QI( IDataObject *i, REFIID r, void **o ) { return so_QI( &src.IOleObject_iface, r, o ); }
static ULONG WINAPI sd_AddRef( IDataObject *i ) { return so_AddRef( NULL ); }
static ULONG WINAPI sd_Release( IDataObject *i ) { return so_Release( NULL ); }
static HRESULT WINAPI sd_GetData( IDataObject *i, FORMATETC *f, STGMEDIUM *m )
{
    HDC hdc;
    src.getdata++;
    if (f->cfFormat != CF_ENHMETAFILE) return DV_E_FORMATETC;
    hdc = CreateEnhMetaFileW( NULL, NULL, NULL, NULL );
    Rectangle( hdc, 0, 0, 10, 10 );
    m->tymed = TYMED_ENHMF;
    m->hEnhMetaFile = CloseEnhMetaFile( hdc );
    m->pUnkForRelease = NULL;
    return S_OK;
}
static HRESULT WINAPI sd_GetDataHere( IDataObject *i, FORMATETC *f, STGMEDIUM *m ) { return E_NOTIMPL; }
static HRESULT WINAPI sd_QueryGetData( IDataObject *i, FORMATETC *f ) { return f->cfFormat == CF_ENHMETAFILE ? S_OK : DV_E_FORMATETC; }
static HRESULT WINAPI sd_GetCanonical( IDataObject *i, FORMATETC *a, FORMATETC *b ) { return E_NOTIMPL; }
static HRESULT WINAPI sd_SetData( IDataObject *i, FORMATETC *f, STGMEDIUM *m, BOOL r ) { return E_NOTIMPL; }
static HRESULT WINAPI sd_EnumFormatEtc( IDataObject *i, DWORD d, IEnumFORMATETC **e ) { return E_NOTIMPL; }
static HRESULT WINAPI sd_DAdvise( IDataObject *i, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c ) { *c = 9; return S_OK; }
static HRESULT WINAPI sd_DUnadvise( IDataObject *i, DWORD c ) { return S_OK; }
static HRESULT WINAPI sd_EnumDAdvise( IDataObject *i, IEnumSTATDATA **e ) { return E_NOTIMPL; }
static const IDataObjectVtbl sd_vtbl = { sd_QI, sd_AddRef, sd_Release, sd_GetData, sd_GetDataHere, sd_QueryGetData,
    sd_GetCanonical, sd_SetData, sd_EnumFormatEtc, sd_DAdvise, sd_DUnadvise, sd_EnumDAdvise };

/* ---- a data object offering a "Link Source" ---- */
static IStream *link_source_stream;
static CLIPFORMAT cf_link_source;
static HRESULT WINAPI ld_QI( IDataObject *i, REFIID r, void **o )
{
    if (IsEqualIID( r, &IID_IUnknown ) || IsEqualIID( r, &IID_IDataObject )) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ld_AddRef( IDataObject *i ) { return 2; }
static ULONG WINAPI ld_Release( IDataObject *i ) { return 1; }
static HRESULT WINAPI ld_GetData( IDataObject *i, FORMATETC *f, STGMEDIUM *m )
{
    if (f->cfFormat != cf_link_source || !(f->tymed & TYMED_ISTREAM)) return DV_E_FORMATETC;
    m->tymed = TYMED_ISTREAM;
    m->pstm = link_source_stream;
    IStream_AddRef( m->pstm );
    m->pUnkForRelease = NULL;
    return S_OK;
}
static HRESULT WINAPI ld_QueryGetData( IDataObject *i, FORMATETC *f ) { return f->cfFormat == cf_link_source ? S_OK : DV_E_FORMATETC; }
static const IDataObjectVtbl ld_vtbl = { ld_QI, ld_AddRef, ld_Release, ld_GetData, sd_GetDataHere, ld_QueryGetData,
    sd_GetCanonical, sd_SetData, sd_EnumFormatEtc, sd_DAdvise, sd_DUnadvise, sd_EnumDAdvise };
static IDataObject link_data = { &ld_vtbl };

static IStorage *new_storage( void )
{
    ILockBytes *bytes;
    IStorage *stg = NULL;
    CreateILockBytesOnHGlobal( NULL, TRUE, &bytes );
    StgCreateDocfileOnILockBytes( bytes, STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &stg );
    ILockBytes_Release( bytes );
    return stg;
}

int main( void )
{
    IMoniker *pmk, *got = NULL, *fmk;
    IOleObject *obj = NULL, *obj2 = NULL;
    IOleLink *link = NULL, *link2 = NULL;
    IPersistStorage *ps;
    IUnknown *unk = (void *)1, *handler;
    IStorage *stg, *stg2;
    LPOLESTR name = NULL;
    DWORD opt;
    HRESULT hr;

    OleInitialize( NULL );
    src.IOleObject_iface.lpVtbl = &so_vtbl;
    src.IDataObject_iface.lpVtbl = &sd_vtbl;
    src.ref = 1;

    /* a link to a source in this process (a pointer moniker binds to it) */
    CreatePointerMoniker( (IUnknown *)&src.IOleObject_iface, &pmk );
    stg = new_storage();
    hr = OleCreateLink( pmk, &IID_IOleObject, OLERENDER_DRAW, NULL, NULL, stg, (void **)&obj );
    check( hr == S_OK && obj, "OleCreateLink (draw)", hr );
    if (!obj) { printf( "RESULT: FAIL\n" ); return 1; }
    hr = IOleObject_QueryInterface( obj, &IID_IOleLink, (void **)&link );
    check( hr == S_OK, "the object is an IOleLink", hr );
    check( src.updates >= 1, "creating it brought it up to date from the source (Update)", src.updates );
    check( src.getdata >= 1, "the presentation came from the source's data", src.getdata );
    hr = IOleLink_GetSourceMoniker( link, &got );
    check( hr == S_OK && got && IMoniker_IsEqual( got, pmk ) == S_OK, "GetSourceMoniker is the source's moniker", hr );
    if (got) IMoniker_Release( got );
    hr = IOleLink_GetBoundSource( link, &unk );
    check( hr == S_OK && unk == (IUnknown *)&src.IOleObject_iface, "GetBoundSource is the source", hr );
    if (hr == S_OK) IUnknown_Release( unk );
    hr = IOleLink_UnbindSource( link );
    unk = (void *)1;
    check( hr == S_OK && IOleLink_GetBoundSource( link, &unk ) == E_FAIL && !unk, "UnbindSource lets go of it", hr );
    src.updates = 0;
    hr = IOleLink_Update( link, NULL );
    check( hr == S_OK && src.updates == 1, "IOleLink::Update binds and updates the source", src.updates );
    IOleLink_UnbindSource( link );
    hr = IOleLink_BindIfRunning( link );
    check( hr == S_OK && IOleLink_GetBoundSource( link, &unk ) == S_OK, "BindIfRunning binds a running source", hr );
    if (unk) IUnknown_Release( unk );
    hr = IOleLink_BindToSource( link, 0, NULL );
    check( hr == S_OK, "BindToSource (bound already)", hr );
    check( IOleObject_DoVerb( obj, OLEIVERB_SHOW, NULL, NULL, 0, NULL, NULL ) == S_OK, "DoVerb goes to the source", 0 );

    check( IOleLink_SetUpdateOptions( link, 5 ) == E_INVALIDARG, "SetUpdateOptions(bad) is E_INVALIDARG", 0 );
    hr = IOleLink_SetUpdateOptions( link, OLEUPDATE_ONCALL );
    IOleLink_GetUpdateOptions( link, &opt );
    check( hr == S_OK && opt == OLEUPDATE_ONCALL, "Set/GetUpdateOptions", opt );
    IOleLink_Release( link );
    IOleObject_Release( obj );
    IStorage_Release( stg );

    /* a link to a file, kept in its storage */
    stg = new_storage();
    obj = NULL;
    hr = OleCreateLinkToFile( L"C:\\linked\\report.txt", &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, (void **)&obj );
    check( hr == S_OK && obj, "OleCreateLinkToFile (no rendering: no binding)", hr );
    IOleObject_QueryInterface( obj, &IID_IOleLink, (void **)&link );
    hr = IOleLink_GetSourceDisplayName( link, &name );
    check( hr == S_OK && name && !lstrcmpiW( name, L"C:\\linked\\report.txt" ), "GetSourceDisplayName", hr );
    CoTaskMemFree( name );
    IOleLink_SetUpdateOptions( link, OLEUPDATE_ONCALL );
    stg2 = new_storage();
    IOleObject_QueryInterface( obj, &IID_IPersistStorage, (void **)&ps );
    hr = OleSave( ps, stg2, FALSE );
    check( hr == S_OK, "OleSave of a link", hr );
    IPersistStorage_SaveCompleted( ps, NULL );
    IPersistStorage_Release( ps );
    hr = OleLoad( stg2, &IID_IOleObject, NULL, (void **)&obj2 );
    check( hr == S_OK && obj2, "OleLoad of the link's storage", hr );
    if (obj2)
    {
        hr = IOleObject_QueryInterface( obj2, &IID_IOleLink, (void **)&link2 );
        check( hr == S_OK, "the loaded object is a link", hr );
        if (link2)
        {
            name = NULL;
            hr = IOleLink_GetSourceDisplayName( link2, &name );
            check( hr == S_OK && name && !lstrcmpiW( name, L"C:\\linked\\report.txt" ), "and keeps its source", hr );
            CoTaskMemFree( name );
            opt = 0;
            IOleLink_GetUpdateOptions( link2, &opt );
            check( opt == OLEUPDATE_ONCALL, "and its update option", opt );
            /* a display name is parsed, which needs the file there */
            CreateDirectoryW( L"C:\\linked", NULL );
            CloseHandle( CreateFileW( L"C:\\linked\\other.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
            hr = IOleLink_SetSourceDisplayName( link2, L"C:\\linked\\other.txt" );
            name = NULL;
            IOleLink_GetSourceDisplayName( link2, &name );
            check( hr == S_OK && name && !lstrcmpiW( name, L"C:\\linked\\other.txt" ), "SetSourceDisplayName", hr );
            CoTaskMemFree( name );
            IOleLink_Release( link2 );
        }
        IOleObject_Release( obj2 );
    }
    IOleLink_Release( link );
    IOleObject_Release( obj );

    /* an embedding is no link */
    OleCreateDefaultHandler( &CLSID_NULL, NULL, &IID_IUnknown, (void **)&handler );
    check( IUnknown_QueryInterface( handler, &IID_IOleLink, (void **)&link ) == E_NOINTERFACE, "an embedding has no IOleLink", 0 );
    IUnknown_Release( handler );

    /* from data that names its link source */
    cf_link_source = RegisterClipboardFormatW( L"Link Source" );
    CreateStreamOnHGlobal( NULL, TRUE, &link_source_stream );
    CreateFileMoniker( L"C:\\linked\\sheet.dat", &fmk );
    {
        IPersistStream *pst;
        IMoniker_QueryInterface( fmk, &IID_IPersistStream, (void **)&pst );
        OleSaveToStream( pst, link_source_stream );
        IStream_Write( link_source_stream, &CLSID_NULL, sizeof(CLSID), NULL );
        IPersistStream_Release( pst );
    }
    obj = NULL;
    hr = OleCreateLinkFromData( &link_data, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, new_storage(), (void **)&obj );
    check( hr == S_OK && obj, "OleCreateLinkFromData (a Link Source)", hr );
    if (obj)
    {
        link = NULL;
        IOleObject_QueryInterface( obj, &IID_IOleLink, (void **)&link );
        name = NULL;
        if (link) IOleLink_GetSourceDisplayName( link, &name );
        check( link && name && !lstrcmpiW( name, L"C:\\linked\\sheet.dat" ), "it links to the data's source", (long)(LONG_PTR)link );
        CoTaskMemFree( name );
        if (link) IOleLink_Release( link );
        IOleObject_Release( obj );
    }

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
