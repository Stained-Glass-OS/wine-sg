/* SetFileValidData needs SeManageVolumePrivilege, enabled (patches/sg/2230):
 * a writable handle without it is ERROR_PRIVILEGE_NOT_HELD; a read-only
 * handle is ERROR_ACCESS_DENIED first; with it the length checks apply. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BOOL set_priv(BOOL enable)
{
    HANDLE tok;
    TOKEN_PRIVILEGES tp;
    BOOL ok;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
    ok = OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tok ) &&
         LookupPrivilegeValueA( NULL, SE_MANAGE_VOLUME_NAME, &tp.Privileges[0].Luid ) &&
         AdjustTokenPrivileges( tok, FALSE, &tp, 0, NULL, NULL ) && GetLastError() != ERROR_NOT_ALL_ASSIGNED;
    if (tok) CloseHandle( tok );
    return ok;
}

int main(void)
{
    char path[MAX_PATH];
    HANDLE h;
    DWORD n, err;
    BOOL ret;

    GetTempPathA( MAX_PATH, path );
    lstrcatA( path, "sg_validdata_probe.tmp" );
    h = CreateFileA( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( h, "data", 4, &n, NULL );
    CloseHandle( h );

    set_priv( FALSE );
    h = CreateFileA( path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
    SetLastError( 0 );
    ret = SetFileValidData( h, 4 );
    check( !ret && GetLastError() == ERROR_ACCESS_DENIED, "read-only handle: ERROR_ACCESS_DENIED" );
    CloseHandle( h );

    h = CreateFileA( path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
    SetLastError( 0 );
    ret = SetFileValidData( h, 4 );
    err = GetLastError();
    check( !ret && err == ERROR_PRIVILEGE_NOT_HELD, "privilege not enabled: ERROR_PRIVILEGE_NOT_HELD" );
    CloseHandle( h );

    if (!set_priv( TRUE ))
        printf( "SKIP  cannot enable SeManageVolumePrivilege\n" );
    else
    {
        h = CreateFileA( path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
        SetLastError( 0 );
        ret = SetFileValidData( h, 0 );
        check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "enabled, length 0: ERROR_INVALID_PARAMETER" );
        SetLastError( 0 );
        ret = SetFileValidData( h, 8 );
        check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "enabled, length beyond the file: ERROR_INVALID_PARAMETER" );
        ret = SetFileValidData( h, 4 );
        check( ret, "enabled, length 4 of a 4-byte file: success" );
        CloseHandle( h );
        set_priv( FALSE );
        h = CreateFileA( path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
        SetLastError( 0 );
        ret = SetFileValidData( h, 4 );
        check( !ret && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "disabled again: ERROR_PRIVILEGE_NOT_HELD" );
        CloseHandle( h );
    }
    DeleteFileA( path );
    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
