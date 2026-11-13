/* test/sparseaumid-gate.sh's probe (7755): a packaged program's application user model ID when no
 * app execution alias names it.
 *   self         GetCurrentApplicationUserModelId, and GetApplicationUserModelId of itself
 *   of PATH      GetApplicationUserModelId of PATH started suspended
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *current_fn)( UINT32 *, WCHAR * );
typedef LONG (WINAPI *process_fn)( HANDLE, UINT32 *, WCHAR * );

int wmain( int argc, WCHAR **argv )
{
    HMODULE k32 = GetModuleHandleW( L"kernel32.dll" );
    current_fn current = (current_fn)GetProcAddress( k32, "GetCurrentApplicationUserModelId" );
    process_fn of_process = (process_fn)GetProcAddress( k32, "GetApplicationUserModelId" );
    WCHAR id[256];
    UINT32 len = ARRAYSIZE(id);
    LONG ret;

    if (argc < 2 || !current || !of_process) return 1;
    if (!wcscmp( argv[1], L"self" ))
    {
        ret = current( &len, id );
        printf( "Current=%ld %ls\n", ret, ret ? L"-" : id );
        len = ARRAYSIZE(id);
        ret = of_process( GetCurrentProcess(), &len, id );
        printf( "OfSelf=%ld %ls\n", ret, ret ? L"-" : id );
    }
    else if (!wcscmp( argv[1], L"of" ) && argc > 2)
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (!CreateProcessW( argv[2], NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi ))
        {
            printf( "Start=%lu\n", GetLastError() );
            return 0;
        }
        ret = of_process( pi.hProcess, &len, id );
        printf( "Of=%ld %ls\n", ret, ret ? L"-" : id );
        TerminateProcess( pi.hProcess, 0 );
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
    }
    return 0;
}
