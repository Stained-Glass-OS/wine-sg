/* reole-probe: the OLE container side of the rich edit control (Stained Glass
 * OS wine-sg 6002): IRichEditOle host names, storage hand-off, aspect, link
 * state, conversion, ActivateAs, data import (pictures), in-place deactivation;
 * the client site's SaveObject/GetMoniker/GetContainer and its in-place site. */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <stdio.h>

static const GUID IID_IRichEditOle_ = {0x00020d00,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID IID_IRichEditOleCallback_ = {0x00020d03,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const CLSID CLSID_probe_old = {0x6e5c9a10,0x1d1e,0x4c2b,{0x9f,0x1a,0x60,0x02,0x00,0x00,0x00,0x01}};
static const CLSID CLSID_probe_new = {0x6e5c9a10,0x1d1e,0x4c2b,{0x9f,0x1a,0x60,0x02,0x00,0x00,0x00,0x02}};

/* mingw's richole.h has no C call macros */
#define RE(m, i, ...) ((i)->lpVtbl->m( (i), ##__VA_ARGS__ ))
#define IRichEditOle_GetObject(i, ...) RE(GetObject, i, __VA_ARGS__)
#define IRichEditOle_SetHostNames(i, ...) RE(SetHostNames, i, __VA_ARGS__)
#define IRichEditOle_GetClientSite(i, ...) RE(GetClientSite, i, __VA_ARGS__)
#define IRichEditOle_InsertObject(i, ...) RE(InsertObject, i, __VA_ARGS__)
#define IRichEditOle_InPlaceDeactivate(i) RE(InPlaceDeactivate, i)
#define IRichEditOle_SetDvaspect(i, ...) RE(SetDvaspect, i, __VA_ARGS__)
#define IRichEditOle_SetLinkAvailable(i, ...) RE(SetLinkAvailable, i, __VA_ARGS__)
#define IRichEditOle_HandsOffStorage(i, ...) RE(HandsOffStorage, i, __VA_ARGS__)
#define IRichEditOle_SaveCompleted(i, ...) RE(SaveCompleted, i, __VA_ARGS__)
#define IRichEditOle_ConvertObject(i, ...) RE(ConvertObject, i, __VA_ARGS__)
#define IRichEditOle_ActivateAs(i, ...) RE(ActivateAs, i, __VA_ARGS__)
#define IRichEditOle_ImportDataObject(i, ...) RE(ImportDataObject, i, __VA_ARGS__)
#define IRichEditOle_GetObjectCount(i) RE(GetObjectCount, i)
#define IRichEditOle_Release(i) RE(Release, i)

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

/* ---- a test object: IOleObject, IPersistStorage, IOleInPlaceObject ---- */
struct testobj
{
    IOleObject IOleObject_iface;
    IPersistStorage IPersistStorage_iface;
    IOleInPlaceObject IOleInPlaceObject_iface;
    LONG ref;
    WCHAR app[64], obj[64];
    int saves, handsoff, save_completed, inplace_deactivated, closed;
    IStorage *completed_stg;
};
static struct testobj *obj_of( IOleObject *i ) { return CONTAINING_RECORD( i, struct testobj, IOleObject_iface ); }
static struct testobj *obj_of_ps( IPersistStorage *i ) { return CONTAINING_RECORD( i, struct testobj, IPersistStorage_iface ); }
static struct testobj *obj_of_ip( IOleInPlaceObject *i ) { return CONTAINING_RECORD( i, struct testobj, IOleInPlaceObject_iface ); }

static HRESULT WINAPI to_QI( IOleObject *i, REFIID riid, void **out )
{
    struct testobj *t = obj_of( i );
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IOleObject )) *out = &t->IOleObject_iface;
    else if (IsEqualIID( riid, &IID_IPersistStorage ) || IsEqualIID( riid, &IID_IPersist )) *out = &t->IPersistStorage_iface;
    else if (IsEqualIID( riid, &IID_IOleInPlaceObject ) || IsEqualIID( riid, &IID_IOleWindow )) *out = &t->IOleInPlaceObject_iface;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement( &t->ref );
    return S_OK;
}
static ULONG WINAPI to_AddRef( IOleObject *i ) { return InterlockedIncrement( &obj_of( i )->ref ); }
static ULONG WINAPI to_Release( IOleObject *i ) { return InterlockedDecrement( &obj_of( i )->ref ); }
static HRESULT WINAPI to_SetClientSite( IOleObject *i, IOleClientSite *s ) { return S_OK; }
static HRESULT WINAPI to_GetClientSite( IOleObject *i, IOleClientSite **s ) { *s = NULL; return E_NOTIMPL; }
static HRESULT WINAPI to_SetHostNames( IOleObject *i, LPCOLESTR app, LPCOLESTR obj )
{
    struct testobj *t = obj_of( i );
    lstrcpynW( t->app, app ? app : L"", 64 );
    lstrcpynW( t->obj, obj ? obj : L"", 64 );
    return S_OK;
}
static HRESULT WINAPI to_Close( IOleObject *i, DWORD opt ) { obj_of( i )->closed++; return S_OK; }
static HRESULT WINAPI to_SetMoniker( IOleObject *i, DWORD w, IMoniker *m ) { return E_NOTIMPL; }
static HRESULT WINAPI to_GetMoniker( IOleObject *i, DWORD a, DWORD w, IMoniker **m ) { *m = NULL; return E_NOTIMPL; }
static HRESULT WINAPI to_InitFromData( IOleObject *i, IDataObject *d, BOOL c, DWORD r ) { return E_NOTIMPL; }
static HRESULT WINAPI to_GetClipboardData( IOleObject *i, DWORD r, IDataObject **d ) { *d = NULL; return E_NOTIMPL; }
static HRESULT WINAPI to_DoVerb( IOleObject *i, LONG v, MSG *m, IOleClientSite *s, LONG l, HWND h, LPCRECT r ) { return E_NOTIMPL; }
static HRESULT WINAPI to_EnumVerbs( IOleObject *i, IEnumOLEVERB **e ) { return E_NOTIMPL; }
static HRESULT WINAPI to_Update( IOleObject *i ) { return S_OK; }
static HRESULT WINAPI to_IsUpToDate( IOleObject *i ) { return S_OK; }
static HRESULT WINAPI to_GetUserClassID( IOleObject *i, CLSID *c ) { *c = CLSID_probe_old; return S_OK; }
static HRESULT WINAPI to_GetUserType( IOleObject *i, DWORD f, LPOLESTR *t ) { return OLE_S_USEREG; }
static HRESULT WINAPI to_SetExtent( IOleObject *i, DWORD a, SIZEL *s ) { return S_OK; }
static HRESULT WINAPI to_GetExtent( IOleObject *i, DWORD a, SIZEL *s ) { s->cx = a == DVASPECT_ICON ? 800 : 2000; s->cy = 1000; return S_OK; }
static HRESULT WINAPI to_Advise( IOleObject *i, IAdviseSink *a, DWORD *c ) { *c = 1; return S_OK; }
static HRESULT WINAPI to_Unadvise( IOleObject *i, DWORD c ) { return S_OK; }
static HRESULT WINAPI to_EnumAdvise( IOleObject *i, IEnumSTATDATA **e ) { return E_NOTIMPL; }
static HRESULT WINAPI to_GetMiscStatus( IOleObject *i, DWORD a, DWORD *s ) { *s = 0; return S_OK; }
static HRESULT WINAPI to_SetColorScheme( IOleObject *i, LOGPALETTE *p ) { return E_NOTIMPL; }
static const IOleObjectVtbl to_vtbl = { to_QI, to_AddRef, to_Release, to_SetClientSite, to_GetClientSite,
    to_SetHostNames, to_Close, to_SetMoniker, to_GetMoniker, to_InitFromData, to_GetClipboardData, to_DoVerb,
    to_EnumVerbs, to_Update, to_IsUpToDate, to_GetUserClassID, to_GetUserType, to_SetExtent, to_GetExtent,
    to_Advise, to_Unadvise, to_EnumAdvise, to_GetMiscStatus, to_SetColorScheme };

static HRESULT WINAPI ps_QI( IPersistStorage *i, REFIID r, void **o ) { return to_QI( &obj_of_ps( i )->IOleObject_iface, r, o ); }
static ULONG WINAPI ps_AddRef( IPersistStorage *i ) { return to_AddRef( &obj_of_ps( i )->IOleObject_iface ); }
static ULONG WINAPI ps_Release( IPersistStorage *i ) { return to_Release( &obj_of_ps( i )->IOleObject_iface ); }
static HRESULT WINAPI ps_GetClassID( IPersistStorage *i, CLSID *c ) { *c = CLSID_probe_old; return S_OK; }
static HRESULT WINAPI ps_IsDirty( IPersistStorage *i ) { return S_OK; }
static HRESULT WINAPI ps_InitNew( IPersistStorage *i, IStorage *s ) { return S_OK; }
static HRESULT WINAPI ps_Load( IPersistStorage *i, IStorage *s ) { return S_OK; }
static HRESULT WINAPI ps_Save( IPersistStorage *i, IStorage *s, BOOL same ) { obj_of_ps( i )->saves++; return S_OK; }
static HRESULT WINAPI ps_SaveCompleted( IPersistStorage *i, IStorage *s )
{
    struct testobj *t = obj_of_ps( i );
    t->save_completed++;
    t->completed_stg = s;
    return S_OK;
}
static HRESULT WINAPI ps_HandsOffStorage( IPersistStorage *i ) { obj_of_ps( i )->handsoff++; return S_OK; }
static const IPersistStorageVtbl ps_vtbl = { ps_QI, ps_AddRef, ps_Release, ps_GetClassID, ps_IsDirty,
    ps_InitNew, ps_Load, ps_Save, ps_SaveCompleted, ps_HandsOffStorage };

static HRESULT WINAPI ip_QI( IOleInPlaceObject *i, REFIID r, void **o ) { return to_QI( &obj_of_ip( i )->IOleObject_iface, r, o ); }
static ULONG WINAPI ip_AddRef( IOleInPlaceObject *i ) { return to_AddRef( &obj_of_ip( i )->IOleObject_iface ); }
static ULONG WINAPI ip_Release( IOleInPlaceObject *i ) { return to_Release( &obj_of_ip( i )->IOleObject_iface ); }
static HRESULT WINAPI ip_GetWindow( IOleInPlaceObject *i, HWND *h ) { *h = NULL; return E_FAIL; }
static HRESULT WINAPI ip_CSHelp( IOleInPlaceObject *i, BOOL b ) { return S_OK; }
static HRESULT WINAPI ip_InPlaceDeactivate( IOleInPlaceObject *i ) { obj_of_ip( i )->inplace_deactivated++; return S_OK; }
static HRESULT WINAPI ip_UIDeactivate( IOleInPlaceObject *i ) { return S_OK; }
static HRESULT WINAPI ip_SetObjectRects( IOleInPlaceObject *i, LPCRECT a, LPCRECT b ) { return S_OK; }
static HRESULT WINAPI ip_ReactivateAndUndo( IOleInPlaceObject *i ) { return S_OK; }
static const IOleInPlaceObjectVtbl ip_vtbl = { ip_QI, ip_AddRef, ip_Release, ip_GetWindow, ip_CSHelp,
    ip_InPlaceDeactivate, ip_UIDeactivate, ip_SetObjectRects, ip_ReactivateAndUndo };

static void testobj_init( struct testobj *t )
{
    memset( t, 0, sizeof(*t) );
    t->IOleObject_iface.lpVtbl = &to_vtbl;
    t->IPersistStorage_iface.lpVtbl = &ps_vtbl;
    t->IOleInPlaceObject_iface.lpVtbl = &ip_vtbl;
    t->ref = 1;
}

/* ---- the host's callback ---- */
static int show_container_ui_true, show_container_ui_false, inplace_context;

static HRESULT WINAPI cb_QI( IRichEditOleCallback *i, REFIID r, void **o )
{
    if (IsEqualIID( r, &IID_IUnknown ) || IsEqualIID( r, &IID_IRichEditOleCallback_ )) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cb_AddRef( IRichEditOleCallback *i ) { return 2; }
static ULONG WINAPI cb_Release( IRichEditOleCallback *i ) { return 1; }
static HRESULT WINAPI cb_GetNewStorage( IRichEditOleCallback *i, IStorage **s ) { return E_NOTIMPL; }
static HRESULT WINAPI cb_GetInPlaceContext( IRichEditOleCallback *i, IOleInPlaceFrame **f, IOleInPlaceUIWindow **d, OLEINPLACEFRAMEINFO *fi )
{
    inplace_context++;
    *f = NULL; *d = NULL;
    return S_OK;
}
static HRESULT WINAPI cb_ShowContainerUI( IRichEditOleCallback *i, BOOL show )
{
    if (show) show_container_ui_true++; else show_container_ui_false++;
    return S_OK;
}
static HRESULT WINAPI cb_QueryInsertObject( IRichEditOleCallback *i, CLSID *c, IStorage *s, LONG cp ) { return S_OK; }
static HRESULT WINAPI cb_DeleteObject( IRichEditOleCallback *i, IOleObject *o ) { return S_OK; }
static HRESULT WINAPI cb_QueryAcceptData( IRichEditOleCallback *i, IDataObject *d, CLIPFORMAT *f, DWORD r, BOOL rl, HGLOBAL h ) { return S_OK; }
static HRESULT WINAPI cb_ContextSensitiveHelp( IRichEditOleCallback *i, BOOL e ) { return S_OK; }
static HRESULT WINAPI cb_GetClipboardData( IRichEditOleCallback *i, CHARRANGE *c, DWORD r, IDataObject **d ) { return E_NOTIMPL; }
static HRESULT WINAPI cb_GetDragDropEffect( IRichEditOleCallback *i, BOOL d, DWORD k, DWORD *e ) { return E_NOTIMPL; }
static HRESULT WINAPI cb_GetContextMenu( IRichEditOleCallback *i, WORD t, IOleObject *o, CHARRANGE *c, HMENU *m ) { return E_NOTIMPL; }
static const IRichEditOleCallbackVtbl cb_vtbl = { cb_QI, cb_AddRef, cb_Release, cb_GetNewStorage, cb_GetInPlaceContext,
    cb_ShowContainerUI, cb_QueryInsertObject, cb_DeleteObject, cb_QueryAcceptData, cb_ContextSensitiveHelp,
    cb_GetClipboardData, cb_GetDragDropEffect, cb_GetContextMenu };
static IRichEditOleCallback callback = { &cb_vtbl };

static IStorage *new_storage( void )
{
    ILockBytes *bytes;
    IStorage *stg = NULL;
    CreateILockBytesOnHGlobal( NULL, TRUE, &bytes );
    StgCreateDocfileOnILockBytes( bytes, STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &stg );
    ILockBytes_Release( bytes );
    return stg;
}

static LONG get_obj( IRichEditOle *re, LONG iob, REOBJECT *reo, DWORD flags )
{
    memset( reo, 0, sizeof(*reo) );
    reo->cbStruct = sizeof(*reo);
    return IRichEditOle_GetObject( re, iob, reo, flags );
}

int main( void )
{
    struct testobj t;
    IRichEditOle *re;
    IOleClientSite *site;
    IOleInPlaceSite *ipsite;
    IOleContainer *container = (void *)1;
    IMoniker *mk = (void *)1;
    IOleInPlaceFrame *frame;
    IOleInPlaceUIWindow *doc;
    OLEINPLACEFRAMEINFO fi = { sizeof(fi) };
    RECT pos, clip;
    REOBJECT reo;
    IStorage *stg, *stg2;
    HWND hwnd;
    HRESULT hr;
    LONG count;
    HKEY key;
    WCHAR text[64];

    OleInitialize( NULL );
    LoadLibraryA( "riched20.dll" );
    hwnd = CreateWindowExW( 0, L"RichEdit20W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 400, 300, 0, 0, 0, 0 );
    check( hwnd != NULL, "rich edit control created", GetLastError() );
    SendMessageW( hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&re );
    check( re != NULL, "EM_GETOLEINTERFACE", 0 );
    SendMessageW( hwnd, EM_SETOLECALLBACK, 0, (LPARAM)&callback );

    /* host names go to objects as they are inserted */
    hr = IRichEditOle_SetHostNames( re, "Probe App", "Probe Document" );
    check( hr == S_OK, "SetHostNames", hr );
    check( IRichEditOle_SetHostNames( re, NULL, "x" ) == E_INVALIDARG, "SetHostNames(NULL app) is E_INVALIDARG", 0 );

    testobj_init( &t );
    stg = new_storage();
    IRichEditOle_GetClientSite( re, &site );
    memset( &reo, 0, sizeof(reo) );
    reo.cbStruct = sizeof(reo);
    reo.cp = REO_CP_SELECTION;
    reo.clsid = CLSID_probe_old;
    reo.poleobj = &t.IOleObject_iface;
    reo.pstg = stg;
    reo.polesite = site;
    reo.dvaspect = DVASPECT_CONTENT;
    hr = IRichEditOle_InsertObject( re, &reo );
    check( hr == S_OK, "InsertObject", hr );
    check( !lstrcmpW( t.app, L"Probe App" ) && !lstrcmpW( t.obj, L"Probe Document" ),
           "the inserted object got the host names", t.app[0] );

    /* the client site */
    t.saves = 0;
    hr = IOleClientSite_SaveObject( site );
    check( hr == S_OK && t.saves == 1, "IOleClientSite::SaveObject saves the object to its storage", hr );
    hr = IOleClientSite_GetMoniker( site, OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_OBJREL, &mk );
    check( hr == E_NOTIMPL && !mk, "IOleClientSite::GetMoniker: no monikers (E_NOTIMPL, NULL)", hr );
    hr = IOleClientSite_GetContainer( site, &container );
    check( hr == E_NOINTERFACE && !container, "IOleClientSite::GetContainer: E_NOINTERFACE, NULL", hr );
    check( IOleClientSite_RequestNewObjectLayout( site ) == E_NOTIMPL, "RequestNewObjectLayout is E_NOTIMPL (not used)", 0 );

    /* the in-place site */
    hr = IOleClientSite_QueryInterface( site, &IID_IOleInPlaceSite, (void **)&ipsite );
    check( hr == S_OK, "IOleInPlaceSite", hr );
    check( IOleInPlaceSite_CanInPlaceActivate( ipsite ) == S_OK, "CanInPlaceActivate (content aspect)", 0 );
    check( IOleInPlaceSite_OnInPlaceActivate( ipsite ) == S_OK, "OnInPlaceActivate", 0 );
    get_obj( re, 0, &reo, REO_GETOBJ_NO_INTERFACES );
    check( reo.dwFlags & REO_INPLACEACTIVE, "the object is marked active in place", reo.dwFlags );
    hr = IOleInPlaceSite_GetWindowContext( ipsite, &frame, &doc, &pos, &clip, &fi );
    check( hr == S_OK && inplace_context == 1, "GetWindowContext asks the host (GetInPlaceContext)", hr );
    check( pos.right > pos.left && pos.bottom > pos.top, "GetWindowContext gives the object's rectangle", pos.right - pos.left );
    check( IOleInPlaceSite_OnUIActivate( ipsite ) == S_OK && show_container_ui_false == 1,
           "OnUIActivate hides the container's UI (ShowContainerUI(FALSE))", show_container_ui_false );
    check( IOleInPlaceSite_OnUIDeactivate( ipsite, FALSE ) == S_OK && show_container_ui_true == 1,
           "OnUIDeactivate shows the container's UI again", show_container_ui_true );
    hr = IRichEditOle_InPlaceDeactivate( re );
    check( hr == S_OK && t.inplace_deactivated == 1, "IRichEditOle::InPlaceDeactivate deactivates the active object", hr );
    check( IOleInPlaceSite_OnInPlaceDeactivate( ipsite ) == S_OK, "OnInPlaceDeactivate", 0 );
    get_obj( re, 0, &reo, REO_GETOBJ_NO_INTERFACES );
    check( !(reo.dwFlags & REO_INPLACEACTIVE), "the object is no longer active in place", reo.dwFlags );
    {
        SIZE none = {0, 0};
        check( IOleInPlaceSite_Scroll( ipsite, none ) == S_OK, "Scroll", 0 );
    }
    check( IOleInPlaceSite_DiscardUndoState( ipsite ) == S_OK, "DiscardUndoState", 0 );

    /* aspect */
    hr = IRichEditOle_SetDvaspect( re, 0, DVASPECT_ICON );
    get_obj( re, 0, &reo, REO_GETOBJ_NO_INTERFACES );
    check( hr == S_OK && reo.dvaspect == DVASPECT_ICON, "SetDvaspect(ICON)", reo.dvaspect );
    check( IOleInPlaceSite_CanInPlaceActivate( ipsite ) == S_FALSE, "an object shown as an icon is not activated in place", 0 );
    IRichEditOle_SetDvaspect( re, 0, DVASPECT_CONTENT );
    check( IRichEditOle_SetDvaspect( re, 0, 7 ) == E_INVALIDARG, "SetDvaspect(bad aspect) is E_INVALIDARG", 0 );
    check( IRichEditOle_SetLinkAvailable( re, 0, TRUE ) == E_INVALIDARG, "SetLinkAvailable on an embedded object: E_INVALIDARG", 0 );

    /* storage hand-off and save completion */
    hr = IRichEditOle_HandsOffStorage( re, 0 );
    get_obj( re, 0, &reo, REO_GETOBJ_PSTG );
    check( hr == S_OK && t.handsoff == 1 && !reo.pstg, "HandsOffStorage: the object lets go of its storage", (long)(LONG_PTR)reo.pstg );
    stg2 = new_storage();
    hr = IRichEditOle_SaveCompleted( re, 0, stg2 );
    get_obj( re, 0, &reo, REO_GETOBJ_PSTG );
    check( hr == S_OK && t.completed_stg == stg2 && reo.pstg == stg2, "SaveCompleted gives the object its new storage", hr );
    if (reo.pstg) IStorage_Release( reo.pstg );
    check( IRichEditOle_HandsOffStorage( re, 5 ) == E_INVALIDARG, "HandsOffStorage(bad index) is E_INVALIDARG", 0 );

    /* conversion: the storage names the new class, the object is loaded again */
    t.closed = 0;
    hr = IRichEditOle_ConvertObject( re, 0, &CLSID_probe_new, "Probe Type" );
    get_obj( re, 0, &reo, REO_GETOBJ_POLEOBJ );
    check( hr == S_OK && IsEqualCLSID( &reo.clsid, &CLSID_probe_new ), "ConvertObject: the object's class is the new one", hr );
    check( reo.poleobj && reo.poleobj != &t.IOleObject_iface && t.closed, "ConvertObject: the object was loaded again", t.closed );
    if (reo.poleobj) IOleObject_Release( reo.poleobj );

    /* ActivateAs: objects of the class are loaded again, treated as the new one */
    RegCreateKeyW( HKEY_CLASSES_ROOT, L"CLSID\\{6e5c9a10-1d1e-4c2b-9f1a-600200000002}", &key );
    RegCloseKey( key );
    get_obj( re, 0, &reo, REO_GETOBJ_POLEOBJ );
    {
        IOleObject *before = reo.poleobj;
        hr = IRichEditOle_ActivateAs( re, &CLSID_probe_new, &CLSID_probe_old );
        if (before) IOleObject_Release( before );
        get_obj( re, 0, &reo, REO_GETOBJ_POLEOBJ );
        check( hr == S_OK && reo.poleobj && reo.poleobj != before, "ActivateAs loads the class's objects again", hr );
        if (reo.poleobj) IOleObject_Release( reo.poleobj );
    }

    /* data import: text, and a picture (as a picture object) */
    SendMessageW( hwnd, WM_SETTEXT, 0, (LPARAM)L"" );
    OpenClipboard( hwnd );
    EmptyClipboard();
    {
        HGLOBAL g = GlobalAlloc( GMEM_MOVEABLE, 16 );
        lstrcpyW( GlobalLock( g ), L"pasted" );
        GlobalUnlock( g );
        SetClipboardData( CF_UNICODETEXT, g );
    }
    CloseClipboard();
    hr = IRichEditOle_ImportDataObject( re, NULL, 0, NULL );
    GetWindowTextW( hwnd, text, 64 );
    check( hr == S_OK && !lstrcmpW( text, L"pasted" ), "ImportDataObject (the clipboard's text)", hr );

    OpenClipboard( hwnd );
    EmptyClipboard();
    {
        HDC hdc = GetDC( NULL );
        HBITMAP bmp = CreateCompatibleBitmap( hdc, 32, 24 );
        ReleaseDC( NULL, hdc );
        SetClipboardData( CF_BITMAP, bmp );
    }
    CloseClipboard();
    count = IRichEditOle_GetObjectCount( re );
    hr = IRichEditOle_ImportDataObject( re, NULL, 0, NULL );
    check( hr == S_OK && IRichEditOle_GetObjectCount( re ) == count + 1, "ImportDataObject (a picture) inserts a picture object", hr );
    count = IRichEditOle_GetObjectCount( re );
    SendMessageW( hwnd, WM_PASTE, 0, 0 );
    check( IRichEditOle_GetObjectCount( re ) == count + 1, "WM_PASTE of a picture inserts a picture object", IRichEditOle_GetObjectCount( re ) );
    hr = IRichEditOle_ImportDataObject( re, NULL, CF_UNICODETEXT, NULL );
    check( hr != S_OK, "ImportDataObject in a format the data does not have fails", hr );

    IOleInPlaceSite_Release( ipsite );
    IOleClientSite_Release( site );
    IRichEditOle_Release( re );
    DestroyWindow( hwnd );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
