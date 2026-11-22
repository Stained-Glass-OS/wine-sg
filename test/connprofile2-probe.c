/* connprofile2-probe: Windows.Networking.Connectivity ConnectionProfile answers IConnectionProfile2 (Stained
 * Glass OS wine-sg 8299; test/connprofile2-gate.sh).  Qt 6's network information plugin used it without
 * checking it was there, and Audacity 4 crashed at start on E_NOINTERFACE.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *fn)( void *, ... );
#define CALL(obj, idx, ...) (((fn *)*(void **)(obj))[idx]((obj), ##__VA_ARGS__))

static const GUID IID_IActivationFactory_ = { 0x00000035, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID IID_INetworkInformationStatics_ = { 0x5074f851, 0x950d, 0x4165, { 0x9c, 0x15, 0x36, 0x56, 0x19, 0x48, 0x1e, 0xea } };
static const GUID IID_IConnectionProfile2_ = { 0xe2045145, 0x4c9f, 0x400c, { 0x91, 0x50, 0x7e, 0xc7, 0xd6, 0xe2, 0x88, 0x8a } };

int main( void )
{
    HMODULE combase = LoadLibraryA( "combase.dll" );
    HRESULT (WINAPI *pRoInitialize)( int ) = (void *)GetProcAddress( combase, "RoInitialize" );
    HRESULT (WINAPI *pGetFactory)( void *, REFIID, void ** ) = (void *)GetProcAddress( combase, "RoGetActivationFactory" );
    HRESULT (WINAPI *pCreateString)( const WCHAR *, UINT32, void ** ) = (void *)GetProcAddress( combase, "WindowsCreateString" );
    void *name, *statics = NULL, *profile = NULL, *p2 = NULL, *adapter = NULL, *ref = (void *)1;
    BYTE wwan = 9, wlan = 9;
    INT32 domain = 9;
    UINT32 iana = 0;
    HRESULT hr;
    int fails = 0;

    pRoInitialize( 1 );
    pCreateString( L"Windows.Networking.Connectivity.NetworkInformation", 51, &name );
    hr = pGetFactory( name, &IID_INetworkInformationStatics_, &statics );
    if (FAILED(hr)) { printf( "FAIL  no NetworkInformation statics (%#lx)\nRESULT: FAIL\n", hr ); return 1; }
    hr = CALL( statics, 7, &profile );   /* GetInternetConnectionProfile */
    if (!profile)
    {
        void *list = NULL; UINT32 n = 0;
        CALL( statics, 6, &list );        /* GetConnectionProfiles */
        if (list && SUCCEEDED(CALL( list, 7, &n )) && n) CALL( list, 6, 0, &profile );   /* IVectorView Size, GetAt */
    }
    if (!profile) { printf( "SKIP: no connection profile on this host\n" ); return 77; }

    hr = CALL( profile, 0, &IID_IConnectionProfile2_, &p2 );
    printf( "%s  the profile answers IConnectionProfile2 (%#lx)\n", hr == S_OK && p2 ? "PASS" : "FAIL", hr );
    if (hr != S_OK || !p2) { printf( "RESULT: FAIL\n" ); return 1; }
    hr = CALL( p2, 6, &wwan );
    printf( "%s  IsWwanConnectionProfile (%#lx, %u)\n", hr == S_OK && wwan <= 1 ? "PASS" : "FAIL", hr, wwan );
    if (hr != S_OK || wwan > 1) fails++;
    hr = CALL( p2, 7, &wlan );
    CALL( profile, 11, &adapter );       /* get_NetworkAdapter */
    if (adapter) CALL( adapter, 8, &iana );   /* IanaInterfaceType */
    printf( "%s  IsWlanConnectionProfile agrees with the adapter's interface type (%#lx, %u, iana %u)\n",
            hr == S_OK && wlan == (iana == 71) ? "PASS" : "FAIL", hr, wlan, iana );
    if (hr != S_OK || wlan != (iana == 71)) fails++;
    hr = CALL( p2, 10, &ref );
    printf( "%s  ServiceProviderGuid: none (%#lx, %p)\n", hr == S_OK && !ref ? "PASS" : "FAIL", hr, ref );
    if (hr != S_OK || ref) fails++;
    hr = CALL( p2, 12, &domain );
    printf( "%s  GetDomainConnectivityLevel: None (%#lx, %d)\n", hr == S_OK && domain == 0 ? "PASS" : "FAIL", hr, domain );
    if (hr != S_OK || domain) fails++;
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
