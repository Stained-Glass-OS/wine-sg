/* toast-probe: Windows.UI.Notifications toasts and Windows.Data.Xml.Dom
 * (patches/sg/0437), for test/toast-gate.sh.
 *
 * Builds a toast the two ways programs do -- from a template edited through
 * the DOM (Firefox, Thunderbird) and from XML text (Chrome, Electron, .NET)
 * -- shows it, and plays the person: clicks its body, its button, its close
 * button, or lets it time out; the program hides one and replaces one by
 * tag. Prints name=value lines.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stdlib.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(p_IID_IInspectable, 0xaf86e2e0, 0xb12d, 0x4c6a, 0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90);
DEFINE_GUID(p_IID_IAgileObject, 0x94ea2b94, 0xe9cc, 0x49e0, 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90);
DEFINE_GUID(p_IID_IToastNotificationManagerStatics, 0x50ac103f, 0xd235, 0x4598, 0xbb, 0xef, 0x98, 0xfe, 0x4d, 0x1a, 0x3a, 0xd4);
DEFINE_GUID(p_IID_IToastNotificationFactory, 0x04124b20, 0x82c6, 0x4229, 0xb1, 0x09, 0xfd, 0x9e, 0xd4, 0x66, 0x2b, 0x53);
DEFINE_GUID(p_IID_IToastNotification2, 0x9dfb9fd1, 0x143a, 0x490e, 0x90, 0xbf, 0xb9, 0xfb, 0xa7, 0x13, 0x2d, 0xe7);
DEFINE_GUID(p_IID_IToastActivatedEventArgs, 0xe3bf92f3, 0xc197, 0x436f, 0x82, 0x65, 0x06, 0x25, 0x82, 0x4f, 0x8d, 0xac);
DEFINE_GUID(p_IID_IToastDismissedEventArgs, 0x3f89d935, 0xd9cb, 0x4538, 0xa0, 0xf0, 0xff, 0xe7, 0x65, 0x99, 0x38, 0xf8);
DEFINE_GUID(p_IID_ActivatedHandler, 0xab54de2d, 0x97d9, 0x5528, 0xb6, 0xad, 0x10, 0x5a, 0xfe, 0x15, 0x65, 0x30);
DEFINE_GUID(p_IID_DismissedHandler, 0x61c2402f, 0x0ed0, 0x5a18, 0xab, 0x69, 0x59, 0xf4, 0xaa, 0x99, 0xa3, 0x68);
DEFINE_GUID(p_IID_IXmlDocumentIO, 0x6cd0e74e, 0xee65, 0x4489, 0x9e, 0xbf, 0xca, 0x43, 0xe8, 0x7b, 0xa6, 0x37);
DEFINE_GUID(p_IID_IXmlNodeSerializer, 0x5cc5b382, 0xe6dd, 0x4991, 0xab, 0xef, 0x06, 0xd8, 0xd2, 0xe7, 0xbd, 0x0c);
DEFINE_GUID(p_IID_IXmlDocument, 0xf7f3a506, 0x1e87, 0x42d6, 0xbc, 0xfb, 0xb8, 0xc8, 0x09, 0xfa, 0x54, 0x94);
DEFINE_GUID(p_IID_IXmlElement, 0x2dfb8a1f, 0x6b10, 0x4ef8, 0x9f, 0x83, 0xef, 0xcc, 0xe8, 0xfa, 0xec, 0x37);
DEFINE_GUID(p_IID_IXmlNode, 0x1c741d59, 0x2122, 0x47d5, 0xa8, 0x56, 0x83, 0xf3, 0xd4, 0x21, 0x48, 0x75);

typedef HRESULT (WINAPI *qi_fn)( void *, REFIID, void ** );
typedef ULONG (WINAPI *release_fn)( void * );
typedef HRESULT (WINAPI *get_obj_fn)( void *, void ** );
typedef HRESULT (WINAPI *hstr_obj_fn)( void *, HSTRING, void ** );
typedef HRESULT (WINAPI *hstr_fn)( void *, HSTRING );
typedef HRESULT (WINAPI *get_hstr_fn)( void *, HSTRING * );
typedef HRESULT (WINAPI *uint_obj_fn)( void *, UINT32, void ** );
typedef HRESULT (WINAPI *obj_obj_fn)( void *, void *, void ** );
typedef HRESULT (WINAPI *obj_fn)( void *, void * );
typedef HRESULT (WINAPI *two_hstr_fn)( void *, HSTRING, HSTRING );
typedef HRESULT (WINAPI *add_fn)( void *, void *, INT64 * );
typedef HRESULT (WINAPI *int_obj_fn)( void *, int, void ** );
typedef HRESULT (WINAPI *get_int_fn)( void *, int * );
typedef HRESULT (WINAPI *get_uint_fn)( void *, UINT32 * );

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

static void print_hstring( const char *name, HSTRING str )
{
    printf( "%s=%ls\n", name, str ? WindowsGetStringRawBuffer( str, NULL ) : L"" );
}

/* the handlers: Activated records the arguments, Dismissed the reason */
static WCHAR last_arguments[256];
static DWORD last_thread;
static int last_reason = -1;
static HANDLE event_fired;

struct handler { void *vtbl; BOOL activated; };

static HRESULT WINAPI handler_QueryInterface( struct handler *iface, REFIID iid, void **out )
{
    /* not agile, like the handlers of Firefox */
    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, iface->activated ? &p_IID_ActivatedHandler : &p_IID_DismissedHandler ))
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
    last_thread = GetCurrentThreadId();
    if (iface->activated)
    {
        void *activated = qi( args, &p_IID_IToastActivatedEventArgs );
        HSTRING str = NULL;
        last_arguments[0] = 0;
        if (activated && SUCCEEDED(SLOT( activated, 6, get_hstr_fn )( activated, &str )))
        {
            lstrcpynW( last_arguments, str ? WindowsGetStringRawBuffer( str, NULL ) : L"", ARRAYSIZE(last_arguments) );
            WindowsDeleteString( str );
        }
        else lstrcpyW( last_arguments, L"<no args>" );
        release( activated );
    }
    else
    {
        void *dismissed = qi( args, &p_IID_IToastDismissedEventArgs );
        last_reason = -2;
        if (dismissed) SLOT( dismissed, 6, get_int_fn )( dismissed, &last_reason );
        release( dismissed );
    }
    if (getenv( "TOAST_PROBE_TRACE" )) printf( "  event %s %ls %d\n", iface->activated ? "activated" : "dismissed", last_arguments, last_reason );
    SetEvent( event_fired );
    return S_OK;
}

static void *handler_vtbl[] = { handler_QueryInterface, handler_AddRef, handler_Release, handler_Invoke };
static struct handler on_activated = { handler_vtbl, TRUE }, on_dismissed = { handler_vtbl, FALSE };

static void *manager, *factory, *notifier;

static void *make_toast( void *doc, const WCHAR *tag )
{
    void *toast = NULL, *toast2;
    INT64 token;
    SLOT( factory, 6, obj_obj_fn )( factory, doc, &toast );
    if (!toast) return NULL;
    SLOT( toast, 11, add_fn )( toast, &on_activated, &token );
    SLOT( toast, 9, add_fn )( toast, &on_dismissed, &token );
    if (tag && (toast2 = qi( toast, &p_IID_IToastNotification2 )))
    {
        SLOT( toast2, 6, hstr_fn )( toast2, hs( tag ) );
        release( toast2 );
    }
    return toast;
}

static void *load_xml( const WCHAR *xml, HRESULT *hr )
{
    void *inspectable = NULL, *io;
    RoActivateInstance( hs( L"Windows.Data.Xml.Dom.XmlDocument" ), (IInspectable **)&inspectable );
    if (!inspectable) return NULL;
    io = qi( inspectable, &p_IID_IXmlDocumentIO );
    *hr = io ? SLOT( io, 6, hstr_fn )( io, hs( xml ) ) : E_NOINTERFACE;
    release( io );
    return inspectable;
}

static HWND wait_window( const WCHAR *title, DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    HWND hwnd;
    do
    {
        if ((hwnd = FindWindowW( L"WineToastNotification", title )) && IsWindowVisible( hwnd )) return hwnd;
        Sleep( 50 );
    } while ((int)(end - GetTickCount()) > 0);
    return NULL;
}

static int count_windows( void )
{
    HWND hwnd = NULL;
    int n = 0;
    while ((hwnd = FindWindowExW( HWND_DESKTOP, hwnd, L"WineToastNotification", NULL ))) if (IsWindowVisible( hwnd )) n++;
    return n;
}

static BOOL wait_event( DWORD ms )
{
    return WaitForSingleObject( event_fired, ms ) == WAIT_OBJECT_0;
}

static void click( HWND hwnd, int x, int y )
{
    PostMessageW( hwnd, WM_LBUTTONUP, 0, MAKELPARAM( x, y ) );
}

static void get_xml( const char *name, void *node )
{
    void *serializer = qi( node, &p_IID_IXmlNodeSerializer );
    HSTRING str = NULL;
    if (serializer) SLOT( serializer, 6, get_hstr_fn )( serializer, &str );
    print_hstring( name, str );
    release( serializer );
}

/* the Firefox way: a template, edited */
static void *template_toast_doc( void )
{
    void *doc = NULL, *list = NULL, *node = NULL, *text = NULL, *out = NULL, *element = NULL, *actions = NULL, *action = NULL;
    UINT32 len = 0;

    SLOT( manager, 8, int_obj_fn )( manager, 5 /* ToastText02 */, &doc );
    if (!doc) return NULL;
    get_xml( "template", doc );

    SLOT( doc, 16, hstr_obj_fn )( doc, hs( L"text" ), &list );
    if (list) SLOT( list, 6, get_uint_fn )( list, &len );
    printf( "template_texts=%u\n", len );
    if (len < 2) return doc;

    SLOT( list, 7, uint_obj_fn )( list, 0, &node );
    SLOT( doc, 11, hstr_obj_fn )( doc, hs( L"Probe title" ), &text );
    SLOT( node, 22, obj_obj_fn )( node, qi( text, &p_IID_IXmlNode ), &out );
    release( out ); release( text ); release( node );
    SLOT( list, 7, uint_obj_fn )( list, 1, &node );
    SLOT( doc, 11, hstr_obj_fn )( doc, hs( L"A toast made from a template" ), &text );
    SLOT( node, 22, obj_obj_fn )( node, qi( text, &p_IID_IXmlNode ), &out );
    release( out ); release( text ); release( node ); release( list );

    list = NULL;
    SLOT( doc, 16, hstr_obj_fn )( doc, hs( L"toast" ), &list );
    SLOT( list, 7, uint_obj_fn )( list, 0, &node );
    element = qi( node, &p_IID_IXmlElement );
    SLOT( element, 8, two_hstr_fn )( element, hs( L"launch" ), hs( L"from-body" ) );

    SLOT( doc, 9, hstr_obj_fn )( doc, hs( L"actions" ), &actions );
    SLOT( doc, 9, hstr_obj_fn )( doc, hs( L"action" ), &action );
    SLOT( action, 8, two_hstr_fn )( action, hs( L"content" ), hs( L"Open" ) );
    SLOT( action, 8, two_hstr_fn )( action, hs( L"arguments" ), hs( L"from-button" ) );
    {   /* IXmlElement is not IXmlNode: go through QI */
        void *actions_node = qi( actions, &p_IID_IXmlNode ), *action_node = qi( action, &p_IID_IXmlNode );
        SLOT( actions_node, 22, obj_obj_fn )( actions_node, action_node, &out ); release( out );
        SLOT( node, 22, obj_obj_fn )( node, actions_node, &out ); release( out );
        release( actions_node ); release( action_node );
    }
    release( action ); release( actions ); release( element ); release( node ); release( list );
    get_xml( "edited", doc );
    return doc;
}

/* 7: a program's single-threaded apartment gets its events on its own thread */
static DWORD WINAPI sta_thread( void *xmldoc )
{
    void *toast;
    HWND hwnd;
    MSG msg;
    DWORD end;
    BOOL fired = FALSE;

    RoInitialize( RO_INIT_SINGLETHREADED );
    toast = make_toast( xmldoc, NULL );
    SLOT( notifier, 6, obj_fn )( notifier, toast );
    if ((hwnd = wait_window( L"Loaded title", 5000 )))
    {
        click( hwnd, 60, 45 );
        end = GetTickCount() + 5000;
        while (!fired && (int)(end - GetTickCount()) > 0)
        {
            if (MsgWaitForMultipleObjects( 1, &event_fired, FALSE, 100, QS_ALLINPUT ) == WAIT_OBJECT_0) fired = TRUE;
            while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        }
    }
    printf( "sta_event=%d on_own_thread=%d\n", fired, last_thread == GetCurrentThreadId() );
    release( toast );
    RoUninitialize();
    return 0;
}

/* 0436: the transacted registry calls Firefox registers its toast identity with */
static void check_transacted( void )
{
    typedef LSTATUS (WINAPI *create_fn)( HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, SECURITY_ATTRIBUTES *, HKEY *, DWORD *, HANDLE, void * );
    typedef LSTATUS (WINAPI *open_fn)( HKEY, LPCWSTR, DWORD, REGSAM, HKEY *, HANDLE, void * );
    typedef LSTATUS (WINAPI *delete_fn)( HKEY, LPCWSTR, REGSAM, DWORD, HANDLE, void * );
    HMODULE advapi32 = LoadLibraryW( L"advapi32.dll" );
    create_fn create = (create_fn)GetProcAddress( advapi32, "RegCreateKeyTransactedW" );
    open_fn open = (open_fn)GetProcAddress( advapi32, "RegOpenKeyTransactedW" );
    delete_fn delete = (delete_fn)GetProcAddress( advapi32, "RegDeleteKeyTransactedW" );
    static const WCHAR key[] = L"Software\\Classes\\AppUserModelId\\SG.Probe.Transacted";
    LSTATUS a = -1, b = -1, c = -1, d = -1;
    HKEY hkey;

    if (create && (a = create( HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL, (HANDLE)1, NULL )) == 0)
    {
        b = RegSetValueExW( hkey, L"DisplayName", 0, REG_SZ, (const BYTE *)L"Probe", sizeof(L"Probe") );
        RegCloseKey( hkey );
    }
    if (open && (c = open( HKEY_CURRENT_USER, key, 0, KEY_READ, &hkey, (HANDLE)1, NULL )) == 0) RegCloseKey( hkey );
    if (delete) d = delete( HKEY_CURRENT_USER, key, 0, 0, (HANDLE)1, NULL );
    printf( "transacted=%d (%ld %ld %ld %ld)\n", !a && !b && !c && !d, a, b, c, d );
}

int main( void )
{
    void *doc, *toast, *xmldoc;
    HWND hwnd;
    RECT rect, work;
    HRESULT hr;
    int setting = -1;

    check_transacted();
    RoInitialize( RO_INIT_MULTITHREADED );
    event_fired = CreateEventW( NULL, FALSE, FALSE, NULL );
    SystemParametersInfoW( SPI_GETWORKAREA, 0, &work, 0 );

    hr = RoGetActivationFactory( hs( L"Windows.UI.Notifications.ToastNotificationManager" ),
                                 &p_IID_IToastNotificationManagerStatics, &manager );
    printf( "manager=%08lx\n", hr );
    hr = RoGetActivationFactory( hs( L"Windows.UI.Notifications.ToastNotification" ), &p_IID_IToastNotificationFactory, &factory );
    printf( "factory=%08lx\n", hr );
    if (!manager || !factory) return 1;
    hr = SLOT( manager, 7, hstr_obj_fn )( manager, hs( L"SG.Probe.Toast" ), &notifier );
    printf( "notifier=%08lx\n", hr );
    if (!notifier) return 1;
    SLOT( notifier, 8, get_int_fn )( notifier, &setting );
    printf( "setting=%d\n", setting );

    /* 1: from a template, clicked on its body */
    doc = template_toast_doc();
    toast = make_toast( doc, NULL );
    hr = SLOT( notifier, 6, obj_fn )( notifier, toast );
    hwnd = wait_window( L"Probe title", 5000 );
    printf( "show=%08lx shown=%d\n", hr, hwnd != NULL );
    if (hwnd)
    {
        GetWindowRect( hwnd, &rect );
        printf( "corner=%d (%ld,%ld %ldx%ld work %ld,%ld)\n", rect.right <= work.right && rect.right > work.right - 64 &&
                rect.bottom <= work.bottom && rect.bottom > work.bottom - 64, rect.left, rect.top,
                rect.right - rect.left, rect.bottom - rect.top, work.right, work.bottom );
        click( hwnd, 60, 45 );
        { BOOL fired = wait_event( 5000 ); printf( "body_click=%d args=%ls\n", fired, last_arguments ); }
        Sleep( 300 );
        printf( "gone_after_click=%d\n", !IsWindow( hwnd ) );
    }
    release( toast );

    /* 2: the same, its button */
    toast = make_toast( doc, NULL );
    SLOT( notifier, 6, obj_fn )( notifier, toast );
    if ((hwnd = wait_window( L"Probe title", 5000 )))
    {
        GetClientRect( hwnd, &rect );
        click( hwnd, rect.right / 2, rect.bottom - 32 );
        { BOOL fired = wait_event( 5000 ); printf( "button_click=%d args=%ls\n", fired, last_arguments ); }
    }
    else printf( "button_click=0 args=(no toast)\n" );
    release( toast );

    /* 3: hidden by the program */
    toast = make_toast( doc, NULL );
    SLOT( notifier, 6, obj_fn )( notifier, toast );
    if ((hwnd = wait_window( L"Probe title", 5000 )))
    {
        SLOT( notifier, 7, obj_fn )( notifier, toast );
        { BOOL fired = wait_event( 5000 ); Sleep( 200 ); printf( "hide=%d reason=%d gone=%d\n", fired, last_reason, !IsWindow( hwnd ) ); }
    }
    else printf( "hide=0 reason=(no toast)\n" );
    release( toast );
    release( doc );

    /* 4: from XML text, closed with its close button */
    xmldoc = load_xml( L"<toast launch=\"x\"><visual><binding template=\"ToastGeneric\"><text>Loaded title</text>"
                       L"<text>A toast from XML text</text><text placement=\"attribution\">via probe</text>"
                       L"</binding></visual></toast>", &hr );
    printf( "loadxml=%08lx\n", hr );
    toast = make_toast( xmldoc, NULL );
    SLOT( notifier, 6, obj_fn )( notifier, toast );
    if ((hwnd = wait_window( L"Loaded title", 5000 )))
    {
        GetClientRect( hwnd, &rect );
        click( hwnd, rect.right - 24, 20 );
        { BOOL fired = wait_event( 5000 ); printf( "close=%d reason=%d\n", fired, last_reason ); }
    }
    else printf( "close=0 reason=(no toast)\n" );
    release( toast );

    /* 5: left alone, it times out */
    toast = make_toast( xmldoc, NULL );
    SLOT( notifier, 6, obj_fn )( notifier, toast );
    hwnd = wait_window( L"Loaded title", 5000 );
    { BOOL fired = hwnd && wait_event( 12000 ); printf( "timeout=%d reason=%d\n", fired, last_reason ); }
    release( toast );

    {
        HANDLE thread = CreateThread( NULL, 0, sta_thread, xmldoc, 0, NULL );
        WaitForSingleObject( thread, 20000 );
        CloseHandle( thread );
    }

    /* 6: a second toast with the same tag replaces the first */
    toast = make_toast( xmldoc, L"same" );
    SLOT( notifier, 6, obj_fn )( notifier, toast );
    wait_window( L"Loaded title", 5000 );
    release( toast );
    toast = make_toast( xmldoc, L"same" );
    SLOT( notifier, 6, obj_fn )( notifier, toast );
    Sleep( 1000 );
    printf( "replaced=%d (%d)\n", count_windows() == 1, count_windows() );
    SLOT( notifier, 7, obj_fn )( notifier, toast );
    release( toast );
    release( xmldoc );

    /* bad XML is refused */
    release( load_xml( L"<toast><visual>", &hr ) );
    printf( "badxml=%d (%08lx)\n", FAILED(hr), hr );

    release( notifier );
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
