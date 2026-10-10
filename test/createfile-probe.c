/* CreateFile and OpenFile edge cases (patches/sg/2230): TRUNCATE_EXISTING
 * needs GENERIC_WRITE itself; CREATE_NEW on an existing directory is
 * ERROR_ACCESS_DENIED (a file cannot be made of it), with backup semantics
 * it is ERROR_FILE_EXISTS; a failed OpenFile leaves OFSTRUCT.cBytes alone. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    char dir[MAX_PATH], file[MAX_PATH];
    HANDLE h;
    OFSTRUCT ofs;
    HFILE hf;
    DWORD n;

    GetTempPathA( MAX_PATH, dir );
    strcpy( file, dir ); strcat( file, "sg_createfile_probe.tmp" );
    h = CreateFileA( file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( h, "abcd", 4, &n, NULL );
    CloseHandle( h );

    SetLastError( 0 );
    h = CreateFileA( file, GENERIC_READ, 0, NULL, TRUNCATE_EXISTING, 0, NULL );
    check( h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "TRUNCATE_EXISTING with GENERIC_READ: ERROR_INVALID_PARAMETER" );
    SetLastError( 0 );
    h = CreateFileA( file, 0, 0, NULL, TRUNCATE_EXISTING, 0, NULL );
    check( h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "TRUNCATE_EXISTING with no access: ERROR_INVALID_PARAMETER" );
    SetLastError( 0 );
    h = CreateFileA( file, FILE_WRITE_DATA, 0, NULL, TRUNCATE_EXISTING, 0, NULL );
    check( h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "TRUNCATE_EXISTING with FILE_WRITE_DATA only: ERROR_INVALID_PARAMETER" );
    check( GetFileAttributesA( file ) != INVALID_FILE_ATTRIBUTES, "the file is still there" );
    h = CreateFileA( file, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
    n = GetFileSize( h, NULL );
    CloseHandle( h );
    check( n == 4, "and was not truncated" );
    h = CreateFileA( file, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, 0, NULL );
    check( h != INVALID_HANDLE_VALUE, "TRUNCATE_EXISTING with GENERIC_WRITE works" );
    CloseHandle( h );
    check( GetFileAttributesA( file ) != INVALID_FILE_ATTRIBUTES, "(file still there)" );

    SetLastError( 0 );
    h = CreateFileA( dir, GENERIC_READ, 0, NULL, CREATE_NEW, 0, NULL );
    check( h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "CREATE_NEW on a directory: ERROR_ACCESS_DENIED" );
    SetLastError( 0 );
    h = CreateFileA( dir, GENERIC_READ, 0, NULL, CREATE_NEW, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    check( h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS, "CREATE_NEW on a directory with backup semantics: ERROR_FILE_EXISTS" );
    SetLastError( 0 );
    h = CreateFileA( file, GENERIC_READ, 0, NULL, CREATE_NEW, 0, NULL );
    check( h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS, "CREATE_NEW on a file: ERROR_FILE_EXISTS" );

    memset( &ofs, 0xA5, sizeof(ofs) );
    hf = OpenFile( "C:\\sg_no_such_file_here.tmp", &ofs, OF_EXIST );
    check( hf == HFILE_ERROR && ofs.cBytes == 0xA5 && ofs.nErrCode == ERROR_FILE_NOT_FOUND, "a failed OpenFile leaves cBytes alone and sets nErrCode" );
    memset( &ofs, 0xA5, sizeof(ofs) );
    hf = OpenFile( file, &ofs, OF_READ );
    check( hf != HFILE_ERROR && ofs.cBytes == sizeof(OFSTRUCT), "a successful OpenFile sets cBytes" );
    if (hf != HFILE_ERROR) _lclose( hf );
    memset( &ofs, 0xA5, sizeof(ofs) );
    hf = OpenFile( file, &ofs, OF_PARSE );
    check( hf == 0 && ofs.cBytes == sizeof(OFSTRUCT), "OF_PARSE sets cBytes" );

    DeleteFileA( file );
    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
