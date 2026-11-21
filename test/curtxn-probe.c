/* curtxn-probe: ntdll RtlGetCurrentTransaction / RtlSetCurrentTransaction (Stained Glass OS wine-sg 8281;
 * test/curtxn-gate.sh): the thread's current transaction is kept and read back, per thread (two Store
 * apps import them); Set always failed and Get always gave NULL.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static HANDLE (WINAPI *pGet)(void);
static BOOL (WINAPI *pSet)(HANDLE);
static HANDLE seen_in_thread = (HANDLE)1;

static DWORD WINAPI other( void *arg ) { seen_in_thread = pGet(); return 0; }

int main( void )
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    HANDLE t;
    int fails = 0;

    pGet = (void *)GetProcAddress( ntdll, "RtlGetCurrentTransaction" );
    pSet = (void *)GetProcAddress( ntdll, "RtlSetCurrentTransaction" );
    printf( "%s  none at first\n", !pGet() ? "PASS" : "FAIL" ); if (pGet()) fails++;
    printf( "%s  Set succeeds\n", pSet( (HANDLE)0x1234 ) ? "PASS" : "FAIL" );
    printf( "%s  and Get gives it back\n", pGet() == (HANDLE)0x1234 ? "PASS" : "FAIL" ); if (pGet() != (HANDLE)0x1234) fails++;
    t = CreateThread( NULL, 0, other, NULL, 0, NULL );
    WaitForSingleObject( t, 5000 );
    printf( "%s  another thread has none of its own\n", !seen_in_thread ? "PASS" : "FAIL" ); if (seen_in_thread) fails++;
    pSet( NULL );
    printf( "%s  cleared\n", !pGet() ? "PASS" : "FAIL" ); if (pGet()) fails++;
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
