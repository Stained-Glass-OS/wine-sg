/* sh32small-probe: SHSetFolderPathA/W, PathIsSlowA/W and SHTestTokenMembership (shell32 ordinals 231/232,
 * 239/240, 245; Stained Glass OS wine-sg 8242; test/sh32small-gate.sh). They were spec stubs.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    HMODULE sh = LoadLibraryA( "shell32.dll" );
    HRESULT (WINAPI *pSetW)(int, HANDLE, DWORD, const WCHAR *) = (void *)GetProcAddress( sh, (const char *)232 );
    HRESULT (WINAPI *pSetA)(int, HANDLE, DWORD, const char *) = (void *)GetProcAddress( sh, (const char *)231 );
    BOOL (WINAPI *pSlowW)(const WCHAR *, DWORD) = (void *)GetProcAddress( sh, (const char *)239 );
    BOOL (WINAPI *pSlowA)(const char *, DWORD) = (void *)GetProcAddress( sh, (const char *)240 );
    BOOL (WINAPI *pTest)(HANDLE, ULONG) = (void *)GetProcAddress( sh, (const char *)245 );
    WCHAR orig[MAX_PATH], now[MAX_PATH], file[MAX_PATH];
    char nowA[MAX_PATH];
    HANDLE f;
    HRESULT hr;

    SHGetFolderPathW( NULL, CSIDL_TEMPLATES, NULL, SHGFP_TYPE_CURRENT, orig );
    CreateDirectoryW( L"C:\\sg-templates-moved", NULL );
    hr = pSetW( CSIDL_TEMPLATES, NULL, 0, L"C:\\sg-templates-moved" );
    SHGetFolderPathW( NULL, CSIDL_TEMPLATES, NULL, SHGFP_TYPE_CURRENT, now );
    check( hr == S_OK && !lstrcmpiW( now, L"C:\\sg-templates-moved" ), "SHSetFolderPathW moves CSIDL_TEMPLATES", hr );
    hr = pSetA( CSIDL_TEMPLATES | CSIDL_FLAG_DONT_VERIFY, NULL, 0, "C:\\sg-templates-a" );
    SHGetFolderPathA( NULL, CSIDL_TEMPLATES | CSIDL_FLAG_DONT_VERIFY, NULL, SHGFP_TYPE_CURRENT, nowA );
    check( hr == S_OK && !lstrcmpiA( nowA, "C:\\sg-templates-a" ), "SHSetFolderPathA (CSIDL flags ignored)", hr );
    pSetW( CSIDL_TEMPLATES, NULL, 0, orig );
    check( pSetW( CSIDL_WINDOWS, NULL, 0, L"C:\\x" ) == E_INVALIDARG, "a fixed folder (Windows): E_INVALIDARG", 0 );
    check( pSetW( 0x7f, NULL, 0, L"C:\\x" ) == E_INVALIDARG, "an unknown CSIDL: E_INVALIDARG", 0 );

    GetTempPathW( MAX_PATH, file );
    lstrcatW( file, L"sg-slow.txt" );
    f = CreateFileW( file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    CloseHandle( f );
    check( !pSlowW( file, INVALID_FILE_ATTRIBUTES ) && !pSlowA( "C:\\windows", INVALID_FILE_ATTRIBUTES ),
           "PathIsSlow: a local file or folder is not", 0 );
    check( pSlowW( file, FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_ARCHIVE ), "an offline file is (given attributes)", 0 );
    DeleteFileW( file );

    check( pTest( NULL, DOMAIN_ALIAS_RID_ADMINS ) == IsUserAnAdmin(), "SHTestTokenMembership: Administrators as IsUserAnAdmin", IsUserAnAdmin() );
    check( !pTest( NULL, DOMAIN_ALIAS_RID_GUESTS ), "not a member of Guests", 0 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
