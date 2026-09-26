/* Per-program compatibility settings (wine-sg 0420): run as
 *   appcompat-probe report          -- print what this process was given
 *   appcompat-probe launch PATH     -- CreateProcess PATH, print the error
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *rtlgetversion)(OSVERSIONINFOW *);

int wmain( int argc, WCHAR **argv )
{
    if (argc >= 2 && !lstrcmpW( argv[1], L"report" ))
    {
        WCHAR buf[512];
        OSVERSIONINFOW v = { sizeof(v) };
        rtlgetversion get = (rtlgetversion)GetProcAddress( GetModuleHandleW( L"ntdll.dll" ), "RtlGetVersion" );
        printf( "VAR=%ls\n", GetEnvironmentVariableW( L"SGTEST_VAR", buf, 512 ) ? buf : L"(unset)" );
        printf( "DEL=%ls\n", GetEnvironmentVariableW( L"SGTEST_DEL", buf, 512 ) ? buf : L"(unset)" );
        printf( "CMDLINE=%ls\n", GetCommandLineW() );
        if (get) get( &v );
        printf( "VERSION=%lu.%lu\n", v.dwMajorVersion, v.dwMinorVersion );
        return 0;
    }
    if (argc >= 3 && !lstrcmpW( argv[1], L"launch" ))
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[MAX_PATH + 16];
        swprintf( cmd, MAX_PATH + 16, L"\"%ls\" report", argv[2] );
        if (CreateProcessW( argv[2], cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            WaitForSingleObject( pi.hProcess, 30000 );
            CloseHandle( pi.hThread ); CloseHandle( pi.hProcess );
            printf( "LAUNCH=ok\n" );
        }
        else printf( "LAUNCH=error %lu\n", GetLastError() );
        return 0;
    }
    return 2;
}
