/* bgplayer-probe: a desktop player's transport controls through BackgroundMediaPlayer
 * (patches/sg/6536), as foobar2000 gets them: BackgroundMediaPlayer.Current is one MediaPlayer,
 * its SystemMediaTransportControls are one object, and they reach the bus (MPRIS) like a window's;
 * a window's GetForWindow gives the same controls each time.
 * Prints name=value lines; see test/bgplayer-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Media
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <systemmediatransportcontrolsinterop.h>
#include <stdio.h>
#include <wchar.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
typedef HRESULT (WINAPI *get_obj_fn)( void *, void ** );
/* Windows.Media.Playback (Wine's include/windows.media.playback.idl; mingw's headers lack it) */
DEFINE_GUID(IID_IBackgroundMediaPlayerStatics_, 0x856ddbc1, 0x55f7, 0x471f, 0xa0, 0xf2, 0x68, 0xac, 0x4c, 0x90, 0x45, 0x92);
DEFINE_GUID(IID_IMediaPlayer2_, 0x3c841218, 0x2123, 0x4fc5, 0x90, 0x82, 0x2f, 0x88, 0x3f, 0x77, 0xbd, 0xf5);

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, wcslen( s ), &h );
    return h;
}

static HANDLE press_event;
static LONG last_button = -1;

static HRESULT WINAPI handler_QueryInterface( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface, REFIID iid, void **out )
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
static ULONG WINAPI handler_AddRef( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface ) { return 2; }
static ULONG WINAPI handler_Release( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface ) { return 1; }
static HRESULT WINAPI handler_Invoke( ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs *iface,
                                      ISystemMediaTransportControls *sender, ISystemMediaTransportControlsButtonPressedEventArgs *args )
{
    SystemMediaTransportControlsButton button;
    if (SUCCEEDED(ISystemMediaTransportControlsButtonPressedEventArgs_get_Button( args, &button )))
    {
        last_button = button;
        SetEvent( press_event );
    }
    return S_OK;
}
static ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgsVtbl handler_vtbl =
{
    handler_QueryInterface, handler_AddRef, handler_Release, handler_Invoke,
};
static ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs handler = { &handler_vtbl };

int main( void )
{
    ISystemMediaTransportControlsInterop *interop;
    ISystemMediaTransportControls *smtc = NULL, *smtc2 = NULL, *w1 = NULL, *w2 = NULL;
    void *statics = NULL, *player = NULL, *player_again = NULL, *player2 = NULL;
    EventRegistrationToken token;
    HSTRING str;
    HWND hwnd;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_MULTITHREADED );
    press_event = CreateEventW( NULL, FALSE, FALSE, NULL );

    /* a window's controls: the same object each time */
    hwnd = CreateWindowW( L"STATIC", L"bgplayer", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL );
    str = hs( L"Windows.Media.SystemMediaTransportControls" );
    hr = RoGetActivationFactory( str, &IID_ISystemMediaTransportControlsInterop, (void **)&interop );
    WindowsDeleteString( str );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControlsInterop_GetForWindow( interop, hwnd, &IID_ISystemMediaTransportControls, (void **)&w1 );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControlsInterop_GetForWindow( interop, hwnd, &IID_ISystemMediaTransportControls, (void **)&w2 );
    printf( "window_same=%d\n", SUCCEEDED(hr) && w1 && w1 == w2 );
    if (w1) ISystemMediaTransportControls_Release( w1 );
    if (w2) ISystemMediaTransportControls_Release( w2 );

    /* BackgroundMediaPlayer.Current: one player, one set of controls */
    str = hs( L"Windows.Media.Playback.BackgroundMediaPlayer" );
    hr = RoGetActivationFactory( str, &IID_IBackgroundMediaPlayerStatics_, &statics );
    WindowsDeleteString( str );
    printf( "statics=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;
    hr = SLOT( statics, 6, get_obj_fn )( statics, &player );            /* get_Current */
    if (SUCCEEDED(hr)) hr = SLOT( statics, 6, get_obj_fn )( statics, &player_again );
    printf( "current=%d same_current=%d\n", SUCCEEDED(hr), SUCCEEDED(hr) && player == player_again );
    if (FAILED(hr)) return 1;
    hr = IUnknown_QueryInterface( (IUnknown *)player, &IID_IMediaPlayer2_, &player2 );
    if (SUCCEEDED(hr)) hr = SLOT( player2, 6, get_obj_fn )( player2, (void **)&smtc );   /* get_SystemMediaTransportControls */
    if (SUCCEEDED(hr)) hr = SLOT( player2, 6, get_obj_fn )( player2, (void **)&smtc2 );
    printf( "controls=%d same_controls=%d pid=%lu\n", SUCCEEDED(hr), SUCCEEDED(hr) && smtc == smtc2, GetCurrentProcessId() );
    if (FAILED(hr)) return 1;

    ISystemMediaTransportControls_add_ButtonPressed( smtc, &handler, &token );
    ISystemMediaTransportControls_put_IsPlayEnabled( smtc, TRUE );
    ISystemMediaTransportControls_put_IsPauseEnabled( smtc, TRUE );
    ISystemMediaTransportControls_put_IsEnabled( smtc, TRUE );
    ISystemMediaTransportControls_put_PlaybackStatus( smtc, MediaPlaybackStatus_Playing );
    printf( "stage=ready\n" );

    /* the gate presses PlayPause on the bus: Pause comes back */
    WaitForSingleObject( press_event, 60000 );
    printf( "pause=%d\n", last_button == SystemMediaTransportControlsButton_Pause );
    ISystemMediaTransportControls_put_IsEnabled( smtc, FALSE );
    printf( "done=1\n" );
    return 0;
}
