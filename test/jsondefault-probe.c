/* test/jsondefault-gate.sh's probe (7721): Windows.Data.Json's IJsonObjectWithDefaultValues.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
static const GUID iid_object_statics = {0x2289f159,0x54de,0x45d8,{0xab,0xcc,0x22,0x60,0x3f,0xa0,0x66,0xa0}};
static const GUID iid_array_statics = {0xdb1434a9,0xe164,0x499f,{0x93,0xe2,0x8a,0x8f,0x49,0xbb,0x90,0xba}};
static const GUID iid_defaults = {0xd960d2a2,0xb7f0,0x4f00,{0x8e,0x44,0xd8,0x2c,0xf4,0x15,0xea,0x13}};

static HSTRING hs( const WCHAR *s ) { HSTRING h = NULL; WindowsCreateString( s, wcslen( s ), &h ); return h; }

int main( void )
{
    void *statics = NULL, *obj = NULL, *d = NULL, *v = NULL;
    HSTRING str = NULL;
    double num = 0;
    BOOLEAN b = 0;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_MULTITHREADED );
    hr = RoGetActivationFactory( hs( L"Windows.Data.Json.JsonObject" ), &iid_object_statics, &statics );
    printf( "Statics=0x%08lx\n", hr );
    if (FAILED(hr)) return 1;
    hr = SLOT( statics, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( statics, hs( L"{\"s\":\"x\",\"n\":3,\"b\":true,\"o\":{\"k\":1}}" ), &obj );
    printf( "Parse=0x%08lx\n", hr );
    if (FAILED(hr)) return 1;
    {
        void *bad = (void *)1, *arr_statics = NULL, *arr = NULL;
        BOOLEAN ok = 2;
        hr = SLOT( statics, 7, HRESULT (WINAPI *)( void *, HSTRING, void **, BOOLEAN * ) )( statics, hs( L"[1]" ), &bad, &ok );
        printf( "TryParseArrayAsObject=0x%08lx %d %d\n", hr, ok, bad == NULL );
        hr = RoGetActivationFactory( hs( L"Windows.Data.Json.JsonArray" ), &iid_array_statics, &arr_statics );
        if (SUCCEEDED(hr)) hr = SLOT( arr_statics, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( arr_statics, hs( L"[1,2]" ), &arr );
        printf( "ArrayParse=0x%08lx\n", hr );
    }
    hr = SLOT( obj, 0, HRESULT (WINAPI *)( void *, const GUID *, void ** ) )( obj, &iid_defaults, &d );
    printf( "Defaults=0x%08lx\n", hr );
    if (FAILED(hr)) return 0;
    SLOT( d, 8, HRESULT (WINAPI *)( void *, HSTRING, HSTRING, HSTRING * ) )( d, hs( L"s" ), hs( L"def" ), &str );
    printf( "StringPresent=%ls\n", WindowsGetStringRawBuffer( str, NULL ) );
    SLOT( d, 8, HRESULT (WINAPI *)( void *, HSTRING, HSTRING, HSTRING * ) )( d, hs( L"missing" ), hs( L"def" ), &str );
    printf( "StringMissing=%ls\n", WindowsGetStringRawBuffer( str, NULL ) );
    SLOT( d, 10, HRESULT (WINAPI *)( void *, HSTRING, double, double * ) )( d, hs( L"n" ), 7.0, &num );
    printf( "NumberPresent=%g\n", num );
    SLOT( d, 10, HRESULT (WINAPI *)( void *, HSTRING, double, double * ) )( d, hs( L"missing" ), 7.0, &num );
    printf( "NumberMissing=%g\n", num );
    SLOT( d, 11, HRESULT (WINAPI *)( void *, HSTRING, BOOLEAN, BOOLEAN * ) )( d, hs( L"b" ), FALSE, &b );
    printf( "BoolPresent=%d\n", b );
    SLOT( d, 11, HRESULT (WINAPI *)( void *, HSTRING, BOOLEAN, BOOLEAN * ) )( d, hs( L"missing" ), TRUE, &b );
    printf( "BoolMissing=%d\n", b );
    hr = SLOT( d, 7, HRESULT (WINAPI *)( void *, HSTRING, void *, void ** ) )( d, hs( L"o" ), NULL, &v );
    printf( "ObjectPresent=%d\n", SUCCEEDED(hr) && v != NULL );
    v = (void *)1;
    hr = SLOT( d, 7, HRESULT (WINAPI *)( void *, HSTRING, void *, void ** ) )( d, hs( L"missing" ), NULL, &v );
    printf( "ObjectMissing=0x%08lx %d\n", hr, v == NULL );
    v = (void *)1;
    hr = SLOT( d, 6, HRESULT (WINAPI *)( void *, HSTRING, void *, void ** ) )( d, hs( L"missing" ), NULL, &v );
    printf( "ValueMissing=0x%08lx %d\n", hr, v == NULL );
    return 0;
}
