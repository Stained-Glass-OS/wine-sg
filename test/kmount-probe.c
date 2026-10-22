/* kmount-probe: volumes mounted on folders (Stained Glass OS wine-sg 6013):
 * SetVolumeMountPoint, GetVolumeNameForVolumeMountPoint of the folder,
 * Find{First,Next}VolumeMountPoint, DeleteVolumeMountPoint. */
#include <windows.h>
#include <stdio.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

int main( void )
{
    WCHAR vol[MAX_PATH], vol2[MAX_PATH], name[MAX_PATH];
    HANDLE find, f;
    DWORD attr;
    BOOL ret;

    if (!GetVolumeNameForVolumeMountPointW( L"C:\\", vol, MAX_PATH ))
    {
        printf( "FAIL  no volume name for C:\\ (%lu)\nRESULT: FAIL\n", GetLastError() );
        return 1;
    }
    printf( "      C:\\ is %ls\n", vol );
    CreateDirectoryW( L"C:\\mnt", NULL );
    CreateDirectoryW( L"C:\\mnt\\cvol", NULL );
    CreateDirectoryW( L"C:\\mnt\\full", NULL );
    f = CreateFileW( L"C:\\mnt\\full\\x.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    CloseHandle( f );
    f = CreateFileW( L"C:\\kmount-marker.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    CloseHandle( f );

    SetLastError( 0xdeadbeef );
    check( !SetVolumeMountPointW( L"C:\\mnt\\cvol", vol ) && GetLastError() == ERROR_INVALID_NAME,
           "a mount point without its trailing backslash: ERROR_INVALID_NAME", GetLastError() );
    SetLastError( 0xdeadbeef );
    check( !SetVolumeMountPointW( L"C:\\mnt\\full\\", vol ) && GetLastError() == ERROR_DIR_NOT_EMPTY,
           "on a folder that is not empty: ERROR_DIR_NOT_EMPTY", GetLastError() );
    SetLastError( 0xdeadbeef );
    check( !SetVolumeMountPointW( L"C:\\mnt\\cvol\\", L"C:\\" ) && GetLastError() == ERROR_INVALID_PARAMETER,
           "a volume that is no volume name: ERROR_INVALID_PARAMETER", GetLastError() );

    ret = SetVolumeMountPointW( L"C:\\mnt\\cvol\\", vol );
    check( ret, "SetVolumeMountPoint", GetLastError() );
    attr = GetFileAttributesW( L"C:\\mnt\\cvol" );
    check( attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT) && (attr & FILE_ATTRIBUTE_DIRECTORY),
           "the folder is a mount point (a reparse point)", attr );
    check( GetFileAttributesW( L"C:\\mnt\\cvol\\kmount-marker.txt" ) != INVALID_FILE_ATTRIBUTES,
           "the volume's files are there", GetLastError() );
    ret = GetVolumeNameForVolumeMountPointW( L"C:\\mnt\\cvol\\", vol2, MAX_PATH );
    check( ret && !lstrcmpiW( vol, vol2 ), "GetVolumeNameForVolumeMountPoint of the folder", GetLastError() );

    find = FindFirstVolumeMountPointW( L"C:\\", name, MAX_PATH );
    check( find != INVALID_HANDLE_VALUE && !lstrcmpiW( name, L"mnt\\cvol\\" ), "FindFirstVolumeMountPoint finds mnt\\cvol\\", GetLastError() );
    if (find != INVALID_HANDLE_VALUE)
    {
        SetLastError( 0xdeadbeef );
        check( !FindNextVolumeMountPointW( find, name, MAX_PATH ) && GetLastError() == ERROR_NO_MORE_FILES,
               "and no more", GetLastError() );
        check( FindVolumeMountPointClose( find ), "FindVolumeMountPointClose", GetLastError() );
    }

    ret = DeleteVolumeMountPointW( L"C:\\mnt\\cvol\\" );
    check( ret, "DeleteVolumeMountPoint", GetLastError() );
    attr = GetFileAttributesW( L"C:\\mnt\\cvol" );
    check( attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_REPARSE_POINT) && (attr & FILE_ATTRIBUTE_DIRECTORY),
           "an empty folder stays in its place", attr );
    check( GetFileAttributesW( L"C:\\kmount-marker.txt" ) != INVALID_FILE_ATTRIBUTES, "the volume itself is untouched", 0 );
    SetLastError( 0xdeadbeef );
    find = FindFirstVolumeMountPointW( L"C:\\", name, MAX_PATH );
    check( find == INVALID_HANDLE_VALUE && GetLastError() == ERROR_NO_MORE_FILES, "no mount points any more", GetLastError() );
    SetLastError( 0xdeadbeef );
    check( !DeleteVolumeMountPointW( L"C:\\mnt\\cvol\\" ), "deleting it again fails", GetLastError() );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
