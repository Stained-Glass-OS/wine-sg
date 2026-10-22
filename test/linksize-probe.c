/* linksize-probe: a symbolic link to a file has no size of its own
 * (GetFileAttributesEx), opening it reaches the target, stat() follows it
 * (Stained Glass OS wine-sg 6001, LINK-SIZE)
 *   linksize-probe.exe DIR   (DIR holds "unixlink", a link made by ln -s to DIR\target.txt) */
#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>

static int failures;

static void check( int ok, const char *what, unsigned long long got, unsigned long long expect )
{
    if (ok) printf( "ok    %s (%llu)\n", what, got );
    else { printf( "FAIL  %s: got %#llx, expected %#llx\n", what, got, expect ); failures++; }
}

static void attrs( const WCHAR *path, const char *name, int link, unsigned long long size )
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    char what[160];
    BOOL r = GetFileAttributesExW( path, GetFileExInfoStandard, &fad );

    snprintf( what, sizeof(what), "%s: GetFileAttributesEx succeeds", name );
    check( r, what, r, 1 );
    if (!r) return;
    snprintf( what, sizeof(what), "%s: reparse point attribute", name );
    check( !!(fad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == link, what,
           fad.dwFileAttributes, link ? FILE_ATTRIBUTE_REPARSE_POINT : 0 );
    snprintf( what, sizeof(what), "%s: GetFileAttributesEx size", name );
    check( (((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow) == size, what,
           ((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow, size );
}

static void opened( const WCHAR *path, const char *name, DWORD flags, unsigned long long size )
{
    char what[160];
    LARGE_INTEGER li = {0};
    HANDLE h = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, flags, NULL );

    snprintf( what, sizeof(what), "%s: CreateFile%s", name, flags ? " (open reparse point)" : "" );
    check( h != INVALID_HANDLE_VALUE, what, GetLastError(), 0 );
    if (h == INVALID_HANDLE_VALUE) return;
    GetFileSizeEx( h, &li );
    snprintf( what, sizeof(what), "%s: GetFileSizeEx of the handle%s", name, flags ? " (open reparse point)" : "" );
    check( (unsigned long long)li.QuadPart == size, what, li.QuadPart, size );
    CloseHandle( h );
}

static void stat_size( const WCHAR *path, const char *name, unsigned long long size )
{
    struct _stat64 st;
    char what[160];
    int r = _wstat64( path, &st );

    snprintf( what, sizeof(what), "%s: _wstat64 follows the link", name );
    check( !r && (unsigned long long)st.st_size == size && (st.st_mode & _S_IFREG), what, r ? -1 : st.st_size, size );
}

int wmain( int argc, WCHAR **argv )
{
    WCHAR dir[MAX_PATH], target[MAX_PATH], link[MAX_PATH], dlink[MAX_PATH], unixlink[MAX_PATH], sub[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA fad;
    HANDLE h;
    DWORD written;

    if (argc < 2) return 2;
    lstrcpyW( dir, argv[1] );
    swprintf( target, MAX_PATH, L"%ls\\target.txt", dir );
    swprintf( link, MAX_PATH, L"%ls\\link.txt", dir );
    swprintf( dlink, MAX_PATH, L"%ls\\dirlink", dir );
    swprintf( sub, MAX_PATH, L"%ls\\sub", dir );
    swprintf( unixlink, MAX_PATH, L"%ls\\unixlink", dir );

    h = CreateFileW( target, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( h, "seven!!", 7, &written, NULL );
    CloseHandle( h );
    CreateDirectoryW( sub, NULL );

    check( CreateSymbolicLinkW( link, target, 0 ), "CreateSymbolicLink (file)", GetLastError(), 0 );
    check( CreateSymbolicLinkW( dlink, sub, SYMBOLIC_LINK_FLAG_DIRECTORY ), "CreateSymbolicLink (directory)", GetLastError(), 0 );

    attrs( target, "target", 0, 7 );
    attrs( link, "file link", 1, 0 );
    attrs( unixlink, "link made on the Unix side", 1, 0 );
    opened( link, "file link", 0, 7 );
    opened( unixlink, "link made on the Unix side", 0, 7 );
    stat_size( link, "file link", 7 );
    stat_size( unixlink, "link made on the Unix side", 7 );
    stat_size( target, "target", 7 );

    GetFileAttributesExW( dlink, GetFileExInfoStandard, &fad );
    check( (fad.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ==
           (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT), "directory link: directory and reparse point",
           fad.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT );

    DeleteFileW( link );
    RemoveDirectoryW( dlink );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
