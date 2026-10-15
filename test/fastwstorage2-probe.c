/* fastwstorage2-probe: Windows.Storage.Streams.RandomAccessStreamReference.OpenReadAsync over a stream
 * (a read-only copy with a content type) and CreateFromUri (file: and http: addresses, other schemes).
 * WinRT interfaces are called through their vtable slots; argv[1] is the port of a local http server. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
typedef HRESULT (WINAPI *qi_fn)( void *, REFIID, void ** );
typedef ULONG (WINAPI *rel_fn)( void * );
#define QI(obj, iid, out) SLOT( obj, 0, qi_fn )( obj, iid, (void **)(out) )
#define RELEASE(obj) do { if (obj) SLOT( obj, 2, rel_fn )( obj ); } while (0)

DEFINE_GUID(p_IID_IRandomAccessStream, 0x905a0fe1, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IInputStream, 0x905a0fe2, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IOutputStream, 0x905a0fe6, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IBufferFactory, 0x71af914d, 0xc10f, 0x484b, 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27);
DEFINE_GUID(p_IID_IBufferByteAccess, 0x905a0fef, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IRandomAccessStreamReferenceStatics, 0x857309dc, 0x3fbf, 0x4e7d, 0x98, 0x6f, 0xef, 0x3b, 0x1a, 0x07, 0xa9, 0x64);
DEFINE_GUID(p_IID_IRandomAccessStreamWithContentType, 0xcc254827, 0x4b3d, 0x438f, 0x92, 0x32, 0x10, 0xc7, 0x6b, 0xc7, 0xe0, 0x38);
DEFINE_GUID(p_IID_IContentTypeProvider, 0x97d098a5, 0x3b99, 0x4de9, 0x88, 0xa5, 0xe1, 0x1d, 0x2f, 0x50, 0xc7, 0x95);
DEFINE_GUID(p_IID_IUriRuntimeClassFactory, 0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
DEFINE_GUID(p_IID_IStorageFileStatics, 0x5984c710, 0xdaf2, 0x43c8, 0x8b, 0xb4, 0xa4, 0xd3, 0xea, 0xcf, 0xd0, 0x3f);
DEFINE_GUID(p_IID_IAsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

#define RS_CREATE_FROM_STREAM 8
#define RS_CREATE_FROM_URI    7
#define REF_OPEN_READ         6
#define AOP_GET_RESULTS       8
#define AOWP_GET_RESULTS      10
#define RAS_GET_SIZE          6
#define RAS_GET_POSITION      10
#define RAS_GET_CAN_WRITE     14
#define IS_READ_ASYNC         6
#define OS_WRITE_ASYNC        6
#define CT_GET_CONTENT_TYPE   6
#define BUF_GET_LENGTH        7
#define BUF_PUT_LENGTH        8

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, (UINT32)wcslen( s ), &h );
    return h;
}

static void *factory( const WCHAR *name, const GUID *iid )
{
    HSTRING h = hs( name );
    void *out = NULL;
    RoGetActivationFactory( h, iid, &out );
    WindowsDeleteString( h );
    return out;
}

static BYTE *buffer_bytes( void *buffer )
{
    void *access = NULL;
    BYTE *bytes = NULL;
    if (SUCCEEDED( QI( buffer, &p_IID_IBufferByteAccess, &access ) ))
    {
        SLOT( access, 3, HRESULT (WINAPI *)( void *, BYTE ** ) )( access, &bytes );
        RELEASE( access );
    }
    return bytes;
}

static void *make_uri( const WCHAR *url )
{
    void *f = factory( L"Windows.Foundation.Uri", &p_IID_IUriRuntimeClassFactory ), *uri = NULL;
    HSTRING h = hs( url );
    if (f) SLOT( f, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( f, h, &uri );
    WindowsDeleteString( h );
    RELEASE( f );
    return uri;
}

/* open the reference; returns the result stream (as IRandomAccessStream) and the hr of GetResults */
static HRESULT open_read( void *reference, void **stream, HRESULT *open_hr )
{
    void *op = NULL;
    HRESULT hr;

    *stream = NULL;
    *open_hr = SLOT( reference, REF_OPEN_READ, HRESULT (WINAPI *)( void *, void ** ) )( reference, &op );
    if (!op) return *open_hr;
    hr = SLOT( op, AOP_GET_RESULTS, HRESULT (WINAPI *)( void *, void ** ) )( op, stream );
    RELEASE( op );
    return hr;
}

/* read everything of a stream through IInputStream */
static int read_all( void *stream, BYTE *out, int max )
{
    void *in = NULL, *bf = factory( L"Windows.Storage.Streams.Buffer", &p_IID_IBufferFactory ), *buf = NULL, *op = NULL, *res = NULL;
    UINT32 len = 0;

    if (FAILED( QI( stream, &p_IID_IInputStream, &in ) ) || !bf) return -1;
    SLOT( bf, 6, HRESULT (WINAPI *)( void *, UINT32, void ** ) )( bf, max, &buf );
    SLOT( in, IS_READ_ASYNC, HRESULT (WINAPI *)( void *, void *, UINT32, int, void ** ) )( in, buf, max, 0, &op );
    if (op) SLOT( op, AOWP_GET_RESULTS, HRESULT (WINAPI *)( void *, void ** ) )( op, &res );
    if (res)
    {
        SLOT( res, BUF_GET_LENGTH, HRESULT (WINAPI *)( void *, UINT32 * ) )( res, &len );
        memcpy( out, buffer_bytes( res ), len );
    }
    RELEASE( res ); RELEASE( op ); RELEASE( buf ); RELEASE( bf ); RELEASE( in );
    return res ? (int)len : -1;
}

static void check_typed_stream( void *stream, const WCHAR *expect_type, const char *expect_data, int expect_len, const char *what )
{
    void *ras = NULL, *typed = NULL, *provider = NULL;
    UINT64 size = 0, pos = 99;
    BOOLEAN can_write = 1;
    BYTE data[256];
    int n;

    CHECK( stream != NULL, "%s: a stream", what );
    if (!stream) return;
    CHECK( SUCCEEDED( QI( stream, &p_IID_IRandomAccessStream, &ras ) ), "%s: IRandomAccessStream", what );
    CHECK( SUCCEEDED( QI( stream, &p_IID_IRandomAccessStreamWithContentType, &typed ) ), "%s: IRandomAccessStreamWithContentType", what );
    CHECK( SUCCEEDED( QI( stream, &p_IID_IContentTypeProvider, &provider ) ), "%s: IContentTypeProvider", what );
    if (provider)
    {
        HSTRING type = NULL;
        SLOT( provider, CT_GET_CONTENT_TYPE, HRESULT (WINAPI *)( void *, HSTRING * ) )( provider, &type );
        /* the empty string is the null HSTRING */
        CHECK( !wcscmp( type ? WindowsGetStringRawBuffer( type, NULL ) : L"", expect_type ), "%s: content type %ls", what,
               type ? WindowsGetStringRawBuffer( type, NULL ) : L"(null)" );
        WindowsDeleteString( type );
    }
    if (ras)
    {
        SLOT( ras, RAS_GET_SIZE, HRESULT (WINAPI *)( void *, UINT64 * ) )( ras, &size );
        CHECK( size == (UINT64)expect_len, "%s: size %I64u", what, size );
        SLOT( ras, RAS_GET_POSITION, HRESULT (WINAPI *)( void *, UINT64 * ) )( ras, &pos );
        CHECK( pos == 0, "%s: position %I64u", what, pos );
        SLOT( ras, RAS_GET_CAN_WRITE, HRESULT (WINAPI *)( void *, BOOLEAN * ) )( ras, &can_write );
        CHECK( !can_write, "%s: not writable", what );
    }
    n = read_all( stream, data, sizeof(data) );
    CHECK( n == expect_len && !memcmp( data, expect_data, expect_len ), "%s: data (%d bytes)", what, n );
    RELEASE( provider ); RELEASE( typed ); RELEASE( ras );
}

int main( int argc, char **argv )
{
    static const char content[] = "stained glass reference data";
    void *statics, *ms, *out_stream = NULL, *buffer_factory, *wbuf = NULL, *wop = NULL, *reference = NULL, *opened = NULL, *uri;
    void *o2 = NULL, *o3 = NULL;
    HRESULT hr, ohr;
    WCHAR path[MAX_PATH], url[MAX_PATH * 3], type[128];
    DWORD len, tlen = sizeof(type);
    char buf[64];
    HANDLE h;
    BYTE data[64];
    int port = argc > 1 ? atoi( argv[1] ) : 0;

    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_MULTITHREADED );
    /* the content type is what the extension registers */
    RegSetKeyValueW( HKEY_LOCAL_MACHINE, L"Software\\Classes\\.htm", L"Content Type", REG_SZ, L"text/html", 20 );

    statics = factory( L"Windows.Storage.Streams.RandomAccessStreamReference", &p_IID_IRandomAccessStreamReferenceStatics );
    CHECK( statics != NULL, "statics" );
    if (!statics) goto done;

    /* ---- over an in-memory stream ---- */
    {
        HSTRING cls = hs( L"Windows.Storage.Streams.InMemoryRandomAccessStream" );
        IInspectable *insp = NULL;
        RoActivateInstance( cls, &insp );
        WindowsDeleteString( cls );
        ms = NULL;
        if (insp) { QI( insp, &p_IID_IRandomAccessStream, &ms ); RELEASE( insp ); }
    }
    CHECK( ms != NULL, "memory stream" );
    buffer_factory = factory( L"Windows.Storage.Streams.Buffer", &p_IID_IBufferFactory );
    SLOT( buffer_factory, 6, HRESULT (WINAPI *)( void *, UINT32, void ** ) )( buffer_factory, 64, &wbuf );
    memcpy( buffer_bytes( wbuf ), content, sizeof(content) - 1 );
    SLOT( wbuf, BUF_PUT_LENGTH, HRESULT (WINAPI *)( void *, UINT32 ) )( wbuf, sizeof(content) - 1 );
    QI( ms, &p_IID_IOutputStream, &out_stream );
    SLOT( out_stream, OS_WRITE_ASYNC, HRESULT (WINAPI *)( void *, void *, void ** ) )( out_stream, wbuf, &wop );
    CHECK( wop != NULL, "write" );
    RELEASE( wop );

    hr = SLOT( statics, RS_CREATE_FROM_STREAM, HRESULT (WINAPI *)( void *, void *, void ** ) )( statics, ms, &reference );
    CHECK( hr == S_OK && reference, "CreateFromStream %08lx", hr );
    hr = open_read( reference, &opened, &ohr );
    CHECK( ohr == S_OK && hr == S_OK, "OpenReadAsync %08lx %08lx", ohr, hr );
    check_typed_stream( opened, L"", content, sizeof(content) - 1, "memory stream" );
    /* writing to the read stream is refused */
    if (opened)
    {
        void *os = NULL, *op = NULL;
        HRESULT whr = S_OK;
        void *result = NULL;
        QI( opened, &p_IID_IOutputStream, &os );
        if (os)
        {
            whr = SLOT( os, OS_WRITE_ASYNC, HRESULT (WINAPI *)( void *, void *, void ** ) )( os, wbuf, &op );
            if (op) whr = SLOT( op, AOWP_GET_RESULTS, HRESULT (WINAPI *)( void *, void * ) )( op, &result );
            CHECK( FAILED( whr ), "write to the read stream: %08lx", whr );
        }
        RELEASE( op ); RELEASE( os );
    }
    RELEASE( opened );
    /* a later change to the source does not change an already opened copy, and a second open sees it */
    RELEASE( reference );

    /* ---- over a stream that has a content type (opened from a file) ---- */
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"sgtyped.htm" );
    h = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( h, content, sizeof(content) - 1, &len, NULL );
    CloseHandle( h );
    wcscpy( type, L"text/html" ); (void)tlen;
    {
        void *fs = factory( L"Windows.Storage.StorageFile", &p_IID_IStorageFileStatics ), *fop = NULL, *file = NULL, *sop = NULL, *fstream = NULL;
        HSTRING hp = hs( path );
        if (fs) SLOT( fs, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( fs, hp, &fop );
        if (fop) SLOT( fop, AOP_GET_RESULTS, HRESULT (WINAPI *)( void *, void ** ) )( fop, &file );
        if (file) SLOT( file, 8, HRESULT (WINAPI *)( void *, int, void ** ) )( file, 0, &sop );
        if (sop) SLOT( sop, AOP_GET_RESULTS, HRESULT (WINAPI *)( void *, void ** ) )( sop, &fstream );
        CHECK( fstream != NULL, "a stream opened from a file" );
        if (fstream)
        {
            reference = NULL;
            SLOT( statics, RS_CREATE_FROM_STREAM, HRESULT (WINAPI *)( void *, void *, void ** ) )( statics, fstream, &reference );
            hr = open_read( reference, &opened, &ohr );
            CHECK( ohr == S_OK && hr == S_OK, "OpenReadAsync typed %08lx %08lx", ohr, hr );
            check_typed_stream( opened, type, content, sizeof(content) - 1, "typed stream" );
            RELEASE( opened ); RELEASE( reference );
        }
        RELEASE( fstream ); RELEASE( sop ); RELEASE( file ); RELEASE( fop ); RELEASE( fs );
        WindowsDeleteString( hp );
    }
    DeleteFileW( path );

    /* ---- file address ---- */
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"sgref.htm" );
    h = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( h, content, sizeof(content) - 1, &len, NULL );
    CloseHandle( h );
    len = ARRAYSIZE(url);
    UrlCreateFromPathW( path, url, &len, 0 );
    wcscpy( type, L"text/html" ); (void)tlen;
    uri = make_uri( url );
    CHECK( uri != NULL, "file uri" );
    hr = SLOT( statics, RS_CREATE_FROM_URI, HRESULT (WINAPI *)( void *, void *, void ** ) )( statics, uri, &reference );
    CHECK( hr == S_OK && reference, "CreateFromUri %08lx", hr );
    hr = SLOT( statics, RS_CREATE_FROM_URI, HRESULT (WINAPI *)( void *, void *, void ** ) )( statics, NULL, &o2 );
    CHECK( hr == E_POINTER, "CreateFromUri(NULL) %08lx", hr );
    if (reference)
    {
        hr = open_read( reference, &opened, &ohr );
        CHECK( ohr == S_OK && hr == S_OK, "OpenReadAsync file %08lx %08lx", ohr, hr );
        check_typed_stream( opened, type, content, sizeof(content) - 1, "file" );
        RELEASE( opened );
        /* the file is read when asked for: a missing file fails then, not at creation */
        DeleteFileW( path );
        hr = open_read( reference, &opened, &ohr );
        CHECK( FAILED( hr ) && !opened, "missing file %08lx", hr );
        RELEASE( opened );
        RELEASE( reference );
    }
    RELEASE( uri );

    /* ---- http address ---- */
    if (port)
    {
        swprintf( url, ARRAYSIZE(url), L"http://127.0.0.1:%d/data.htm?x=1#frag", port );
        uri = make_uri( url );
        CHECK( uri != NULL, "http uri" );
        reference = NULL;
        SLOT( statics, RS_CREATE_FROM_URI, HRESULT (WINAPI *)( void *, void *, void ** ) )( statics, uri, &reference );
        if (reference)
        {
            hr = open_read( reference, &opened, &ohr );
            CHECK( ohr == S_OK && hr == S_OK, "OpenReadAsync http %08lx %08lx", ohr, hr );
            check_typed_stream( opened, type, "served over http", 16, "http" );
            RELEASE( opened );
            RELEASE( reference );
        }
        RELEASE( uri );
        swprintf( url, ARRAYSIZE(url), L"http://127.0.0.1:%d/missing.htm", port );
        uri = make_uri( url );
        reference = NULL;
        SLOT( statics, RS_CREATE_FROM_URI, HRESULT (WINAPI *)( void *, void *, void ** ) )( statics, uri, &reference );
        if (reference)
        {
            hr = open_read( reference, &opened, &ohr );
            CHECK( FAILED( hr ) && !opened, "http 404 fails: %08lx", hr );
            RELEASE( opened );
            RELEASE( reference );
        }
        RELEASE( uri );
    }

    /* ---- a scheme with no data source ---- */
    uri = make_uri( L"ftp://example.invalid/file.txt" );
    reference = NULL;
    SLOT( statics, RS_CREATE_FROM_URI, HRESULT (WINAPI *)( void *, void *, void ** ) )( statics, uri, &reference );
    CHECK( reference != NULL, "ftp reference is made" );
    if (reference)
    {
        hr = open_read( reference, &opened, &ohr );
        CHECK( FAILED( hr ) && !opened, "ftp is not supported: %08lx", hr );
        RELEASE( reference );
    }
    RELEASE( uri );

    /* the downloaded copies are gone with their streams */
    {
        WIN32_FIND_DATAW fd;
        HANDLE find;
        int left = 0;
        GetTempPathW( MAX_PATH, path );
        wcscat( path, L"sgu*" );
        if ((find = FindFirstFileW( path, &fd )) != INVALID_HANDLE_VALUE)
        {
            do left++; while (FindNextFileW( find, &fd ));
            FindClose( find );
        }
        CHECK( left == 0, "temporary download files left: %d", left );
    }

done:
    (void)buf; (void)data; (void)o3;
    printf( "checks %d fails %d\n", checks, fails );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
