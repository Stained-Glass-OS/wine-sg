/* wmedia-probe: Windows.Media leftovers (patches/sg/6514).
 *
 * SystemMediaTransportControlsDisplayUpdater::CopyFromFileAsync was a stub: it now reads the
 * file's tags. Writes small files with ID3v2.4, ID3v2.3, ID3v1, MP4, FLAC and Ogg tags (and one
 * with none), copies each through a window's transport controls and reads the music and video
 * properties and the thumbnail back. MediaDevice's DefaultAudioRender/CaptureDeviceChanged were
 * never raised: changes the user's default output in the driver's registry key and waits for them.
 *
 * Prints name=value lines; see test/wmedia-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#define WIDL_using_Windows_Media
#define WIDL_using_Windows_Media_Devices
#define WIDL_using_Windows_Storage
#define WIDL_using_Windows_Storage_Streams
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.media.devices.h>
#include <windows.storage.h>
#include <systemmediatransportcontrolsinterop.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
typedef HRESULT (WINAPI *get_results_fn)( void *, void ** );
typedef HRESULT (WINAPI *get_results_bool_fn)( void *, boolean * );
typedef ULONG (WINAPI *release_fn)( void * );
typedef HRESULT (WINAPI *from_path_fn)( void *, HSTRING, void ** );
typedef HRESULT (WINAPI *get_string_fn)( void *, HSTRING * );
/* (mingw's headers lack these) */
DEFINE_GUID(IID_IStorageFileStatics_, 0x5984c710, 0xdaf2, 0x43c8, 0x8b, 0xb4, 0xa4, 0xd3, 0xea, 0xcf, 0xd0, 0x3f);

static WCHAR dir[MAX_PATH];

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

/* ---- the test files ---- */

static BYTE buf[8192];
static int len;

static void put( const void *p, int n ) { memcpy( buf + len, p, n ); len += n; }
static void put8( BYTE b ) { buf[len++] = b; }
static void be32( DWORD v ) { put8( v >> 24 ); put8( v >> 16 ); put8( v >> 8 ); put8( v ); }
static void le32( DWORD v ) { put8( v ); put8( v >> 8 ); put8( v >> 16 ); put8( v >> 24 ); }
static void ss32( DWORD v ) { put8( (v >> 21) & 0x7f ); put8( (v >> 14) & 0x7f ); put8( (v >> 7) & 0x7f ); put8( v & 0x7f ); }

static const BYTE jpeg[] = { 0xff, 0xd8, 0xff, 0xe0, 0, 16, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0xff, 0xd9 };

static void write_file( const WCHAR *name )
{
    WCHAR path[MAX_PATH];
    HANDLE file;
    DWORD written;

    swprintf( path, MAX_PATH, L"%ls\\%ls", dir, name );
    file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( file, buf, len, &written, NULL );
    CloseHandle( file );
}

static void id3_frame( int version, const char *id, const void *data, int n )
{
    put( id, 4 );
    if (version == 4) ss32( n ); else be32( n );
    put8( 0 ); put8( 0 );
    put( data, n );
}

static void make_files( void )
{
    BYTE frame[512];
    int n, start, i;

    /* ID3v2.4 */
    len = 0;
    put( "ID3\x04\x00\x00", 6 );
    start = len; ss32( 0 );
    frame[0] = 3; memcpy( frame + 1, "Caf\xc3\xa9 Song", 10 ); id3_frame( 4, "TIT2", frame, 11 );
    frame[0] = 1; frame[1] = 0xff; frame[2] = 0xfe;
    n = 3; { const WCHAR *a = L"Artist \x03a9"; for (i = 0; a[i]; i++) { frame[n++] = a[i] & 0xff; frame[n++] = a[i] >> 8; } }
    id3_frame( 4, "TPE1", frame, n );
    frame[0] = 0; memcpy( frame + 1, "Album One", 9 ); id3_frame( 4, "TALB", frame, 10 );
    frame[0] = 0; memcpy( frame + 1, "The Band", 8 ); id3_frame( 4, "TPE2", frame, 9 );
    frame[0] = 0; memcpy( frame + 1, "7/12", 4 ); id3_frame( 4, "TRCK", frame, 5 );
    frame[0] = 0; memcpy( frame + 1, "(17)", 4 ); id3_frame( 4, "TCON", frame, 5 );
    frame[0] = 0; memcpy( frame + 1, "image/jpeg\0\x03\0", 13 ); memcpy( frame + 14, jpeg, sizeof(jpeg) );
    id3_frame( 4, "APIC", frame, 14 + sizeof(jpeg) );
    n = len - 10;
    len = start; ss32( n ); len = 10 + n;
    memset( buf + len, 0x55, 300 ); len += 300;
    write_file( L"v24.mp3" );

    /* ID3v2.3, a frame longer than 127 bytes (plain sizes) */
    len = 0;
    put( "ID3\x03\x00\x00", 6 );
    start = len; be32( 0 );
    frame[0] = 0; memset( frame + 1, 'L', 200 ); id3_frame( 3, "TIT2", frame, 201 );
    frame[0] = 0; memcpy( frame + 1, "Second", 6 ); id3_frame( 3, "TPE1", frame, 7 );
    n = len - 10;
    len = start; ss32( n ); len = 10 + n;
    memset( buf + len, 0x55, 300 ); len += 300;
    write_file( L"v23.mp3" );

    /* ID3v1 only */
    len = 0;
    memset( buf, 0x55, 400 ); len = 400;
    memset( buf + len, 0, 128 );
    memcpy( buf + len, "TAG", 3 );
    memcpy( buf + len + 3, "Old Title", 9 );
    memcpy( buf + len + 33, "Old Artist", 10 );
    memcpy( buf + len + 63, "Old Album", 9 );
    buf[len + 126] = 5;
    buf[len + 127] = 8;
    len += 128;
    write_file( L"v1.mp3" );

    /* MP4 (M4A): ftyp, moov/udta/meta/ilst */
    {
        static BYTE ilst[1024];
        int il = 0, item, mv;
#define ITEM_TEXT(type4, text) do { int tl = strlen(text); \
            int sz = 8 + 16 + tl; ilst[il++] = sz >> 24; ilst[il++] = sz >> 16; ilst[il++] = sz >> 8; ilst[il++] = sz; \
            memcpy( ilst + il, type4, 4 ); il += 4; \
            sz = 16 + tl; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = sz >> 8; ilst[il++] = sz; memcpy( ilst + il, "data", 4 ); il += 4; \
            ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 1; memset( ilst + il, 0, 4 ); il += 4; \
            memcpy( ilst + il, text, tl ); il += tl; } while (0)
        ITEM_TEXT( "\xa9nam", "M4A Title" );
        ITEM_TEXT( "\xa9" "ART", "M4A Artist" );
        ITEM_TEXT( "aART", "M4A Band" );
        item = il;
        /* trkn: binary 0 0 0 3 0 10 */
        ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 8 + 16 + 6; memcpy( ilst + il, "trkn", 4 ); il += 4;
        ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 16 + 6; memcpy( ilst + il, "data", 4 ); il += 4;
        memset( ilst + il, 0, 8 ); il += 8;
        ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 3; ilst[il++] = 0; ilst[il++] = 10;
        /* covr: JPEG (13) */
        ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 8 + 16 + sizeof(jpeg); memcpy( ilst + il, "covr", 4 ); il += 4;
        ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 16 + sizeof(jpeg); memcpy( ilst + il, "data", 4 ); il += 4;
        ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 0; ilst[il++] = 13; memset( ilst + il, 0, 4 ); il += 4;
        memcpy( ilst + il, jpeg, sizeof(jpeg) ); il += sizeof(jpeg);
        (void)item;

        len = 0;
        be32( 16 ); put( "ftypM4A \0\0\0\0", 12 );
        mv = 8 + 8 + 12 + 8 + il;     /* moov(udta(meta(ver + ilst))) */
        be32( mv ); put( "moov", 4 );
        be32( mv - 8 ); put( "udta", 4 );
        be32( mv - 16 ); put( "meta", 4 ); be32( 0 );
        be32( 8 + il ); put( "ilst", 4 );
        put( ilst, il );
        memset( buf + len, 0x55, 200 ); len += 200;
        write_file( L"tune.m4a" );

        /* the same as a video */
        len = 0;
        be32( 16 ); put( "ftypisom\0\0\0\0", 12 );
        il = 0;
        ITEM_TEXT( "\xa9nam", "Video Title" );
        mv = 8 + 8 + 12 + 8 + il;
        be32( mv ); put( "moov", 4 );
        be32( mv - 8 ); put( "udta", 4 );
        be32( mv - 16 ); put( "meta", 4 ); be32( 0 );
        be32( 8 + il ); put( "ilst", 4 );
        put( ilst, il );
        write_file( L"clip.mp4" );
    }

    /* FLAC: STREAMINFO, VORBIS_COMMENT, PICTURE (last) */
    {
        static const char *comments[] = { "TITLE=Flac Title", "ARTIST=Flac Artist", "ALBUM=Flac Album", "TRACKNUMBER=4", "GENRE=Ambient" };
        int c, block_start;

        len = 0;
        put( "fLaC", 4 );
        put8( 0 ); put8( 0 ); put8( 0 ); put8( 34 ); memset( buf + len, 0, 34 ); len += 34;
        put8( 4 ); block_start = len; put8( 0 ); put8( 0 ); put8( 0 );
        le32( 4 ); put( "test", 4 ); le32( ARRAY_SIZE(comments) );
        for (c = 0; c < ARRAY_SIZE(comments); c++) { le32( strlen( comments[c] ) ); put( comments[c], strlen( comments[c] ) ); }
        n = len - block_start - 3;
        buf[block_start] = n >> 16; buf[block_start + 1] = n >> 8; buf[block_start + 2] = n;
        put8( 0x86 ); block_start = len; put8( 0 ); put8( 0 ); put8( 0 );
        be32( 3 ); be32( 10 ); put( "image/jpeg", 10 ); be32( 0 ); be32( 1 ); be32( 1 ); be32( 24 ); be32( 0 );
        be32( sizeof(jpeg) ); put( jpeg, sizeof(jpeg) );
        n = len - block_start - 3;
        buf[block_start] = n >> 16; buf[block_start + 1] = n >> 8; buf[block_start + 2] = n;
        memset( buf + len, 0x55, 100 ); len += 100;
        write_file( L"song.flac" );
    }

    /* Ogg Vorbis: one page, identification and comment packets */
    {
        static const char *comments[] = { "title=Ogg Title", "artist=Ogg Artist" };
        BYTE ident[30] = { 1, 'v', 'o', 'r', 'b', 'i', 's' }, comment[200];
        int cl = 0, c;

        comment[cl++] = 3; memcpy( comment + cl, "vorbis", 6 ); cl += 6;
        comment[cl++] = 4; comment[cl++] = 0; comment[cl++] = 0; comment[cl++] = 0; memcpy( comment + cl, "test", 4 ); cl += 4;
        comment[cl++] = ARRAY_SIZE(comments); comment[cl++] = 0; comment[cl++] = 0; comment[cl++] = 0;
        for (c = 0; c < ARRAY_SIZE(comments); c++)
        {
            int l = strlen( comments[c] );
            comment[cl++] = l; comment[cl++] = 0; comment[cl++] = 0; comment[cl++] = 0;
            memcpy( comment + cl, comments[c], l ); cl += l;
        }
        comment[cl++] = 1;
        len = 0;
        put( "OggS", 4 ); put8( 0 ); put8( 2 ); memset( buf + len, 0, 8 + 4 + 4 + 4 ); len += 20;
        put8( 2 ); put8( sizeof(ident) ); put8( cl );
        put( ident, sizeof(ident) ); put( comment, cl );
        write_file( L"song.ogg" );
    }

    /* no tags at all */
    len = 0;
    memset( buf, 0x55, 500 ); len = 500;
    write_file( L"untagged.mp3" );
}

/* ---- through the transport controls ---- */

static ISystemMediaTransportControlsDisplayUpdater *updater;

static boolean copy_from( const WCHAR *name, MediaPlaybackType type )
{
    IActivationFactory *factory;
    void *statics;
    void *op;
    IAsyncOperation_boolean *bop;
    IStorageFile *file = NULL;
    WCHAR path[MAX_PATH];
    boolean result = FALSE;
    HSTRING str;
    HRESULT hr;

    swprintf( path, MAX_PATH, L"%ls\\%ls", dir, name );
    str = hs( L"Windows.Storage.StorageFile" );
    hr = RoGetActivationFactory( str, &IID_IStorageFileStatics_, &statics );
    WindowsDeleteString( str );
    if (FAILED(hr)) { printf( "storagefile hr=%#lx\n", hr ); return FALSE; }
    str = hs( path );
    hr = SLOT( statics, 6, from_path_fn )( statics, str, &op );
    WindowsDeleteString( str );
    SLOT( statics, 2, release_fn )( statics );
    if (FAILED(hr) || FAILED(wait_async( op )) || FAILED(SLOT( op, 8, get_results_fn )( op, (void **)&file )) || !file)
    {
        printf( "getfile %ls failed hr=%#lx\n", name, hr );
        return FALSE;
    }
    SLOT( op, 2, release_fn )( op );
    hr = ISystemMediaTransportControlsDisplayUpdater_CopyFromFileAsync( updater, type, file, &bop );
    if (SUCCEEDED(hr) && SUCCEEDED(wait_async( bop ))) SLOT( bop, 8, get_results_bool_fn )( bop, &result );
    else printf( "copy %ls hr=%#lx\n", name, hr );
    if (SUCCEEDED(hr)) SLOT( bop, 2, release_fn )( bop );
    IStorageFile_Release( file );
    (void)factory;
    return result;
}

static void music( WCHAR *title, WCHAR *artist, WCHAR *album, WCHAR *band, UINT32 *track, WCHAR *genre, BOOL *thumb )
{
    IMusicDisplayProperties *props;
    IMusicDisplayProperties2 *props2;
    void *thumbnail = NULL;
    HSTRING s;

    title[0] = artist[0] = album[0] = band[0] = genre[0] = 0;
    *track = 0;
    ISystemMediaTransportControlsDisplayUpdater_get_MusicProperties( updater, &props );
    IMusicDisplayProperties_get_Title( props, &s ); wcscpy( title, WindowsGetStringRawBuffer( s, NULL ) ); WindowsDeleteString( s );
    IMusicDisplayProperties_get_Artist( props, &s ); wcscpy( artist, WindowsGetStringRawBuffer( s, NULL ) ); WindowsDeleteString( s );
    IMusicDisplayProperties_get_AlbumArtist( props, &s ); wcscpy( band, WindowsGetStringRawBuffer( s, NULL ) ); WindowsDeleteString( s );
    if (SUCCEEDED(IMusicDisplayProperties_QueryInterface( props, &IID_IMusicDisplayProperties2, (void **)&props2 )))
    {
        IVector_HSTRING *genres;
        UINT32 count = 0;

        IMusicDisplayProperties2_get_AlbumTitle( props2, &s ); wcscpy( album, WindowsGetStringRawBuffer( s, NULL ) ); WindowsDeleteString( s );
        IMusicDisplayProperties2_get_TrackNumber( props2, track );
        IMusicDisplayProperties2_get_Genres( props2, &genres );
        IVector_HSTRING_get_Size( genres, &count );
        if (count) { IVector_HSTRING_GetAt( genres, 0, &s ); wcscpy( genre, WindowsGetStringRawBuffer( s, NULL ) ); WindowsDeleteString( s ); }
        IVector_HSTRING_Release( genres );
        IMusicDisplayProperties2_Release( props2 );
    }
    IMusicDisplayProperties_Release( props );
    ISystemMediaTransportControlsDisplayUpdater_get_Thumbnail( updater, (void *)&thumbnail );
    *thumb = thumbnail != NULL;
    if (thumbnail) SLOT( thumbnail, 2, release_fn )( thumbnail );
}

static void check_music( const char *name, const WCHAR *file, const WCHAR *want_title, const WCHAR *want_artist,
                         const WCHAR *want_album, const WCHAR *want_band, UINT32 want_track, const WCHAR *want_genre, BOOL want_thumb )
{
    WCHAR title[512], artist[256], album[256], band[256], genre[256];
    UINT32 track;
    BOOL thumb, ok;

    ok = copy_from( file, MediaPlaybackType_Music );
    music( title, artist, album, band, &track, genre, &thumb );
    ok = ok && !wcscmp( title, want_title ) && !wcscmp( artist, want_artist ) && !wcscmp( album, want_album ) &&
         !wcscmp( band, want_band ) && track == want_track && !wcscmp( genre, want_genre ) && thumb == want_thumb;
    printf( "%s=%d title=%.40ls artist=%ls album=%ls band=%ls track=%u genre=%ls thumb=%d\n", name, ok, title, artist,
            album, band, track, genre, thumb );
}

/* ---- default device events ---- */

static LONG render_events, capture_events;
static WCHAR last_id[256];
static AudioDeviceRole last_role = -1;

struct handler { void *vtbl; };

static HRESULT WINAPI handler_QueryInterface( void *iface, REFIID iid, void **out ) { *out = iface; return S_OK; }
static ULONG WINAPI handler_AddRef( void *iface ) { return 2; }
static ULONG WINAPI handler_Release( void *iface ) { return 1; }
static HRESULT WINAPI handler_Invoke( void *iface, IInspectable *sender, IDefaultAudioDeviceChangedEventArgs *args, LONG *counter )
{
    HSTRING id;
    IDefaultAudioDeviceChangedEventArgs_get_Id( args, &id );
    lstrcpynW( last_id, WindowsGetStringRawBuffer( id, NULL ), 256 );
    WindowsDeleteString( id );
    IDefaultAudioDeviceChangedEventArgs_get_Role( args, &last_role );
    InterlockedIncrement( counter );
    return S_OK;
}
static HRESULT WINAPI render_Invoke( void *iface, IInspectable *sender, IDefaultAudioDeviceChangedEventArgs *args )
{
    return handler_Invoke( iface, sender, args, &render_events );
}
static HRESULT WINAPI capture_Invoke( void *iface, IInspectable *sender, IDefaultAudioDeviceChangedEventArgs *args )
{
    return handler_Invoke( iface, sender, args, &capture_events );
}
static void *render_vtbl[] = { handler_QueryInterface, handler_AddRef, handler_Release, render_Invoke };
static void *capture_vtbl[] = { handler_QueryInterface, handler_AddRef, handler_Release, capture_Invoke };
static struct handler render_handler = { render_vtbl }, capture_handler = { capture_vtbl };

static BOOL wait_for( LONG *counter, LONG value )
{
    int i;
    for (i = 0; i < 50 && *counter < value; i++) Sleep( 100 );
    return *counter >= value;
}

static void device_events( void )
{
    IMediaDeviceStatics *statics;
    EventRegistrationToken render_token, capture_token;
    WCHAR key_name[MAX_PATH], subkey[64];
    HSTRING str;
    HKEY drivers, key = NULL;
    DWORD i, size;
    HRESULT hr;

    str = hs( L"Windows.Media.Devices.MediaDevice" );
    hr = RoGetActivationFactory( str, &IID_IMediaDeviceStatics, (void **)&statics );
    WindowsDeleteString( str );
    if (FAILED(hr)) { printf( "device_statics=0 hr=%#lx\n", hr ); return; }
    IMediaDeviceStatics_add_DefaultAudioRenderDeviceChanged( statics, (void *)&render_handler, &render_token );
    IMediaDeviceStatics_add_DefaultAudioCaptureDeviceChanged( statics, (void *)&capture_handler, &capture_token );

    /* the driver's key: HKCU\Software\Wine\Drivers\<module> (made when mmdevapi loads a driver) */
    if (!RegOpenKeyExW( HKEY_CURRENT_USER, L"Software\\Wine\\Drivers", 0, KEY_READ, &drivers ))
    {
        for (i = 0; ; i++)
        {
            size = ARRAY_SIZE(subkey);
            if (RegEnumKeyExW( drivers, i, subkey, &size, NULL, NULL, NULL, NULL )) break;
            if (wcsstr( subkey, L".drv" ))
            {
                swprintf( key_name, MAX_PATH, L"Software\\Wine\\Drivers\\%ls", subkey );
                RegOpenKeyExW( HKEY_CURRENT_USER, key_name, 0, KEY_SET_VALUE, &key );
                break;
            }
        }
        RegCloseKey( drivers );
    }
    printf( "driver_key=%d %ls\n", key != NULL, key ? subkey : L"" );
    if (!key) return;
    Sleep( 1000 );  /* mmdevapi's notification thread reads the current values first */

    RegSetValueExW( key, L"DefaultOutput", 0, REG_SZ, (BYTE *)L"{0.0.0.00000000}.{11111111-2222-3333-4444-555555555555}",
                    sizeof(L"{0.0.0.00000000}.{11111111-2222-3333-4444-555555555555}") );
    {
        BOOL got = wait_for( &render_events, 1 );
        printf( "render_event=%d events=%ld role=%d id=%ls\n", got && last_role == AudioDeviceRole_Default &&
                wcsstr( last_id, L"{0.0.0.00000000}.{11111111-2222-3333-4444-555555555555}" ) &&
                !wcsncmp( last_id, L"\\\\?\\SWD#MMDEVAPI#", 17 ), render_events, last_role, last_id );
    }
    Sleep( 300 );
    printf( "render_once=%d events=%ld\n", render_events == 1, render_events );
    RegSetValueExW( key, L"DefaultVoiceInput", 0, REG_SZ, (BYTE *)L"{0.0.1.00000000}.{66666666-2222-3333-4444-555555555555}",
                    sizeof(L"{0.0.1.00000000}.{66666666-2222-3333-4444-555555555555}") );
    {
        BOOL got = wait_for( &capture_events, 1 );
        printf( "capture_event=%d events=%ld role=%d id=%ls\n", got && last_role == AudioDeviceRole_Communications &&
                wcsstr( last_id, L"66666666" ) != NULL, capture_events, last_role, last_id );
    }
    IMediaDeviceStatics_remove_DefaultAudioRenderDeviceChanged( statics, render_token );
    RegDeleteValueW( key, L"DefaultOutput" );
    Sleep( 500 );
    printf( "removed=%d events=%ld\n", render_events == 1, render_events );
    RegDeleteValueW( key, L"DefaultVoiceInput" );
    RegCloseKey( key );
    IMediaDeviceStatics_Release( statics );
}

int main( void )
{
    ISystemMediaTransportControlsInterop *interop;
    ISystemMediaTransportControls *smtc;
    void *video;
    HSTRING str;
    HWND hwnd;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_MULTITHREADED );
    GetTempPathW( MAX_PATH, dir );
    wcscat( dir, L"wmedia" );
    CreateDirectoryW( dir, NULL );
    make_files();

    hwnd = CreateWindowW( L"STATIC", L"wmedia", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL );
    str = hs( L"Windows.Media.SystemMediaTransportControls" );
    hr = RoGetActivationFactory( str, &IID_ISystemMediaTransportControlsInterop, (void **)&interop );
    WindowsDeleteString( str );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControlsInterop_GetForWindow( interop, hwnd, &IID_ISystemMediaTransportControls, (void **)&smtc );
    if (SUCCEEDED(hr)) hr = ISystemMediaTransportControls_get_DisplayUpdater( smtc, &updater );
    printf( "updater=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;

    check_music( "id3v24", L"v24.mp3", L"Caf\xe9 Song", L"Artist \x03a9", L"Album One", L"The Band", 7, L"Rock", TRUE );
    {
        WCHAR longt[201];
        wmemset( longt, 'L', 200 ); longt[200] = 0;
        check_music( "id3v23", L"v23.mp3", longt, L"Second", L"", L"", 0, L"", FALSE );
    }
    check_music( "id3v1", L"v1.mp3", L"Old Title", L"Old Artist", L"Old Album", L"", 5, L"Jazz", FALSE );
    check_music( "mp4", L"tune.m4a", L"M4A Title", L"M4A Artist", L"", L"M4A Band", 3, L"", TRUE );
    check_music( "flac", L"song.flac", L"Flac Title", L"Flac Artist", L"Flac Album", L"", 4, L"Ambient", TRUE );
    check_music( "ogg", L"song.ogg", L"Ogg Title", L"Ogg Artist", L"", L"", 0, L"", FALSE );
    check_music( "untagged", L"untagged.mp3", L"untagged", L"", L"", L"", 0, L"", FALSE );

    {
        boolean ok = copy_from( L"clip.mp4", MediaPlaybackType_Video );
        MediaPlaybackType type = 0;
        HSTRING title = NULL;
        ISystemMediaTransportControlsDisplayUpdater_get_VideoProperties( updater, (void *)&video );
        SLOT( video, 6, get_string_fn )( video, &title );
        ISystemMediaTransportControlsDisplayUpdater_get_Type( updater, &type );
        printf( "video=%d title=%ls type=%d\n", ok && !wcscmp( WindowsGetStringRawBuffer( title, NULL ), L"Video Title" ) &&
                type == MediaPlaybackType_Video, WindowsGetStringRawBuffer( title, NULL ), type );
        WindowsDeleteString( title );
        SLOT( video, 2, release_fn )( video );
    }
    {
        boolean ok = TRUE;
        IAsyncOperation_boolean *bop = NULL;
        hr = ISystemMediaTransportControlsDisplayUpdater_CopyFromFileAsync( updater, MediaPlaybackType_Music, NULL, &bop );
        ok = FAILED(hr);
        printf( "null_file=%d hr=%#lx\n", ok, hr );
    }

    device_events();
    printf( "done=1\n" );
    return 0;
}
