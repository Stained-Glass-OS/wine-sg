/* bigreserve-probe: reserve 16 x 32 GB of inaccessible address space, the
 * way Chromium's V8 and PartitionAlloc do in every renderer, then release
 * it, holding after each step for the gate to read our memory (wine-sg 0432).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    void *p[16];
    int i, ok = 0;
    char *c;

    setvbuf( stdout, NULL, _IONBF, 0 );
    printf( "START\n" );
    Sleep( 3000 );
    for (i = 0; i < 16; i++)
        if ((p[i] = VirtualAlloc( NULL, (SIZE_T)32 << 30, MEM_RESERVE, PAGE_NOACCESS ))) ok++;
    printf( "RESERVED %d\n", ok );
    /* still usable: commit and touch a page in the middle of one */
    c = VirtualAlloc( (char *)p[3] + ((SIZE_T)16 << 30), 4096, MEM_COMMIT, PAGE_READWRITE );
    if (c) { c[0] = 42; printf( "COMMITTED %d\n", c[0] ); }
    printf( "PROTECT_OK %d\n", !IsBadReadPtr( c, 1 ) && IsBadReadPtr( (char *)p[3] + 4096, 1 ) );
    Sleep( 4000 );
    for (i = 0; i < 16; i++) if (p[i]) VirtualFree( p[i], 0, MEM_RELEASE );
    printf( "RELEASED\n" );
    Sleep( 4000 );
    printf( "DONE\n" );
    return 0;
}
