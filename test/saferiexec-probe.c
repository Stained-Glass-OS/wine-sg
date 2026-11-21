/* saferiexec-probe: advapi32 SaferiIsExecutableFileType, which was not exported (Stained Glass OS wine-sg 8257;
 * test/saferiexec-gate.sh): every Temurin/Corretto JDK's awt.dll imports it.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static BOOL (WINAPI *pIsExec)(const WCHAR *, BOOLEAN);
static void check( const WCHAR *path, BOOLEAN shell, BOOL want )
{
    BOOL got = pIsExec( path, shell );
    printf( "%s  %ls (shell %d): %d\n", got == want ? "PASS" : "FAIL", path, shell, got );
    if (got != want) failures++;
}

int main( void )
{
    HKEY key;
    pIsExec = (void *)GetProcAddress( LoadLibraryA( "advapi32.dll" ), "SaferiIsExecutableFileType" );
    if (!pIsExec) { printf( "FAIL  advapi32 exports SaferiIsExecutableFileType\nRESULT: FAIL\n" ); return 1; }
    printf( "PASS  advapi32 exports SaferiIsExecutableFileType\n" );
    check( L"C:\\x\\setup.exe", FALSE, TRUE );
    check( L"C:\\x\\setup.EXE", TRUE, FALSE );
    check( L"run.bat", TRUE, TRUE );
    check( L"C:\\docs\\note.txt", FALSE, FALSE );
    check( L"C:\\docs\\link.LNK", FALSE, TRUE );
    check( L"C:\\dir.exe\\noext", FALSE, FALSE );
    check( L"", FALSE, FALSE );
    /* a policy's own list replaces the default one */
    RegCreateKeyA( HKEY_LOCAL_MACHINE, "Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers", &key );
    RegSetValueExA( key, "ExecutableTypes", 0, REG_MULTI_SZ, (const BYTE *)"TXT\0EXE\0\0", 10 );
    check( L"C:\\docs\\note.txt", FALSE, TRUE );
    check( L"run.bat", FALSE, FALSE );
    RegDeleteValueA( key, "ExecutableTypes" );
    RegCloseKey( key );
    check( L"run.bat", FALSE, TRUE );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
