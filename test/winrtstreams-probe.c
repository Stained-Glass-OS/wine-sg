/* winrtstreams-probe: what Telegram Desktop and Signal Desktop need of
 * WinRT at start (patches/sg/0390-0391).
 *
 *  - Windows.Foundation.Metadata.ApiInformation answers IsPropertyPresent,
 *    IsMethodPresent, IsEventPresent and IsTypePresent (S_OK, not
 *    E_NOTIMPL, which C++/WinRT turns into an exception thrown out of a
 *    Node module's initialization -- Signal); a registered runtime class is
 *    a present type, an unknown one is not;
 *  - Windows.Storage.Streams.DataWriter activates (Telegram's media
 *    controls hold one from the start), writes bytes, numbers in either byte
 *    order and strings, and detaches them as a Buffer;
 *  - a DataWriter over an InMemoryRandomAccessStream stores into it
 *    (StoreAsync completes, its handler is called), and the stream reads
 *    the bytes back into a Buffer (IBufferByteAccess);
 *  - RandomAccessStreamReference.CreateFromStream makes a reference.
 *
 * WinRT interfaces are called through their vtable slots, counted from the
 * public IDL; IIDs are the public ones.
 *
 * Prints name=value lines; see test/winrtstreams-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <string.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
typedef HRESULT (WINAPI *qi_fn)( void *, REFIID, void ** );
typedef ULONG (WINAPI *rel_fn)( void * );
#define QI(obj, iid, out) SLOT( obj, 0, qi_fn )( obj, iid, (void **)(out) )
#define RELEASE(obj) do { if (obj) SLOT( obj, 2, rel_fn )( obj ); } while (0)

DEFINE_GUID(p_IID_IActivationFactory, 0x00000035, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(p_IID_IApiInformationStatics, 0x997439fe, 0xf681, 0x4a11, 0xb4, 0x16, 0xc1, 0x3a, 0x47, 0xe8, 0xba, 0x36);
DEFINE_GUID(p_IID_IDataWriter, 0x64b89265, 0xd341, 0x4922, 0xb3, 0x8a, 0xdd, 0x4a, 0xf8, 0x80, 0x8c, 0x4e);
DEFINE_GUID(p_IID_IDataWriterFactory, 0x338c67c2, 0x8b84, 0x4c2b, 0x9c, 0x50, 0x7b, 0x87, 0x67, 0x84, 0x7a, 0x1f);
DEFINE_GUID(p_IID_IRandomAccessStream, 0x905a0fe1, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IInputStream, 0x905a0fe2, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IOutputStream, 0x905a0fe6, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IBufferFactory, 0x71af914d, 0xc10f, 0x484b, 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27);
DEFINE_GUID(p_IID_IBufferByteAccess, 0x905a0fef, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda);
DEFINE_GUID(p_IID_IRandomAccessStreamReferenceStatics, 0x857309dc, 0x3fbf, 0x4e7d, 0x98, 0x6f, 0xef, 0x3b, 0x1a, 0x07, 0xa9, 0x64);

/* IApiInformationStatics */
#define API_IS_TYPE_PRESENT      6
#define API_IS_METHOD_PRESENT    7
#define API_IS_EVENT_PRESENT     9
#define API_IS_PROPERTY_PRESENT  10
/* IDataWriter */
#define DW_UNSTORED_LENGTH       6
#define DW_PUT_BYTE_ORDER        10
#define DW_WRITE_BYTES           12
#define DW_WRITE_UINT32          21
#define DW_WRITE_STRING          27
#define DW_STORE_ASYNC           29
#define DW_DETACH_BUFFER         31
/* IDataWriterFactory, IBufferFactory */
#define FACTORY_CREATE           6
/* IRandomAccessStream */
#define RAS_GET_SIZE             6
#define RAS_GET_INPUT_STREAM_AT  8
/* IInputStream */
#define IS_READ_ASYNC            6
/* IAsyncOperation<UINT32> */
#define AOP_PUT_COMPLETED        6
#define AOP_GET_RESULTS          8
/* IAsyncOperationWithProgress<IBuffer *, UINT32> */
#define AOWP_GET_RESULTS         10
/* IBuffer */
#define BUF_GET_LENGTH           7
/* IRandomAccessStreamReferenceStatics */
#define RS_CREATE_FROM_STREAM    8

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

static void *activate( const WCHAR *name, const GUID *iid )
{
    HSTRING h = hs( name );
    IInspectable *inspectable = NULL;
    void *out = NULL;
    if (SUCCEEDED( RoActivateInstance( h, &inspectable ) ))
    {
        QI( inspectable, iid, &out );
        RELEASE( inspectable );
    }
    WindowsDeleteString( h );
    return out;
}

/* a Completed handler: IUnknown + Invoke(operation, status) */
struct handler { void **vtbl; LONG calls; int status; };
static HRESULT WINAPI h_qi( struct handler *h, REFIID iid, void **out ) { *out = h; return S_OK; }
static ULONG WINAPI h_addref( struct handler *h ) { return 2; }
static ULONG WINAPI h_release( struct handler *h ) { return 1; }
static HRESULT WINAPI h_invoke( struct handler *h, void *operation, int status )
{
    h->calls++;
    h->status = status;
    return S_OK;
}
static void *handler_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

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

static void check_api_information( void )
{
    void *api = factory( L"Windows.Foundation.Metadata.ApiInformation", &p_IID_IApiInformationStatics );
    HSTRING type = hs( L"Windows.UI.Notifications.ToastNotification" ), member = hs( L"ExpiresOnReboot" );
    HSTRING known = hs( L"Windows.Foundation.Metadata.ApiInformation" ), unknown = hs( L"Probe.No.Such.Type" );
    BOOLEAN value = 2, known_present = 0, unknown_present = 1;
    HRESULT hr, hr2, hr3, hr4;

    if (!api)
    {
        printf( "api_information=0\n" );
        return;
    }
    hr = SLOT( api, API_IS_PROPERTY_PRESENT, HRESULT (WINAPI *)( void *, HSTRING, HSTRING, BOOLEAN * ) )(
            api, type, member, &value );
    printf( "api_property=%d (%#lx %d)\n", hr == S_OK && value <= 1, hr, value );
    value = 2;
    hr2 = SLOT( api, API_IS_METHOD_PRESENT, HRESULT (WINAPI *)( void *, HSTRING, HSTRING, BOOLEAN * ) )(
            api, type, member, &value );
    value = 2;
    hr3 = SLOT( api, API_IS_EVENT_PRESENT, HRESULT (WINAPI *)( void *, HSTRING, HSTRING, BOOLEAN * ) )(
            api, type, member, &value );
    printf( "api_method_event=%d (%#lx %#lx)\n", hr2 == S_OK && hr3 == S_OK, hr2, hr3 );
    hr = SLOT( api, API_IS_TYPE_PRESENT, HRESULT (WINAPI *)( void *, HSTRING, BOOLEAN * ) )( api, known, &known_present );
    hr4 = SLOT( api, API_IS_TYPE_PRESENT, HRESULT (WINAPI *)( void *, HSTRING, BOOLEAN * ) )( api, unknown, &unknown_present );
    printf( "api_type=%d (%#lx %d %#lx %d)\n", hr == S_OK && known_present && hr4 == S_OK && !unknown_present,
            hr, known_present, hr4, unknown_present );
    RELEASE( api );
}

static void check_streams( void )
{
    static const BYTE bytes[] = { 1, 2, 3 };
    void *writer, *stream = NULL, *out = NULL, *writer2 = NULL, *op = NULL, *buffer = NULL, *in = NULL;
    void *buffer_factory, *writer_factory, *read_op = NULL, *result = NULL, *ref_statics, *reference = NULL;
    struct handler handler = { handler_vtbl, 0, -1 };
    UINT32 length = 0, stored = 0, count = 0;
    UINT64 size = 0;
    HSTRING text = hs( L"hi\x00e9" );
    BYTE *b;
    HRESULT hr;

    /* Telegram: a default-constructed DataWriter */
    writer = activate( L"Windows.Storage.Streams.DataWriter", &p_IID_IDataWriter );
    printf( "datawriter_activate=%d\n", writer != NULL );
    if (!writer) return;

    SLOT( writer, DW_WRITE_BYTES, HRESULT (WINAPI *)( void *, UINT32, const BYTE * ) )( writer, 3, bytes );
    SLOT( writer, DW_WRITE_UINT32, HRESULT (WINAPI *)( void *, UINT32 ) )( writer, 0x0a0b0c0d ); /* big endian */
    SLOT( writer, DW_PUT_BYTE_ORDER, HRESULT (WINAPI *)( void *, int ) )( writer, 0 );
    SLOT( writer, DW_WRITE_UINT32, HRESULT (WINAPI *)( void *, UINT32 ) )( writer, 0x0a0b0c0d ); /* little */
    SLOT( writer, DW_WRITE_STRING, HRESULT (WINAPI *)( void *, HSTRING, UINT32 * ) )( writer, text, &count ); /* UTF-8 */
    SLOT( writer, DW_UNSTORED_LENGTH, HRESULT (WINAPI *)( void *, UINT32 * ) )( writer, &length );
    hr = SLOT( writer, DW_DETACH_BUFFER, HRESULT (WINAPI *)( void *, void ** ) )( writer, &buffer );
    b = buffer ? buffer_bytes( buffer ) : NULL;
    printf( "datawriter_bytes=%d (%#lx len %u count %u)\n", SUCCEEDED(hr) && length == 15 && count == 4 && b &&
            !memcmp( b, "\x01\x02\x03\x0a\x0b\x0c\x0d\x0d\x0c\x0b\x0a" "hi\xc3\xa9", 15 ), hr, length, count );
    RELEASE( buffer ); buffer = NULL;
    RELEASE( writer );

    /* Telegram's thumbnail: DataWriter(InMemoryRandomAccessStream()), WriteBytes, StoreAsync */
    stream = activate( L"Windows.Storage.Streams.InMemoryRandomAccessStream", &p_IID_IRandomAccessStream );
    printf( "memory_stream=%d\n", stream != NULL );
    if (!stream) return;
    QI( stream, &p_IID_IOutputStream, &out );
    writer_factory = factory( L"Windows.Storage.Streams.DataWriter", &p_IID_IDataWriterFactory );
    if (writer_factory && out)
        SLOT( writer_factory, FACTORY_CREATE, HRESULT (WINAPI *)( void *, void *, void ** ) )( writer_factory, out, &writer2 );
    if (writer2)
    {
        SLOT( writer2, DW_WRITE_BYTES, HRESULT (WINAPI *)( void *, UINT32, const BYTE * ) )( writer2, 3, bytes );
        hr = SLOT( writer2, DW_STORE_ASYNC, HRESULT (WINAPI *)( void *, void ** ) )( writer2, &op );
        if (op)
        {
            SLOT( op, AOP_PUT_COMPLETED, HRESULT (WINAPI *)( void *, void * ) )( op, &handler );
            SLOT( op, AOP_GET_RESULTS, HRESULT (WINAPI *)( void *, UINT32 * ) )( op, &stored );
        }
        SLOT( stream, RAS_GET_SIZE, HRESULT (WINAPI *)( void *, UINT64 * ) )( stream, &size );
        printf( "store_async=%d (%#lx handler %ld status %d stored %u size %I64u)\n", SUCCEEDED(hr) && handler.calls == 1
                && handler.status == 1 && stored == 3 && size == 3, hr, handler.calls, handler.status, stored, size );
    }
    else printf( "store_async=0 (no writer)\n" );

    /* and read it back */
    buffer_factory = factory( L"Windows.Storage.Streams.Buffer", &p_IID_IBufferFactory );
    if (buffer_factory)
        SLOT( buffer_factory, FACTORY_CREATE, HRESULT (WINAPI *)( void *, UINT32, void ** ) )( buffer_factory, 16, &buffer );
    SLOT( stream, RAS_GET_INPUT_STREAM_AT, HRESULT (WINAPI *)( void *, UINT64, void ** ) )( stream, 0, &in );
    length = 0;
    if (buffer && in &&
        SUCCEEDED( SLOT( in, IS_READ_ASYNC, HRESULT (WINAPI *)( void *, void *, UINT32, int, void ** ) )( in, buffer, 16, 0, &read_op ) ))
    {
        SLOT( read_op, AOWP_GET_RESULTS, HRESULT (WINAPI *)( void *, void ** ) )( read_op, &result );
        if (result) SLOT( result, BUF_GET_LENGTH, HRESULT (WINAPI *)( void *, UINT32 * ) )( result, &length );
    }
    b = result ? buffer_bytes( result ) : NULL;
    printf( "stream_read=%d (len %u)\n", length == 3 && b && !memcmp( b, bytes, 3 ), length );

    ref_statics = factory( L"Windows.Storage.Streams.RandomAccessStreamReference", &p_IID_IRandomAccessStreamReferenceStatics );
    if (ref_statics)
        SLOT( ref_statics, RS_CREATE_FROM_STREAM, HRESULT (WINAPI *)( void *, void *, void ** ) )( ref_statics, stream, &reference );
    printf( "stream_reference=%d\n", reference != NULL );

    RELEASE( reference ); RELEASE( ref_statics ); RELEASE( result ); RELEASE( read_op ); RELEASE( in );
    RELEASE( buffer ); RELEASE( buffer_factory ); RELEASE( op ); RELEASE( writer2 ); RELEASE( writer_factory );
    RELEASE( out ); RELEASE( stream );
}

int main( void )
{
    RoInitialize( RO_INIT_MULTITHREADED );
    check_api_information();
    check_streams();
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
