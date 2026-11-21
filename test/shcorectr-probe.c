/* shcorectr-probe: shcore's global counter ordinals 130-132 (SHGlobalCounterGetValue/Increment/Decrement by
 * SHGLOBALCOUNTER number) forward to shlwapi's 632-634 (Stained Glass OS wine-sg 8251;
 * test/shcorectr-gate.sh); they were stubs, so a caller linking them from shcore crashed.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%ld)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    HMODULE shcore = LoadLibraryA( "shcore.dll" ), shlwapi = LoadLibraryA( "shlwapi.dll" );
    LONG (WINAPI *pGet)(int) = (void *)GetProcAddress( shcore, (const char *)130 );
    LONG (WINAPI *pInc)(int) = (void *)GetProcAddress( shcore, (const char *)131 );
    LONG (WINAPI *pDec)(int) = (void *)GetProcAddress( shcore, (const char *)132 );
    LONG (WINAPI *pGetW)(int) = (void *)GetProcAddress( shlwapi, (const char *)632 );
    LONG start, v;

    if (!pGet || !pInc || !pDec || !pGetW) { printf( "FAIL  ordinals exported\nRESULT: FAIL\n" ); return 1; }
    start = pGet( 7 );
    v = pInc( 7 );
    check( v == start + 1, "shcore #131 increments counter 7", v );
    check( pGetW( 7 ) == start + 1, "shlwapi #632 sees the same counter", pGetW( 7 ) );
    check( pGet( 7 ) == start + 1, "shcore #130 reads it", pGet( 7 ) );
    v = pDec( 7 );
    check( v == start && pGet( 7 ) == start, "shcore #132 decrements it", v );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
