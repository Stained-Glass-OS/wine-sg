/* oobecomplete-probe: kernel32 OOBEComplete, which was not exported (Stained Glass OS wine-sg 8268;
 * test/oobecomplete-gate.sh): finished unless setup marks the out-of-box experience as running.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    BOOL (WINAPI *pOOBE)(BOOL *) = (void *)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "OOBEComplete" );
    BOOL done = 5, ret;
    DWORD one = 1;
    HKEY key;
    int fails = 0;

    if (!pOOBE) { printf( "FAIL  kernel32 exports OOBEComplete\nRESULT: FAIL\n" ); return 1; }
    ret = pOOBE( &done );
    printf( "%s  a set-up machine: complete (%d, %d)\n", ret && done == TRUE ? "PASS" : "FAIL", ret, done );
    if (!(ret && done == TRUE)) fails++;
    RegCreateKeyA( HKEY_LOCAL_MACHINE, "System\\Setup", &key );
    RegSetValueExA( key, "OOBEInProgress", 0, REG_DWORD, (BYTE *)&one, sizeof(one) );
    ret = pOOBE( &done );
    printf( "%s  OOBEInProgress set: not complete (%d, %d)\n", ret && done == FALSE ? "PASS" : "FAIL", ret, done );
    if (!(ret && done == FALSE)) fails++;
    RegDeleteValueA( key, "OOBEInProgress" );
    RegCloseKey( key );
    ret = pOOBE( NULL );
    printf( "%s  no pointer: FALSE, ERROR_INVALID_PARAMETER (%lu)\n", !ret && GetLastError() == ERROR_INVALID_PARAMETER ? "PASS" : "FAIL", GetLastError() );
    if (ret || GetLastError() != ERROR_INVALID_PARAMETER) fails++;
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
