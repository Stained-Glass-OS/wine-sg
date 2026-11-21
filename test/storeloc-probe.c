/* storeloc-probe: crypt32 CertEnumSystemStoreLocation, which was not exported (Stained Glass OS wine-sg 8265;
 * test/storeloc-gate.sh): the eight predefined system store locations, with their location IDs.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static int count, stop_after = 100, ok = 1;
static BOOL WINAPI on_location( const WCHAR *name, DWORD flags, void *reserved, void *arg )
{
    static const WCHAR *want[] = { L"CurrentUser", L"LocalMachine", L"CurrentService", L"Services", L"Users",
                                   L"CurrentUserGroupPolicy", L"LocalMachineGroupPolicy", L"LocalMachineEnterprise" };
    static const DWORD ids[] = { 1, 2, 4, 5, 6, 7, 8, 9 };
    if (count < 8 && (lstrcmpW( name, want[count] ) || flags != ids[count] << 16 || arg != (void *)0x1234)) ok = 0;
    printf( "      %ls %#lx\n", name, flags );
    return ++count < stop_after;
}

int main( void )
{
    BOOL (WINAPI *pEnum)(DWORD, void *, PFN_CERT_ENUM_SYSTEM_STORE_LOCATION) =
        (void *)GetProcAddress( LoadLibraryA( "crypt32.dll" ), "CertEnumSystemStoreLocation" );
    BOOL ret;
    int fails = 0;

    if (!pEnum) { printf( "FAIL  crypt32 exports CertEnumSystemStoreLocation\nRESULT: FAIL\n" ); return 1; }
    ret = pEnum( 0, (void *)0x1234, on_location );
    printf( "%s  eight locations, names and IDs as documented\n", ret && count == 8 && ok ? "PASS" : "FAIL" );
    if (!(ret && count == 8 && ok)) fails++;
    count = 0; stop_after = 3;
    ret = pEnum( 0, (void *)0x1234, on_location );
    printf( "%s  a callback returning FALSE stops it (FALSE after 3)\n", !ret && count == 3 ? "PASS" : "FAIL" );
    if (ret || count != 3) fails++;
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
