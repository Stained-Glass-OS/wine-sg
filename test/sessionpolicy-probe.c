/* test/sessionpolicy-gate.sh's probe (7706): Windows.System.RemoteDesktop.InteractiveSession.IsRemote,
 * Windows.System.Profile.WindowsIntegrityPolicy, and kernel32 WerRegister/UnregisterAdditionalProcess.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

static const IID iid_session = {0x60884631,0xdd3a,0x4576,{0x9c,0x8d,0xe8,0x02,0x76,0x18,0xbd,0xce}};
static const IID iid_integrity = {0x7d1d81db,0x8d63,0x4789,{0x9e,0xa5,0xdd,0xcf,0x65,0xa9,0x4f,0x3c}};

typedef struct { void **vtbl; } obj;
typedef HRESULT (WINAPI *get_bool)( void *, boolean * );
typedef HRESULT (WINAPI *add_event)( void *, void *, INT64 * );
typedef HRESULT (WINAPI *remove_event)( void *, INT64 );
typedef HRESULT (WINAPI *release_func)( void * );

static void *factory( const WCHAR *name, const IID *iid )
{
    HSTRING str;
    void *out = NULL;
    HRESULT hr;

    WindowsCreateString( name, wcslen( name ), &str );
    hr = RoGetActivationFactory( str, iid, &out );
    printf( "Factory=0x%08lx\n", hr );
    WindowsDeleteString( str );
    return out;
}

/* a do-nothing IUnknown-shaped handler: the event is never raised */
static HRESULT WINAPI h_qi( void *iface, const IID *iid, void **out ) { *out = iface; return S_OK; }
static ULONG WINAPI h_ref( void *iface ) { return 1; }
static HRESULT WINAPI h_invoke( void *iface, void *sender, void *args ) { return S_OK; }
static void *handler_vtbl[] = { h_qi, h_ref, h_ref, h_invoke };
static obj handler = { handler_vtbl };

int main( int argc, char **argv )
{
    typedef HRESULT (WINAPI *reg_func)( DWORD, DWORD );
    typedef HRESULT (WINAPI *unreg_func)( DWORD );
    HMODULE k32 = GetModuleHandleA( "kernel32" );
    obj *o;
    boolean b;
    INT64 token = 0;

    RoInitialize( RO_INIT_MULTITHREADED );
    if (argc > 1 && !strcmp( argv[1], "session" ))
    {
        if (!(o = factory( L"Windows.System.RemoteDesktop.InteractiveSession", &iid_session ))) return 1;
        HRESULT hr;
        b = 2;
        hr = ((get_bool)o->vtbl[6])( o, &b );
        printf( "IsRemote=0x%08lx %d\n", hr, b );
        printf( "SystemRemote=%d\n", !!GetSystemMetrics( SM_REMOTESESSION ) );
    }
    else if (argc > 1 && !strcmp( argv[1], "integrity" ))
    {
        int i;
        if (!(o = factory( L"Windows.System.Profile.WindowsIntegrityPolicy", &iid_integrity ))) return 1;
        for (i = 6; i <= 9; i++)
        {
            HRESULT hr;
            b = 2;
            hr = ((get_bool)o->vtbl[i])( o, &b );
            printf( "Prop%d=0x%08lx %d\n", i - 5, hr, b );
        }
        printf( "AddPolicyChanged=0x%08lx\n", ((add_event)o->vtbl[10])( o, &handler, &token ) );
        printf( "RemovePolicyChanged=0x%08lx\n", ((remove_event)o->vtbl[11])( o, token ) );
    }
    else if (argc > 1 && !strcmp( argv[1], "wer" ))
    {
        reg_func reg = (reg_func)GetProcAddress( k32, "WerRegisterAdditionalProcess" );
        unreg_func unreg = (unreg_func)GetProcAddress( k32, "WerUnregisterAdditionalProcess" );
        if (!reg || !unreg) { printf( "Wer=(missing)\n" ); return 1; }
        printf( "Reg0=0x%08lx\n", reg( 0, 0 ) );
        printf( "Reg=0x%08lx\n", reg( 1234, 0 ) );
        printf( "RegAgain=0x%08lx\n", reg( 1234, 5678 ) );
        printf( "Unreg=0x%08lx\n", unreg( 1234 ) );
        printf( "UnregAgain=0x%08lx\n", unreg( 1234 ) );
    }
    return 0;
}
