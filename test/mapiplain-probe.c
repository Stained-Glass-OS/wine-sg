/* mapiplain-probe: the 64-bit mapi32 exports its functions by their plain names, as Windows' does (Stained
 * Glass OS wine-sg 8264; test/mapiplain-gate.sh); it had only the x86 "@N" names, so a 64-bit program
 * importing HrGetOneProp, HrSetOneProp, HrQueryAllRows or FreeProws did not get them.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    static const char *names[][2] = { { "HrGetOneProp", "HrGetOneProp@12" }, { "HrSetOneProp", "HrSetOneProp@8" },
                                      { "HrQueryAllRows", "HrQueryAllRows@24" }, { "FreeProws", "FreeProws@4" },
                                      { "ScInitMapiUtil", "ScInitMapiUtil@4" } };
    HMODULE mapi = LoadLibraryA( "mapi32.dll" );
    void (WINAPI *pFreeProws)(void *);
    int i, fails = 0;

    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        FARPROC plain = GetProcAddress( mapi, names[i][0] ), deco = GetProcAddress( mapi, names[i][1] );
        BOOL ok = plain && plain == deco;
        printf( "%s  %s (the same function as %s)\n", ok ? "PASS" : "FAIL", names[i][0], names[i][1] );
        if (!ok) fails++;
    }
    if ((pFreeProws = (void *)GetProcAddress( mapi, "FreeProws" ))) pFreeProws( NULL );
    printf( "PASS  FreeProws(NULL) returns\n" );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
