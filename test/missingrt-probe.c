/* missingrt-gate.sh's probe: LoadLibrary of each plug-in named, as a program
 * registering its plug-ins does; prints "load NAME 0|1".
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
int main( int argc, char **argv )
{
    int i;
    for (i = 1; i < argc; i++) printf( "load %s %d\n", argv[i], LoadLibraryA( argv[i] ) != NULL );
    return 0;
}
