/* toastactivate-probe: clicking a notification in the notification centre
 * reaches the program that sent it (patches/sg/0819-0821), for
 * test/toastactivate-gate.sh.
 *
 *   probe events AUMID TITLE   shows a toast and holds it, its Activated
 *                              handler (not agile) in its single-threaded
 *                              apartment: C:\events.txt
 *   probe comrun AUMID TITLE   registers its COM activator, shows a toast and
 *                              waits for INotificationActivationCallback::
 *                              Activate: C:\com.txt
 *   probe comshow AUMID TITLE  shows a toast and leaves
 *   probe comserver -Embedding the activator, started by COM: C:\comserver.txt
 *   probe popup AUMID TITLE    registers its activator, shows a toast and
 *                              clicks it on screen: C:\popup.txt
 *   probe tag AUMID            two toasts with one tag
 *   probe link AUMID CLSID     a Start menu shortcut with the AppUserModelID
 *                              and the toast activator, read back
 *   probe balloon              a notification-area icon's balloon; waits for
 *                              NIN_BALLOONUSERCLICK: C:\balloon.txt
 *   probe centre SEQ           SgActivateNotification( SEQ ), as sg-notify
 *   probe                      (no arguments) C:\launched.txt
 * A toast's history entry number is written to C:\<mode>.seq.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <shellapi.h>
#include <shlobj.h>
#include <propsys.h>
#include <propvarutil.h>
#include <stdio.h>
#include <stdlib.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(p_IID_IToastNotificationManagerStatics, 0x50ac103f, 0xd235, 0x4598, 0xbb, 0xef, 0x98, 0xfe, 0x4d, 0x1a, 0x3a, 0xd4);
DEFINE_GUID(p_IID_IToastNotificationFactory, 0x04124b20, 0x82c6, 0x4229, 0xb1, 0x09, 0xfd, 0x9e, 0xd4, 0x66, 0x2b, 0x53);
DEFINE_GUID(p_IID_IToastNotification2, 0x9dfb9fd1, 0x143a, 0x490e, 0x90, 0xbf, 0xb9, 0xfb, 0xa7, 0x13, 0x2d, 0xe7);
DEFINE_GUID(p_IID_IToastActivatedEventArgs, 0xe3bf92f3, 0xc197, 0x436f, 0x82, 0x65, 0x06, 0x25, 0x82, 0x4f, 0x8d, 0xac);
DEFINE_GUID(p_IID_ActivatedHandler, 0xab54de2d, 0x97d9, 0x5528, 0xb6, 0xad, 0x10, 0x5a, 0xfe, 0x15, 0x65, 0x30);
DEFINE_GUID(p_IID_IXmlDocumentIO, 0x6cd0e74e, 0xee65, 0x4489, 0x9e, 0xbf, 0xca, 0x43, 0xe8, 0x7b, 0xa6, 0x37);
DEFINE_GUID(p_IID_INotificationActivationCallback, 0x53e31837, 0x6600, 0x4a81, 0x93, 0x95, 0x75, 0xcf, 0xfe, 0x74, 0x6f, 0x94);
DEFINE_GUID(p_PKEY_fmtid_aumodel, 0x9f4c2855, 0x9f79, 0x4b39, 0xa8, 0xd0, 0xe1, 0xd4, 0x2d, 0xe1, 0xd5, 0xf3);
/* the probe's activators: one registered by AppUserModelID, one by shortcut */
DEFINE_GUID(probe_clsid_com, 0x5a3c1e10, 0x7b2d, 0x4c11, 0x9e, 0x01, 0x53, 0x47, 0x54, 0x41, 0x00, 0x01);
DEFINE_GUID(probe_clsid_lnk, 0x5a3c1e10, 0x7b2d, 0x4c11, 0x9e, 0x01, 0x53, 0x47, 0x54, 0x41, 0x00, 0x02);

typedef HRESULT (WINAPI *qi_fn)( void *, REFIID, void ** );
typedef ULONG (WINAPI *release_fn)( void * );
typedef HRESULT (WINAPI *hstr_obj_fn)( void *, HSTRING, void ** );
typedef HRESULT (WINAPI *hstr_fn)( void *, HSTRING );
typedef HRESULT (WINAPI *get_hstr_fn)( void *, HSTRING * );
typedef HRESULT (WINAPI *obj_obj_fn)( void *, void *, void ** );
typedef HRESULT (WINAPI *obj_fn)( void *, void * );
typedef HRESULT (WINAPI *add_fn)( void *, void *, INT64 * );

static void release( void *obj ) { if (obj) SLOT( obj, 2, release_fn )( obj ); }

static void *qi( void *obj, REFIID iid )
{
    void *out = NULL;
    if (obj) SLOT( obj, 0, qi_fn )( obj, iid, &out );
    return out;
}

static HSTRING hs( const WCHAR *str )
{
    HSTRING ret = NULL;
    WindowsCreateString( str, wcslen( str ), &ret );
    return ret;
}

static void write_file( const char *name, const char *text )
{
    char path[64];
    FILE *f;
    snprintf( path, sizeof(path), "C:\\%s", name );
    if ((f = fopen( path, "w" )))
    {
        fputs( text, f );
        fclose( f );
    }
}

static HANDLE done_event;
static DWORD main_thread;

/* the Activated handler: not agile, as Firefox's */
static char event_text[512];

struct handler { void *vtbl; };

static HRESULT WINAPI handler_QueryInterface( struct handler *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &p_IID_ActivatedHandler ))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI handler_AddRef( struct handler *iface ) { return 2; }
static ULONG WINAPI handler_Release( struct handler *iface ) { return 1; }

static HRESULT WINAPI handler_Invoke( struct handler *iface, void *sender, IUnknown *args )
{
    void *activated = qi( args, &p_IID_IToastActivatedEventArgs );
    HSTRING str = NULL;
    if (activated) SLOT( activated, 6, get_hstr_fn )( activated, &str );
    snprintf( event_text, sizeof(event_text), "event=1 args=%ls own_thread=%d\n",
              str ? WindowsGetStringRawBuffer( str, NULL ) : L"", GetCurrentThreadId() == main_thread );
    WindowsDeleteString( str );
    release( activated );
    SetEvent( done_event );
    return S_OK;
}

static void *handler_vtbl[] = { handler_QueryInterface, handler_AddRef, handler_Release, handler_Invoke };
static struct handler on_activated = { handler_vtbl };

/* the COM activator */
static char com_text[512];

static HRESULT WINAPI callback_QueryInterface( void *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &p_IID_INotificationActivationCallback ))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI callback_AddRef( void *iface ) { return 2; }
static ULONG WINAPI callback_Release( void *iface ) { return 1; }

static HRESULT WINAPI callback_Activate( void *iface, const WCHAR *aumid, const WCHAR *args, const void *data, ULONG count )
{
    snprintf( com_text, sizeof(com_text), "com=1 aumid=%ls args=%ls count=%lu\n", aumid ? aumid : L"(null)",
              args ? args : L"(null)", count );
    SetEvent( done_event );
    return S_OK;
}

static void *callback_vtbl[] = { callback_QueryInterface, callback_AddRef, callback_Release, callback_Activate };
static struct { void *vtbl; } activator = { callback_vtbl };

static HRESULT WINAPI factory_QueryInterface( void *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IClassFactory ))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI factory_AddRef( void *iface ) { return 2; }
static ULONG WINAPI factory_Release( void *iface ) { return 1; }
static HRESULT WINAPI factory_CreateInstance( void *iface, IUnknown *outer, REFIID iid, void **out )
{
    if (outer) return CLASS_E_NOAGGREGATION;
    return callback_QueryInterface( &activator, iid, out );
}
static HRESULT WINAPI factory_LockServer( void *iface, BOOL lock ) { return S_OK; }

static void *factory_vtbl[] = { factory_QueryInterface, factory_AddRef, factory_Release, factory_CreateInstance, factory_LockServer };
static struct { void *vtbl; } class_factory = { factory_vtbl };

static void register_activators( void )
{
    DWORD cookie;
    HRESULT hr = CoRegisterClassObject( &probe_clsid_com, (IUnknown *)&class_factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie );
    HRESULT hr2 = CoRegisterClassObject( &probe_clsid_lnk, (IUnknown *)&class_factory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie );
    printf( "registered=%08lx %08lx\n", hr, hr2 );
}

/* pumps messages until done_event or the time is up */
static BOOL pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        if (MsgWaitForMultipleObjects( 1, &done_event, FALSE, 100, QS_ALLINPUT ) == WAIT_OBJECT_0) return TRUE;
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
    }
    return FALSE;
}

static void *notifier, *factory;

static DWORD history_next( void )
{
    DWORD next = 0, size = sizeof(next);
    RegGetValueW( HKEY_CURRENT_USER, L"Software\\Stained Glass\\Notifications\\History", L"Next", RRF_RT_REG_DWORD, NULL,
                  &next, &size );
    return next;
}

static void write_seq( const char *mode )
{
    char name[64], text[32];
    snprintf( name, sizeof(name), "%s.seq", mode );
    snprintf( text, sizeof(text), "%lu\n", history_next() - 1 );
    write_file( name, text );
}

static BOOL start_toasts( const WCHAR *aumid )
{
    void *manager = NULL;
    RoGetActivationFactory( hs( L"Windows.UI.Notifications.ToastNotificationManager" ), &p_IID_IToastNotificationManagerStatics,
                            &manager );
    RoGetActivationFactory( hs( L"Windows.UI.Notifications.ToastNotification" ), &p_IID_IToastNotificationFactory, &factory );
    if (!manager || !factory) return FALSE;
    SLOT( manager, 7, hstr_obj_fn )( manager, hs( aumid ), &notifier );
    release( manager );
    return notifier != NULL;
}

static void *show_toast( const WCHAR *title, const WCHAR *launch, BOOL handler, const WCHAR *tag )
{
    WCHAR xml[1024];
    void *doc = NULL, *io, *toast = NULL, *toast2;
    INT64 token;

    swprintf( xml, ARRAYSIZE(xml), L"<toast launch=\"%ls\"><visual><binding template=\"ToastGeneric\"><text>%ls</text>"
              L"<text>Probe body</text></binding></visual></toast>", launch, title );
    RoActivateInstance( hs( L"Windows.Data.Xml.Dom.XmlDocument" ), (IInspectable **)&doc );
    if (!doc || !(io = qi( doc, &p_IID_IXmlDocumentIO ))) return NULL;
    SLOT( io, 6, hstr_fn )( io, hs( xml ) );
    release( io );
    SLOT( factory, 6, obj_obj_fn )( factory, doc, &toast );
    release( doc );
    if (!toast) return NULL;
    if (handler) SLOT( toast, 11, add_fn )( toast, &on_activated, &token );
    if (tag && (toast2 = qi( toast, &p_IID_IToastNotification2 )))
    {
        SLOT( toast2, 6, hstr_fn )( toast2, hs( tag ) );
        release( toast2 );
    }
    printf( "show=%08lx\n", SLOT( notifier, 6, obj_fn )( notifier, toast ) );
    return toast;
}

static HWND own_popup( const WCHAR *title, DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    HWND hwnd;
    MSG msg;
    do
    {
        if ((hwnd = FindWindowW( L"WineToastNotification", title )) && IsWindowVisible( hwnd )) return hwnd;
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 50 );
    } while ((int)(end - GetTickCount()) > 0);
    return NULL;
}

static BOOL history_has( DWORD seq )
{
    WCHAR path[128];
    HKEY key;
    swprintf( path, ARRAYSIZE(path), L"Software\\Stained Glass\\Notifications\\History\\%08lu", seq );
    if (RegOpenKeyExW( HKEY_CURRENT_USER, path, 0, KEY_READ, &key )) return FALSE;
    RegCloseKey( key );
    return TRUE;
}

static int link_mode( const WCHAR *aumid, const WCHAR *clsid_text )
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], exe[MAX_PATH];
    IShellLinkW *link = NULL;
    IPersistFile *file = NULL;
    IPropertyStore *store = NULL;
    PROPERTYKEY key_id = { p_PKEY_fmtid_aumodel, 5 }, key_activator = { p_PKEY_fmtid_aumodel, 26 };
    PROPVARIANT value;
    CLSID clsid;
    HRESULT hr;

    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    CLSIDFromString( clsid_text, &clsid );
    SHGetFolderPathW( NULL, CSIDL_PROGRAMS, NULL, 0, dir );
    swprintf( path, ARRAYSIZE(path), L"%ls\\Probe Lnk.lnk", dir );
    GetModuleFileNameW( NULL, exe, ARRAYSIZE(exe) );

    /* as an installer does */
    hr = CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link );
    if (FAILED(hr)) { printf( "link=%08lx\n", hr ); return 1; }
    IShellLinkW_SetPath( link, exe );
    IShellLinkW_QueryInterface( link, &IID_IPropertyStore, (void **)&store );
    value.vt = VT_LPWSTR;
    value.pwszVal = (WCHAR *)aumid;
    hr = IPropertyStore_SetValue( store, &key_id, &value );
    value.vt = VT_CLSID;
    value.puuid = &clsid;
    hr |= IPropertyStore_SetValue( store, &key_activator, &value );
    hr |= IPropertyStore_Commit( store );
    IPropertyStore_Release( store );
    IShellLinkW_QueryInterface( link, &IID_IPersistFile, (void **)&file );
    hr |= IPersistFile_Save( file, path, TRUE );
    IPersistFile_Release( file );
    IShellLinkW_Release( link );
    printf( "link_saved=%08lx\n", hr );

    /* read back from the file */
    CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IPersistFile, (void **)&file );
    hr = IPersistFile_Load( file, path, STGM_READ );
    IPersistFile_QueryInterface( file, &IID_IPropertyStore, (void **)&store );
    PropVariantInit( &value );
    IPropertyStore_GetValue( store, &key_id, &value );
    printf( "link_aumid=%ls\n", value.vt == VT_LPWSTR ? value.pwszVal : L"(none)" );
    PropVariantClear( &value );
    IPropertyStore_GetValue( store, &key_activator, &value );
    printf( "link_activator=%d\n", value.vt == VT_CLSID && IsEqualGUID( value.puuid, &clsid ) );
    PropVariantClear( &value );
    {
        DWORD count = 0;
        IPropertyStore_GetCount( store, &count );
        printf( "link_count=%lu\n", count );
    }
    {
        WCHAR target[MAX_PATH] = L"";
        IShellLinkW *again;
        if (SUCCEEDED(IPersistFile_QueryInterface( file, &IID_IShellLinkW, (void **)&again )))
        {
            IShellLinkW_GetPath( again, target, ARRAYSIZE(target), NULL, 0 );
            IShellLinkW_Release( again );
        }
        printf( "link_target_ok=%d\n", !lstrcmpiW( target, exe ) );
    }
    IPropertyStore_Release( store );
    IPersistFile_Release( file );
    return 0;
}

#define WM_PROBE_ICON (WM_APP + 5)
static char balloon_text[128];

static LRESULT CALLBACK icon_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_PROBE_ICON && LOWORD( lp ) == NIN_BALLOONUSERCLICK)
    {
        snprintf( balloon_text, sizeof(balloon_text), "click=1 id=%u\n", HIWORD( lp ) );
        SetEvent( done_event );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static int balloon_mode( void )
{
    NOTIFYICONDATAW nid = { sizeof(nid) };
    WNDCLASSW wc = { 0 };
    HWND hwnd;
    BOOL fired;

    wc.lpfnWndProc = icon_proc;
    wc.lpszClassName = L"ProbeIcon";
    RegisterClassW( &wc );
    hwnd = CreateWindowW( L"ProbeIcon", L"probe", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL );
    nid.hWnd = hwnd;
    nid.uID = 7;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_PROBE_ICON;
    nid.hIcon = LoadIconW( NULL, (const WCHAR *)IDI_INFORMATION );
    lstrcpyW( nid.szTip, L"Balloon Probe" );
    printf( "add=%d\n", Shell_NotifyIconW( NIM_ADD, &nid ) );
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW( NIM_SETVERSION, &nid );
    nid.uFlags = NIF_INFO;
    lstrcpyW( nid.szInfoTitle, L"Balloon to click" );
    lstrcpyW( nid.szInfo, L"From the notification centre." );
    printf( "balloon=%d\n", Shell_NotifyIconW( NIM_MODIFY, &nid ) );
    fflush( stdout );
    Sleep( 500 );
    write_seq( "balloon" );
    fired = pump( 40000 );
    write_file( "balloon.txt", fired ? balloon_text : "click=0\n" );
    Shell_NotifyIconW( NIM_DELETE, &nid );
    return 0;
}

int main( int argc, char **argv )
{
    WCHAR aumid[256] = L"", title[256] = L"";
    const char *mode = argc > 1 ? argv[1] : "";

    done_event = CreateEventW( NULL, TRUE, FALSE, NULL );
    main_thread = GetCurrentThreadId();
    if (argc > 2) MultiByteToWideChar( CP_ACP, 0, argv[2], -1, aumid, ARRAYSIZE(aumid) );
    if (argc > 3) MultiByteToWideChar( CP_ACP, 0, argv[3], -1, title, ARRAYSIZE(title) );

    if (!*mode)
    {
        write_file( "launched.txt", "launched=1\n" );
        return 0;
    }
    if (!strcmp( mode, "centre" ) && argc > 2)
    {
        typedef HRESULT (WINAPI *activate_fn)( DWORD, const WCHAR * );
        HMODULE module = LoadLibraryW( L"windows.ui.dll" );
        activate_fn activate = (activate_fn)GetProcAddress( module, "SgActivateNotification" );
        HRESULT hr = activate ? activate( strtoul( argv[2], NULL, 10 ), NULL ) : E_NOTIMPL;
        printf( "hr=%08lx\n", hr );
        return 0;
    }
    if (!strcmp( mode, "link" ) && argc > 3) return link_mode( aumid, title );
    if (!strcmp( mode, "balloon" )) return balloon_mode();
    if (!strcmp( mode, "comserver" ))
    {
        BOOL fired;
        CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
        register_activators();
        fired = pump( 40000 );
        write_file( "comserver.txt", fired ? com_text : "com=0\n" );
        return 0;
    }

    RoInitialize( RO_INIT_SINGLETHREADED );
    if (!start_toasts( aumid )) { printf( "no toasts\n" ); return 1; }

    if (!strcmp( mode, "events" ))
    {
        void *toast = show_toast( title, L"from-centre", TRUE, NULL );
        BOOL fired;
        write_seq( mode );
        fired = pump( 40000 );
        write_file( "events.txt", fired ? event_text : "event=0\n" );
        release( toast );
    }
    else if (!strcmp( mode, "comrun" ))
    {
        void *toast;
        BOOL fired;
        register_activators();
        toast = show_toast( title, L"from-centre", FALSE, NULL );
        write_seq( mode );
        fired = pump( 40000 );
        write_file( "com.txt", fired ? com_text : "com=0\n" );
        release( toast );
    }
    else if (!strcmp( mode, "comshow" ))
    {
        release( show_toast( title, L"from-centre", FALSE, NULL ) );
        write_seq( mode );
    }
    else if (!strcmp( mode, "popup" ))
    {
        void *toast;
        HWND hwnd;
        DWORD seq, end;
        char text[600];
        register_activators();
        toast = show_toast( title, L"from-popup", TRUE, NULL );
        seq = history_next() - 1;
        printf( "listed=%d\n", history_has( seq ) );
        if ((hwnd = own_popup( title, 5000 ))) PostMessageW( hwnd, WM_LBUTTONUP, 0, MAKELPARAM( 60, 45 ) );
        /* both: the event and the activator */
        end = GetTickCount() + 15000;
        while ((!event_text[0] || !com_text[0]) && (int)(end - GetTickCount()) > 0)
        {
            ResetEvent( done_event );
            pump( 200 );
        }
        snprintf( text, sizeof(text), "popup=%d %s%s still_listed=%d\n", hwnd != NULL, event_text[0] ? event_text : "event=0\n",
                  com_text[0] ? com_text : "com=0\n", history_has( seq ) );
        write_file( "popup.txt", text );
        release( toast );
    }
    else if (!strcmp( mode, "tag" ))
    {
        release( show_toast( L"Tagged one", L"t1", FALSE, L"same" ) );
        release( show_toast( L"Tagged two", L"t2", FALSE, L"same" ) );
        release( show_toast( L"Untagged", L"t3", FALSE, NULL ) );
    }
    fflush( stdout );
    return 0;
}
