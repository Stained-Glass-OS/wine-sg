/* olestreamin-probe: ole32 OleConvertOLESTREAMToIStorageEx, which was a spec stub (Stained Glass OS wine-sg
 * 8274; test/olestreamin-gate.sh): an OLE 1 stream made by OleConvertIStorageToOLESTREAMEx comes back as a
 * storage without a presentation stream, and the presentation format, size and data are handed back, in an
 * HGLOBAL (TYMED_NULL given) or the caller's stream.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check( int ok, const char *what ) { printf( "%s  %s\n", ok ? "PASS" : "FAIL", what ); if (!ok) failures++; }

struct mem_stream { OLESTREAM stream; BYTE data[1 << 16]; DWORD size, pos; };
static DWORD CALLBACK mem_get( LPOLESTREAM s, void *buf, DWORD n )
{
    struct mem_stream *m = (struct mem_stream *)s;
    if (m->pos + n > m->size) n = m->size - m->pos;
    memcpy( buf, m->data + m->pos, n ); m->pos += n; return n;
}
static DWORD CALLBACK mem_put( LPOLESTREAM s, const void *buf, DWORD n )
{
    struct mem_stream *m = (struct mem_stream *)s;
    if (m->size + n > sizeof(m->data)) return 0;
    memcpy( m->data + m->size, buf, n ); m->size += n; return n;
}
static OLESTREAMVTBL vtbl = { mem_get, mem_put };

static IStorage *new_storage( const WCHAR *path )
{
    IStorage *stg = NULL;
    StgCreateDocfile( path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg );
    return stg;
}

int main( void )
{
    HRESULT (WINAPI *pIn)(LPOLESTREAM, LPSTORAGE, CLIPFORMAT *, LONG *, LONG *, DWORD *, LPSTGMEDIUM) =
        (void *)GetProcAddress( LoadLibraryA( "ole32.dll" ), "OleConvertOLESTREAMToIStorageEx" );
    static struct mem_stream ms;
    WCHAR dir[MAX_PATH], p1[MAX_PATH], p2[MAX_PATH], p3[MAX_PATH];
    BYTE src[64];
    IStorage *stg, *back, *back2;
    IStream *s;
    STGMEDIUM medium;
    CLIPFORMAT cf = 0;
    LONG w = 0, h = 0;
    DWORD size = 0, k;
    HRESULT hr;
    char text[160];

    if (!pIn) { printf( "FAIL  export\nRESULT: FAIL\n" ); return 1; }
    CoInitialize( NULL );
    GetTempPathW( MAX_PATH, dir );
    swprintf( p1, MAX_PATH, L"%lsolein1.stg", dir ); swprintf( p2, MAX_PATH, L"%lsolein2.stg", dir ); swprintf( p3, MAX_PATH, L"%lsolein3.stg", dir );
    stg = new_storage( p1 );
    WriteClassStg( stg, &CLSID_NULL );
    WriteFmtUserTypeStg( stg, 0, L"SG test object" );
    IStorage_CreateStream( stg, L"Payload", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s );
    IStream_Write( s, "payload", 7, NULL );
    IStream_Release( s );
    for (k = 0; k < sizeof(src); k++) src[k] = (BYTE)(0x20 + k * 5);
    memset( &medium, 0, sizeof(medium) );
    medium.tymed = TYMED_HGLOBAL;
    medium.hGlobal = GlobalAlloc( GMEM_MOVEABLE, sizeof(src) );
    memcpy( GlobalLock( medium.hGlobal ), src, sizeof(src) ); GlobalUnlock( medium.hGlobal );
    ms.stream.lpstbl = &vtbl;
    hr = OleConvertIStorageToOLESTREAMEx( stg, CF_DIB, 1000, 500, sizeof(src), &medium, &ms.stream );
    check( hr == S_OK, "an OLE 1 stream with a DIB presentation (OleConvertIStorageToOLESTREAMEx)" );
    GlobalFree( medium.hGlobal );

    back = new_storage( p2 );
    memset( &medium, 0, sizeof(medium) );
    ms.pos = 0;
    hr = pIn( &ms.stream, back, &cf, &w, &h, &size, &medium );
    sprintf( text, "converted back: S_OK, CF_DIB, 1000 x 500, %u bytes (hr %#lx, cf %u, %ld x %ld, %lu)", (unsigned)sizeof(src), hr, cf, w, h, size );
    check( hr == S_OK && cf == CF_DIB && w == 1000 && h == 500 && size == sizeof(src), text );
    check( medium.tymed == TYMED_HGLOBAL && medium.hGlobal && !memcmp( GlobalLock( medium.hGlobal ), src, sizeof(src) ), "  the data in an HGLOBAL" );
    if (medium.hGlobal) { GlobalUnlock( medium.hGlobal ); GlobalFree( medium.hGlobal ); }
    hr = IStorage_OpenStream( back, L"Payload", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s );
    check( hr == S_OK, "  the storage holds the object's own stream" );
    if (hr == S_OK) IStream_Release( s );
    hr = IStorage_OpenStream( back, L"\2OlePres000", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s );
    check( hr != S_OK, "  and no presentation stream" );
    if (hr == S_OK) IStream_Release( s );

    back2 = new_storage( p3 );
    memset( &medium, 0, sizeof(medium) );
    medium.tymed = TYMED_ISTREAM;
    CreateStreamOnHGlobal( NULL, TRUE, &medium.pstm );
    ms.pos = 0;
    hr = pIn( &ms.stream, back2, &cf, &w, &h, &size, &medium );
    {
        STATSTG st;
        IStream_Stat( medium.pstm, &st, STATFLAG_NONAME );
        check( hr == S_OK && st.cbSize.QuadPart == sizeof(src), "TYMED_ISTREAM: the data written into the caller's stream" );
    }
    IStream_Release( medium.pstm );
    check( pIn( NULL, back2, &cf, &w, &h, &size, &medium ) == E_INVALIDARG, "no OLE 1 stream: E_INVALIDARG" );
    {
        WCHAR p4[MAX_PATH];
        IStorage *plain;
        swprintf( p4, MAX_PATH, L"%lsolein4.stg", dir );
        plain = new_storage( p4 );
        ms.pos = 0;
        hr = OleConvertOLESTREAMToIStorage( &ms.stream, plain, NULL );
        if (hr == S_OK) hr = IStorage_OpenStream( plain, L"Payload", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s );
        check( hr == S_OK, "OleConvertOLESTREAMToIStorage keeps the embedded object's streams too" );
        if (hr == S_OK) IStream_Release( s );
        IStorage_Release( plain );
    }
    IStorage_Release( back2 ); IStorage_Release( back ); IStorage_Release( stg );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
