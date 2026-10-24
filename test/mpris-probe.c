/* mpris-probe: the transport controls on the desktop (patches/sg/6523).
 *
 * Sets up a window's SystemMediaTransportControls the way a player does: enabled, Playing, Play,
 * Pause and Next enabled, the music properties and cover from a tagged file, Update(). The gate
 * reads the MPRIS player on its own session bus and presses buttons; each comes back here as
 * ButtonPressed. Then Paused (the gate presses PlayPause: Play), then disabled (off the bus).
 * "probe nobus": the same without a session bus, which must change nothing.
 *
 * Prints name=value lines; see test/mpris-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Media
#define WIDL_using_Windows_Storage
#define WIDL_using_Windows_Storage_Streams
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.storage.h>
#include <systemmediatransportcontrolsinterop.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
typedef HRESULT (WINAPI *get_results_fn)( void *, void ** );
typedef HRESULT (WINAPI *get_results_bool_fn)( void *, boolean * );
typedef ULONG (WINAPI *release_fn)( void * );
typedef HRESULT (WINAPI *from_path_fn)( void *, HSTRING, void ** );
/* (mingw's headers lack these) */
DEFINE_GUID(IID_IStorageFileStatics_, 0x5984c710, 0xdaf2, 0x43c8, 0x8b, 0xb4, 0xa4, 0xd3, 0xea, 0xcf, 0xd0, 0x3f);

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, wcslen( s ), &h );
    return h;
}

static HRESULT wait_async( void *op )
{
    IAsyncInfo *info;
    AsyncStatus status = Started;
    HRESULT hr = E_FAIL;
    int i;

    if (FAILED(IUnknown_QueryInterface( (IUnknown *)op, &IID_IAsyncInfo, (void **)&info ))) return E_NOINTERFACE;
    for (i = 0; i < 1000; i++)
    {
        IAsyncInfo_get_Status( info, &status );
        if (status != Started) break;
        Sleep( 10 );
    }
    if (status == Completed) hr = S_OK;
    else if (status == Error) IAsyncInfo_get_ErrorCode( info, &hr );
    IAsyncInfo_Release( info );
    return hr;
}

/* a tagged MP3: ID3v2.3 title, artist, album, album artist, track and a JPEG cover */
static BYTE buf[4096];
static int len;
static void put( const void *p, int n ) { memcpy( buf + len, p, n ); len += n; }
static void be32( DWORD v ) { buf[len++] = v >> 24; buf[len++] = v >> 16; buf[len++] = v >> 8; buf[len++] = v; }
static void frame( const char *id, const void *data, int n ) { put( id, 4 ); be32( n ); put( "\0\0", 2 ); put( data, n ); }
static void text( const char *id, const char *s )
{
    BYTE f[256];
    f[0] = 3;   /* UTF-8 (the reader takes it in 2.3 too) */
    memcpy( f + 1, s, strlen( s ) );
    frame( id, f, 1 + strlen( s ) );
}
static const BYTE jpeg[] = { 0xff, 0xd8, 0xff, 0xe0, 0, 16, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0xff, 0xd9 };

static void make_file( const WCHAR *path )
{
    BYTE f[256];
    DWORD written, n;
    HANDLE file;

    len = 0;
    put( "ID3\x03\x00\x00", 6 );
    be32( 0 );
    text( "TIT2", "Probe Song \xc3\xa9" );
    text( "TPE1", "Probe Artist" );
    text( "TALB", "Probe Album" );
    text( "TPE2", "Probe Band" );
    text( "TRCK", "4/9" );
    memcpy( f, "\0image/jpeg\0\x03\0", 14 );
    memcpy( f + 14, jpeg, sizeof(jpeg) );
    frame( "APIC", f, 14 + sizeof(jpeg) );
    n = len - 10;
    buf[6] = (n >> 21) & 0x7f; buf[7] = (n >> 14) & 0x7f; buf[8] = (n >> 7) & 0x7f; buf[9] = n & 0x7f;
    memset( buf + len, 0x55, 300 ); len += 300;
    file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( file, buf, len, &written, NULL );
    CloseHandle( file );
}

static boolean copy_from( ISystemMediaTransportControlsDisplayUpdater *updater, const WCHAR *path )
{
    IAsyncOperation_boolean *bop;
    IStorageFile *file = NULL;
    boolean result = FALSE;
    void *statics, *op;
    HSTRING str;
    HRESULT hr;

    str = hs( L"Windows.Storage.StorageFile" );
    hr = RoGetActivationFactory( str, &IID_IStorageFileStatics_, &statics );
    WindowsDeleteString( str );
    if (FAILED(hr)) return FALSE;
    str = hs( path );
    hr = SLOT( statics, 6, from_path_fn )( statics, str, &op );
    WindowsDeleteString( str );
    SLOT( statics, 2, release_fn )( statics );
    if (FAILED(hr) || FAILED(wait_async( op )) || FAILED(SLOT( op, 8, get_results_fn )( op, (void **)&file )) || !file) return FALSE;
    SLOT( op, 2, release_fn )( op );
    hr = ISystemMediaTransportControlsDisplayUpdater_CopyFromFileAsync( updater, MediaPlaybackType_Music, file, &bop );
    if (SUCCEEDED(hr) && SUCCEEDED(wait_async( bop ))) SLOT( bop, 8, get_results_bool_fn )( bop, &result );
    if (SUCCEEDED(hr)) SLOT( bop, 2, release_fn )( bop );
    IStorageFile_Release( file );
    return result;
}

/* the ButtonPressed handler */
static LONG presses;
static SystemMediaTransportControlsButton pressed[16];
static DWORD press_thread[16];
static HANDLE press_event;

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
    SystemMediaTransportControlsButton button = -1;
    HSTRING name = NULL;
    LONG n;

    if (FAILED(ISystemMediaTransportControlsButtonPressedEventArgs_get_Button( args, &button ))) button = -2;
    IInspectable_GetRuntimeClassName( (IInspectable *)args, &name );
    printf( "pressed button=%d sender=%d class=%ls\n", button, sender != NULL, WindowsGetStringRawBuffer( name, NULL ) );
    WindowsDeleteString( name );
    n = InterlockedIncrement( &presses ) - 1;
    if (n < 16) { pressed[n] = button; press_thread[n] = GetCurrentThreadId(); }
    SetEvent( press_event );
    return S_OK;
}
static ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgsVtbl handler_vtbl =
{
    handler_QueryInterface, handler_AddRef, handler_Release, handler_Invoke,
};
static ITypedEventHandler_SystemMediaTransportControls_SystemMediaTransportControlsButtonPressedEventArgs handler = { &handler_vtbl };

static BOOL wait_presses( LONG count, DWORD ms )
{
    DWORD start = GetTickCount();
    while (presses < count && GetTickCount() - start < ms) WaitForSingleObject( press_event, 200 );
    return presses >= count;
}

static BOOL wait_file( const WCHAR *path, DWORD ms )
{
    DWORD start = GetTickCount();
    while (GetFileAttributesW( path ) == INVALID_FILE_ATTRIBUTES)
    {
        if (GetTickCount() - start > ms) return FALSE;
        Sleep( 100 );
    }
    return TRUE;
}

int main( int argc, char **argv )
{
    ISystemMediaTransportControlsDisplayUpdater *updater;
    ISystemMediaTransportControlsInterop *interop;
    ISystemMediaTransportControls *smtc;
    EventRegistrationToken token;
    HSTRING str;
    HWND hwnd;
    HRESULT hr;
    BOOL ok;

    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_MULTITHREADED );
    press_event = CreateEventW( NULL, FALSE, FALSE, NULL );
    make_file( L"C:\\song.mp3" );

    hwnd = CreateWindowW( L"STATIC", L"mpris", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL );
    str = hs( L"Windows.Media.SystemMediaTransportControls" );
    hr = RoGetActivationFactory( str, &IID_ISystemMediaTransportControlsInterop, (void **)&interop );
    WindowsDeleteString( str );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControlsInterop_GetForWindow( interop, hwnd, &IID_ISystemMediaTransportControls, (void **)&smtc );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControls_get_DisplayUpdater( smtc, &updater );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControls_add_ButtonPressed( smtc, &handler, &token );
    printf( "controls=%d hr=%#lx pid=%lu\n", SUCCEEDED(hr), hr, GetCurrentProcessId() );
    if (FAILED(hr)) return 1;

    /* disabled: nothing on the bus yet */
    ISystemMediaTransportControls_put_PlaybackStatus( smtc, MediaPlaybackStatus_Playing );
    printf( "stage=disabled\n" );
    Sleep( 1500 );

    ok = copy_from( updater, L"C:\\song.mp3" );
    ISystemMediaTransportControls_put_IsPlayEnabled( smtc, TRUE );
    ISystemMediaTransportControls_put_IsPauseEnabled( smtc, TRUE );
    ISystemMediaTransportControls_put_IsNextEnabled( smtc, TRUE );
    ISystemMediaTransportControls_put_IsEnabled( smtc, TRUE );
    hr = ISystemMediaTransportControlsDisplayUpdater_Update( updater );
    printf( "copied=%d update=%#lx\n", ok, hr );
    printf( "stage=ready\n" );
    if (argc > 1 && !strcmp( argv[1], "nobus" ))
    {
        /* no session bus: everything works as before, nothing is published */
        Sleep( 1500 );
        ISystemMediaTransportControls_put_PlaybackStatus( smtc, MediaPlaybackStatus_Paused );
        ISystemMediaTransportControls_put_IsEnabled( smtc, FALSE );
        ISystemMediaTransportControlsDisplayUpdater_Release( updater );
        ISystemMediaTransportControls_Release( smtc );
        printf( "nobus=%d\n", SUCCEEDED(hr) && ok );
        return 0;
    }

    /* the gate presses PlayPause (Playing: Pause), then Next */
    ok = wait_presses( 2, 60000 );
    printf( "pause=%d\n", ok && pressed[0] == SystemMediaTransportControlsButton_Pause );
    printf( "next=%d\n", ok && pressed[1] == SystemMediaTransportControlsButton_Next );
    printf( "other_thread=%d\n", ok && press_thread[0] != GetCurrentThreadId() );

    ISystemMediaTransportControls_put_PlaybackStatus( smtc, MediaPlaybackStatus_Paused );
    printf( "stage=paused\n" );
    /* PlayPause again: Play; Previous is not enabled, so the gate's press of it is not raised */
    ok = wait_presses( 3, 60000 );
    printf( "play=%d\n", ok && pressed[2] == SystemMediaTransportControlsButton_Play );
    wait_file( L"C:\\go", 60000 );
    Sleep( 500 );
    printf( "no_previous=%d presses=%ld\n", presses == 3, presses );

    ISystemMediaTransportControls_put_IsEnabled( smtc, FALSE );
    printf( "stage=off\n" );
    wait_file( L"C:\\go2", 60000 );
    ISystemMediaTransportControls_remove_ButtonPressed( smtc, token );
    ISystemMediaTransportControlsDisplayUpdater_Release( updater );
    ISystemMediaTransportControls_Release( smtc );
    ISystemMediaTransportControlsInterop_Release( interop );
    printf( "done=1\n" );
    return 0;
}
