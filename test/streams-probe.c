/* Named streams on a volume without them (wine-sg 0422).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static void try_create( const WCHAR *name, DWORD disp )
{
    HANDLE h = CreateFileW( name, GENERIC_READ | GENERIC_WRITE, 0, NULL, disp, 0, NULL );
    if (h == INVALID_HANDLE_VALUE) printf( "err %lu\n", GetLastError() );
    else { printf( "ok\n" ); CloseHandle( h ); }
}

int wmain( int argc, WCHAR **argv )
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], buf[64];
    DWORD flags = 0, n;
    HANDLE h;
    WIN32_FIND_DATAW fd;
    GetTempPathW( MAX_PATH, dir );
    lstrcatW( dir, L"sg-streams" );
    CreateDirectoryW( dir, NULL );
    swprintf( path, MAX_PATH, L"%ls\\file.txt", dir );
    h = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( h, "hello", 5, &n, NULL ); CloseHandle( h );

    GetVolumeInformationW( L"C:\\", NULL, 0, NULL, NULL, &flags, NULL, 0 );
    printf( "NAMED_STREAMS=%d\n", !!(flags & FILE_NAMED_STREAMS) );

    swprintf( path, MAX_PATH, L"%ls\\file.txt:Zone.Identifier", dir );
    printf( "STREAM_CREATE=" ); try_create( path, CREATE_ALWAYS );
    printf( "STREAM_OPEN=" ); try_create( path, OPEN_EXISTING );

    n = 0;
    swprintf( path, MAX_PATH, L"%ls\\*", dir );
    if ((h = FindFirstFileW( path, &fd )) != INVALID_HANDLE_VALUE)
    {
        do if (fd.cFileName[0] != '.') n++; while (FindNextFileW( h, &fd ));
        FindClose( h );
    }
    printf( "ENTRIES=%lu\n", n );

    swprintf( path, MAX_PATH, L"%ls\\file.txt::$DATA", dir );
    h = CreateFileW( path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
    if (h != INVALID_HANDLE_VALUE && ReadFile( h, buf, 5, &n, NULL ))
    { ((char *)buf)[n] = 0; printf( "DATA=%s\n", (char *)buf ); CloseHandle( h ); }
    else printf( "DATA=err %lu\n", GetLastError() );

    swprintf( path, MAX_PATH, L"%ls\\plain.txt", dir );
    printf( "PLAIN_CREATE=" ); try_create( path, CREATE_ALWAYS );
    return 0;
}
