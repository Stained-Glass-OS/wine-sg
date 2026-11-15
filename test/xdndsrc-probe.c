/* Wine as an XDND source (wine-sg 7787): the Wine side of test/xdndsrc-gate.sh.
 *
 *   xdndsrc-probe.exe source X Y files PATH...   a window; a press in it starts DoDragDrop
 *                                                 with CF_HDROP of PATHs (and a private format)
 *   xdndsrc-probe.exe source X Y text TEXT        ... with CF_UNICODETEXT
 *   xdndsrc-probe.exe target X Y W H              an OLE drop target window
 *
 * Every line is written, flushed, to the file named by XDNDSRC_LOG: "RESULT n hr=0x... effect=N" after each drag,
 * "WINEDROP ole=1 files=..." when the target takes a drop ("ole=1": the private
 * format came along, so the drop came through OLE, not through X). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <stdio.h>

static UINT cf_private;
static int text_mode;
static WCHAR **paths;
static int npaths;
static WCHAR *text;

/* to the file named by XDNDSRC_LOG (a program started in a desktop has no stdout) */
static void out( const char *fmt, ... )
{
    static FILE *f;
    char name[MAX_PATH];
    va_list args;

    if (!f && GetEnvironmentVariableA( "XDNDSRC_LOG", name, sizeof(name) )) f = fopen( name, "ab" );
    va_start( args, fmt );
    vfprintf( f ? f : stdout, fmt, args );
    va_end( args );
    fflush( f ? f : stdout );
}

/* the data object */
typedef struct { IDataObject IDataObject_iface; LONG ref; } dataobj;

static HRESULT WINAPI do_QueryInterface( IDataObject *iface, REFIID riid, void **obj )
{
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IDataObject ))
    {
        *obj = iface;
        IDataObject_AddRef( iface );
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI do_AddRef( IDataObject *iface ) { return InterlockedIncrement( &((dataobj *)iface)->ref ); }
static ULONG WINAPI do_Release( IDataObject *iface ) { return InterlockedDecrement( &((dataobj *)iface)->ref ); }

static BOOL offered( UINT cf )
{
    if (cf == cf_private) return TRUE;
    return text_mode ? cf == CF_UNICODETEXT : cf == CF_HDROP;
}

static HRESULT WINAPI do_GetData( IDataObject *iface, FORMATETC *fe, STGMEDIUM *med )
{
    HGLOBAL mem;
    char *p;
    size_t size;
    int i;

    if (!offered( fe->cfFormat ) || !(fe->tymed & TYMED_HGLOBAL)) return DV_E_FORMATETC;
    if (fe->cfFormat == cf_private)
    {
        mem = GlobalAlloc( GMEM_MOVEABLE, 2 );
        p = GlobalLock( mem );
        p[0] = 'x'; p[1] = 0;
    }
    else if (fe->cfFormat == CF_UNICODETEXT)
    {
        size = (wcslen( text ) + 1) * sizeof(WCHAR);
        mem = GlobalAlloc( GMEM_MOVEABLE, size );
        p = GlobalLock( mem );
        memcpy( p, text, size );
    }
    else
    {
        DROPFILES *df;
        WCHAR *w;

        size = sizeof(DROPFILES) + sizeof(WCHAR);
        for (i = 0; i < npaths; i++) size += (wcslen( paths[i] ) + 1) * sizeof(WCHAR);
        mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, size );
        p = GlobalLock( mem );
        df = (DROPFILES *)p;
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        w = (WCHAR *)(p + sizeof(DROPFILES));
        for (i = 0; i < npaths; i++)
        {
            wcscpy( w, paths[i] );
            w += wcslen( paths[i] ) + 1;
        }
        *w = 0;
    }
    GlobalUnlock( mem );
    med->tymed = TYMED_HGLOBAL;
    med->hGlobal = mem;
    med->pUnkForRelease = NULL;
    return S_OK;
}
static HRESULT WINAPI do_GetDataHere( IDataObject *iface, FORMATETC *fe, STGMEDIUM *med ) { return E_NOTIMPL; }
static HRESULT WINAPI do_QueryGetData( IDataObject *iface, FORMATETC *fe )
{
    return offered( fe->cfFormat ) && (fe->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_FORMATETC;
}
static HRESULT WINAPI do_GetCanonicalFormatEtc( IDataObject *iface, FORMATETC *in, FORMATETC *out )
{
    out->ptd = NULL;
    return E_NOTIMPL;
}
static HRESULT WINAPI do_SetData( IDataObject *iface, FORMATETC *fe, STGMEDIUM *med, BOOL release ) { return E_NOTIMPL; }
static HRESULT WINAPI do_EnumFormatEtc( IDataObject *iface, DWORD dir, IEnumFORMATETC **ret )
{
    FORMATETC fe[2] = { { 0, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL },
                        { 0, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL } };
    if (dir != DATADIR_GET) return E_NOTIMPL;
    fe[0].cfFormat = text_mode ? CF_UNICODETEXT : CF_HDROP;
    fe[1].cfFormat = cf_private;
    return SHCreateStdEnumFmtEtc( 2, fe, ret );
}
static HRESULT WINAPI do_DAdvise( IDataObject *iface, FORMATETC *fe, DWORD f, IAdviseSink *s, DWORD *c ) { return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT WINAPI do_DUnadvise( IDataObject *iface, DWORD c ) { return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT WINAPI do_EnumDAdvise( IDataObject *iface, IEnumSTATDATA **e ) { return OLE_E_ADVISENOTSUPPORTED; }

static IDataObjectVtbl dataobj_vtbl =
{
    do_QueryInterface, do_AddRef, do_Release, do_GetData, do_GetDataHere, do_QueryGetData,
    do_GetCanonicalFormatEtc, do_SetData, do_EnumFormatEtc, do_DAdvise, do_DUnadvise, do_EnumDAdvise
};
static dataobj the_data = { { &dataobj_vtbl }, 1 };

/* the drop source */
static HRESULT WINAPI ds_QueryInterface( IDropSource *iface, REFIID riid, void **obj )
{
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IDropSource ))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI ds_AddRef( IDropSource *iface ) { return 2; }
static ULONG WINAPI ds_Release( IDropSource *iface ) { return 1; }
static HRESULT WINAPI ds_QueryContinueDrag( IDropSource *iface, BOOL esc, DWORD keys )
{
    if (esc) return DRAGDROP_S_CANCEL;
    if (!(keys & MK_LBUTTON)) return DRAGDROP_S_DROP;
    return S_OK;
}
static HRESULT WINAPI ds_GiveFeedback( IDropSource *iface, DWORD effect ) { return DRAGDROP_S_USEDEFAULTCURSORS; }
static IDropSourceVtbl dropsource_vtbl = { ds_QueryInterface, ds_AddRef, ds_Release, ds_QueryContinueDrag, ds_GiveFeedback };
static IDropSource the_source = { &dropsource_vtbl };

/* the drop target */
static BOOL target_ole;
static HRESULT WINAPI dt_QueryInterface( IDropTarget *iface, REFIID riid, void **obj )
{
    if (IsEqualIID( riid, &IID_IUnknown ) || IsEqualIID( riid, &IID_IDropTarget ))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI dt_AddRef( IDropTarget *iface ) { return 2; }
static ULONG WINAPI dt_Release( IDropTarget *iface ) { return 1; }
static HRESULT WINAPI dt_DragEnter( IDropTarget *iface, IDataObject *obj, DWORD keys, POINTL pt, DWORD *effect )
{
    FORMATETC fe = { cf_private, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    target_ole = IDataObject_QueryGetData( obj, &fe ) == S_OK;
    out( "WINEENTER ole=%d\n", target_ole );
    *effect &= DROPEFFECT_COPY;
    return S_OK;
}
static HRESULT WINAPI dt_DragOver( IDropTarget *iface, DWORD keys, POINTL pt, DWORD *effect )
{
    *effect &= DROPEFFECT_COPY;
    return S_OK;
}
static HRESULT WINAPI dt_DragLeave( IDropTarget *iface ) { out( "WINELEAVE\n" ); return S_OK; }
static HRESULT WINAPI dt_Drop( IDropTarget *iface, IDataObject *obj, DWORD keys, POINTL pt, DWORD *effect )
{
    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM med;
    WCHAR name[MAX_PATH];
    UINT i, count;

    out( "WINEDROP ole=%d files=", target_ole );
    if (IDataObject_GetData( obj, &fe, &med ) == S_OK)
    {
        count = DragQueryFileW( med.hGlobal, ~0u, NULL, 0 );
        for (i = 0; i < count; i++)
        {
            char utf8[MAX_PATH * 3];
            DragQueryFileW( med.hGlobal, i, name, MAX_PATH );
            WideCharToMultiByte( CP_UTF8, 0, name, -1, utf8, sizeof(utf8), NULL, NULL );
            out( "%s%s", i ? "|" : "", utf8 );
        }
        ReleaseStgMedium( &med );
    }
    out( "\n" );
    *effect &= DROPEFFECT_COPY;
    return S_OK;
}
static IDropTargetVtbl droptarget_vtbl = { dt_QueryInterface, dt_AddRef, dt_Release, dt_DragEnter, dt_DragOver, dt_DragLeave, dt_Drop };
static IDropTarget the_target = { &droptarget_vtbl };

static int drags;

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_LBUTTONDOWN && GetWindowLongPtrW( hwnd, GWLP_USERDATA ) == 1)
    {
        DWORD effect = 0;
        HRESULT hr = DoDragDrop( &the_data.IDataObject_iface, &the_source,
                                 DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect );
        out( "RESULT %d hr=0x%lx effect=%lu\n", ++drags, hr, effect );
        return 0;
    }
    if (msg == WM_DESTROY) PostQuitMessage( 0 );
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int wmain( int argc, WCHAR **argv )
{
    WNDCLASSW wc = { 0 };
    HWND hwnd;
    MSG msg;
    int x, y, w = 200, h = 200;
    BOOL source;

    if (argc < 4) return 2;
    OleInitialize( NULL );
    cf_private = RegisterClipboardFormatW( L"SG XDND Probe" );
    source = !wcscmp( argv[1], L"source" );
    x = _wtoi( argv[2] );
    y = _wtoi( argv[3] );
    if (source)
    {
        if (argc < 6) return 2;
        text_mode = !wcscmp( argv[4], L"text" );
        if (text_mode) text = argv[5];
        else
        {
            paths = argv + 5;
            npaths = argc - 5;
        }
    }
    else if (argc >= 6)
    {
        w = _wtoi( argv[4] );
        h = _wtoi( argv[5] );
    }

    wc.lpfnWndProc = wndproc;
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.lpszClassName = L"sgxdndprobe";
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"sgxdndprobe", source ? (text_mode ? L"sg-xdnd-text-source" : L"sg-xdnd-source")
                                                    : L"sg-xdnd-wine-target",
                            WS_POPUP | WS_VISIBLE | WS_BORDER, x, y, w, h, NULL, NULL, NULL, NULL );
    SetWindowLongPtrW( hwnd, GWLP_USERDATA, source ? 1 : 2 );
    if (!source) out( "REGISTER hr=0x%lx\n", RegisterDragDrop( hwnd, &the_target ) );
    out( "READY %s\n", source ? "source" : "target" );
    while (GetMessageW( &msg, NULL, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    OleUninitialize();
    return 0;
}
