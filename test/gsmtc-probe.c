/* gsmtc-probe: Windows.Media.Control's GlobalSystemMediaTransportControlsSessionManager, for
 * test/gsmtc-gate.sh (Stained Glass OS wine-sg 9862).
 *
 * "probe player": a window's SystemMediaTransportControls set up as a music player does (enabled,
 * Playing, Play, Pause and Next enabled, title and artist); a Pause or Play pressed from outside sets
 * that status. Runs until C:\stop exists.
 * "probe manager": the other side, as a launcher or a widget does: RequestAsync, the sessions, this
 * player's session (SourceAppUserModelId probe.exe), its media properties, playback info and controls;
 * then PlaybackInfoChanged and TryPauseAsync, which must reach the player and come back as a change.
 *
 * Prints name=value lines. (mingw's headers lack Windows.Media.Control: the calls go by vtable slot.)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Media
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <systemmediatransportcontrolsinterop.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
typedef ULONG (WINAPI *release_fn)( void * );
typedef HRESULT (WINAPI *get_obj_fn)( void *, void ** );
typedef HRESULT (WINAPI *get_hs_fn)( void *, HSTRING * );
typedef HRESULT (WINAPI *get_u32_fn)( void *, UINT32 * );
typedef HRESULT (WINAPI *get_int_fn)( void *, INT32 * );
typedef HRESULT (WINAPI *get_bool_fn)( void *, boolean * );
typedef HRESULT (WINAPI *get_at_fn)( void *, UINT32, void ** );
typedef HRESULT (WINAPI *add_handler_fn)( void *, void *, EventRegistrationToken * );

DEFINE_GUID(IID_IManagerStatics_, 0x2050c4ee, 0x11a0, 0x57de, 0xae, 0xd7, 0xc9, 0x7c, 0x70, 0x33, 0x82, 0x45);

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, wcslen( s ), &h );
    return h;
}

static void print_hs( const char *name, HSTRING str )
{
    printf( "%s=%ls\n", name, str ? WindowsGetStringRawBuffer( str, NULL ) : L"" );
}

/* the result of a done (or soon done) IAsyncOperation<T>: GetResults is slot 8 */
static HRESULT results( void *op, void **out )
{
    IAsyncInfo *info;
    AsyncStatus status = Started;
    int i;

    *out = NULL;
    if (FAILED(IUnknown_QueryInterface( (IUnknown *)op, &IID_IAsyncInfo, (void **)&info ))) return E_NOINTERFACE;
    for (i = 0; i < 500 && status == Started; i++)
    {
        IAsyncInfo_get_Status( info, &status );
        if (status == Started) Sleep( 10 );
    }
    IAsyncInfo_Release( info );
    if (status != Completed) return E_FAIL;
    return SLOT( op, 8, get_obj_fn )( op, out );
}

/*
 * The player
 */
static ISystemMediaTransportControls *smtc;

static HRESULT WINAPI button_QueryInterface( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs ))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI button_AddRef( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface ) { return 2; }
static ULONG WINAPI button_Release( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface ) { return 1; }
static HRESULT WINAPI button_Invoke( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface,
                                     ISystemMediaTransportControls *sender, ISystemMediaTransportControlsButtonPressedEventArgs *args )
{
    SystemMediaTransportControlsButton button = -1;

    ISystemMediaTransportControlsButtonPressedEventArgs_get_Button( args, &button );
    printf( "pressed=%d\n", button );
    if (button == SystemMediaTransportControlsButton_Pause)
        ISystemMediaTransportControls_put_PlaybackStatus( smtc, MediaPlaybackStatus_Paused );
    else if (button == SystemMediaTransportControlsButton_Play)
        ISystemMediaTransportControls_put_PlaybackStatus( smtc, MediaPlaybackStatus_Playing );
    return S_OK;
}
static ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgsVtbl button_vtbl =
{
    button_QueryInterface, button_AddRef, button_Release, button_Invoke,
};
static ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs button_handler = { &button_vtbl };

static int player( void )
{
    ISystemMediaTransportControlsDisplayUpdater *updater = NULL;
    ISystemMediaTransportControlsInterop *interop;
    IMusicDisplayProperties *music = NULL;
    EventRegistrationToken token;
    HSTRING str;
    HWND hwnd;
    HRESULT hr;
    int i;

    hwnd = CreateWindowW( L"STATIC", L"gsmtc player", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL );
    str = hs( L"Windows.Media.SystemMediaTransportControls" );
    hr = RoGetActivationFactory( str, &IID_ISystemMediaTransportControlsInterop, (void **)&interop );
    WindowsDeleteString( str );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControlsInterop_GetForWindow( interop, hwnd, &IID_ISystemMediaTransportControls, (void **)&smtc );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControls_get_DisplayUpdater( smtc, &updater );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControls_add_ButtonPressed( smtc, &button_handler, &token );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControlsDisplayUpdater_put_Type( updater, MediaPlaybackType_Music );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControlsDisplayUpdater_get_MusicProperties( updater, &music );
    if (SUCCEEDED(hr))
    {
        str = hs( L"Gate Song" );
        IMusicDisplayProperties_put_Title( music, str );
        WindowsDeleteString( str );
        str = hs( L"Gate Artist" );
        IMusicDisplayProperties_put_Artist( music, str );
        WindowsDeleteString( str );
        IMusicDisplayProperties_Release( music );
        ISystemMediaTransportControls_put_IsPlayEnabled( smtc, TRUE );
        ISystemMediaTransportControls_put_IsPauseEnabled( smtc, TRUE );
        ISystemMediaTransportControls_put_IsNextEnabled( smtc, TRUE );
        ISystemMediaTransportControls_put_PlaybackStatus( smtc, MediaPlaybackStatus_Playing );
        ISystemMediaTransportControls_put_IsEnabled( smtc, TRUE );
        hr = ISystemMediaTransportControlsDisplayUpdater_Update( updater );
    }
    printf( "player=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;
    printf( "stage=ready\n" );
    for (i = 0; i < 1200 && GetFileAttributesW( L"C:\\stop" ) == INVALID_FILE_ATTRIBUTES; i++) Sleep( 100 );
    ISystemMediaTransportControls_put_IsEnabled( smtc, FALSE );
    ISystemMediaTransportControlsDisplayUpdater_Release( updater );
    ISystemMediaTransportControls_Release( smtc );
    printf( "player_done=1\n" );
    return 0;
}

/*
 * The manager's side
 */
static LONG changes;
static HANDLE change_event;

static HRESULT WINAPI changed_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    /* TypedEventHandler<GlobalSystemMediaTransportControlsSession, PlaybackInfoChangedEventArgs>: any IID
     * but IMarshal-like ones is fine for a probe; answer IUnknown, IAgileObject and anything else */
    *out = iface;
    return S_OK;
}
static ULONG WINAPI changed_AddRef( IUnknown *iface ) { return 2; }
static ULONG WINAPI changed_Release( IUnknown *iface ) { return 1; }
static HRESULT WINAPI changed_Invoke( IUnknown *iface, void *sender, void *args )
{
    HSTRING name = NULL;

    IInspectable_GetRuntimeClassName( (IInspectable *)args, &name );
    printf( "changed class=%ls sender=%d\n", name ? WindowsGetStringRawBuffer( name, NULL ) : L"", sender != NULL );
    WindowsDeleteString( name );
    InterlockedIncrement( &changes );
    SetEvent( change_event );
    return S_OK;
}
static void *changed_vtbl[] = { changed_QueryInterface, changed_AddRef, changed_Release, changed_Invoke };
static struct { void **vtbl; } changed_handler = { changed_vtbl };

static int playback_status( void *session, boolean *play, boolean *pause, boolean *next, boolean *previous )
{
    void *info = NULL, *controls = NULL;
    INT32 status = -1;

    if (FAILED(SLOT( session, 9, get_obj_fn )( session, &info )) || !info) return -1;
    SLOT( info, 7, get_int_fn )( info, &status );
    if (play && SUCCEEDED(SLOT( info, 6, get_obj_fn )( info, &controls )) && controls)
    {
        SLOT( controls, 6, get_bool_fn )( controls, play );
        SLOT( controls, 7, get_bool_fn )( controls, pause );
        SLOT( controls, 12, get_bool_fn )( controls, next );
        SLOT( controls, 13, get_bool_fn )( controls, previous );
        SLOT( controls, 2, release_fn )( controls );
    }
    SLOT( info, 2, release_fn )( info );
    return status;
}

static int manager( void )
{
    void *statics = NULL, *op = NULL, *mgr = NULL, *view = NULL, *session = NULL, *current = NULL, *props = NULL;
    boolean play = 0, pause = 0, next = 0, previous = 1, result = 0;
    EventRegistrationToken token;
    UINT32 count = 0, i;
    HSTRING str = NULL;
    HRESULT hr;
    int status, tries;

    change_event = CreateEventW( NULL, FALSE, FALSE, NULL );
    str = hs( L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager" );
    hr = RoGetActivationFactory( str, &IID_IManagerStatics_, &statics );
    WindowsDeleteString( str );
    printf( "factory=%#lx\n", hr );
    if (FAILED(hr)) { printf( "END\n" ); return 0; }
    hr = SLOT( statics, 6, get_obj_fn )( statics, &op );
    if (SUCCEEDED(hr)) hr = results( op, &mgr );
    printf( "request=%#lx manager=%d\n", hr, mgr != NULL );
    if (!mgr) { printf( "END\n" ); return 0; }

    /* the player's session: probe.exe */
    for (tries = 0; tries < 100 && !session; tries++)
    {
        if (view) { SLOT( view, 2, release_fn )( view ); view = NULL; }
        if (FAILED(SLOT( mgr, 7, get_obj_fn )( mgr, &view )) || !view) break;
        SLOT( view, 7, get_u32_fn )( view, &count );
        for (i = 0; i < count; i++)
        {
            void *s = NULL;
            if (FAILED(SLOT( view, 6, get_at_fn )( view, i, &s )) || !s) continue;
            SLOT( s, 6, get_hs_fn )( s, &str );
            if (!wcscmp( WindowsGetStringRawBuffer( str, NULL ), L"probe.exe" )) session = s;
            else SLOT( s, 2, release_fn )( s );
            WindowsDeleteString( str );
            str = NULL;
        }
        if (!session) Sleep( 100 );
    }
    printf( "sessions=%u found=%d\n", count, session != NULL );
    if (!session) { printf( "END\n" ); return 0; }
    SLOT( session, 6, get_hs_fn )( session, &str );
    print_hs( "aumid", str );
    WindowsDeleteString( str );

    SLOT( mgr, 6, get_obj_fn )( mgr, &current );
    printf( "current=%d\n", current == session );
    if (current) SLOT( current, 2, release_fn )( current );

    op = NULL;
    hr = SLOT( session, 7, get_obj_fn )( session, &op );
    if (SUCCEEDED(hr)) hr = results( op, &props );
    if (props)
    {
        SLOT( props, 6, get_hs_fn )( props, &str );
        print_hs( "title", str );
        WindowsDeleteString( str );
        SLOT( props, 9, get_hs_fn )( props, &str );
        print_hs( "artist", str );
        WindowsDeleteString( str );
        SLOT( props, 2, release_fn )( props );
    }
    else printf( "props=%#lx\n", hr );

    status = playback_status( session, &play, &pause, &next, &previous );
    printf( "status=%d play=%d pause=%d next=%d previous=%d\n", status, play, pause, next, previous );

    hr = SLOT( session, 27, add_handler_fn )( session, &changed_handler, &token );
    printf( "add_handler=%#lx\n", hr );
    op = NULL;
    hr = SLOT( session, 11, get_obj_fn )( session, &op );
    if (SUCCEEDED(hr)) hr = results( op, (void **)&result );   /* IAsyncOperation<boolean>: the value, not an object */
    printf( "pause_result=%d hr=%#lx\n", (BOOL)(result & 1), hr );
    for (tries = 0; tries < 50 && !changes; tries++) WaitForSingleObject( change_event, 100 );
    printf( "event=%d\n", changes > 0 );
    status = playback_status( session, NULL, NULL, NULL, NULL );
    printf( "status_after=%d\n", status );
    printf( "END\n" );
    return 0;
}

int main( int argc, char **argv )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_MULTITHREADED );
    if (argc > 1 && !strcmp( argv[1], "player" )) return player();
    return manager();
}
