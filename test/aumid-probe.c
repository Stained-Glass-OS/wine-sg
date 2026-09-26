/* GetCurrentApplicationUserModelId / GetApplicationUserModelId (wine-sg 0423):
 *   aumid-probe            -- deploy a test package (registry + app execution
 *                             alias), start a copy of itself inside it, report
 *   aumid-probe child      -- report its own AUMID
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *current_fn)(UINT32 *, WCHAR *);
typedef LONG (WINAPI *process_fn)(HANDLE, UINT32 *, WCHAR *);

#define FAMILY  L"SgTest.Aumid_8wekyb3d8bbwe"
#define FULL    L"SgTest.Aumid_1.0.0.0_x64__8wekyb3d8bbwe"
#define AUMID   FAMILY L"!App"

int wmain( int argc, WCHAR **argv )
{
    HMODULE k32 = GetModuleHandleW( L"kernel32.dll" );
    current_fn current = (current_fn)GetProcAddress( k32, "GetCurrentApplicationUserModelId" );
    process_fn of_process = (process_fn)GetProcAddress( k32, "GetApplicationUserModelId" );
    WCHAR id[256], self[MAX_PATH], alias[4096];
    UINT32 len;
    LONG ret;
    HKEY key;

    if (!current || !of_process) { printf( "MISSING %d %d\n", !!current, !!of_process ); return 1; }
    if (argc > 1 && !lstrcmpW( argv[1], L"child" ))
    {
        len = ARRAYSIZE(id);
        ret = current( &len, id );
        printf( "CHILD %ld %ls\n", ret, ret ? L"" : id );
        len = 3;
        ret = current( &len, id );
        printf( "CHILD_SMALL %ld %u\n", ret, len );
        return 0;
    }

    len = ARRAYSIZE(id);
    printf( "UNPACKAGED %ld\n", current( &len, id ) );

    /* A deployed package, as appxdeploymentclient records one. */
    GetModuleFileNameW( NULL, self, MAX_PATH );
    CreateDirectoryW( L"C:\\sgpkg", NULL );
    CreateDirectoryW( L"C:\\sgpkg\\App", NULL );
    CreateDirectoryW( L"C:\\sgalias", NULL );
    CopyFileW( self, L"C:\\sgpkg\\App\\app.exe", FALSE );
    {
        static const WCHAR text[] = L"\xfeff[Stained Glass AppExecLink]\r\nPackageFamilyName=" FAMILY
            L"\r\nAppUserModelId=" AUMID L"\r\nTarget=C:\\sgpkg\\App\\app.exe\r\n";
        HANDLE f = CreateFileW( L"C:\\sgalias\\app.exe", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
        DWORD n;
        WriteFile( f, text, sizeof(text) - sizeof(WCHAR), &n, NULL );
        CloseHandle( f );
    }
    RegCreateKeyExW( HKEY_CURRENT_USER, L"Software\\Wine\\AppModel\\Packages\\" FULL, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL );
    RegSetValueExW( key, L"InstallLocation", 0, REG_SZ, (BYTE *)L"C:\\sgpkg\\App", sizeof(L"C:\\sgpkg\\App") );
    memset( alias, 0, sizeof(alias) );
    lstrcpyW( alias, L"C:\\sgalias\\app.exe" );
    RegSetValueExW( key, L"Aliases", 0, REG_MULTI_SZ, (BYTE *)alias, (lstrlenW( alias ) + 2) * sizeof(WCHAR) );
    RegCloseKey( key );

    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[] = L"C:\\sgpkg\\App\\app.exe child";
        if (!CreateProcessW( L"C:\\sgpkg\\App\\app.exe", cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi ))
        { printf( "LAUNCH err %lu\n", GetLastError() ); return 1; }
        len = ARRAYSIZE(id);
        ret = of_process( pi.hProcess, &len, id );
        printf( "OFPROCESS %ld %ls\n", ret, ret ? L"" : id );
        fflush( stdout );
        ResumeThread( pi.hThread );
        WaitForSingleObject( pi.hProcess, 30000 );
        CloseHandle( pi.hThread ); CloseHandle( pi.hProcess );
    }
    len = ARRAYSIZE(id);
    ret = of_process( GetCurrentProcess(), &len, id );
    printf( "SELF %ld\n", ret );
    return 0;
}
