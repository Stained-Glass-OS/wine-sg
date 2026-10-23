/* blockmap-probe: IAppxBlockMapReader and the manifest's device capabilities (Stained Glass OS
 * wine-sg 6055), for test/blockmap-gate.sh.
 *
 *   blockmap-probe package PACKAGE DATAFILE TAMPERED     the block map of a package reader
 *   blockmap-probe standalone BLOCKMAP.xml               IAppxFactory::CreateBlockMapReader
 *   blockmap-probe validated BLOCKMAP.xml SIGNATURE.p7x  CreateValidatedBlockMapReader
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
typedef HRESULT (WINAPI *fn_ptr)( void *, void ** );
typedef HRESULT (WINAPI *fn_bool)( void *, BOOL * );
#define P(...) printf(__VA_ARGS__)

/* slots: IAppxFactory 3 CreatePackageWriter, 4 CreatePackageReader, 5 CreateManifestReader,
 * 6 CreateBlockMapReader, 7 CreateValidatedBlockMapReader; IAppxPackageReader 3 GetBlockMap,
 * 7 GetManifest; IAppxManifestReader 8 GetDeviceCapabilities; IAppxBlockMapReader 3 GetFile,
 * 4 GetFiles, 5 GetHashMethod, 6 GetStream; IAppxBlockMapFile 3 GetBlocks, 4 GetLocalFileHeaderSize,
 * 5 GetName, 6 GetUncompressedSize, 7 ValidateFileHash; IAppxBlockMapBlock 3 GetHash,
 * 4 GetCompressedSize; enumerators 3 GetCurrent, 4 GetHasCurrent, 5 MoveNext */

static IStream *open_file( const char *path )
{
    WCHAR wpath[MAX_PATH];
    IStream *s = NULL;
    MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );
    SHCreateStreamOnFileEx( wpath, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, NULL, &s );
    return s;
}

static void describe( void *bm )
{
    void *files = NULL, *file = NULL, *blocks = NULL, *block = NULL, *uri = NULL;
    IStream *xml = NULL;
    BOOL has = FALSE;
    LONG count = 0;
    WCHAR *name;
    UINT32 lfh = 0, hash_size = 0, csize = 0;
    UINT64 size = 0;
    BYTE *hash;
    BSTR uristr = NULL;
    STATSTG st;
    HRESULT hr;

    SLOT( bm, 4, fn_ptr )( bm, &files );
    for (SLOT( files, 4, fn_bool )( files, &has ); has; SLOT( files, 5, fn_bool )( files, &has )) count++;
    P( "FileCount=%ld\n", count );
    hr = SLOT( bm, 3, HRESULT (WINAPI *)( void *, const WCHAR *, void ** ) )( bm, L"Public\\data.bin", &file );
    P( "GetFile=0x%08lx\n", hr );
    if (file)
    {
        SLOT( file, 5, HRESULT (WINAPI *)( void *, WCHAR ** ) )( file, &name );
        SLOT( file, 6, HRESULT (WINAPI *)( void *, UINT64 * ) )( file, &size );
        SLOT( file, 4, HRESULT (WINAPI *)( void *, UINT32 * ) )( file, &lfh );
        P( "File=%ls %llu %u\n", name, (unsigned long long)size, lfh );
        CoTaskMemFree( name );
        SLOT( file, 3, fn_ptr )( file, &blocks );
        count = 0;
        for (SLOT( blocks, 4, fn_bool )( blocks, &has ); has; SLOT( blocks, 5, fn_bool )( blocks, &has )) count++;
        P( "BlockCount=%ld\n", count );
        IUnknown_Release( (IUnknown *)blocks );
        SLOT( file, 3, fn_ptr )( file, &blocks );
        SLOT( blocks, 3, fn_ptr )( blocks, &block );
        if (block)
        {
            SLOT( block, 3, HRESULT (WINAPI *)( void *, UINT32 *, BYTE ** ) )( block, &hash_size, &hash );
            SLOT( block, 4, HRESULT (WINAPI *)( void *, UINT32 * ) )( block, &csize );
            P( "Block=%u %02x%02x %u\n", hash_size, hash[0], hash[1], csize );
        }
    }
    hr = SLOT( bm, 3, HRESULT (WINAPI *)( void *, const WCHAR *, void ** ) )( bm, L"No\\such.file", &block );
    P( "GetMissing=0x%08lx\n", hr );
    hr = SLOT( bm, 5, fn_ptr )( bm, &uri );
    if (uri) IUri_GetAbsoluteUri( (IUri *)uri, &uristr );
    P( "HashMethod=0x%08lx %ls\n", hr, uristr ? uristr : L"(none)" );
    SLOT( bm, 6, HRESULT (WINAPI *)( void *, IStream ** ) )( bm, &xml );
    if (xml && SUCCEEDED(IStream_Stat( xml, &st, STATFLAG_NONAME ))) P( "StreamSize=%lu\n", (ULONG)st.cbSize.QuadPart );
}

int main( int argc, char **argv )
{
    void *factory = NULL, *reader = NULL, *bm = NULL, *manifest = NULL, *caps = NULL, *file = NULL;
    IStream *s;
    HRESULT hr;
    BOOL has = FALSE, valid = FALSE;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    if (FAILED(hr = CoCreateInstance( &CLSID_AppxFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxFactory_, &factory )))
    {
        P( "Factory=0x%08lx\n", hr );
        return 1;
    }
    if (argc >= 5 && !strcmp( argv[1], "package" ))
    {
        hr = SLOT( factory, 4, HRESULT (WINAPI *)( void *, IStream *, void ** ) )( factory, open_file( argv[2] ), &reader );
        P( "CreatePackageReader=0x%08lx\n", hr );
        if (!reader) return 1;
        hr = SLOT( reader, 3, fn_ptr )( reader, &bm );
        P( "GetBlockMap=0x%08lx\n", hr );
        if (bm)
        {
            describe( bm );
            SLOT( bm, 3, HRESULT (WINAPI *)( void *, const WCHAR *, void ** ) )( bm, L"Public\\data.bin", &file );
            SLOT( file, 7, HRESULT (WINAPI *)( void *, IStream *, BOOL * ) )( file, open_file( argv[3] ), &valid );
            P( "Validate=%d\n", valid );
            valid = TRUE;
            SLOT( file, 7, HRESULT (WINAPI *)( void *, IStream *, BOOL * ) )( file, open_file( argv[4] ), &valid );
            P( "ValidateTampered=%d\n", valid );
        }
        SLOT( reader, 7, fn_ptr )( reader, &manifest );
        hr = SLOT( manifest, 8, fn_ptr )( manifest, &caps );
        P( "GetDeviceCapabilities=0x%08lx\n", hr );
        if (caps)
        {
            WCHAR *cap;
            P( "DeviceCapabilities=" );
            for (SLOT( caps, 4, fn_bool )( caps, &has ); has; SLOT( caps, 5, fn_bool )( caps, &has ))
            {
                SLOT( caps, 3, HRESULT (WINAPI *)( void *, WCHAR ** ) )( caps, &cap );
                P( "%ls,", cap );
                CoTaskMemFree( cap );
            }
            P( "\n" );
        }
    }
    else if (argc >= 3 && !strcmp( argv[1], "standalone" ))
    {
        s = open_file( argv[2] );
        hr = SLOT( factory, 6, HRESULT (WINAPI *)( void *, IStream *, void ** ) )( factory, s, &bm );
        P( "CreateBlockMapReader=0x%08lx\n", hr );
        if (bm) describe( bm );
    }
    else if (argc >= 4 && !strcmp( argv[1], "validated" ))
    {
        WCHAR sig[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, argv[3], -1, sig, MAX_PATH );
        s = open_file( argv[2] );
        hr = SLOT( factory, 7, HRESULT (WINAPI *)( void *, IStream *, const WCHAR *, void ** ) )( factory, s, sig, &bm );
        P( "CreateValidatedBlockMapReader=0x%08lx\n", hr );
    }
    P( "done=1\n" );
    return 0;
}
