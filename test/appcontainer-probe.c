/* kernelbase's AppContainer SID names (wine-sg 0434): Chromium's sandbox
 * registers its AppContainer SIDs with AppContainerRegisterSid and stops the
 * browser when the export is missing.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *reg_fn)( PSID, const WCHAR *, const WCHAR * );
typedef HRESULT (WINAPI *unreg_fn)( PSID );
typedef HRESULT (WINAPI *lookup_fn)( PSID, WCHAR ** );
typedef void (WINAPI *free_fn)( void * );
typedef HRESULT (WINAPI *derive_fn)( const WCHAR *, PSID * );

int main( void )
{
    HMODULE kb = GetModuleHandleA( "kernelbase.dll" ), ue = LoadLibraryA( "userenv.dll" );
    reg_fn reg = (reg_fn)GetProcAddress( kb, "AppContainerRegisterSid" );
    unreg_fn unreg = (unreg_fn)GetProcAddress( kb, "AppContainerUnregisterSid" );
    lookup_fn lookup = (lookup_fn)GetProcAddress( kb, "AppContainerLookupMoniker" );
    free_fn fr = (free_fn)GetProcAddress( kb, "AppContainerFreeMemory" );
    derive_fn derive = (derive_fn)GetProcAddress( ue, "DeriveAppContainerSidFromAppContainerName" );
    WCHAR *moniker = NULL;
    PSID sid = NULL;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    printf( "EXPORTS %d %d %d %d\n", !!reg, !!unreg, !!lookup, !!fr );
    if (!reg || !unreg || !lookup || !fr || !derive) return 1;
    hr = derive( L"sg.test.appcontainer", &sid );
    printf( "DERIVED %08lx\n", hr );
    printf( "REGISTER %08lx\n", reg( sid, L"sg.test.appcontainer", L"Stained Glass test" ) );
    hr = lookup( sid, &moniker );
    printf( "LOOKUP %08lx %ls\n", hr, moniker ? moniker : L"(none)" );
    fr( moniker ); moniker = NULL;
    printf( "UNREGISTER %08lx\n", unreg( sid ) );
    hr = lookup( sid, &moniker );
    printf( "LOOKUP_AFTER %08lx %ls\n", hr, moniker ? moniker : L"(none)" );
    return 0;
}
