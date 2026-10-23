/* appxwriter-probe: IAppxPackageWriter (Stained Glass OS wine-sg 6056), for test/appxwriter-gate.sh.
 *
 *   appxwriter-probe write OUT.msix DATA LOGO MANIFEST BADMANIFEST
 *   appxwriter-probe read PACKAGE DATA      reads it back with a package reader
 *
 * Prints KEY=VALUE lines; HRESULTs as hex. The interfaces are called through their vtable slots.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>
#include <stdio.h>

static const GUID CLSID_AppxFactory_ = {0x5842a140,0xff9f,0x4166,{0x8f,0x5c,0x62,0xf5,0xb7,0xb0,0xc7,0x81}};
static const GUID IID_IAppxFactory_ = {0xbeb94909,0xe451,0x438b,{0xb5,0xa7,0xd7,0x9e,0x76,0x7b,0x75,0xd8}};
#define SLOT(o, n, t) ((t)((*(void ***)(o))[n]))
#define P(...) printf(__VA_ARGS__)
typedef HRESULT (WINAPI *fn_add)( void *, const WCHAR *, const WCHAR *, int, IStream * );
typedef HRESULT (WINAPI *fn_close)( void *, IStream * );
struct settings { BOOL zip32; void *hash; };

static IStream *file_stream( const char *path, BOOL write )
{
    WCHAR wpath[MAX_PATH];
    IStream *s = NULL;
    MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );
    SHCreateStreamOnFileEx( wpath, write ? STGM_CREATE | STGM_WRITE : STGM_READ | STGM_SHARE_DENY_WRITE, 0, write, NULL, &s );
    return s;
}

static void *new_writer( void *factory, IStream *out )
{
    struct settings settings = { FALSE, NULL };
    void *writer = NULL;
    HRESULT hr = SLOT( factory, 3, HRESULT (WINAPI *)( void *, IStream *, struct settings *, void ** ) )(
            factory, out, &settings, &writer );
    P( "CreatePackageWriter=0x%08lx\n", hr );
    return writer;
}

int main( int argc, char **argv )
{
    void *factory = NULL, *writer, *reader = NULL, *file = NULL, *files = NULL;
    IStream *out, *s;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    CoCreateInstance( &CLSID_AppxFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxFactory_, &factory );
    if (!factory) return 1;
    if (argc >= 7 && !strcmp( argv[1], "write" ))
    {
        out = file_stream( argv[2], TRUE );
        if (!(writer = new_writer( factory, out ))) return 1;
        hr = SLOT( writer, 3, fn_add )( writer, L"Public\\data.bin", L"application/octet-stream", 1, file_stream( argv[3], FALSE ) );
        P( "AddDeflated=0x%08lx\n", hr );
        hr = SLOT( writer, 3, fn_add )( writer, L"Assets\\logo.png", L"image/png", 0, file_stream( argv[4], FALSE ) );
        P( "AddStored=0x%08lx\n", hr );
        hr = SLOT( writer, 3, fn_add )( writer, L"Docs\\read me.txt", L"text/plain", 2, file_stream( argv[4], FALSE ) );
        P( "AddSpaced=0x%08lx\n", hr );
        hr = SLOT( writer, 3, fn_add )( writer, L"Public\\DATA.bin", L"application/octet-stream", 1, file_stream( argv[3], FALSE ) );
        P( "AddDuplicate=0x%08lx\n", hr );
        hr = SLOT( writer, 3, fn_add )( writer, L"AppxManifest.xml", L"text/xml", 1, file_stream( argv[3], FALSE ) );
        P( "AddFootprint=0x%08lx\n", hr );
        hr = SLOT( writer, 4, fn_close )( writer, file_stream( argv[5], FALSE ) );
        P( "Close=0x%08lx\n", hr );
        hr = SLOT( writer, 3, fn_add )( writer, L"late.bin", L"application/octet-stream", 1, file_stream( argv[3], FALSE ) );
        P( "AddAfterClose=0x%08lx\n", hr );
        IUnknown_Release( (IUnknown *)writer );
        IStream_Release( out );

        /* a manifest the reader refuses is refused */
        CreateStreamOnHGlobal( NULL, TRUE, &s );
        writer = new_writer( factory, s );
        hr = SLOT( writer, 4, fn_close )( writer, file_stream( argv[6], FALSE ) );
        P( "CloseBadManifest=0x%08lx\n", hr );
    }
    else if (argc >= 4 && !strcmp( argv[1], "read" ))
    {
        BOOL has = FALSE;
        LONG count = 0;
        int option = -1;
        UINT64 size = 0;
        IStream *data = NULL;
        BYTE *a, *b;
        ULONG got;

        hr = SLOT( factory, 4, HRESULT (WINAPI *)( void *, IStream *, void ** ) )( factory, file_stream( argv[2], FALSE ), &reader );
        P( "CreatePackageReader=0x%08lx\n", hr );
        if (!reader) return 1;
        SLOT( reader, 6, HRESULT (WINAPI *)( void *, void ** ) )( reader, &files );
        for (SLOT( files, 4, HRESULT (WINAPI *)( void *, BOOL * ) )( files, &has ); has;
             SLOT( files, 5, HRESULT (WINAPI *)( void *, BOOL * ) )( files, &has ))
            count++;
        P( "PayloadCount=%ld\n", count );
        hr = SLOT( reader, 5, HRESULT (WINAPI *)( void *, const WCHAR *, void ** ) )( reader, L"Public\\data.bin", &file );
        if (file)
        {
            SLOT( file, 3, HRESULT (WINAPI *)( void *, int * ) )( file, &option );
            SLOT( file, 6, HRESULT (WINAPI *)( void *, UINT64 * ) )( file, &size );
            SLOT( file, 7, HRESULT (WINAPI *)( void *, IStream ** ) )( file, &data );
            a = malloc( size );
            b = malloc( size );
            IStream_Read( data, a, size, &got );
            IStream_Read( file_stream( argv[3], FALSE ), b, size, &got );
            P( "Data=%d %llu %d\n", option, (unsigned long long)size, !memcmp( a, b, size ) );
        }
        file = NULL;
        hr = SLOT( reader, 5, HRESULT (WINAPI *)( void *, const WCHAR *, void ** ) )( reader, L"Docs\\read me.txt", &file );
        P( "Spaced=0x%08lx\n", hr );
        file = NULL;
        hr = SLOT( reader, 5, HRESULT (WINAPI *)( void *, const WCHAR *, void ** ) )( reader, L"Assets\\logo.png", &file );
        option = -1;
        if (file) SLOT( file, 3, HRESULT (WINAPI *)( void *, int * ) )( file, &option );
        P( "Stored=0x%08lx %d\n", hr, option );
    }
    P( "done=1\n" );
    return 0;
}
