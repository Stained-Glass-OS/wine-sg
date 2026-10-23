/* appxbundlew-probe: IAppxBundleWriter/3/4 and IAppxPackageWriter2/3 (Stained Glass OS wine-sg 6060),
 * for test/appxbundlew-gate.sh. Works in C:\ on the files the gate puts there (manifest-*.xml, data.bin,
 * cgm.xml, badcgm.xml) and writes app_x64.msix, app_x86.msix, res_fr.msix, other.msix, w23.msix,
 * b.msixbundle, flat.msixbundle and now.msixbundle.
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
static const GUID CLSID_AppxBundleFactory_ = {0x378e0446,0x5384,0x43b7,{0x88,0x77,0xe7,0xdb,0xdd,0x88,0x34,0x46}};
static const GUID IID_IAppxBundleFactory_ = {0xbba65864,0x965f,0x4a5f,{0x85,0x5f,0xf0,0x74,0xbd,0xbf,0x3a,0x7b}};
static const GUID IID_IAppxPackageWriter_ = {0x9099e33b,0x246f,0x41e4,{0x88,0x1a,0x00,0x8e,0xb6,0x13,0xf8,0x58}};
static const GUID IID_IAppxPackageWriter2_ = {0x2cf5c4fd,0xe54c,0x4ea5,{0xba,0x4e,0xf8,0xc4,0xb1,0x05,0xa8,0xc8}};
static const GUID IID_IAppxPackageWriter3_ = {0xa83aacd3,0x41c0,0x4501,{0xb8,0xa3,0x74,0x16,0x4f,0x50,0xb2,0xfd}};
static const GUID IID_IAppxBundleWriter_ = {0xec446fe8,0xbfec,0x4c64,{0xab,0x4f,0x49,0xf0,0x38,0xf0,0xc6,0xd2}};
static const GUID IID_IAppxBundleWriter2_ = {0x6d8fe971,0x01cc,0x49a0,{0xb6,0x85,0x23,0x38,0x51,0x27,0x99,0x62}};
static const GUID IID_IAppxBundleWriter3_ = {0xad711152,0xf969,0x4193,{0x82,0xd5,0x9d,0xdf,0x27,0x86,0xd2,0x1a}};
static const GUID IID_IAppxBundleWriter4_ = {0x9cd9d523,0x5009,0x4c01,{0x98,0x82,0xdc,0x02,0x9f,0xbd,0x47,0xa3}};
#define SLOT(o, n, t) ((t)((*(void ***)(o))[n]))
#define P(...) printf(__VA_ARGS__)
typedef HRESULT (WINAPI *fn_add)( void *, const WCHAR *, const WCHAR *, int, IStream * );
typedef HRESULT (WINAPI *fn_close)( void *, IStream * );
typedef HRESULT (WINAPI *fn_close2)( void *, IStream *, IStream * );
typedef HRESULT (WINAPI *fn_addpkg)( void *, const WCHAR *, IStream * );
typedef HRESULT (WINAPI *fn_addpkg4)( void *, const WCHAR *, IStream *, BOOL );
typedef HRESULT (WINAPI *fn_void)( void * );
typedef HRESULT (WINAPI *fn_wstr)( void *, const WCHAR * );
typedef HRESULT (WINAPI *fn_qi)( void *, REFIID, void ** );
struct settings { BOOL zip32; void *hash; };
struct payload_stream { IStream *stream; const WCHAR *name, *type; int option; };
typedef HRESULT (WINAPI *fn_addfiles)( void *, UINT32, struct payload_stream *, UINT64 );

static void *factory, *bundle_factory;

static IStream *file_stream( const WCHAR *path, BOOL write )
{
    IStream *s = NULL;
    SHCreateStreamOnFileEx( path, write ? STGM_CREATE | STGM_WRITE : STGM_READ | STGM_SHARE_DENY_WRITE, 0, write, NULL, &s );
    return s;
}

static void *new_writer( IStream *out )
{
    struct settings settings = { FALSE, NULL };
    void *writer = NULL;
    SLOT( factory, 3, HRESULT (WINAPI *)( void *, IStream *, struct settings *, void ** ) )( factory, out, &settings, &writer );
    return writer;
}

static HRESULT make_package( const WCHAR *path, const WCHAR *manifest )
{
    IStream *out = file_stream( path, TRUE );
    void *w = new_writer( out );
    HRESULT hr;

    hr = SLOT( w, 3, fn_add )( w, L"Public\\data.bin", L"application/octet-stream", 1, file_stream( L"C:\\data.bin", FALSE ) );
    if (SUCCEEDED(hr)) hr = SLOT( w, 4, fn_close )( w, file_stream( manifest, FALSE ) );
    IUnknown_Release( (IUnknown *)w );
    IStream_Release( out );
    return hr;
}

static void *new_bundle_writer( IStream *out, UINT64 version )
{
    void *writer = NULL;
    HRESULT hr = SLOT( bundle_factory, 3, HRESULT (WINAPI *)( void *, IStream *, UINT64, void ** ) )(
            bundle_factory, out, version, &writer );
    if (FAILED(hr)) P( "CreateBundleWriterFailed=0x%08lx\n", hr );
    return writer;
}

static void *query( void *obj, const GUID *iid, HRESULT *hr )
{
    void *out = NULL;
    *hr = SLOT( obj, 0, fn_qi )( obj, iid, &out );
    return out;
}

/* the bundle's packages as its reader sees them: file type offset size; ... */
static void read_bundle( const char *key, const WCHAR *path )
{
    void *reader = NULL, *manifest = NULL, *items = NULL, *info, *id = NULL;
    BOOL has = FALSE;
    WCHAR *name;
    UINT64 offset, size, version = 0;
    int type;
    HRESULT hr;

    hr = SLOT( bundle_factory, 4, HRESULT (WINAPI *)( void *, IStream *, void ** ) )( bundle_factory,
            file_stream( path, FALSE ), &reader );
    P( "%s=0x%08lx", key, hr );
    if (!reader) { P( "\n" ); return; }
    SLOT( reader, 5, HRESULT (WINAPI *)( void *, void ** ) )( reader, &manifest );
    SLOT( manifest, 3, HRESULT (WINAPI *)( void *, void ** ) )( manifest, &id );
    SLOT( id, 6, HRESULT (WINAPI *)( void *, UINT64 * ) )( id, &version );
    P( " %u.%u.%u.%u", (UINT)(version >> 48), (UINT)(version >> 32) & 0xffff, (UINT)(version >> 16) & 0xffff,
       (UINT)version & 0xffff );
    SLOT( manifest, 4, HRESULT (WINAPI *)( void *, void ** ) )( manifest, &items );
    for (SLOT( items, 4, HRESULT (WINAPI *)( void *, BOOL * ) )( items, &has ); has;
         SLOT( items, 5, HRESULT (WINAPI *)( void *, BOOL * ) )( items, &has ))
    {
        info = NULL;
        SLOT( items, 3, HRESULT (WINAPI *)( void *, void ** ) )( items, &info );
        type = -1; offset = size = 0; name = NULL;
        SLOT( info, 3, HRESULT (WINAPI *)( void *, int * ) )( info, &type );
        SLOT( info, 5, HRESULT (WINAPI *)( void *, WCHAR ** ) )( info, &name );
        SLOT( info, 6, HRESULT (WINAPI *)( void *, UINT64 * ) )( info, &offset );
        SLOT( info, 7, HRESULT (WINAPI *)( void *, UINT64 * ) )( info, &size );
        P( "; %ls %d %d %d", name ? name : L"?", type, offset != 0, size != 0 );
        CoTaskMemFree( name );
    }
    P( "\n" );
}

int main( int argc, char **argv )
{
    void *w, *w2, *w3, *w4, *b, *b2, *b3, *b4, *reader = NULL, *files = NULL, *file = NULL;
    struct payload_stream batch[3];
    IStream *out, *s;
    BOOL has = FALSE;
    LONG count = 0;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    CoCreateInstance( &CLSID_AppxFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxFactory_, &factory );
    CoCreateInstance( &CLSID_AppxBundleFactory_, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxBundleFactory_, &bundle_factory );
    if (!factory || !bundle_factory) { P( "factories=0\n" ); return 1; }

    /* the packages to bundle, by our own package writer */
    P( "MakeX64=0x%08lx\n", make_package( L"C:\\app_x64.msix", L"C:\\manifest-x64.xml" ) );
    P( "MakeX86=0x%08lx\n", make_package( L"C:\\app_x86.msix", L"C:\\manifest-x86.xml" ) );
    P( "MakeFr=0x%08lx\n", make_package( L"C:\\res_fr.msix", L"C:\\manifest-fr.xml" ) );
    P( "MakeOther=0x%08lx\n", make_package( L"C:\\other.msix", L"C:\\manifest-other.xml" ) );

    /* IAppxPackageWriter2/3 */
    out = file_stream( L"C:\\w23.msix", TRUE );
    w = new_writer( out );
    w2 = query( w, &IID_IAppxPackageWriter2_, &hr );
    P( "QueryWriter2=0x%08lx\n", hr );
    w3 = query( w, &IID_IAppxPackageWriter3_, &hr );
    P( "QueryWriter3=0x%08lx\n", hr );
    query( w, &IID_IAppxBundleWriter_, &hr );
    P( "QueryPackageAsBundle=0x%08lx\n", hr );
    if (!w2 || !w3) return 1;
    batch[0].stream = file_stream( L"C:\\data.bin", FALSE ); batch[0].name = L"a.bin";
    batch[1].stream = file_stream( L"C:\\data.bin", FALSE ); batch[1].name = L"b.bin";
    batch[2].stream = file_stream( L"C:\\data.bin", FALSE ); batch[2].name = L"A.BIN";
    batch[0].type = batch[1].type = batch[2].type = L"application/octet-stream";
    batch[0].option = 1; batch[1].option = 0; batch[2].option = 1;
    hr = SLOT( w3, 3, fn_addfiles )( w3, 3, batch, 1 << 20 );
    P( "AddPayloadFilesRepeated=0x%08lx\n", hr );
    batch[0].stream = file_stream( L"C:\\data.bin", FALSE );
    batch[1].stream = file_stream( L"C:\\data.bin", FALSE );
    hr = SLOT( w3, 3, fn_addfiles )( w3, 2, batch, 1 << 20 );
    P( "AddPayloadFiles=0x%08lx\n", hr );
    hr = SLOT( w2, 3, fn_close2 )( w2, file_stream( L"C:\\manifest-x64.xml", FALSE ), file_stream( L"C:\\badcgm.xml", FALSE ) );
    P( "CloseBadMap=0x%08lx\n", hr );
    hr = SLOT( w2, 3, fn_close2 )( w2, file_stream( L"C:\\manifest-x64.xml", FALSE ), file_stream( L"C:\\cgm.xml", FALSE ) );
    P( "CloseWithMap=0x%08lx\n", hr );
    IUnknown_Release( (IUnknown *)w3 );
    IUnknown_Release( (IUnknown *)w2 );
    IUnknown_Release( (IUnknown *)w );
    IStream_Release( out );
    hr = SLOT( factory, 4, HRESULT (WINAPI *)( void *, IStream *, void ** ) )( factory, file_stream( L"C:\\w23.msix", FALSE ), &reader );
    if (reader)
    {
        SLOT( reader, 6, HRESULT (WINAPI *)( void *, void ** ) )( reader, &files );
        for (SLOT( files, 4, HRESULT (WINAPI *)( void *, BOOL * ) )( files, &has ); has;
             SLOT( files, 5, HRESULT (WINAPI *)( void *, BOOL * ) )( files, &has ))
            count++;
        SLOT( reader, 4, HRESULT (WINAPI *)( void *, int, void ** ) )( reader, 4 /* CONTENTGROUPMAP */, &file );
    }
    P( "ReadW23=0x%08lx %ld %d\n", hr, count, file != NULL );

    /* a bundle of two application packages and a resource package */
    out = file_stream( L"C:\\b.msixbundle", TRUE );
    b = new_bundle_writer( out, 0x0001000200030004ull );
    if (!b) return 1;
    b2 = query( b, &IID_IAppxBundleWriter2_, &hr );
    P( "QueryBundle2=0x%08lx\n", hr );
    b3 = query( b, &IID_IAppxBundleWriter3_, &hr );
    P( "QueryBundle3=0x%08lx\n", hr );
    b4 = query( b, &IID_IAppxBundleWriter4_, &hr );
    P( "QueryBundle4=0x%08lx\n", hr );
    query( b, &IID_IAppxPackageWriter_, &hr );
    P( "QueryBundleAsPackage=0x%08lx\n", hr );
    if (!b3 || !b4) return 1;
    P( "AddX64=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"app_x64.msix", file_stream( L"C:\\app_x64.msix", FALSE ) ) );
    P( "AddX86=0x%08lx\n", SLOT( b4, 3, fn_addpkg4 )( b4, L"app_x86.msix", file_stream( L"C:\\app_x86.msix", FALSE ), TRUE ) );
    P( "AddFr=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"res_fr.msix", file_stream( L"C:\\res_fr.msix", FALSE ) ) );
    P( "AddOtherApp=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"other.msix", file_stream( L"C:\\other.msix", FALSE ) ) );
    P( "AddRepeated=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"APP_X64.msix", file_stream( L"C:\\app_x64.msix", FALSE ) ) );
    P( "AddNotPackageName=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"app.zip", file_stream( L"C:\\app_x64.msix", FALSE ) ) );
    P( "AddNotPackage=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"data.msix", file_stream( L"C:\\data.bin", FALSE ) ) );
    P( "AddExternal=0x%08lx\n", SLOT( b2, 3, fn_addpkg )( b2, L"ext.msix", file_stream( L"C:\\app_x64.msix", FALSE ) ) );
    P( "BundleClose=0x%08lx\n", SLOT( b, 4, fn_void )( b ) );
    P( "AddAfterClose=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"late.msix", file_stream( L"C:\\app_x64.msix", FALSE ) ) );
    IUnknown_Release( (IUnknown *)b4 );
    IUnknown_Release( (IUnknown *)b3 );
    IUnknown_Release( (IUnknown *)b2 );
    IUnknown_Release( (IUnknown *)b );
    IStream_Release( out );
    read_bundle( "ReadBundle", L"C:\\b.msixbundle" );

    /* a flat bundle: one package in it, one referenced; SHA-512 block hashes */
    out = file_stream( L"C:\\flat.msixbundle", TRUE );
    b = new_bundle_writer( out, 0x0002000000000000ull );
    b3 = query( b, &IID_IAppxBundleWriter3_, &hr );
    P( "FlatAddX64=0x%08lx\n", SLOT( b, 3, fn_addpkg )( b, L"app_x64.msix", file_stream( L"C:\\app_x64.msix", FALSE ) ) );
    P( "FlatReference=0x%08lx\n", SLOT( b3, 3, fn_addpkg )( b3, L"app_x86.msix", file_stream( L"C:\\app_x86.msix", FALSE ) ) );
    P( "FlatBadHash=0x%08lx\n", SLOT( b3, 4, fn_wstr )( b3, L"http://example.invalid/md5" ) );
    P( "FlatClose=0x%08lx\n", SLOT( b3, 4, fn_wstr )( b3, L"http://www.w3.org/2001/04/xmlenc#sha512" ) );
    IUnknown_Release( (IUnknown *)b3 );
    IUnknown_Release( (IUnknown *)b );
    IStream_Release( out );
    read_bundle( "ReadFlat", L"C:\\flat.msixbundle" );

    /* no application package: no bundle; version 0: one from the time */
    CreateStreamOnHGlobal( NULL, TRUE, &s );
    b = new_bundle_writer( s, 1 );
    SLOT( b, 3, fn_addpkg )( b, L"res_fr.msix", file_stream( L"C:\\res_fr.msix", FALSE ) );
    P( "CloseResourceOnly=0x%08lx\n", SLOT( b, 4, fn_void )( b ) );
    IUnknown_Release( (IUnknown *)b );
    out = file_stream( L"C:\\now.msixbundle", TRUE );
    b = new_bundle_writer( out, 0 );
    SLOT( b, 3, fn_addpkg )( b, L"app_x64.msix", file_stream( L"C:\\app_x64.msix", FALSE ) );
    P( "CloseVersionZero=0x%08lx\n", SLOT( b, 4, fn_void )( b ) );
    IUnknown_Release( (IUnknown *)b );
    IStream_Release( out );
    P( "done=1\n" );
    return 0;
}
