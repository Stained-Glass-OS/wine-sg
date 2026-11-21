/* raseap-probe: rasapi32 RasGetEapUserIdentity / RasFreeEapUserIdentity, which were not exported (Stained
 * Glass OS wine-sg 8271; test/raseap-gate.sh): a missing entry is not found, an entry without EAP has no
 * EAP identity.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <ras.h>
#include <raserror.h>
#include <stdio.h>

int main( void )
{
    HMODULE ras = LoadLibraryA( "rasapi32.dll" );
    DWORD (WINAPI *pGet)(const WCHAR *, const WCHAR *, DWORD, HWND, void **) = (void *)GetProcAddress( ras, "RasGetEapUserIdentityW" );
    void (WINAPI *pFree)(void *) = (void *)GetProcAddress( ras, "RasFreeEapUserIdentityW" );
    RASENTRYW entry;
    void *id = (void *)1;
    DWORD ret;
    int fails = 0;

    if (!pGet || !pFree) { printf( "FAIL  rasapi32 exports them\nRESULT: FAIL\n" ); return 1; }
    ret = pGet( NULL, L"SG no such entry", 0, NULL, &id );
    printf( "%s  a missing entry: ERROR_CANNOT_FIND_PHONEBOOK_ENTRY (%lu), no identity\n", ret == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY && !id ? "PASS" : "FAIL", ret );
    if (ret != ERROR_CANNOT_FIND_PHONEBOOK_ENTRY || id) fails++;

    memset( &entry, 0, sizeof(entry) );
    entry.dwSize = sizeof(entry);
    entry.dwfOptions = RASEO_RemoteDefaultGateway;
    entry.dwfNetProtocols = RASNP_Ip;
    entry.dwFramingProtocol = RASFP_Ppp;
    entry.dwType = RASET_Vpn;
    entry.dwVpnStrategy = VS_Default;
    wcscpy( entry.szLocalPhoneNumber, L"vpn.example.test" );
    wcscpy( entry.szDeviceType, L"vpn" );
    wcscpy( entry.szDeviceName, L"WAN Miniport (PPTP)" );
    ret = RasSetEntryPropertiesW( NULL, L"SG EAP probe", &entry, sizeof(entry), NULL, 0 );
    printf( "      RasSetEntryProperties: %lu\n", ret );
    id = (void *)1;
    ret = pGet( NULL, L"SG EAP probe", 0, NULL, &id );
    printf( "%s  an entry without EAP: ERROR_INVALID_FUNCTION_FOR_ENTRY (%lu)\n", ret == ERROR_INVALID_FUNCTION_FOR_ENTRY && !id ? "PASS" : "FAIL", ret );
    if (ret != ERROR_INVALID_FUNCTION_FOR_ENTRY || id) fails++;
    RasDeleteEntryW( NULL, L"SG EAP probe" );
    ret = pGet( NULL, NULL, 0, NULL, &id );
    printf( "%s  no entry name: ERROR_INVALID_PARAMETER (%lu)\n", ret == ERROR_INVALID_PARAMETER ? "PASS" : "FAIL", ret );
    if (ret != ERROR_INVALID_PARAMETER) fails++;
    pFree( NULL );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
