/* bcdboot.exe in system32 (patches/sg/7907): Windows always has it, and Rufus reads its version
 * (GetFileVersionInfo + VS_FIXEDFILEINFO) to know its options -- with none there it read through a
 * NULL pointer at start. Prints key=1 per check. */
#include <windows.h>
#include <stdio.h>

static int version_of( const char *path, DWORD *ms, DWORD *ls )
{
    DWORD handle, size = GetFileVersionInfoSizeA( path, &handle );
    VS_FIXEDFILEINFO *info;
    UINT len;
    void *data;
    int ok = 0;

    if (!size || !(data = malloc( size ))) return 0;
    if (GetFileVersionInfoA( path, 0, size, data ) && VerQueryValueA( data, "\\", (void **)&info, &len ) && info)
    {
        *ms = info->dwFileVersionMS; *ls = info->dwFileVersionLS; ok = 1;
    }
    free( data );
    return ok;
}

int main(void)
{
    char sys[MAX_PATH], path[MAX_PATH], cmd[MAX_PATH * 2];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD ms = 0, ls = 0, code = 99;
    BOOL wow = FALSE;

    IsWow64Process( GetCurrentProcess(), &wow );
    GetSystemDirectoryA( sys, MAX_PATH );
    sprintf( path, "%s\\bcdboot.exe", sys );
    printf( "exists=%d\n", GetFileAttributesA( path ) != INVALID_FILE_ATTRIBUTES );
    printf( "version=%d\n", version_of( path, &ms, &ls ) && HIWORD(ms) == 10 );
    printf( "      %s %u.%u.%u.%u\n", path, HIWORD(ms), LOWORD(ms), HIWORD(ls), LOWORD(ls) );
    if (wow)
    {
        /* Rufus (64-bit) reads %windir%\sysnative from a 32-bit view the same way */
        GetWindowsDirectoryA( sys, MAX_PATH );
        sprintf( path, "%s\\sysnative\\bcdboot.exe", sys );
        printf( "sysnative=%d\n", version_of( path, &ms, &ls ) && HIWORD(ms) == 10 );
    }
    else printf( "sysnative=1\n" );
    sprintf( cmd, "\"%s\\bcdboot.exe\" C:\\windows /s C:", wow ? "C:\\windows\\sysnative" : sys );
    if (CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
    {
        WaitForSingleObject( pi.hProcess, 30000 );
        GetExitCodeProcess( pi.hProcess, &code );
        CloseHandle( pi.hProcess ); CloseHandle( pi.hThread );
    }
    printf( "fails=%d\n", code == 1 );
    printf( "done=1\n" );
    return 0;
}
