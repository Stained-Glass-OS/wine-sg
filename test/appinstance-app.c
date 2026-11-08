/* test/appinstance-gate.sh's packaged probe (7708): Windows.ApplicationModel.AppInstance.
 *   register KEY [HOLD_SECONDS]   FindOrRegisterInstanceForKey: Register=hr, Current=0|1, Key=...; then waits
 *   instances                     GetInstances: Instances=hr count, the keys
 *   unregister KEY                FindOrRegister, Unregister, then GetInstances count
 *   statics                       RecommendedInstance / GetActivatedEventArgs: null
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stdlib.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
static const GUID iid_statics = {0x9d11e77f,0x9ea6,0x47af,{0xa6,0xec,0x46,0x78,0x4c,0x5b,0xa2,0x54}};

static void *statics(void)
{
    HSTRING name;
    void *out = NULL;
    HRESULT hr;
    WindowsCreateString( L"Windows.ApplicationModel.AppInstance", 36, &name );
    hr = RoGetActivationFactory( name, &iid_statics, &out );
    printf( "Factory=0x%08lx\n", hr );
    return out;
}

static void print_instance( void *inst )
{
    HSTRING key = NULL;
    BOOLEAN cur = 2;
    SLOT( inst, 6, HRESULT (WINAPI *)( void *, HSTRING * ) )( inst, &key );
    SLOT( inst, 7, HRESULT (WINAPI *)( void *, BOOLEAN * ) )( inst, &cur );
    printf( "Current=%d\nKey=%ls\n", cur, WindowsGetStringRawBuffer( key, NULL ) );
}

static int instances( void *s )
{
    void *vec = NULL, *inst;
    UINT32 n = 0, i;
    HRESULT hr = SLOT( s, 10, HRESULT (WINAPI *)( void *, void ** ) )( s, &vec );
    if (SUCCEEDED(hr)) SLOT( vec, 7, HRESULT (WINAPI *)( void *, UINT32 * ) )( vec, &n );
    printf( "Instances=0x%08lx %u\n", hr, n );
    for (i = 0; i < n; i++)
        if (SUCCEEDED(SLOT( vec, 6, HRESULT (WINAPI *)( void *, UINT32, void ** ) )( vec, i, &inst ))) print_instance( inst );
    return n;
}

int wmain( int argc, WCHAR **argv )
{
    void *s, *inst = NULL;
    HSTRING key;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    if (!(s = statics())) return 1;
    if (argc >= 3 && !wcscmp( argv[1], L"register" ))
    {
        WindowsCreateString( argv[2], wcslen( argv[2] ), &key );
        hr = SLOT( s, 8, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( s, key, &inst );
        printf( "Register=0x%08lx\n", hr );
        if (SUCCEEDED(hr)) print_instance( inst );
        fflush( stdout );
        if (argc > 3) Sleep( _wtoi( argv[3] ) * 1000 );
    }
    else if (argc >= 2 && !wcscmp( argv[1], L"instances" )) instances( s );
    else if (argc >= 3 && !wcscmp( argv[1], L"unregister" ))
    {
        WindowsCreateString( argv[2], wcslen( argv[2] ), &key );
        SLOT( s, 8, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( s, key, &inst );
        printf( "Unregister=0x%08lx\n", SLOT( s, 9, HRESULT (WINAPI *)( void * ) )( s ) );
        instances( s );
    }
    else if (argc >= 2 && !wcscmp( argv[1], L"statics" ))
    {
        void *a = (void *)1, *b = (void *)1;
        HRESULT h1 = SLOT( s, 6, HRESULT (WINAPI *)( void *, void ** ) )( s, &a );
        HRESULT h2 = SLOT( s, 7, HRESULT (WINAPI *)( void *, void ** ) )( s, &b );
        printf( "Recommended=0x%08lx %d\nActivated=0x%08lx %d\n", h1, !a, h2, !b );
    }
    return 0;
}
