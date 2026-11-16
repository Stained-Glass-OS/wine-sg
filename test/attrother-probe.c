/* A SYSTEM service clearing the read-only flag of a user's file that has none, then deleting it
 * (wine-sg 7794, GOG GALAXY's service), for test/attrother-gate.sh: another account owns PATH.
 *   attrother-probe.exe PATH   prints "clear=OK|ERR n" and "delete=OK|ERR n"
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    if (argc < 2) return 2;
    if (SetFileAttributesW( argv[1], FILE_ATTRIBUTE_NORMAL )) printf( "clear=OK\n" );
    else printf( "clear=ERR %lu\n", GetLastError() );
    if (DeleteFileW( argv[1] )) printf( "delete=OK\n" );
    else printf( "delete=ERR %lu\n", GetLastError() );
    return 0;
}
