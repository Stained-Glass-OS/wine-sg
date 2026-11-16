/* SYSTEM sets the read-only flag of a file another account owns (wine-sg 7795), for test/attrsys-gate.sh.
 *   attrsys-probe.exe ro|normal PATH   prints "set=OK|ERR n attr=0x.."
 *   attrsys-probe.exe make PATH        makes PATH (as whoever runs it): "made 1"
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    HANDLE h;
    DWORD done;

    if (argc < 3) return 2;
    if (!wcscmp( argv[1], L"make" ))
    {
        h = CreateFileW( argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
        printf( "made %d\n", h != INVALID_HANDLE_VALUE && WriteFile( h, "x", 1, &done, NULL ) );
        CloseHandle( h );
        return 0;
    }
    if (SetFileAttributesW( argv[2], !wcscmp( argv[1], L"ro" ) ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL ))
        printf( "set=OK attr=%#lx\n", GetFileAttributesW( argv[2] ) );
    else
        printf( "set=ERR %lu attr=%#lx\n", GetLastError(), GetFileAttributesW( argv[2] ) );
    return 0;
}
