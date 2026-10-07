/* deskwatch-probe (test/deskwatch-gate.sh, wine-sg 1476): writes a file in the
 * user's Desktop folder N times (argv[1]), a little at a time and closed after
 * each write, as a download's .part file is -- each write is a change the
 * desktop's watcher hears. Prints "done N". */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char **argv )
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    int n = argc > 1 ? atoi( argv[1] ) : 1000, i;
    char block[512];

    memset( block, 'x', sizeof(block) );
    if (FAILED(SHGetFolderPathW( NULL, CSIDL_DESKTOPDIRECTORY | CSIDL_FLAG_CREATE, NULL, 0, dir ))) return 1;
    swprintf( path, MAX_PATH, L"%ls\\deskwatch-probe.part", dir );
    for (i = 0; i < n; i++)
    {
        HANDLE h = CreateFileW( path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL );
        DWORD done;
        if (h == INVALID_HANDLE_VALUE) { printf( "open %ls: error %lu\n", path, GetLastError() ); return 2; }
        WriteFile( h, block, sizeof(block), &done, NULL );
        CloseHandle( h );
        if (i % 50 == 49) Sleep( 30 );   /* let the watcher answer, as a download's pace does */
    }
    DeleteFileW( path );
    printf( "done %d\n", n );
    return 0;
}
