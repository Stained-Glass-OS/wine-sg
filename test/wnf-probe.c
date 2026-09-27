/* NtQueryWnfStateData (wine-sg 0433): ask for the quiet-hours state the way
 * Mozilla's notification code does before announcing new mail.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *query_fn)( const void *, const void *, const void *, ULONG *, void *, ULONG * );

int main( void )
{
    /* a well-known state name's value (the shell's quiet-hours profile) */
    static const ULONGLONG state = 0xd83063ea3bf5075ULL;
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    query_fn q = (query_fn)GetProcAddress( ntdll, "NtQueryWnfStateData" );
    query_fn zq = (query_fn)GetProcAddress( ntdll, "ZwQueryWnfStateData" );
    ULONG stamp = 0xdead, size = 4, data = 0xbeef;
    LONG st;

    setvbuf( stdout, NULL, _IONBF, 0 );
    printf( "EXPORTED %d %d\n", !!q, !!zq );
    if (!q) return 1;
    st = q( &state, NULL, NULL, &stamp, &data, &size );
    printf( "QUERY %08lx stamp %lu size %lu\n", st, stamp, size );
    st = q( NULL, NULL, NULL, &stamp, &data, &size );
    printf( "NULLNAME %08lx\n", st );
    return 0;
}
