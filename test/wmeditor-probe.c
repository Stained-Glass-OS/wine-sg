/* wmeditor-probe: the Windows Media metadata editor (patches/sg/6515).
 *
 * WMCreateEditor was a stub. Writes a small ASF file (header with a content description and
 * extended descriptors, a data object and payload bytes), opens it in the editor, reads,
 * changes, adds and deletes attributes, flushes, and checks the file: the new values on a
 * second open, the payload after the header unchanged, the file size in the file properties;
 * then changes without a flush are dropped.
 *
 * Prints name=value lines; see test/wmeditor-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdkidl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID header_guid = {0x75b22630, 0x668e, 0x11cf, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static const GUID fpo_guid = {0x8cabdca1, 0xa947, 0x11cf, {0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}};
static const GUID cdo_guid = {0x75b22633, 0x668e, 0x11cf, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static const GUID ecd_guid = {0xd2d0a440, 0xe307, 0x11d2, {0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50}};
static const GUID data_guid = {0x75b22636, 0x668e, 0x11cf, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static HRESULT (WINAPI *pWMCreateEditor)( IWMMetadataEditor ** );
#define WMCreateEditor pWMCreateEditor
static const char payload[] = "SG-PAYLOAD-0123456789-SG-PAYLOAD";

static BYTE buf[4096];
static int len;
static void put( const void *p, int n ) { memcpy( buf + len, p, n ); len += n; }
static void put16( WORD v ) { put( &v, 2 ); }
static void put32( DWORD v ) { put( &v, 4 ); }
static void put64( UINT64 v ) { put( &v, 8 ); }
static void putws( const WCHAR *s ) { put( s, (wcslen( s ) + 1) * 2 ); }

static void write_asf( const WCHAR *path )
{
    int header_size_at, count = 0, start, fpo_size_at, data_start;
    HANDLE file;
    DWORD written;
    UINT64 size;

    len = 0;
    put( &header_guid, 16 ); header_size_at = len; put64( 0 ); put32( 0 ); buf[len++] = 1; buf[len++] = 2;

    /* file properties: file id, file size, creation, packets, durations, preroll, flags, packet sizes, bitrate */
    start = len; put( &fpo_guid, 16 ); put64( 104 );
    memset( buf + len, 0x11, 16 ); len += 16; fpo_size_at = len; put64( 0 ); put64( 0 ); put64( 0 );
    put64( 10000000 ); put64( 10000000 ); put64( 0 ); put32( 2 ); put32( 512 ); put32( 512 ); put32( 128000 );
    count++;
    (void)start;

    /* content description: Title, Author */
    start = len; put( &cdo_guid, 16 ); put64( 0 );
    put16( sizeof(L"Old Title") ); put16( sizeof(L"Old Author") ); put16( 0 ); put16( 0 ); put16( 0 );
    putws( L"Old Title" ); putws( L"Old Author" );
    size = len - start; memcpy( buf + start + 16, &size, 8 ); count++;

    /* extended descriptors: WM/AlbumTitle (string), WM/TrackNumber (DWORD), WM/Flag (BOOL) */
    start = len; put( &ecd_guid, 16 ); put64( 0 );
    put16( 3 );
    put16( sizeof(L"WM/AlbumTitle") ); putws( L"WM/AlbumTitle" ); put16( 0 ); put16( sizeof(L"Old Album") ); putws( L"Old Album" );
    put16( sizeof(L"WM/TrackNumber") ); putws( L"WM/TrackNumber" ); put16( 3 ); put16( 4 ); put32( 3 );
    put16( sizeof(L"WM/Flag") ); putws( L"WM/Flag" ); put16( 2 ); put16( 4 ); put32( 1 );
    size = len - start; memcpy( buf + start + 16, &size, 8 ); count++;

    size = len; memcpy( buf + header_size_at, &size, 8 ); memcpy( buf + header_size_at + 8, &count, 4 );

    /* data object, no packets, then payload bytes */
    data_start = len; put( &data_guid, 16 ); put64( 50 ); memset( buf + len, 0x11, 16 ); len += 16; put64( 0 ); buf[len++] = 1; buf[len++] = 1;
    (void)data_start;
    put( payload, sizeof(payload) );
    size = len; memcpy( buf + fpo_size_at, &size, 8 );

    file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( file, buf, len, &written, NULL );
    CloseHandle( file );
}

static BOOL get_string( IWMHeaderInfo3 *info, const WCHAR *name, WCHAR *out, int out_size )
{
    WMT_ATTR_DATATYPE type;
    WORD stream = 0, size = out_size * 2;

    out[0] = 0;
    return SUCCEEDED(IWMHeaderInfo3_GetAttributeByName( info, &stream, name, &type, (BYTE *)out, &size )) && type == WMT_TYPE_STRING;
}

static DWORD get_dword( IWMHeaderInfo3 *info, const WCHAR *name, WMT_ATTR_DATATYPE want )
{
    WMT_ATTR_DATATYPE type;
    WORD stream = 0, size = 4;
    DWORD value = 0xdeadbeef;

    if (FAILED(IWMHeaderInfo3_GetAttributeByName( info, &stream, name, &type, (BYTE *)&value, &size )) || type != want)
        return 0xdeadbeef;
    return value;
}

static IWMHeaderInfo3 *open_editor( IWMMetadataEditor **editor, const WCHAR *path, HRESULT *hr )
{
    IWMHeaderInfo3 *info = NULL;

    *hr = WMCreateEditor( editor );
    if (FAILED(*hr)) return NULL;
    if (FAILED(*hr = IWMMetadataEditor_Open( *editor, path ))) return NULL;
    IWMMetadataEditor_QueryInterface( *editor, &IID_IWMHeaderInfo3, (void **)&info );
    return info;
}

static int find_index( IWMHeaderInfo3 *info, const WCHAR *name )
{
    WORD indices[8], count = 8;
    if (FAILED(IWMHeaderInfo3_GetAttributeIndices( info, 0, name, NULL, indices, &count )) || !count) return -1;
    return indices[0];
}

int main( void )
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], str[256];
    IWMMetadataEditor *editor;
    IWMHeaderInfo3 *info;
    HRESULT hr;
    HANDLE file;
    DWORD size, read;
    BYTE *data;
    BOOL ok;
    int index;

    setvbuf( stdout, NULL, _IONBF, 0 );
    pWMCreateEditor = (void *)GetProcAddress( LoadLibraryA( "wmvcore.dll" ), "WMCreateEditor" );
    if (!pWMCreateEditor) { printf( "open=0 no WMCreateEditor\n" ); return 1; }
    GetTempPathW( MAX_PATH, dir );
    swprintf( path, MAX_PATH, L"%lswmeditor.wma", dir );
    write_asf( path );

    /* reading */
    info = open_editor( &editor, path, &hr );
    printf( "open=%d hr=%#lx\n", info != NULL, hr );
    if (!info) return 1;
    ok = get_string( info, L"Title", str, 256 ) && !wcscmp( str, L"Old Title" );
    printf( "read_title=%d title=%ls\n", ok, str );
    ok = get_string( info, L"Author", str, 256 ) && !wcscmp( str, L"Old Author" );
    printf( "read_author=%d\n", ok );
    ok = get_string( info, L"WM/AlbumTitle", str, 256 ) && !wcscmp( str, L"Old Album" );
    printf( "read_extended=%d album=%ls\n", ok, str );
    printf( "read_types=%d track=%lu flag=%lu\n", get_dword( info, L"WM/TrackNumber", WMT_TYPE_DWORD ) == 3 &&
            get_dword( info, L"WM/Flag", WMT_TYPE_BOOL ) == 1, get_dword( info, L"WM/TrackNumber", WMT_TYPE_DWORD ),
            get_dword( info, L"WM/Flag", WMT_TYPE_BOOL ) );
    printf( "open_twice=%d\n", FAILED(IWMMetadataEditor_Open( editor, path )) );

    /* changing */
    IWMHeaderInfo3_SetAttribute( info, 0, L"Title", WMT_TYPE_STRING, (BYTE *)L"New Title", sizeof(L"New Title") );
    IWMHeaderInfo3_SetAttribute( info, 0, L"WM/Genre", WMT_TYPE_STRING, (BYTE *)L"Jazz", sizeof(L"Jazz") );
    {
        DWORD track = 9;
        IWMHeaderInfo3_SetAttribute( info, 0, L"WM/TrackNumber", WMT_TYPE_DWORD, (BYTE *)&track, 4 );
    }
    index = find_index( info, L"Author" );
    if (index >= 0) IWMHeaderInfo3_DeleteAttribute( info, 0, index );
    hr = IWMMetadataEditor_Flush( editor );
    printf( "flush=%d hr=%#lx\n", hr == S_OK, hr );
    IWMMetadataEditor_Close( editor );
    IWMHeaderInfo3_Release( info );
    IWMMetadataEditor_Release( editor );

    /* the file as written */
    info = open_editor( &editor, path, &hr );
    printf( "reopen=%d hr=%#lx\n", info != NULL, hr );
    if (!info) return 1;
    ok = get_string( info, L"Title", str, 256 ) && !wcscmp( str, L"New Title" );
    printf( "saved_title=%d title=%ls\n", ok, str );
    ok = get_string( info, L"WM/Genre", str, 256 ) && !wcscmp( str, L"Jazz" );
    printf( "saved_new=%d genre=%ls\n", ok, str );
    printf( "saved_kept=%d\n", get_string( info, L"WM/AlbumTitle", str, 256 ) && !wcscmp( str, L"Old Album" ) &&
            get_dword( info, L"WM/TrackNumber", WMT_TYPE_DWORD ) == 9 && get_dword( info, L"WM/Flag", WMT_TYPE_BOOL ) == 1 );
    printf( "saved_deleted=%d\n", !get_string( info, L"Author", str, 256 ) );

    /* not flushed: dropped */
    IWMHeaderInfo3_SetAttribute( info, 0, L"Title", WMT_TYPE_STRING, (BYTE *)L"Dropped", sizeof(L"Dropped") );
    IWMMetadataEditor_Close( editor );
    IWMHeaderInfo3_Release( info );
    IWMMetadataEditor_Release( editor );
    info = open_editor( &editor, path, &hr );
    ok = info && get_string( info, L"Title", str, 256 ) && !wcscmp( str, L"New Title" );
    printf( "close_drops=%d title=%ls\n", ok, str );
    if (info) { IWMMetadataEditor_Close( editor ); IWMHeaderInfo3_Release( info ); IWMMetadataEditor_Release( editor ); }

    /* the bytes */
    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    size = GetFileSize( file, NULL );
    data = malloc( size );
    ReadFile( file, data, size, &read, NULL );
    CloseHandle( file );
    {
        UINT64 header_size, file_size = 0;
        DWORD pos;

        memcpy( &header_size, data + 16, 8 );
        ok = read == size && size > sizeof(payload) && !memcmp( data + size - sizeof(payload), payload, sizeof(payload) ) &&
             !memcmp( data + header_size, &data_guid, 16 );
        printf( "payload_kept=%d size=%lu header=%llu\n", ok, size, header_size );
        for (pos = 30; pos + 24 <= header_size;)
        {
            UINT64 object_size;
            memcpy( &object_size, data + pos + 16, 8 );
            if (!memcmp( data + pos, &fpo_guid, 16 )) memcpy( &file_size, data + pos + 40, 8 );
            if (object_size < 24) break;
            pos += object_size;
        }
        printf( "file_size=%d stored=%llu\n", file_size == size, file_size );
    }
    free( data );

    /* not an ASF file */
    swprintf( path, MAX_PATH, L"%lsnotasf.wma", dir );
    file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( file, "hello, this is not ASF at all, really not", 40, &read, NULL );
    CloseHandle( file );
    WMCreateEditor( &editor );
    hr = IWMMetadataEditor_Open( editor, path );
    printf( "not_asf=%d hr=%#lx\n", FAILED(hr), hr );
    IWMMetadataEditor_Release( editor );

    printf( "done=1\n" );
    return 0;
}
