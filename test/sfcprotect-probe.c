/* sfcprotect-probe: sfc_os SfcIsFileProtected (Stained Glass OS wine-sg 8261; test/sfcprotect-gate.sh): the
 * system's own modules in the system directory are protected, other files are not (it said "not protected"
 * for everything). pywin32 (Anki, BleachBit, Picard, NVDA) imports it.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static BOOL (WINAPI *pProtected)(HANDLE, const WCHAR *);
static int failures;
static void check( const WCHAR *path, BOOL want )
{
    BOOL got;
    SetLastError( 0xdead );
    got = pProtected( NULL, path );
    printf( "%s  %ls: %d (error %lu)\n", got == want ? "PASS" : "FAIL", path, got, got ? 0 : GetLastError() );
    if (got != want || (!got && GetLastError() != ERROR_FILE_NOT_FOUND)) failures++;
}

int main( void )
{
    WCHAR sys[MAX_PATH], path[MAX_PATH], tmp[MAX_PATH];
    HANDLE f;
    DWORD w;

    pProtected = (void *)GetProcAddress( LoadLibraryA( "sfc_os.dll" ), "SfcIsFileProtected" );
    GetSystemDirectoryW( sys, MAX_PATH );
    swprintf( path, MAX_PATH, L"%ls\\kernel32.dll", sys );
    check( path, TRUE );
    swprintf( path, MAX_PATH, L"%ls\\NOTEPAD.EXE", sys );
    check( path, TRUE );
    swprintf( path, MAX_PATH, L"%ls\\no-such-module.dll", sys );
    check( path, FALSE );
    /* a file of one's own in the system directory */
    swprintf( path, MAX_PATH, L"%ls\\sg-sfc-own.dll", sys );
    f = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( f, "MZ not wine", 11, &w, NULL );
    CloseHandle( f );
    check( path, FALSE );
    DeleteFileW( path );
    /* a copy of a system module elsewhere */
    GetTempPathW( MAX_PATH, tmp );
    wcscat( tmp, L"kernel32.dll" );
    swprintf( path, MAX_PATH, L"%ls\\kernel32.dll", sys );
    CopyFileW( path, tmp, FALSE );
    check( tmp, FALSE );
    DeleteFileW( tmp );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
