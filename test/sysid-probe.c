/* sysid-probe: Windows.System.Profile.SystemIdentification, as AnyDesk asks
 * for it. Prints the publisher and user identifiers (hex) and their source,
 * and the publisher identifier as CryptographicBuffer.EncodeToHexString
 * gives it (AnyDesk's next step). */
#include <windows.h>
#include <stdio.h>
#include <roapi.h>
#include <winstring.h>

typedef struct { void **vtbl; } obj;
#define CALL(o, slot, type, ...) ((type)((obj *)(o))->vtbl[slot])((o), ##__VA_ARGS__)

static const GUID IID_ISystemIdentificationStatics = { 0x5581f42a, 0xd3df, 0x4d93, { 0xa3, 0x7d, 0xc4, 0x1a, 0x61, 0x6c, 0x6d, 0x01 } };
static const GUID IID_ICryptographicBufferStatics = { 0x320b7e22, 0x3cb0, 0x4cdf, { 0x86, 0x63, 0x1d, 0x28, 0x91, 0x00, 0x65, 0xeb } };
static const GUID IID_IBufferByteAccess = { 0x905a0fef, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };

typedef HRESULT (WINAPI *get_ptr)(void *, void **);
typedef HRESULT (WINAPI *get_user)(void *, void *, void **);
typedef HRESULT (WINAPI *get_u32)(void *, UINT32 *);
typedef HRESULT (WINAPI *get_int)(void *, int *);
typedef HRESULT (WINAPI *qi)(void *, const GUID *, void **);
typedef ULONG (WINAPI *rel)(void *);
typedef HRESULT (WINAPI *to_hex)(void *, void *, HSTRING *);
typedef HRESULT (WINAPI *from_bytes)(void *, UINT32, BYTE *, void **);
typedef HRESULT (WINAPI *to_bytes)(void *, void *, UINT32 *, BYTE **);

static void *crypto;

/* CryptographicBuffer: the Id in hex, and back through bytes */
static void show_hex( const char *what, void *buffer )
{
    HSTRING hex = NULL;
    void *copy = NULL;
    BYTE *bytes = NULL;
    UINT32 len = 0;
    HRESULT hr;

    if (!crypto) return;
    hr = CALL( crypto, 12, to_hex, buffer, &hex );       /* EncodeToHexString */
    if (FAILED(hr)) { printf( "%s-hex-hr %08lx\n", what, hr ); return; }
    printf( "%s-hex %ls\n", what, WindowsGetStringRawBuffer( hex, NULL ) );
    WindowsDeleteString( hex );
    hr = CALL( crypto, 10, to_bytes, buffer, &len, &bytes ); /* CopyToByteArray */
    if (SUCCEEDED(hr)) hr = CALL( crypto, 9, from_bytes, len, bytes, &copy ); /* CreateFromByteArray */
    if (SUCCEEDED(hr)) hr = CALL( crypto, 12, to_hex, copy, &hex );
    if (FAILED(hr)) { printf( "%s-copy-hr %08lx\n", what, hr ); return; }
    printf( "%s-copyhex %ls\n", what, WindowsGetStringRawBuffer( hex, NULL ) );
    WindowsDeleteString( hex );
    CoTaskMemFree( bytes );
    CALL( copy, 2, rel );
}

static void show( const char *what, void *info, HRESULT hr )
{
    void *buffer = NULL, *access = NULL;
    BYTE *bytes = NULL;
    UINT32 len = 0, i;
    int source = -1;

    if (FAILED(hr) || !info) { printf( "%s-hr %08lx\n", what, hr ); return; }
    hr = CALL( info, 6, get_ptr, &buffer );            /* get_Id */
    if (FAILED(hr) || !buffer) { printf( "%s-id-hr %08lx\n", what, hr ); return; }
    CALL( info, 7, get_int, &source );                 /* get_Source */
    CALL( buffer, 7, get_u32, &len );                  /* IBuffer::get_Length */
    if (SUCCEEDED(CALL( buffer, 0, qi, &IID_IBufferByteAccess, &access )))
        CALL( access, 3, get_ptr, (void **)&bytes );   /* IBufferByteAccess::Buffer */
    printf( "%s-hr 00000000\n%s-len %u\n%s-source %d\n%s-id ", what, what, len, what, source, what );
    for (i = 0; bytes && i < len; i++) printf( "%02x", bytes[i] );
    printf( "\n" );
    show_hex( what, buffer );
    if (access) CALL( access, 2, rel );
    CALL( buffer, 2, rel );
    CALL( info, 2, rel );
}

int main(void)
{
    static const WCHAR name[] = L"Windows.System.Profile.SystemIdentification";
    static const WCHAR crypto_name[] = L"Windows.Security.Cryptography.CryptographicBuffer";
    void *statics = NULL, *info = NULL;
    HSTRING str;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    WindowsCreateString( crypto_name, lstrlenW( crypto_name ), &str );
    hr = RoGetActivationFactory( str, &IID_ICryptographicBufferStatics, &crypto );
    printf( "crypto-hr %08lx\n", hr );
    WindowsCreateString( name, lstrlenW( name ), &str );
    hr = RoGetActivationFactory( str, &IID_ISystemIdentificationStatics, &statics );
    printf( "factory-hr %08lx\n", hr );
    if (FAILED(hr)) return 1;
    hr = CALL( statics, 6, get_ptr, &info );            /* GetSystemIdForPublisher */
    show( "publisher", info, hr );
    info = NULL;
    hr = CALL( statics, 6, get_ptr, &info );
    show( "publisher2", info, hr );
    info = NULL;
    hr = CALL( statics, 7, get_user, NULL, &info );     /* GetSystemIdForUser */
    show( "user", info, hr );
    return 0;
}
