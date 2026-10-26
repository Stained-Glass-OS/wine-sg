/* Fonts written over C:\windows\Fonts's links to the system's fonts
 * (patches/sg/6546), beside test/msifont-gate.sh's MSI: CopyFile and an
 * overwrite (TRUNCATE_EXISTING) of a link's name write a font of their own;
 * a link elsewhere still writes through (scope); a per-user font install
 * (%LOCALAPPDATA%\Microsoft\Windows\Fonts + AddFontResourceEx) works. */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int main( int argc, char **argv )
{
    WCHAR local[MAX_PATH], dir[MAX_PATH], user[MAX_PATH];
    const char data[] = "the application's own font";
    HANDLE h;
    DWORD written, err;
    BOOL ok;

    ok = CopyFileW( L"C:\\payload.ttf", L"C:\\windows\\Fonts\\sgcopy.ttf", FALSE );
    printf( "copy_over_link=%d (err %lu)\n", ok, ok ? 0 : GetLastError() );

    h = CreateFileW( L"C:\\windows\\Fonts\\sgtrunc.ttf", GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, 0, NULL );
    err = GetLastError();
    ok = h != INVALID_HANDLE_VALUE && WriteFile( h, data, sizeof(data) - 1, &written, NULL );
    if (h != INVALID_HANDLE_VALUE) CloseHandle( h );
    printf( "truncate_over_link=%d (err %lu)\n", ok, h == INVALID_HANDLE_VALUE ? err : 0 );

    h = CreateFileW( L"C:\\other\\ro.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    err = GetLastError();
    printf( "other_link_unchanged=%d (err %lu)\n", h == INVALID_HANDLE_VALUE && err == ERROR_ACCESS_DENIED, err );
    if (h != INVALID_HANDLE_VALUE) CloseHandle( h );

    /* per user */
    SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, local );
    swprintf( dir, MAX_PATH, L"%ls\\Microsoft\\Windows\\Fonts", local );
    SHCreateDirectoryExW( NULL, dir, NULL );
    swprintf( user, MAX_PATH, L"%ls\\sguser.ttf", dir );
    ok = CopyFileW( L"C:\\payload.ttf", user, FALSE );
    printf( "user_font_copied=%d user_font_added=%d\n", ok, ok && AddFontResourceExW( user, 0, NULL ) > 0 );
    printf( "done=1\n" );
    return 0;
}
